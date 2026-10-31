# Prepare one attended USB-free SULK02 unlock. Never reboots or automatically retries.
[CmdletBinding(SupportsShouldProcess)]
param([ValidateSet('Arm','Status','Cancel','Collect')][string]$Action='Arm',
 [string]$StateDirectory='E:\UNLOCK\ESP-SECOND02',
 [ValidatePattern('^[A-Za-z]$')][string]$Drive='D')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-firmware-lib.ps1')
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
. (Join-Path $PSScriptRoot 'shared-esp-second-arm-lib.ps1')
$guid=[guid]'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'
$registration=Join-Path $PSScriptRoot 'register-shared-esp-second.ps1'
if($Action -in @('Status','Cancel')){
 $op=if($Action -eq 'Cancel'){'Cleanup'}else{'Status'}
 & $registration -Action $op -StateDirectory $StateDirectory -WhatIf:$WhatIfPreference
 return
}
if($Action -eq 'Collect'){
 if(!$PSCmdlet.ShouldProcess($StateDirectory,'Copy SULK02 evidence, create ZIP and clean up only the exact owned firmware entry')){return}
 $out=Join-Path $StateDirectory ('SULK02-results-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))
 $result=& (Join-Path $PSScriptRoot 'read-shared-esp-unlock.ps1') -WindowsEspGuid $guid -AttemptSlot 9 -SecondSharedAttempt -OutputDirectory $out | ConvertFrom-Json
 $result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $out 'summary.json') -Encoding utf8
 Compress-Archive -LiteralPath $out -DestinationPath ($out+'.zip') -ErrorAction Stop
 $cleanup=$null;$cleanupError=$null
 try {$cleanup=& $registration -Action Cleanup -StateDirectory $StateDirectory | ConvertFrom-Json}catch{$cleanupError=$_.Exception.Message}
 [ordered]@{evidence_zip=($out+'.zip');cleanup=$cleanup;cleanup_error=$cleanupError;windows_return_requires_operator_report=$true;next='This boot test is finished. Do NOT run Cancel or repeat EFI. Send this ZIP and direct/manual Windows result. For the next performance measurement run ./research/profile/benchmark-shared-esp-second.ps1 without another reboot. Report any reboot since SULK02; cleanup errors need separate review.'} | ConvertTo-Json -Depth 6
 return
}
# Reuse registration backup validation and exact current-state checks before arming.
$status=& $registration -Action Status -StateDirectory $StateDirectory | ConvertFrom-Json
if(!$status.boot_order_unchanged -or !$status.temporary_option_matches -or $null -ne $status.boot_next){throw 'Registration is not ready for arming'}
$statePath=[IO.Path]::GetFullPath($StateDirectory)
$backup=Join-Path $statePath 'firmware-backup.json'
$backupHash=(Get-FileHash -LiteralPath $backup).Hash
$state=Get-Content -LiteralPath $backup -Raw | ConvertFrom-Json
$parts=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq $guid})
if($parts.Count -ne 1){throw 'Expected unique pinned system ESP'}
$part=$parts[0];$disk=Get-Disk -Number $part.DiskNumber;$volumes=@($part | Get-Volume)
if($disk.BusType -eq 'USB' -or $disk.PartitionStyle -ne 'GPT' -or $volumes.Count -ne 1 -or $volumes[0].FileSystem -ne 'FAT32' -or $volumes[0].SizeRemaining -lt 32MB){throw 'Expected internal GPT FAT32 ESP with 32 MiB free'}
[uint64]$sector=$disk.LogicalSectorSize
if($sector -notin @(512,4096) -or $part.Offset%$sector -or $part.Size%$sector){throw 'Unsupported ESP geometry'}
$expectedOption=ConvertTo-FirmwareValue (New-SharedEspLoadOption $guid ([uint32]$part.PartitionNumber) ([uint64]([decimal]$part.Offset/$sector)) ([uint64]([decimal]$part.Size/$sector)) ('EVGA SULK02 REGISTER ONLY '+$state.Operation) '\EFI\EVGA-SHARED-LAUNCH02.EFI')
if(!(Test-FirmwareValueEqual $expectedOption $state.Option)){throw 'Registered option no longer matches exact ESP geometry'}
$root=$volumes[0].Path
$logs=Get-AuditCheckedPath $root @('RTX3090-UNLOCK') $true
$receipt=Join-Path $logs 'SULK02.ARMED'
$journal=Join-Path $statePath 'sulk02-arm.json'
foreach($path in @($journal,$receipt,(Join-Path $logs 'SULK02.TXT'),(Join-Path $logs 'EVGA-UNLOCK9.ARMED'))){if(Test-Path -LiteralPath $path){throw 'SULK02 already has state; do not retry or delete evidence'}}
$usbParts=@(Get-Partition -DriveLetter $Drive);$usbVolume=Get-Volume -DriveLetter $Drive
if($usbParts.Count -ne 1 -or $usbParts[0].IsSystem -or $usbParts[0].IsBoot -or $usbVolume.FileSystem -ne 'FAT32' -or (Get-Disk -Number $usbParts[0].DiskNumber).BusType -ne 'USB'){throw 'Connect original FAT32 capture USB for slot reservation'}
$usb=Get-AuditCheckedPath $usbVolume.Path @('RTX3090-UNLOCK') $true
$reservation=Join-Path $usb 'EVGA-UNLOCK9.ARMED'
if(Test-Path -LiteralPath $reservation){throw 'USB slot 9 already reserved; do not repeat'}
$history=& (Join-Path $PSScriptRoot 'inspect-shared-unlock-reconciliation.ps1') -WindowsEspGuid $guid -Drive $Drive | ConvertFrom-Json
if($history.reviewed_next_slot -ne 9 -or @($history.checks.PSObject.Properties | Where-Object {$_.Value -ne $true}).Count){throw 'Reviewed cross-volume history refused'}
$priorPaths=@((Get-AuditCheckedPath $logs @('SULK01.TXT') $false),(Get-AuditCheckedPath $logs @('SULK01.ARMED') $false),(Get-AuditCheckedPath $logs @('EVGA-UNLOCK8.ARMED') $false))
$priorPaths+=@(Get-ChildItem -LiteralPath $usb -File -Filter 'EVGA-UNLOCK*.ARMED' | ForEach-Object {Get-AuditCheckedPath $usb @($_.Name) $false})
$priorPins=@($priorPaths | ForEach-Object {[pscustomobject]@{Path=$_;Hash=(Get-FileHash -LiteralPath $_).Hash}})
$pins=@(
 @(@('EFI','EVGA-SHARED-LAUNCH02.EFI'),'258a93fdf0eb526331f25b562c13a36176980ecc594825c8b4c48b0ec5604bec'),
 @(@('EFI','EVGA-SHARED-UNLOCK02.EFI'),'a15bade7806747f5cec76c411278d6d797d6f932f5d532eaf283add02d6409bf'),
 @(@('EFI','Microsoft','Boot','bootmgfw.efi'),'2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'),
 @(@('EFI','limine','limine_x64.efi'),'a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462'))
