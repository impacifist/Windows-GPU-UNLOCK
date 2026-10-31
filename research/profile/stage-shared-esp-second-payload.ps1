# Payload copies only. Does not create SULK02.ARMED or register a firmware entry.
[CmdletBinding(SupportsShouldProcess)]
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[ValidatePattern('^[A-Za-z]$')][string]$Drive='D')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
. (Join-Path $PSScriptRoot 'shared-esp-second-payload-lib.ps1')
$proof=& (Join-Path $PSScriptRoot 'test-shared-esp-second-stage-preflight.ps1') -WindowsEspGuid $WindowsEspGuid -Drive $Drive | ConvertFrom-Json
if(!$proof.new_targets_absent -or !$proof.manifest_and_sources_match -or $proof.reviewed_next_slot -ne 9){throw 'Payload preflight refused'}
$p=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($p.Count -ne 1 -or (Get-Disk -Number $p[0].DiskNumber).BusType -eq 'USB'){throw 'Expected unique internal system ESP'}
$v=@($p[0] | Get-Volume);if($v.Count -ne 1 -or $v[0].FileSystem -ne 'FAT32' -or $v[0].SizeRemaining -lt 32MB){throw 'Expected FAT32 ESP with 32 MiB free'}
$efi=Get-AuditCheckedPath $v[0].Path @('EFI') $true
$build=Join-Path $PSScriptRoot '../../internal-boot/boot/build/shared-esp-unlock'
$bootPins=@(
 @(@('EFI','Microsoft','Boot','bootmgfw.efi'),'2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'),
 @(@('EFI','limine','limine_x64.efi'),'a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462'))
function Assert-BootPins {
 foreach($pin in $bootPins){$file=Get-AuditCheckedPath $v[0].Path $pin[0] $false;if((Get-FileHash -LiteralPath $file).Hash -ine $pin[1]){throw 'Windows or Limine boot file differs; preserve stage'}}
}
Assert-BootPins
$copied=$false
if($PSCmdlet.ShouldProcess('pinned ESP EFI directory','Create two SULK02 payload files only; leave attempt unarmed')){
 Copy-SharedSecondPayload (Join-Path $build 'BOOTX64.EFI') (Join-Path $efi 'EVGA-SHARED-UNLOCK02.EFI') $proof.child_sha256
 Copy-SharedSecondPayload (Join-Path $build 'LAUNCHER.EFI') (Join-Path $efi 'EVGA-SHARED-LAUNCH02.EFI') $proof.launcher_sha256
 Assert-BootPins
 $copied=$true
}
[ordered]@{boot_files_verified=$true;child_sha256=$proof.child_sha256;launcher_sha256=$proof.launcher_sha256;payload_copied=$copied;attempt_armed=$false;firmware_registered=$false;boot_authorized=$false;next='Preserve files and send output; do not boot'} | ConvertTo-Json
