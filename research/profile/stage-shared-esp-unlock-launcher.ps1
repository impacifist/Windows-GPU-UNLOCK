# GPL-2.0-only. Stage only to a USB FAT32 volume with no existing EFI directory.
[CmdletBinding(SupportsShouldProcess)]
param([Parameter(Mandatory)][ValidatePattern('^[A-Za-z]$')][string]$Drive,[Parameter(Mandatory)][guid]$WindowsEspGuid,[switch]$ReuseExisting)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $repo 'internal-boot/boot/build/shared-esp-unlock'
$m=Get-Content -LiteralPath (Join-Path $build 'launcher-manifest.json') -Raw | ConvertFrom-Json
if($m.Mode -ne 'shared-esp-unlock-usb-launcher' -or $m.AuditFileName -ne 'EVGA-SHARED-UNLOCK.EFI' -or $m.AuditEfiSha256 -notmatch '^[0-9a-fA-F]{64}$' -or $WindowsEspGuid -eq [guid]::Empty -or [guid]$m.WindowsEspGuid -ne $WindowsEspGuid){throw 'Wrong launcher manifest or ESP GUID'}
foreach($pin in @(@('shared-esp-unlock-launcher.c',$m.LauncherSourceSha256),@('shared-esp-audit.c',$m.BaseAuditSha256),@('media.h',$m.MediaSha256))){
 if((Get-FileHash -LiteralPath (Join-Path $repo ('internal-boot/boot/'+$pin[0]))).Hash -ine $pin[1]){throw 'Launcher source changed since build'}
}
$source=Join-Path $build 'LAUNCHER.EFI'
if((Get-FileHash -LiteralPath $source).Hash -ine $m.EfiSha256){throw 'Launcher build hash mismatch'}
$system=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($system.Count -ne 1){throw 'Expected one pinned system ESP'}
$esp=@($system[0] | Get-Volume)
if($esp.Count -ne 1 -or $esp[0].FileSystem -ne 'FAT32' -or !$esp[0].Path){throw 'Expected FAT32 system ESP'}
$audit=Get-AuditCheckedPath $esp[0].Path @('EFI','EVGA-SHARED-UNLOCK.EFI') $false
if((Get-FileHash -LiteralPath $audit).Hash -ine $m.AuditEfiSha256){throw 'Staged audit hash differs from contributor build; stop'}
$letter=$Drive.ToUpperInvariant();$volume=Get-Volume -DriveLetter $letter;$part=@(Get-Partition -DriveLetter $letter)
if($part.Count -ne 1 -or $volume.FileSystem -ne 'FAT32' -or $part[0].IsSystem -or $part[0].IsBoot){throw 'Expected FAT32 non-system USB partition'}
$disk=Get-Disk -Number $part[0].DiskNumber
if($disk.BusType -ne 'USB'){throw 'Target disk is not USB'}
$root=$letter+':\';$efi=Join-Path $root 'EFI'
if((Test-Path -LiteralPath $efi) -and !$ReuseExisting){throw 'EFI directory already exists on target; use a separate USB or explicit -ReuseExisting'}
if($ReuseExisting){
 $boot=Get-AuditCheckedPath $root @('EFI','BOOT') $true
 $existing=Get-AuditCheckedPath $root @('EFI','BOOT','BOOTX64.EFI') $false
 $backup=[IO.Path]::Combine($boot,'BOOTX64.before-shared-unlock.efi')
 if(Test-Path -LiteralPath $backup){
  $backup=Get-AuditCheckedPath $root @('EFI','BOOT','BOOTX64.before-shared-unlock.efi') $false
  if((Get-FileHash -LiteralPath $backup).Hash -ine (Get-FileHash -LiteralPath $existing).Hash){throw 'Existing backup differs from USB boot file; preserve both and stop'}
 }
}
if($PSCmdlet.ShouldProcess($root,'Create EFI/BOOT/BOOTX64.EFI on USB')){
 if($ReuseExisting){
  $oldHash=(Get-FileHash -LiteralPath $existing).Hash
  if(!(Test-Path -LiteralPath $backup)){
  $i=$null;$o=$null
  try {$i=[IO.File]::Open($existing,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);$o=[IO.File]::Open($backup,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$i.CopyTo($o);$o.Flush($true)} finally {if($o){$o.Dispose()};if($i){$i.Dispose()}}
  }
  if((Get-FileHash -LiteralPath $backup).Hash -ine $oldHash -or (Get-FileHash -LiteralPath $existing).Hash -ine $oldHash){throw 'Backup or original changed; preserve files and stop'}
 }else{
  [void][IO.Directory]::CreateDirectory($efi)
  $boot=[IO.Path]::Combine($efi,'BOOT');[void][IO.Directory]::CreateDirectory($boot)
 }
 $target=[IO.Path]::Combine($boot,'BOOTX64.EFI');$i=$null;$o=$null
 try {$i=[IO.File]::Open($source,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);$mode=if($ReuseExisting){[IO.FileMode]::Truncate}else{[IO.FileMode]::CreateNew};$o=[IO.File]::Open($target,$mode,[IO.FileAccess]::Write,[IO.FileShare]::None);$i.CopyTo($o);$o.Flush($true)} finally {if($o){$o.Dispose()};if($i){$i.Dispose()}}
 if((Get-FileHash -LiteralPath $target).Hash -ine $m.EfiSha256 -or (Get-FileHash -LiteralPath $audit).Hash -ine $m.AuditEfiSha256){throw 'Post-copy verification failed; preserve files and report'}
 "USB launcher staged at $target; staged audit hash verified. Backup: $backup. No firmware entries changed."
}
