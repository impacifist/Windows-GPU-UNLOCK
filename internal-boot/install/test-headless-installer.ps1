# GPL-2.0-only. Actual installer with fake firmware/registry/disk and synthetic files.
$ErrorActionPreference='Stop'
$sourceRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $PSScriptRoot 'headless-package.ps1')
$global:HeadlessRealJsonWriter=${function:Write-HeadlessJson}
$testRoot=Join-Path $PSScriptRoot ('build/headless-installer/'+[guid]::NewGuid().ToString('N'))
$global:HeadlessTestRoot=$testRoot;$global:HeadlessChecks=0
function Check($condition,[string]$message){$global:HeadlessChecks++;if(!$condition){throw ('FAIL: '+$message)}}
function TextFile([string]$path,[string]$text){[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path));[IO.File]::WriteAllText($path,$text)}
$fixture=Join-Path $testRoot 'repo';$target=Join-Path $testRoot 'esp';$windows=Join-Path $testRoot 'windows'
$installer=Join-Path $fixture 'internal-boot/Headless.ps1'
[void][IO.Directory]::CreateDirectory((Split-Path $installer));[IO.File]::Copy((Join-Path $sourceRoot 'internal-boot/Headless.ps1'),$installer,$false)
TextFile (Join-Path $windows 'EFI/Microsoft/Boot/bootmgfw.efi') 'fake Windows loader'
$global:HeadlessTarget=$target;$global:HeadlessWindows=$windows
$global:HeadlessConfig=[pscustomobject]@{UnlockDriveLetter='U';UnlockPartitionGuid='{00000000-0000-0000-0000-000000000001}';WindowsPartitionGuid='{00000000-0000-0000-0000-000000000002}';UnlockOffset=1048576;UnlockSize=1GB;UnlockDiskUniqueId='synthetic-disk'}
$childRelative='unlock/build/experimental/entryLatchedSs1Full/internal/automatic/headless-research/RTX3090-EXPERIMENTAL-entryLatchedSs1Full-INTERNAL-AUTO-HEADLESS-RESEARCH.EFI'
$wrapper=Join-Path $fixture 'internal-boot/boot/build/headless/HEADLESS.EFI';$child=Join-Path $fixture ('internal-boot/'+$childRelative)
TextFile $wrapper 'synthetic wrapper';TextFile $child 'synthetic child'
$global:HeadlessPackage=[pscustomobject]@{HardwareProfile='ASUS-ROG-STRIX-Z690-I-4601-RTX3090TI';EfiSha256=(Get-FileHash $wrapper).Hash;ChildSha256=(Get-FileHash $child).Hash}
[void][IO.Directory]::CreateDirectory((Join-Path $fixture 'internal-boot/install'))
[IO.File]::Copy((Join-Path $PSScriptRoot 'automatic-policy.ps1'),(Join-Path $fixture 'internal-boot/install/automatic-policy.ps1'),$false)
$helperPath=(Join-Path $PSScriptRoot 'headless-package.ps1').Replace("'","''")
$stub=@'
. '@HELPER@'
function Assert-HeadlessLocalConfig { if($global:HeadlessFailConfig){throw 'Config unavailable'};return $global:HeadlessConfig }
function Assert-HeadlessPackage { if($global:HeadlessFailConfig){throw 'Build unavailable'};return $global:HeadlessPackage }
function Write-HeadlessJson($Path,$Value){if($global:HeadlessFailStateMode -and $Value.Mode -eq $global:HeadlessFailStateMode){throw 'Injected metadata write failure'};& $global:HeadlessRealJsonWriter $Path $Value}
'@
TextFile (Join-Path $fixture 'internal-boot/install/headless-package.ps1') ($stub.Replace('@HELPER@',$helperPath))
$assets=Join-Path $testRoot 'assets';$specs=@()
foreach($name in @('booter_load_ga102.bin','v67-ga102-610.43.03.bin','gsp_ga10x.bin','gsp_boot_ga102.bin','gsp_boot_desc_ga102.bin')){
 $p=Join-Path $assets $name;TextFile $p ('synthetic '+$name);$specs+=@{Name=$name;Bytes=(Get-Item $p).Length;Sha256=(Get-FileHash $p).Hash.ToLowerInvariant()}
}
TextFile (Join-Path $fixture 'internal-boot/unlock/assets.json') ($specs | ConvertTo-Json)
$global:HeadlessState=Join-Path $fixture 'internal-boot/install/headless-state.json'
$oldProgramData=$env:ProgramData;$env:ProgramData=Join-Path $testRoot 'programdata'
$global:HeadlessOldId='{00000000-0000-0000-0000-000000000010}'
$global:HeadlessOrder=@($global:HeadlessOldId,'{bootmgr}');$global:HeadlessSerial=20
$global:HeadlessEntries=@{'{bootmgr}'=@{Name='Windows Boot Manager';Device='partition=S:';Path='\EFI\Microsoft\Boot\bootmgfw.efi'}}
$global:HeadlessEntries[$global:HeadlessOldId]=@{Name='Existing Auto';Device='partition=U:';Path='\EFI\Old\AUTO.EFI'}
$global:HeadlessHiber=1;$global:HeadlessHiberExists=$true;$global:HeadlessBcdCalls=0;$global:HeadlessOffWrites=0
function EntryText([string]$id){$e=$global:HeadlessEntries[$id];return "Windows Boot Manager`r`nidentifier              $id`r`ndevice                  $($e.Device)`r`npath                    $($e.Path)`r`ndescription             $($e.Name)`r`nlocale                  ko-KR"}
function FirmwareText {return "Firmware Boot Manager`r`nidentifier              {fwbootmgr}`r`ndisplayorder            $($global:HeadlessOrder -join "`r`n                        ")`r`ntimeout                 1"}
function bcdedit {
 $a=@($args);$global:HeadlessBcdCalls++;$global:LASTEXITCODE=0
 switch($a[0]){
  '/enum'{if($a[1] -eq '{fwbootmgr}'){return FirmwareText};if($a[1] -in 'all','firmware'){return ((@((FirmwareText))+@($global:HeadlessEntries.Keys | Sort-Object | ForEach-Object {EntryText $_})) -join "`r`n`r`n")};if(!$global:HeadlessEntries.ContainsKey($a[1])){$global:LASTEXITCODE=1;return};return EntryText $a[1]}
  '/export'{TextFile $a[1] 'synthetic BCD backup';return}
  '/copy'{$global:HeadlessSerial++;$id='{00000000-0000-0000-0000-'+('{0:d12}' -f $global:HeadlessSerial)+'}';$e=$global:HeadlessEntries['{bootmgr}'];$global:HeadlessEntries[$id]=@{Name=$a[3];Device=$e.Device;Path=$e.Path};return "Copied $id"}
  '/delete'{$global:HeadlessEntries.Remove($a[1]);return}
  '/set'{if($a[1] -eq '{fwbootmgr}'){$global:HeadlessOrder=@($a[3..($a.Count-1)]);if($global:HeadlessFailOrder){$global:HeadlessFailOrder=$false;$global:LASTEXITCODE=1};return};$e=$global:HeadlessEntries[$a[1]];$e[$a[2]]=$a[3];if($global:HeadlessFailPath -and $a[2] -eq 'path'){$global:HeadlessFailPath=$false;$global:LASTEXITCODE=1};return}
  default{throw 'Unexpected fake BCDEdit call'}
 }
}
function Get-Partition {param([string]$DriveLetter)
 $p=[pscustomobject]@{Guid=$global:HeadlessConfig.UnlockPartitionGuid;DiskNumber=9;IsSystem=$false;Offset=1048576;Size=1GB;GptType='{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}'}
 if($DriveLetter){return $p};return @($p,[pscustomobject]@{Guid=$global:HeadlessConfig.WindowsPartitionGuid;IsSystem=$true})
}
function Get-Disk {param($Number)return [pscustomobject]@{UniqueId='synthetic-disk'}}
function Get-Volume {param([Parameter(ValueFromPipeline)]$InputObject)process{return [pscustomobject]@{FileSystem='FAT32';Path=$(if($InputObject.IsSystem){$global:HeadlessWindows}else{$global:HeadlessTarget})}}}
function Get-CimInstance {param($ClassName)if($ClassName -eq 'Win32_BaseBoard'){return [pscustomobject]@{Product='ROG STRIX Z690-I GAMING WIFI'}};return [pscustomobject]@{SMBIOSBIOSVersion='4601'}}
function Confirm-SecureBootUEFI {return $false}
function Get-BitLockerVolume {param($MountPoint)return [pscustomobject]@{VolumeStatus=0;ProtectionStatus=0}}
function Get-Item {param($LiteralPath,$Path)
 $p=if($LiteralPath){$LiteralPath}else{$Path}
 if($p -like 'HKLM:*'){$k=[pscustomobject]@{};$k | Add-Member ScriptMethod GetValueNames {if($global:HeadlessHiberExists){'HiberbootEnabled'}};$k | Add-Member ScriptMethod GetValueKind {param($n)'DWord'};$k | Add-Member ScriptMethod GetValue {param($n)$global:HeadlessHiber};return $k}
 return Microsoft.PowerShell.Management\Get-Item -LiteralPath $p
}
function New-ItemProperty {param($LiteralPath,$Name,$PropertyType,$Value,[switch]$Force)
 if($Value -eq 0){$global:HeadlessOffWrites++;$s=Get-Content -LiteralPath $global:HeadlessState -Raw | ConvertFrom-Json;Check ($s.Mode -eq 'promotion-preparing' -and $s.HiberbootBefore.Value -eq 1) 'original Hiber state persisted before first off write'}
 $global:HeadlessHiberExists=$true;$global:HeadlessHiber=[int]$Value
}
function Remove-ItemProperty {param($LiteralPath,$Name,$ErrorAction)$global:HeadlessHiberExists=$false}
try {
 & $installer -Assets $assets
 Check ($global:HeadlessBcdCalls -eq 0 -and $global:HeadlessHiber -eq 1) 'stage-only does not access BCD or registry writes'
 $global:HeadlessFailPath=$true;$failed=$false;try{& $installer -Assets $assets -Register}catch{$failed=$true}
 Check ($failed -and $global:HeadlessEntries.Count -eq 2 -and !(Test-Path -LiteralPath $global:HeadlessState)) 'partial registration deletes only the attributed new entry and its state'
 Check (($global:HeadlessOrder -join ',') -eq (@($global:HeadlessOldId,'{bootmgr}') -join ',')) 'partial registration preserves original priority'
 & $installer -Assets $assets -Register
 $registered=Get-Content -LiteralPath $global:HeadlessState -Raw | ConvertFrom-Json;$id=$registered.EntryId
 Check (($global:HeadlessOrder -join ',') -eq (@($global:HeadlessOldId,'{bootmgr}',$id) -join ',')) 'registration preserves old priority prefix'
 Check ($registered.Mode -eq 'registered' -and $global:HeadlessHiber -eq 1) 'registration leaves power setting unchanged'
 $initial=[IO.File]::ReadAllBytes($global:HeadlessState);$registeredOrder=@($global:HeadlessOrder)
 $failed=$false;try{& $installer -EnableAutomatic}catch{$failed=$true};Check $failed 'promotion requires explicit success flag'
 $global:HeadlessFailOrder=$true;$failed=$false;try{& $installer -EnableAutomatic -ConfirmedSuccess}catch{$failed=$true}
 Check ($failed -and ($global:HeadlessOrder -join ',') -eq ($registeredOrder -join ',') -and $global:HeadlessHiber -eq 1) 'partial order mutation rolls back order and power'
 Check ((Get-Content $global:HeadlessState -Raw | ConvertFrom-Json).Mode -eq 'registered') 'partial mutation restores saved metadata'
 foreach($mode in 'promotion-preparing','automatic'){
  $global:HeadlessFailStateMode=$mode;$failed=$false;try{& $installer -EnableAutomatic -ConfirmedSuccess}catch{$failed=$true};$global:HeadlessFailStateMode=$null
  Check ($failed -and ($global:HeadlessOrder -join ',') -eq ($registeredOrder -join ',') -and $global:HeadlessHiber -eq 1) 'metadata failures roll back order and power'
  Check ((Get-Content $global:HeadlessState -Raw | ConvertFrom-Json).Mode -eq 'registered') 'metadata failure restores original state'
 }
 & $installer -EnableAutomatic -ConfirmedSuccess
 Check (($global:HeadlessOrder -join ',') -eq (@($id,'{bootmgr}',$global:HeadlessOldId) -join ',') -and $global:HeadlessHiber -eq 0) 'promotion guarantees Windows second and preserves remaining relative order'
 $global:HeadlessFailConfig=$true;& $installer -RestoreWindowsFirst
 Check ($global:HeadlessOrder[0] -eq '{bootmgr}' -and $global:HeadlessHiber -eq 1) 'recovery works without configuration/build and restores power'
 [IO.File]::WriteAllText($global:HeadlessState,'broken JSON');& $installer -RestoreWindowsFirst
 Check ($global:HeadlessOrder[0] -eq '{bootmgr}') 'unreadable state cannot prevent Windows-first recovery'
 Write-Output "PASS actual Headless.ps1 mocked integration: $global:HeadlessChecks checks; no real disk/registry/BCD access. Fixtures: $testRoot"
} finally {$env:ProgramData=$oldProgramData}
