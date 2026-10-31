# GPL-2.0-only. Collect evidence and restore the USB after the attended test.
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[Parameter(Mandatory)][ValidatePattern('^[A-Za-z]$')][string]$Drive,[Parameter(Mandatory)][ValidateRange(5,99)][int]$AttemptSlot,[Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$OriginalSha256,[Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'read-shared-esp-unlock.ps1') -WindowsEspGuid $WindowsEspGuid -AttemptSlot $AttemptSlot -OutputDirectory $OutputDirectory
& (Join-Path $PSScriptRoot 'restore-shared-esp-unlock-launcher.ps1') -Drive $Drive -OriginalSha256 $OriginalSha256
Write-Host 'Evidence collected and USB boot file restored. Preserve the attempt reservation and ESP files.'
