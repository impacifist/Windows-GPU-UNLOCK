# Full manager integration with a no-P/Invoke Native, synthetic inventory/assets/evidence.
param([Parameter(Mandatory)][string]$PackageRoot)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test-shared-esp-second-register.ps1')
$scriptRoot=$PSScriptRoot
. (Join-Path $scriptRoot 'evga-package-lib.ps1')
$global:profileAssets=@(Get-EvgaAssetSpecifications (Join-Path $scriptRoot '../../internal-boot/unlock/assets.json'))
$global:managerGpuOk=$true
$global:managerSeed=9
function Get-CimInstance {param($ClassName) if($ClassName -cne 'Win32_VideoController'){throw 'Unexpected CIM'};[pscustomobject]@{PNPDeviceID=$(if($global:managerGpuOk){'PCI\VEN_10DE&DEV_2204&SUBSYS_33873842'}else{'PCI\VEN_10DE&DEV_2782'})}}
function Get-Partition {param([string]$DriveLetter)
 switch($DriveLetter){
  'D' {[pscustomobject]@{DiskNumber=2;PartitionNumber=1;Guid=[guid]::Empty;IsSystem=$false;IsBoot=$false}}
  'U' {[pscustomobject]@{DiskNumber=3;PartitionNumber=1;Guid='11111111-2222-3333-4444-555555555555';GptType='c12a7328-f81f-11d2-ba4b-00a0c93ec93b';IsSystem=$false;IsBoot=$false;Offset=1048576;Size=1GB}}
  {$_ -in @('C','E')} {[pscustomobject]@{DiskNumber=0;PartitionNumber=3;Guid='aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee';IsSystem=$false;IsBoot=$true}}
  default {[pscustomobject]@{DiskNumber=0;PartitionNumber=1;Guid='c36c3e86-02aa-4f76-af1b-96e327f9d1c1';GptType='c12a7328-f81f-11d2-ba4b-00a0c93ec93b';IsSystem=$true;IsBoot=$false;Offset=1048576;Size=1GB}}
 }
}
function Get-Disk {param($Number) [pscustomobject]@{BusType=$(if($Number -eq 2){'USB'}else{'NVMe'});PartitionStyle=$(if($Number -eq 2){'MBR'}else{'GPT'});LogicalSectorSize=512}}
function Get-Volume {param([Parameter(ValueFromPipeline)]$InputObject,[string]$DriveLetter)
 process {
  $kind=if($DriveLetter -eq 'D' -or $InputObject.DiskNumber -eq 2){'usb'}elseif($DriveLetter -eq 'U' -or $InputObject.DiskNumber -eq 3){'separate'}else{'windows'}
  [pscustomobject]@{Path=$global:managerRoots[$kind];UniqueId=('fixture-'+$kind);FileSystem='FAT32';SizeRemaining=1GB}
 }
}
function Get-FileHash {param([string]$LiteralPath,[string]$Algorithm)
 $name=[IO.Path]::GetFileName($LiteralPath)
 if($name -ceq 'bootmgfw.efi'){return [pscustomobject]@{Hash='2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'}}
 if($name -ceq 'SULK02.TXT'){return [pscustomobject]@{Hash='38963300821c434cbffd4a076286edf07799b738d0e042378d00492246b66db4'}}
 if($name -ceq 'EVGA23.TXT'){return [pscustomobject]@{Hash='7c2c6972f2e8c971f36127028aecbcc5df06cfaedc3dae249a6d846569114bc4'}}
 if($LiteralPath -like '*ASSETS*'){
  $spec=@($global:profileAssets | Where-Object Name -CEQ $name)
  if($spec.Count){return [pscustomobject]@{Hash=$spec[0].Sha256}}
 }
 Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath -Algorithm SHA256
}
function Get-Item {param([string]$LiteralPath)
 if($LiteralPath -like '*ASSETS*'){
  $spec=@($global:profileAssets | Where-Object Name -CEQ ([IO.Path]::GetFileName($LiteralPath)))
  if($spec.Count){return [pscustomobject]@{Length=$spec[0].Bytes}}
 }
 Microsoft.PowerShell.Management\Get-Item -LiteralPath $LiteralPath
}
function Setup-ManagerFixture([string]$Layout){
 $base=Join-Path $fixture ('manager-'+$Layout)
 $global:managerRoots=@{windows=(Join-Path $base 'windows');usb=(Join-Path $base 'usb');separate=(Join-Path $base 'separate')}
 foreach($root in $global:managerRoots.Values){[void][IO.Directory]::CreateDirectory((Join-Path $root 'RTX3090-UNLOCK'))}
 [void][IO.Directory]::CreateDirectory((Join-Path $global:managerRoots.windows 'EFI/Microsoft/Boot'))
 Set-Content -LiteralPath (Join-Path $global:managerRoots.windows 'EFI/Microsoft/Boot/bootmgfw.efi') -Value 'fixture boot'
 Set-Content -LiteralPath (Join-Path $global:managerRoots.windows 'RTX3090-UNLOCK/SULK02.TXT') -Value 'fixture seed 9'
 Set-Content -LiteralPath (Join-Path $global:managerRoots.usb 'RTX3090-UNLOCK/EVGA23.TXT') -Value 'fixture seed 7'
 $global:managerSeed=if($Layout -eq 'SharedEsp'){9}else{7}
 foreach($slot in 4..$global:managerSeed){Set-Content -LiteralPath (Join-Path $global:managerRoots.usb ("RTX3090-UNLOCK/EVGA-UNLOCK$slot.ARMED")) -Value 'old marker'}
 $assets=Join-Path $base 'ASSETS';[void][IO.Directory]::CreateDirectory($assets)
 foreach($name in @($global:profileAssets.Name)+@('NVIDIA-LICENSE','NOTICE.md')){Set-Content -LiteralPath (Join-Path $assets $name) -Value 'asset fixture'}
 [void][IO.Directory]::CreateDirectory((Join-Path $global:managerRoots.usb 'EFI/BOOT'))
 Set-Content -LiteralPath (Join-Path $global:managerRoots.usb 'EFI/BOOT/BOOTX64.EFI') -Value 'original loader'
 [SharedEspFirmware.Native]::Values.Clear();[SharedEspFirmware.Native]::Writes.Clear()
 Set-FixtureValue 'BootOrder' ([byte[]](0,0,8,0))
 Set-FixtureValue 'SecureBoot' ([byte[]](0)) 6
 [SharedEspFirmware.Native]::Values['Boot0000']=New-FixtureOption '\EFI\limine\limine_x64.efi'
 [SharedEspFirmware.Native]::Values['Boot0008']=New-FixtureOption '\EFI\Microsoft\Boot\bootmgfw.efi'
 [pscustomobject]@{State=(Join-Path $base 'state');Assets=$assets;OriginalLoader=(Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath (Join-Path $global:managerRoots.usb 'EFI/BOOT/BOOTX64.EFI')).Hash}
}
function Simulate-PackageRun($Attempt){
 [void][SharedEspFirmware.Native]::Values.Remove('BootNext')
 $root=Join-Path $Attempt.TargetRoot 'RTX3090-UNLOCK'
 $data=New-Object byte[] 512;$prefix=[Text.Encoding]::ASCII.GetBytes("STARTED $($Attempt.Run) - DO NOT REPEAT`r`n");[Array]::Copy($prefix,$data,$prefix.Length)
 [IO.File]::WriteAllBytes((Join-Path $root ($Attempt.Run+'.ARMED')),$data)
 $events=@(('evga-unlock-slot-{0:D2}' -f $Attempt.AttemptSlot),'evga-unlock-transaction-return','evga-unlock-assets-released','evga-unlock-close-return','evga-text-stop-ready-windows') | ForEach-Object {$_+' status=0x0000000000000000'}
 Set-Content -LiteralPath (Join-Path $root ($Attempt.Run+'.TXT')) -Value (@('capture-complete=1')+$events+@('capture-complete=1'))
 Set-Content -LiteralPath (Join-Path $root ('EVGA-UNLOCK'+$Attempt.AttemptSlot+'.ARMED')) -Value 'synthetic executed marker'
}
$manager=Join-Path $scriptRoot 'manage-evga-profile.ps1'
foreach($layout in @('SharedEsp','Usb','SeparateEsp')){
 $f=Setup-ManagerFixture $layout
 $slot=$global:managerSeed+1
 $package=Join-Path $PackageRoot ($layout+'-'+$slot)
 $nextPackage=Join-Path $PackageRoot ($layout+'-'+($slot+1))
 & $manager -Action Install -Layout $layout -StateDirectory $f.State -Assets $f.Assets -PackageDirectory $package -WhatIf
 if((Test-Path $f.State) -or [SharedEspFirmware.Native]::Writes.Count){throw 'WhatIf wrote state/firmware'}
 $installed=& $manager -Action Install -Layout $layout -StateDirectory $f.State -Assets $f.Assets -PackageDirectory $package | ConvertFrom-Json
 if(!$installed.prepared -or $installed.slot -ne $slot -or !$installed.requires_fresh_shift_g -or $installed.automatic_reboot){throw 'Install failed'}
 if($layout -eq 'Usb' -and [SharedEspFirmware.Native]::Writes.Count){throw 'USB install wrote firmware'}
 $blocked=$false;try{& $manager -Action Rearm -StateDirectory $f.State -Assets $f.Assets -PackageDirectory $nextPackage}catch{$blocked=$true}
 if(!$blocked){throw 'Incomplete attempt was rearmed'}
 $attempt=Get-Content -LiteralPath (Join-Path $f.State ('attempt-{0:D2}.json' -f $slot)) -Raw | ConvertFrom-Json
 Simulate-PackageRun $attempt
 $collected=& $manager -Action Collect -StateDirectory $f.State | ConvertFrom-Json
 if(!$collected.Success -or !$collected.CleanupCompleted -or !(Test-Path $collected.Zip)){throw 'Collection/cleanup failed'}
 if($layout -eq 'Usb' -and (Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath (Join-Path $global:managerRoots.usb 'EFI/BOOT/BOOTX64.EFI')).Hash -cne $f.OriginalLoader){throw 'USB original loader was not restored'}
 $rearmed=& $manager -Action Rearm -StateDirectory $f.State -Assets $f.Assets -PackageDirectory $nextPackage | ConvertFrom-Json
 if(!$rearmed.prepared -or $rearmed.slot -ne $slot+1){throw 'Rearm failed'}
 $cancelled=& $manager -Action Cancel -StateDirectory $f.State | ConvertFrom-Json
 if(!$cancelled.Cancelled -or !$cancelled.CleanupCompleted -or $cancelled.Success){throw 'Cancel outcome incorrect'}
 $blocked=$false;try{& $manager -Action Rearm -StateDirectory $f.State -Assets $f.Assets -PackageDirectory $nextPackage}catch{$blocked=$true}
 if(!$blocked){throw 'Cancelled attempt was rearmed'}
 foreach($name in [SharedEspFirmware.Native]::Writes){if($name -notin @('BootNext','Boot7E00')){throw 'Manager wrote existing firmware entry'}}
 Write-Host "PASS ${layout}: real Install/WhatIf, incomplete refusal, simulated run, Collect/cleanup, next-slot Rearm, Cancel/refusal; source history preserved. Native/inventory/assets/evidence mocked."
}
