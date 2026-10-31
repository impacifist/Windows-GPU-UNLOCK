# GPL-2.0-only. Stages one no-GPU audit file; never modifies firmware entries or reboots.
[CmdletBinding(SupportsShouldProcess)]
param([Parameter(Mandatory)][guid]$WindowsEspGuid)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $repo 'internal-boot/boot/build/shared-esp-audit'
$manifest=Get-Content -LiteralPath (Join-Path $build 'manifest.json') -Raw | ConvertFrom-Json
if($WindowsEspGuid -eq [guid]::Empty -or [guid]$manifest.WindowsEspGuid -ne $WindowsEspGuid -or $manifest.Mode -ne 'shared-esp-no-gpu-audit' -or $manifest.Visible -ne $true -or $manifest.AuditFileName -ne 'EVGA-SHARED-AUDIT-VISIBLE.EFI'){throw 'Audit build or ESP pin mismatch'}
if((Get-FileHash -LiteralPath (Join-Path $repo 'internal-boot/boot/shared-esp-audit.c')).Hash -ine $manifest.SourceSha256 -or (Get-FileHash -LiteralPath (Join-Path $repo 'internal-boot/boot/media.h')).Hash -ine $manifest.MediaSha256){throw 'Audit source changed since build'}
$partition=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($partition.Count -ne 1){throw 'Expected exactly one pinned Windows system ESP'}
$volumes=@($partition[0] | Get-Volume -ErrorAction Stop)
if($volumes.Count -ne 1 -or $volumes[0].FileSystem -ne 'FAT32' -or !$volumes[0].Path){throw 'Expected accessible FAT32 system volume'}
$source=Join-Path $build 'AUDIT.EFI'
$windowsHash='2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'
$limineHash='a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462'
if($PSCmdlet.ShouldProcess('pinned Windows system ESP','Create EFI/EVGA-SHARED-AUDIT-VISIBLE.EFI without replacing existing files')){
 $target=Copy-SharedEspAudit -Root $volumes[0].Path -Source $source -ExpectedEfiHash $manifest.EfiSha256 -WindowsHash $windowsHash -LimineHash $limineHash -AuditFileName $manifest.AuditFileName
 Write-Output "Staged audit EFI at $target; existing boot file hashes verified. No boot entry changed or reboot performed."
}
