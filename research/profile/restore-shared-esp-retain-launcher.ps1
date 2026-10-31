# Restore a previously backed-up USB boot file; never changes firmware settings.
[CmdletBinding(SupportsShouldProcess)]
param([Parameter(Mandatory)][ValidatePattern('^[A-Za-z]$')][string]$Drive,[Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$OriginalSha256)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
$letter=$Drive.ToUpperInvariant();$part=@(Get-Partition -DriveLetter $letter);$volume=Get-Volume -DriveLetter $letter
if($part.Count -ne 1 -or $part[0].IsSystem -or $part[0].IsBoot -or $volume.FileSystem -ne 'FAT32' -or (Get-Disk -Number $part[0].DiskNumber).BusType -ne 'USB'){throw 'Expected FAT32 non-system USB'}
$root=$letter+':\';$current=Get-AuditCheckedPath $root @('EFI','BOOT','BOOTX64.EFI') $false
$backup=Get-AuditCheckedPath $root @('EFI','BOOT','BOOTX64.before-shared-retain.efi') $false
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'));$manifest=Get-Content -Raw (Join-Path $repo 'internal-boot/boot/build/shared-esp-retain/launcher-manifest.json') | ConvertFrom-Json
if($manifest.Mode -ne 'shared-esp-retain-usb-launcher' -or (Get-FileHash -LiteralPath $current).Hash -ine $manifest.EfiSha256){throw 'Current USB boot file is not the built launcher; preserve it and stop'}
if((Get-FileHash -LiteralPath $backup).Hash -ine $OriginalSha256){throw 'Backup differs from recorded original hash; preserve it and stop'}
if($PSCmdlet.ShouldProcess($current,'Restore verified original USB boot file')){
 $i=$null;$o=$null
 try {$i=[IO.File]::Open($backup,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);$o=[IO.File]::Open($current,[IO.FileMode]::Truncate,[IO.FileAccess]::Write,[IO.FileShare]::None);$i.CopyTo($o);$o.Flush($true)}finally{if($o){$o.Dispose()};if($i){$i.Dispose()}}
 if((Get-FileHash -LiteralPath $current).Hash -ine $OriginalSha256){throw 'Restore verification failed; backup retained'}
 'Original USB boot file restored and verified; backup retained.'
}
