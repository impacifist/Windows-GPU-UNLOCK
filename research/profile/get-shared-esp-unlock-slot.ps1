# Prints only the next qualified slot; never writes either volume.
param([Parameter(Mandatory)][ValidatePattern('^[A-Za-z]$')][string]$Drive)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-unlock-history.ps1')
$letter=$Drive.ToUpperInvariant();$parts=@(Get-Partition -DriveLetter $letter);$volume=Get-Volume -DriveLetter $letter
if($parts.Count -ne 1 -or $parts[0].IsSystem -or $parts[0].IsBoot -or $volume.FileSystem -ne 'FAT32' -or (Get-Disk -Number $parts[0].DiskNumber).BusType -ne 'USB'){throw 'Expected original non-system FAT32 capture USB'}
(Get-SharedUnlockHistory ($letter+':\')).NextSlot
