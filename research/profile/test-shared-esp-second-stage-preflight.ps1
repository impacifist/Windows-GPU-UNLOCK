# Read-only SULK02 staging admission. No file copies, markers or firmware writes.
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[ValidatePattern('^[A-Za-z]$')][string]$Drive='D')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$boot=Join-Path $repo 'internal-boot/boot'
$build=Join-Path $boot 'build/shared-esp-unlock'
$m=Get-Content (Join-Path $build 'manifest.json') -Raw | ConvertFrom-Json
$l=Get-Content (Join-Path $build 'launcher-manifest.json') -Raw | ConvertFrom-Json
if($m.Mode -cne 'shared-esp-second-development' -or $m.AttemptSlot -ne 9 -or $m.CanUnlock -ne $true -or $l.Mode -cne 'shared-esp-second-launcher-development' -or $l.AuditFileName -cne 'EVGA-SHARED-UNLOCK02.EFI'){throw 'Expected second-attempt manifests'}
if([guid]$m.WindowsEspGuid -ne $WindowsEspGuid -or [guid]$l.WindowsEspGuid -ne $WindowsEspGuid){throw 'Manifest ESP mismatch'}
$childHash=(Get-FileHash (Join-Path $build 'BOOTX64.EFI')).Hash
$launcherHash=(Get-FileHash (Join-Path $build 'LAUNCHER.EFI')).Hash
if($childHash -ine $m.EfiSha256 -or $childHash -ine $l.AuditEfiSha256 -or $launcherHash -ine $l.EfiSha256){throw 'Built binary hash mismatch'}
foreach($pin in $m.Sources){
 $path=[IO.Path]::GetFullPath((Join-Path $boot $pin.Path))
 if(!$path.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Get-FileHash -LiteralPath $path).Hash -ine $pin.Sha256){throw 'Source changed after child build'}
}
foreach($pin in @(@('shared-esp-unlock-launcher.c',$l.LauncherSourceSha256),@('shared-esp-audit.c',$l.BaseAuditSha256),@('media.h',$l.MediaSha256))){if((Get-FileHash (Join-Path $boot $pin[0])).Hash -ine $pin[1]){throw 'Launcher source changed after build'}}
$history=& (Join-Path $PSScriptRoot 'inspect-shared-unlock-reconciliation.ps1') -WindowsEspGuid $WindowsEspGuid -Drive $Drive | ConvertFrom-Json
if($history.reviewed_next_slot -ne 9 -or @($history.checks.PSObject.Properties | Where-Object {$_.Value -ne $true}).Count){throw 'History qualification failed'}
$parts=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($parts.Count -ne 1){throw 'Expected unique pinned system ESP'}
$v=@($parts[0] | Get-Volume);if($v.Count -ne 1 -or $v[0].FileSystem -ne 'FAT32' -or !$v[0].Path){throw 'Expected accessible FAT32 ESP'}
$efi=Get-AuditCheckedPath $v[0].Path @('EFI') $true
$logs=Get-AuditCheckedPath $v[0].Path @('RTX3090-UNLOCK') $true
$targets=@((Join-Path $efi 'EVGA-SHARED-UNLOCK02.EFI'),(Join-Path $efi 'EVGA-SHARED-LAUNCH02.EFI'),(Join-Path $logs 'SULK02.ARMED'),(Join-Path $logs 'SULK02.TXT'),(Join-Path $logs 'EVGA-UNLOCK9.ARMED'))
foreach($target in $targets){if(Test-Path -LiteralPath $target){throw 'SULK02 target already exists; preserve partial state'}}
[ordered]@{read_only=$true;manifest_and_sources_match=$true;reviewed_next_slot=9;new_targets_absent=$true;child_sha256=$childHash;launcher_sha256=$launcherHash;staging_implemented=$false;boot_authorized=$false} | ConvertTo-Json
