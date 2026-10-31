# Exact reviewed failure cleanup. No GPU access, rearm, BootOrder write or reboot.
[CmdletBinding(SupportsShouldProcess)]
param([string]$StateDirectory='E:\UNLOCK\EVGA-PROFILE')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'evga-package-lib.ps1')
. (Join-Path $PSScriptRoot 'shared-esp-firmware-lib.ps1')
function Get-EvgaReviewHash($Value){
 $sha=[Security.Cryptography.SHA256]::Create()
 try{([BitConverter]::ToString($sha.ComputeHash([Convert]::FromBase64String($Value.DataBase64)))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
}
function Assert-ReviewedEvp10Snapshot($Attempt,$Now,[scriptblock]$Read){
 $old=@($Attempt.BootEntries);$entries=@($Now.BootEntries)
 if($entries.Count -ne $old.Count+1){throw 'Expected exactly one reviewed appended entry'}
 for($i=0;$i -lt $old.Count;$i++){
  if($entries[$i].Name -cne $old[$i].Name -or !(Test-FirmwareValueEqual $entries[$i].Value $old[$i].Value) -or !(Test-FirmwareValueEqual (& $Read $old[$i].Name) $old[$i].Value)){throw 'Original entry/order changed'}
 }
 $extra=$entries[-1]
 if($extra.Name -cne 'Boot000D' -or $extra.Value.Attributes -ne 7){throw 'Unexpected additional entry'}
 $hash=Get-EvgaReviewHash $extra.Value
 if($hash -cne 'ac1eb792d44382686770c715aacae3681d572ab8f61cc7e6dc9d71fa0fab94ee'){throw 'Reviewed USB entry changed'}
}
if($MyInvocation.InvocationName -eq '.'){return}
$attempt=Get-Content -LiteralPath (Join-Path $StateDirectory 'attempt-10.json') -Raw | ConvertFrom-Json
$result=Get-Content -LiteralPath (Join-Path $StateDirectory 'result-10.json') -Raw | ConvertFrom-Json
if($attempt.Schema -cne 'evga-attended-attempt-v1' -or $attempt.AttemptSlot -ne 10 -or $attempt.Run -cne 'EVP10' -or $attempt.Layout -cne 'SharedEsp' -or $result.Success -or $result.CleanupCompleted){throw 'Not the reviewed failed EVP10 state'}
if((Get-EvgaPackageHash (Join-Path $attempt.TargetRoot 'RTX3090-UNLOCK/EVP10.TXT')) -cne 'bbd5c27a727a9835f7ff886551bebf298509759e211bc3dda66c81158b96eaa5'){throw 'Failed log differs'}
$manifest=Assert-EvgaPackage $attempt.PackageDirectory
if($manifest.AttemptSlot -ne 10 -or $manifest.Layout -cne 'SharedEsp' -or (Get-EvgaPackageHash (Join-Path $attempt.PackageDirectory 'package.json')) -cne $attempt.PackageSha256){throw 'Package differs'}
if(-not ('SharedEspFirmware.Native' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'shared-esp-firmware-native.cs')}
$scope=[SharedEspFirmware.Native]::EnablePrivilege()
try{
 $read={param($name)$v=[SharedEspFirmware.Native]::Read($name);if($v){ConvertTo-FirmwareValue $v.Data $v.Attributes}}
 $now=Get-EvgaFirmwareSnapshot $read
 Assert-ReviewedEvp10Snapshot $attempt $now $read
 if($null -ne (& $read 'BootNext')){throw 'Pending BootNext; cleanup refused'}
 if(!(Test-FirmwareValueEqual (& $read $attempt.OptionName) $attempt.Option)){throw 'Owned temporary entry differs'}
 $decoded=Read-SharedEspLoadOption $attempt.Option
 if($decoded.Path -cne '\EFI\WindowsGPUUnlock\EVP10\UNLOCK.EFI' -or $decoded.PartitionGuid -ne [guid]'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'){throw 'Unexpected owned path'}
 if(!$PSCmdlet.ShouldProcess($attempt.OptionName,'Remove exact EVP10 temporary option; preserve Boot000D and failed slot')){return}
 $reviewed=$attempt | ConvertTo-Json -Depth 20 | ConvertFrom-Json
 $reviewed.BootOrder=$now.BootOrder;$reviewed.BootEntries=$now.BootEntries
 $write={param($name,$value)if($name -cne $attempt.OptionName -or $null -ne $value){throw 'Only owned option deletion permitted'};[SharedEspFirmware.Native]::Write($name,$null,7)}
 $save={param($snapshot)Write-EvgaPackageJson (Join-Path $StateDirectory 'reviewed-evp10-cleanup-before.json') $snapshot}
 Remove-EvgaPackageOption $reviewed $read $write $save
 $receipt=[ordered]@{schema='reviewed-evp10-cleanup-v1';cleanup_completed=$true;failed_attempt_preserved=$true;rearm_authorized=$false;usb_entry_preserved=$true;boot_order_written=$false;gpu_accessed=$false}
 Write-EvgaPackageJson (Join-Path $StateDirectory 'reviewed-evp10-cleanup-result.json') $receipt
 $receipt | ConvertTo-Json
}finally{$scope.Dispose()}