function Assert-SecondFiles {
 if((Get-FileHash -LiteralPath $backup).Hash -ine $backupHash){throw 'Registration backup changed'}
 foreach($prior in $priorPins){if((Get-FileHash -LiteralPath $prior.Path).Hash -ine $prior.Hash){throw 'Prior evidence changed; preserve state'}}
 foreach($pin in $pins){$file=Get-AuditCheckedPath $root $pin[0] $false;if((Get-FileHash -LiteralPath $file).Hash -ine $pin[1]){throw 'Staged EFI or normal boot file hash changed'}}
 foreach($spec in (Get-Content -LiteralPath (Join-Path $PSScriptRoot '../../internal-boot/unlock/assets.json') -Raw | ConvertFrom-Json)){
  $file=Get-AuditCheckedPath $root @('RTX3090-UNLOCK','ASSETS',$spec.Name) $false
  if((Get-Item -LiteralPath $file).Length -ne $spec.Bytes -or (Get-FileHash -LiteralPath $file).Hash -ine $spec.Sha256){throw 'Unlock asset differs'}
 }
}
Assert-SecondFiles
$pending=New-Object byte[] 512
$prefix=[Text.Encoding]::ASCII.GetBytes("PENDING SULK02`r`n");[Array]::Copy($prefix,$pending,$prefix.Length)
$reserved=[Text.Encoding]::ASCII.GetBytes("RESERVED FOR SHARED ESP SULK02 SLOT 9. NOT EXECUTION PROOF. DO NOT DELETE OR RETRY.`r`n")
$journalBytes=[Text.Encoding]::UTF8.GetBytes(([ordered]@{Schema='shared-esp-second-arm-v1';OptionName=$state.OptionName;RegistrationBackupSha256=$backupHash;AttemptSlot=9;Receipt='SULK02.ARMED';Reservation='EVGA-UNLOCK9.ARMED';BootNextSetIsNotExecutionProof=$true} | ConvertTo-Json))
$prepare={
 Write-SharedSecondNewFile $journal $journalBytes
 Write-SharedSecondNewFile $reservation $reserved
 Write-SharedSecondNewFile $receipt $pending
}
$verify={
 Assert-SecondFiles
 foreach($item in @(@($journal,$journalBytes),@($reservation,$reserved),@($receipt,$pending))){
  if([Convert]::ToBase64String([IO.File]::ReadAllBytes($item[0])) -cne [Convert]::ToBase64String($item[1])){throw 'Arming file changed; preserve state and do not reboot'}
 }
 if((Test-Path -LiteralPath (Join-Path $logs 'SULK02.TXT')) -or (Test-Path -LiteralPath (Join-Path $logs 'EVGA-UNLOCK9.ARMED'))){throw 'Runtime evidence appeared before boot; stop'}
}
if(-not ('SharedEspFirmware.Native' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'shared-esp-firmware-native.cs')}
$privilege=[SharedEspFirmware.Native]::EnablePrivilege()
try {
 $read={param($name) $v=[SharedEspFirmware.Native]::Read($name);if($null -ne $v){ConvertTo-FirmwareValue $v.Data $v.Attributes}}
 $write={param($name,$value) if($name -cne 'BootNext' -or $null -eq $value){throw 'Unexpected write requested'};[SharedEspFirmware.Native]::Write($name,[Convert]::FromBase64String($value.DataBase64),[uint32]$value.Attributes)}
 $armed=$false
 if($PSCmdlet.ShouldProcess('Original USB reservation, pinned ESP receipt and UEFI BootNext','Arm SULK02 slot 9 and select the existing launcher for ONE attended unlock boot; never reboot automatically')){
  try {Select-RegisteredSharedSecond $state $read $write $prepare $verify;$armed=$true}catch{throw ('Arming failed; preserve all files/backup and DO NOT REBOOT or retry. Use Status/Cancel or report before continuing. '+$_.Exception.Message)}
 }
 [ordered]@{armed=$armed;attempt_slot=9;boot_next_selected=$armed;boot_order_written=$false;gpu_executed=$false;automatic_reboot=$false;next=$(if($armed){'Safely remove storage USBs, save work and restart normally ONCE. Record launcher text; press fresh Shift+G only after live guards. No repeat. After Windows run this script -Action Collect.'}else{'WhatIf only. Not armed.'})} | ConvertTo-Json
}finally{$privilege.Dispose()}
