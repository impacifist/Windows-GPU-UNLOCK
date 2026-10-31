# Run in a fresh PowerShell process. Fake Native has no P/Invoke, so real entry script cannot touch firmware.
$ErrorActionPreference='Stop'
if('SharedEspFirmware.Native' -as [type]){throw 'Run this fixture in a fresh PowerShell process'}
Add-Type @'
using System;
using System.Collections.Generic;
namespace SharedEspFirmware {
 public sealed class Value {public byte[] Data;public uint Attributes;}
 public static class Native {
  public static Dictionary<string,Value> Values=new Dictionary<string,Value>();
  public static List<string> Writes=new List<string>();
  sealed class Scope:IDisposable {public void Dispose(){}}
  public static IDisposable EnablePrivilege(){return new Scope();}
  public static Value Read(string name){Value value;return Values.TryGetValue(name,out value)?value:null;}
  public static void Write(string name,byte[] data,uint attributes){if(data==null && attributes!=7)throw new Exception("Deletion lost saved attributes");Writes.Add(name);if(data==null)Values.Remove(name);else Values[name]=new Value{Data=data,Attributes=attributes};}
 }
}
'@
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$fixture=Join-Path $repo ('research/profile/build/second-register-entry-'+[guid]::NewGuid().ToString('N'))
$global:entryEsp=Join-Path $fixture 'esp'
[void](New-Item -ItemType Directory -Path (Join-Path $global:entryEsp 'EFI/Microsoft/Boot'),(Join-Path $global:entryEsp 'EFI/limine'),(Join-Path $global:entryEsp 'RTX3090-UNLOCK') -Force)
foreach($name in @('EFI/EVGA-SHARED-LAUNCH02.EFI','EFI/EVGA-SHARED-UNLOCK02.EFI','EFI/Microsoft/Boot/bootmgfw.efi','EFI/limine/limine_x64.efi')){Set-Content -LiteralPath (Join-Path $global:entryEsp $name) -Value 'file fixture'}
$guid=[guid]'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'
function New-FixtureOption([string]$Path){
 $p=[Text.Encoding]::Unicode.GetBytes($Path+[char]0);$m=New-Object IO.MemoryStream;$w=New-Object IO.BinaryWriter($m)
 $w.Write([uint32]1);$w.Write([uint16](42+4+$p.Length+4));$w.Write([Text.Encoding]::Unicode.GetBytes("fixture`0"))
 $w.Write([byte[]](4,1,42,0));$w.Write([uint32]1);$w.Write([uint64]2048);$w.Write([uint64]2097152);$w.Write($guid.ToByteArray());$w.Write([byte[]](2,2))
 $w.Write([byte[]](4,4));$w.Write([uint16](4+$p.Length));$w.Write($p);$w.Write([byte[]](127,255,4,0));$w.Flush()
 $v=New-Object SharedEspFirmware.Value;$v.Data=$m.ToArray();$v.Attributes=7;$w.Dispose();$m.Dispose();return $v
}
function Set-FixtureValue([string]$Name,[byte[]]$Bytes,[uint32]$Attributes=7){$v=New-Object SharedEspFirmware.Value;$v.Data=$Bytes;$v.Attributes=$Attributes;[SharedEspFirmware.Native]::Values[$Name]=$v}
Set-FixtureValue 'BootOrder' ([byte[]](0,0,1,0));Set-FixtureValue 'BootCurrent' ([byte[]](0,0));Set-FixtureValue 'SecureBoot' ([byte[]](0)) 6
[SharedEspFirmware.Native]::Values['Boot0000']=New-FixtureOption '\EFI\limine\limine_x64.efi'
[SharedEspFirmware.Native]::Values['Boot0001']=New-FixtureOption '\EFI\Microsoft\Boot\bootmgfw.efi'
$global:entryBadHash=$false;$global:entryBadChild=$false
function Get-Partition {param([string]$DriveLetter) if($DriveLetter){[pscustomobject]@{Guid='11111111-2222-3333-4444-555555555555'}}else{[pscustomobject]@{Guid='c36c3e86-02aa-4f76-af1b-96e327f9d1c1';IsSystem=$true;DiskNumber=12;PartitionNumber=1;Offset=[uint64]1048576;Size=[uint64]1073741824}}}
function Get-Disk {param($Number) [pscustomobject]@{PartitionStyle='GPT';BusType='NVMe';LogicalSectorSize=512}}
function Get-Volume {param([Parameter(ValueFromPipeline)]$InputObject) process {[pscustomobject]@{FileSystem='FAT32';Path=$global:entryEsp}}}
function Get-FileHash {param([string]$LiteralPath)
 if($LiteralPath -like '*EVGA-SHARED-LAUNCH02.EFI'){return [pscustomobject]@{Hash=if($global:entryBadHash){'0'*64}else{'258a93fdf0eb526331f25b562c13a36176980ecc594825c8b4c48b0ec5604bec'}}}
 if($LiteralPath -like '*EVGA-SHARED-UNLOCK02.EFI'){return [pscustomobject]@{Hash=if($global:entryBadChild){'0'*64}else{'a15bade7806747f5cec76c411278d6d797d6f932f5d532eaf283add02d6409bf'}}}
 if($LiteralPath -like '*bootmgfw.efi'){return [pscustomobject]@{Hash='2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'}}
 if($LiteralPath -like '*limine_x64.efi'){return [pscustomobject]@{Hash='a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462'}}
 Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath
}
$entry=Join-Path $PSScriptRoot 'register-shared-esp-second.ps1';$state=Join-Path $fixture 'state'
$plan=& $entry -StateDirectory $state -WhatIf | ConvertFrom-Json
if($plan.registered -or (Test-Path $state) -or [SharedEspFirmware.Native]::Writes.Count){throw 'Real entry WhatIf wrote state or bypassed local-drive regex'}
$prepared=& $entry -StateDirectory $state | ConvertFrom-Json
if(!$prepared.registered -or $null -ne $prepared.boot_next -or $prepared.attempt_armed -or $prepared.boot_authorized -or !$prepared.limine_first_verified -or !$prepared.windows_entry_verified -or !(Test-Path (Join-Path $state 'firmware-backup.json'))){throw 'Real Prepare did not create valid backup/selection'}
$status=& $entry -Action Status -StateDirectory $state | ConvertFrom-Json
if(!$status.boot_order_unchanged -or !$status.temporary_option_matches -or $null -ne $status.boot_next){throw 'Status did not read actual mocked unselected state'}
$clean=& $entry -Action Cleanup -StateDirectory $state | ConvertFrom-Json
if($clean.temporary_option_present -or $null -ne $clean.boot_next -or !$clean.boot_order_unchanged){throw 'Cleanup did not cancel before boot'}
# An occupied option must be skipped, preserving its exact bytes.
Set-FixtureValue 'Boot7E00' ([byte[]](99,98))
$state2=Join-Path $fixture 'occupied-slot';$prepared=& $entry -StateDirectory $state2 | ConvertFrom-Json
if($prepared.option -cne 'Boot7E01' -or [SharedEspFirmware.Native]::Values['Boot7E00'].Data[0] -ne 99){throw 'Occupied option overwritten'}
$clean=& $entry -Action Cleanup -StateDirectory $state2 | ConvertFrom-Json
if($clean.temporary_option_present -or ![SharedEspFirmware.Native]::Values.ContainsKey('Boot7E00')){throw 'Cleanup touched foreign occupied slot'}
[void][SharedEspFirmware.Native]::Values.Remove('Boot7E00')
$writes=[SharedEspFirmware.Native]::Writes.Count
$blocked=$false;try{& $entry -StateDirectory $state}catch{$blocked=$true}
if(!$blocked -or [SharedEspFirmware.Native]::Writes.Count -ne $writes){throw 'Existing backup reused'}
Set-Content -LiteralPath (Join-Path $global:entryEsp 'RTX3090-UNLOCK/SULK02.ARMED') -Value 'existing'
$blocked=$false;try{& $entry -StateDirectory (Join-Path $fixture 'already-armed')}catch{$blocked=$true}
if(!$blocked -or [SharedEspFirmware.Native]::Writes.Count -ne $writes){throw 'Armed attempt accepted'}
Remove-Item -LiteralPath (Join-Path $global:entryEsp 'RTX3090-UNLOCK/SULK02.ARMED')
$global:entryBadHash=$true;$writes=[SharedEspFirmware.Native]::Writes.Count;$blocked=$false
try{& $entry -StateDirectory (Join-Path $fixture 'bad-hash')}catch{$blocked=$true}
if(!$blocked -or [SharedEspFirmware.Native]::Writes.Count -ne $writes){throw 'Bad installed audit hash was not refused before writes'}
$global:entryBadHash=$false;$global:entryBadChild=$true
$blocked=$false;try{& $entry -StateDirectory (Join-Path $fixture 'bad-child')}catch{$blocked=$true}
if(!$blocked -or [SharedEspFirmware.Native]::Writes.Count -ne $writes){throw 'Bad child hash was not refused'}
$global:entryBadChild=$false
Set-FixtureValue 'BootNext' ([byte[]](3,0));$blocked=$false
try{& $entry -StateDirectory (Join-Path $fixture 'existing-next')}catch{$blocked=$true}
if(!$blocked -or [SharedEspFirmware.Native]::Writes.Count -ne $writes){throw 'Real entry replaced existing BootNext'}
[void][SharedEspFirmware.Native]::Values.Remove('BootNext')
Set-FixtureValue 'SecureBoot' ([byte[]](1)) 6;$blocked=$false
try{& $entry -StateDirectory (Join-Path $fixture 'secure-boot-enabled')}catch{$blocked=$true}
if(!$blocked -or [SharedEspFirmware.Native]::Writes.Count -ne $writes){throw 'Real entry ignored enabled Secure Boot'}
Set-FixtureValue 'SecureBoot' ([byte[]](0)) 6
Set-FixtureValue 'BootOrder' ([byte[]](1,0,0,0));$blocked=$false
try{& $entry -StateDirectory (Join-Path $fixture 'windows-first')}catch{$blocked=$true}
if(!$blocked -or [SharedEspFirmware.Native]::Writes.Count -ne $writes){throw 'Real entry accepted changed first Limine option'}
Set-FixtureValue 'BootOrder' ([byte[]](0,0,1,0))
$blocked=$false;try{& $entry -StateDirectory ([string]::new([char]92,2)+'server'+[char]92+'share')}catch{$blocked=$true}
if(!$blocked -or [SharedEspFirmware.Native]::Writes.Count -ne $writes){throw 'Real entry accepted non-local recovery path'}
foreach($name in [SharedEspFirmware.Native]::Writes){if($name -notin @('Boot7E00','Boot7E01')){throw "Real entry modified existing variable: $name"}}
'PASS SULK02 real entry: register/status/cleanup, WhatIf, occupied slot, immutable backup, armed-state/hash/BootNext/SecureBoot/Limine/local-path refusals. RAM-only Native and mocked inventory/hashes; no firmware or GPU access.'
