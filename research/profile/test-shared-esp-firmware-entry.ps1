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
  public static void Write(string name,byte[] data,uint attributes){Writes.Add(name);if(data==null)Values.Remove(name);else Values[name]=new Value{Data=data,Attributes=attributes};}
 }
}
'@
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$fixture=Join-Path $repo ('research/profile/build/firmware-entry-'+[guid]::NewGuid().ToString('N'))
$global:entryEsp=Join-Path $fixture 'esp'
[void](New-Item -ItemType Directory -Path (Join-Path $global:entryEsp 'EFI/Microsoft/Boot'),(Join-Path $global:entryEsp 'EFI/limine') -Force)
foreach($name in @('EFI/EVGA-SHARED-AUDIT-VISIBLE.EFI','EFI/Microsoft/Boot/bootmgfw.efi','EFI/limine/limine_x64.efi')){Set-Content -LiteralPath (Join-Path $global:entryEsp $name) -Value 'file fixture'}
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
$global:entryBadHash=$false
function Get-Partition {param([string]$DriveLetter) if($DriveLetter){[pscustomobject]@{Guid='11111111-2222-3333-4444-555555555555'}}else{[pscustomobject]@{Guid='c36c3e86-02aa-4f76-af1b-96e327f9d1c1';IsSystem=$true;DiskNumber=12;PartitionNumber=1;Offset=[uint64]1048576;Size=[uint64]1073741824}}}
function Get-Disk {param($Number) [pscustomobject]@{PartitionStyle='GPT';BusType='NVMe';LogicalSectorSize=512}}
function Get-Volume {param([Parameter(ValueFromPipeline)]$InputObject) process {[pscustomobject]@{FileSystem='FAT32';Path=$global:entryEsp}}}
function Get-FileHash {param([string]$LiteralPath)
 if($LiteralPath -like '*EVGA-SHARED-AUDIT-VISIBLE.EFI'){return [pscustomobject]@{Hash=if($global:entryBadHash){'0'*64}else{'f4cf33771fcc1e9c463a94561898457d0ce6ee7e4e17da5546c8a9752d373c92'}}}
 if($LiteralPath -like '*bootmgfw.efi'){return [pscustomobject]@{Hash='2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'}}
 if($LiteralPath -like '*limine_x64.efi'){return [pscustomobject]@{Hash='a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462'}}
 Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath
}
$entry=Join-Path $PSScriptRoot 'shared-esp-firmware-once.ps1';$state=Join-Path $fixture 'state'
$plan=& $entry -StateDirectory $state -WhatIf | ConvertFrom-Json
if($plan.registered -or (Test-Path $state) -or [SharedEspFirmware.Native]::Writes.Count){throw 'Real entry WhatIf wrote state or bypassed local-drive regex'}
$prepared=& $entry -StateDirectory $state | ConvertFrom-Json
if(!$prepared.registered -or !$prepared.limine_first_verified -or !$prepared.windows_entry_verified -or !(Test-Path (Join-Path $state 'firmware-backup.json'))){throw 'Real Prepare did not create valid backup/selection'}
$status=& $entry -Action Status -StateDirectory $state | ConvertFrom-Json
if(!$status.boot_order_unchanged -or !$status.temporary_option_matches -or $null -eq $status.boot_next){throw 'Status did not read actual mocked pending state'}
$clean=& $entry -Action Cleanup -StateDirectory $state | ConvertFrom-Json
if($clean.temporary_option_present -or $null -ne $clean.boot_next -or !$clean.boot_order_unchanged){throw 'Cleanup did not cancel before boot'}
$state2=Join-Path $fixture 'state-after-boot';$prepared=& $entry -StateDirectory $state2 | ConvertFrom-Json
[void][SharedEspFirmware.Native]::Values.Remove('BootNext') # Firmware consumes one-time request.
$clean=& $entry -Action Cleanup -StateDirectory $state2 | ConvertFrom-Json
if($clean.temporary_option_present -or !$clean.boot_order_unchanged){throw 'Cleanup after simulated firmware consumption failed'}
$global:entryBadHash=$true;$writes=[SharedEspFirmware.Native]::Writes.Count;$blocked=$false
try{& $entry -StateDirectory (Join-Path $fixture 'bad-hash')}catch{$blocked=$true}
if(!$blocked -or [SharedEspFirmware.Native]::Writes.Count -ne $writes){throw 'Bad installed audit hash was not refused before writes'}
$global:entryBadHash=$false
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
foreach($name in [SharedEspFirmware.Native]::Writes){if($name -notin @('Boot7E00','BootNext')){throw "Real entry modified existing variable: $name"}}
'PASS real entry-script local path/WhatIf/Prepare/Status/preboot cancellation/postboot cleanup/hash gate. Native firmware is a RAM-only fake; storage inventory and file hashes mocked.'
