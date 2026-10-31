# GPL-2.0-only. Explicit staging phase; does not boot or execute an unlock.
[CmdletBinding(SupportsShouldProcess)]
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[Parameter(Mandatory)][ValidatePattern('^[A-Za-z]$')][string]$Drive)
$ErrorActionPreference='Stop'
$root=$Drive.ToUpperInvariant()+':\'
$slot=& (Join-Path $PSScriptRoot 'get-shared-esp-unlock-slot.ps1') -Drive $Drive
if($slot -isnot [int] -or $slot -lt 5 -or $slot -gt 99){throw 'Slot discovery did not return one valid attempt number'}
$boot=Join-Path $root 'EFI/BOOT/BOOTX64.EFI'
$original=(Get-FileHash -LiteralPath $boot -Algorithm SHA256).Hash
Write-Host "Attempt slot: $slot"
Write-Host "Original USB boot SHA256: $original"
Write-Host 'Save both values outside the USB before proceeding.'
if($WhatIfPreference){
 & (Join-Path $PSScriptRoot 'stage-shared-esp-unlock.ps1') -WindowsEspGuid $WindowsEspGuid -Drive $Drive -WhatIf
 Write-Host 'Dry run did not validate launcher staging or write-time boot hash checks.'
 return
}
if($PSCmdlet.ShouldProcess("ESP and USB $Drive",'Stage one-shot unlock candidate and USB launcher')){
 & (Join-Path $PSScriptRoot 'stage-shared-esp-unlock.ps1') -WindowsEspGuid $WindowsEspGuid -Drive $Drive -Confirm:$false
 & (Join-Path $PSScriptRoot 'stage-shared-esp-unlock-launcher.ps1') -WindowsEspGuid $WindowsEspGuid -Drive $Drive -ReuseExisting -Confirm:$false
 Write-Host 'Staging completed. No reboot performed. Preserve the attempt reservation.'
}
