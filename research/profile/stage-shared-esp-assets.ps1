# GPL-2.0-only. Stages one no-GPU audit file; never modifies firmware entries or reboots.
[CmdletBinding(SupportsShouldProcess)]
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[Parameter(Mandatory)][string]$Assets)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $repo 'internal-boot/boot/build/shared-esp-assets'
$manifest=Get-Content -LiteralPath (Join-Path $build 'manifest.json') -Raw | ConvertFrom-Json
if($WindowsEspGuid -eq [guid]::Empty -or [guid]$manifest.WindowsEspGuid -ne $WindowsEspGuid -or $manifest.Mode -ne 'shared-esp-no-gpu-assets' -or $manifest.Visible -ne $true -or $manifest.AuditFileName -ne 'EVGA-SHARED-ASSETS.EFI'){throw 'Audit build or ESP pin mismatch'}
if((Get-FileHash -LiteralPath (Join-Path $repo 'internal-boot/boot/shared-esp-assets.c')).Hash -ine $manifest.SourceSha256 -or (Get-FileHash -LiteralPath (Join-Path $repo 'internal-boot/boot/media.h')).Hash -ine $manifest.MediaSha256){throw 'Audit source changed since build'}
$partition=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($partition.Count -ne 1){throw 'Expected exactly one pinned Windows system ESP'}
$volumes=@($partition[0] | Get-Volume -ErrorAction Stop)
if($volumes.Count -ne 1 -or $volumes[0].FileSystem -ne 'FAT32' -or !$volumes[0].Path){throw 'Expected accessible FAT32 system volume'}
foreach($pin in @(@('internal-boot/boot/shared-esp-audit.c',$manifest.BaseAuditSha256),@('internal-boot/unlock/sha256.c',$manifest.Sha256SourceSha256))){
 if((Get-FileHash -LiteralPath (Join-Path $repo $pin[0])).Hash -ine $pin[1]){throw 'Source changed since build'}
}
$source=Join-Path $build 'AUDIT.EFI'
if((Get-FileHash -LiteralPath $source).Hash -ine $manifest.EfiSha256){throw 'Preflight build hash mismatch; no write attempted'}
$assetRootAttributes=[IO.File]::GetAttributes($Assets)
if(($assetRootAttributes -band [IO.FileAttributes]::ReparsePoint) -or !($assetRootAttributes -band [IO.FileAttributes]::Directory)){throw 'Invalid asset source root'}
$root=$volumes[0].Path
$assetTarget=Join-Path $root 'RTX3090-UNLOCK'
if(Test-Path -LiteralPath $assetTarget){throw 'RTX3090-UNLOCK already exists on ESP; preserve it and report'}
$specs=@(Get-Content -Raw -LiteralPath (Join-Path $repo 'internal-boot/unlock/assets.json') | ConvertFrom-Json)
$copies=@();[long]$required=16MB
foreach($spec in $specs){
 $src=Get-AuditCheckedPath $Assets @($spec.Name) $false
 if((Get-Item -LiteralPath $src).Length -ne $spec.Bytes -or (Get-FileHash -LiteralPath $src).Hash -ine $spec.Sha256){throw "Source asset mismatch: $($spec.Name)"}
 $required+=$spec.Bytes;$copies+=@{Source=$src;Name=$spec.Name;Hash=$spec.Sha256}
}
foreach($name in @('NVIDIA-LICENSE','NOTICE.md')){
 $src=Get-AuditCheckedPath $Assets @($name) $false
 $copies+=@{Source=$src;Name=$name;Hash=(Get-FileHash -LiteralPath $src).Hash}
}
if($volumes[0].SizeRemaining -lt $required){throw "Insufficient shared ESP space; need $required free bytes including 16 MiB reserve. Do not resize or delete files."}
if(Test-Path -LiteralPath (Join-Path $root 'EFI/EVGA-SHARED-ASSETS.EFI')){throw 'Preflight EFI already exists; preserve and report'}
$windowsHash='2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'
$limineHash='a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462'
if($PSCmdlet.ShouldProcess('pinned Windows system ESP','Create EFI/EVGA-SHARED-ASSETS.EFI without replacing existing files')){
 foreach($bootPin in @(@(@('EFI','Microsoft','Boot','bootmgfw.efi'),$windowsHash),@(@('EFI','limine','limine_x64.efi'),$limineHash))){
  $file=Get-AuditCheckedPath $root $bootPin[0] $false
  if((Get-FileHash -LiteralPath $file).Hash -ine $bootPin[1]){throw 'Boot file changed; no write attempted'}
 }
 [void][IO.Directory]::CreateDirectory((Join-Path $assetTarget 'ASSETS'))
 foreach($copy in $copies){
  $dst=Join-Path $assetTarget ('ASSETS/'+$copy.Name);$i=$null;$o=$null
  try {$i=[IO.File]::OpenRead($copy.Source);$o=[IO.File]::Open($dst,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$i.CopyTo($o);$o.Flush($true)}finally{if($o){$o.Dispose()};if($i){$i.Dispose()}}
  if((Get-FileHash -LiteralPath $dst).Hash -ine $copy.Hash){throw 'Asset copy mismatch; preserve partial files and report'}
 }
 $target=Copy-SharedEspAudit -Root $volumes[0].Path -Source $source -ExpectedEfiHash $manifest.EfiSha256 -WindowsHash $windowsHash -LimineHash $limineHash -AuditFileName $manifest.AuditFileName
 Write-Output "Staged audit EFI at $target; existing boot file hashes verified. No boot entry changed or reboot performed."
}
