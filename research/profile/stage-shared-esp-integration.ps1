# GPL-2.0-only. Stages one observation-only integration image; never modifies firmware entries or reboots.
[CmdletBinding(SupportsShouldProcess)]
param([Parameter(Mandatory)][guid]$WindowsEspGuid)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $repo 'internal-boot/boot/build/shared-esp-integration'
$manifest=Get-Content -LiteralPath (Join-Path $build 'manifest.json') -Raw | ConvertFrom-Json
if($WindowsEspGuid -eq [guid]::Empty -or [guid]$manifest.WindowsEspGuid -ne $WindowsEspGuid -or $manifest.Mode -ne 'shared-esp-observe-integration' -or $manifest.Visible -ne $true -or $manifest.AuditFileName -ne 'EVGA-SHARED-INTEGRATION.EFI'){throw 'Audit build or ESP pin mismatch'}
if((Get-FileHash -LiteralPath (Join-Path $repo 'internal-boot/boot/shared-esp-integration.c')).Hash -ine $manifest.SourceSha256 -or (Get-FileHash -LiteralPath (Join-Path $repo 'internal-boot/boot/media.h')).Hash -ine $manifest.MediaSha256){throw 'Audit source changed since build'}
$partition=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($partition.Count -ne 1){throw 'Expected exactly one pinned Windows system ESP'}
$volumes=@($partition[0] | Get-Volume -ErrorAction Stop)
if($volumes.Count -ne 1 -or $volumes[0].FileSystem -ne 'FAT32' -or !$volumes[0].Path){throw 'Expected accessible FAT32 system volume'}
foreach($pin in @(@('internal-boot/boot/shared-esp-audit.c',$manifest.BaseAuditSha256),@('internal-boot/unlock/sha256.c',$manifest.Sha256SourceSha256),@('internal-boot/efi_runtime.c',$manifest.RuntimeSha256),@('internal-boot/boot/handoff.c',$manifest.HandoffSha256),@('internal-boot/boot/diagnostic.c',$manifest.DiagnosticSha256))){
 if((Get-FileHash -LiteralPath (Join-Path $repo $pin[0])).Hash -ine $pin[1]){throw 'Source changed since build'}
}
$source=Join-Path $build 'AUDIT.EFI'
if((Get-FileHash -LiteralPath $source).Hash -ine $manifest.EfiSha256){throw 'Preflight build hash mismatch; no write attempted'}
$root=$volumes[0].Path
$assetTarget=Get-AuditCheckedPath $root @('RTX3090-UNLOCK') $true
$assets=Get-AuditCheckedPath $root @('RTX3090-UNLOCK','ASSETS') $true
if((Get-FileHash -LiteralPath (Join-Path $repo 'internal-boot/boot/shared-esp-assets.c')).Hash -ine $manifest.AssetsSourceSha256){throw 'Included asset source changed'}
foreach($spec in (Get-Content -Raw -LiteralPath (Join-Path $repo 'internal-boot/unlock/assets.json') | ConvertFrom-Json)){
 $src=Get-AuditCheckedPath $assets @($spec.Name) $false
 if((Get-Item -LiteralPath $src).Length -ne $spec.Bytes -or (Get-FileHash -LiteralPath $src).Hash -ine $spec.Sha256){throw 'Existing ESP asset mismatch; preserve files'}
}
$prior=Get-AuditCheckedPath $root @('RTX3090-UNLOCK','RETAIN01.TXT') $false
$priorBytes=[IO.File]::ReadAllBytes($prior)
if($priorBytes.Length -ne 512 -or [Text.Encoding]::ASCII.GetString($priorBytes).TrimEnd([char]0) -ne "COMPLETE RETAIN01 - FIVE ASSETS RETAINED REHASHED FREED - NO GPU`r`n"){throw 'RETAIN01 completion receipt is required; preserve and report'}
$receipt=Join-Path $assetTarget 'INTG01.ARMED'
if(Test-Path -LiteralPath (Join-Path $assetTarget 'INTG01.TXT')){throw 'INTG01 log already exists; preserve and report'}
if((Test-Path -LiteralPath $receipt) -or (Test-Path -LiteralPath (Join-Path $root 'EFI/EVGA-SHARED-INTEGRATION.EFI'))){throw 'INTG01 already staged or used; preserve and report'}
if($volumes[0].SizeRemaining -lt 16MB){throw 'Need 16 MiB free reserve; do not delete or resize'}
$windowsHash='2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'
$limineHash='a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462'
if($PSCmdlet.ShouldProcess('pinned Windows system ESP','Create EFI/EVGA-SHARED-INTEGRATION.EFI without replacing existing files')){
 foreach($bootPin in @(@(@('EFI','Microsoft','Boot','bootmgfw.efi'),$windowsHash),@(@('EFI','limine','limine_x64.efi'),$limineHash))){
  $file=Get-AuditCheckedPath $root $bootPin[0] $false
  if((Get-FileHash -LiteralPath $file).Hash -ine $bootPin[1]){throw 'Boot file changed; no write attempted'}
 }
 $bytes=New-Object byte[] 512
 [Array]::Copy([Text.Encoding]::ASCII.GetBytes("PENDING INTG01`r`n"),$bytes,16)
 $stream=$null
 try {$stream=[IO.File]::Open($receipt,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{if($stream){$stream.Dispose()}}
 if((Get-Item -LiteralPath $receipt).Length -ne 512 -or (Get-FileHash -LiteralPath $receipt).Hash -ine ([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash($bytes)).Replace('-',''))){throw 'Pending receipt verification failed; preserve it'}
 $target=Copy-SharedEspAudit -Root $volumes[0].Path -Source $source -ExpectedEfiHash $manifest.EfiSha256 -WindowsHash $windowsHash -LimineHash $limineHash -AuditFileName $manifest.AuditFileName
 Write-Output "Staged audit EFI at $target; existing boot file hashes verified. No boot entry changed or reboot performed."
}
