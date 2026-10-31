# GPL-2.0-only. Builds and stages one attended attempt; never boots or changes firmware settings.
[CmdletBinding(SupportsShouldProcess)]
param([Parameter(Mandatory)][ValidatePattern('^[A-Za-z]$')][string]$Drive,
      [Parameter(Mandatory)][guid]$WindowsEspGuid,
      [string]$Clang='clang',[string]$LldLink='lld-link',[string]$Python='python')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'select-evga-attempt.ps1')
if($WindowsEspGuid -eq [guid]::Empty){throw 'An exact Windows ESP GUID is required'}
$letter=$Drive.ToUpperInvariant()
$partitions=@(Get-Partition -DriveLetter $letter)
$volume=Get-Volume -DriveLetter $letter
if($partitions.Count -ne 1 -or $volume.FileSystem -ne 'FAT32'){throw 'Expected one FAT32 USB partition'}
$disk=Get-Disk -Number $partitions[0].DiskNumber
if($disk.BusType -ne 'USB' -or $partitions[0].IsSystem -or $partitions[0].IsBoot){throw 'Target must be a USB data volume'}
$captures=Join-Path ($letter+':\') 'RTX3090-UNLOCK'
if(!(Test-Path -LiteralPath $captures -PathType Container)){throw 'Expected existing capture directory'}
$markers=@(Get-ChildItem -LiteralPath $captures -File -Filter 'EVGA-UNLOCK*.ARMED' | ForEach-Object {$_.Name})
$slot=Get-EvgaNextAttemptSlot -MarkerNames $markers
Write-Output "Next attended attempt: slot $slot. Existing markers and captures will be retained."
if($PSCmdlet.ShouldProcess("$letter`: USB",'Build and stage one experimental GPU unlock attempt')){
 & (Join-Path $PSScriptRoot 'build-evga-text-stop.ps1') -WindowsEspGuid $WindowsEspGuid -Clang $Clang -LldLink $LldLink -Python $Python -AttemptUnlock -AttemptSlot $slot
 & (Join-Path $PSScriptRoot 'stage-evga-text-stop.ps1') -Drive $letter -AttemptUnlock -Confirm:$false
 Write-Output 'Staging complete. No reboot was performed; consult the attended profile guide before booting.'
}
