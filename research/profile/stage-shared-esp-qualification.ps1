# GPL-2.0-only. Stages one attended GPU release/resources qualification image; never modifies firmware entries or reboots.
[CmdletBinding(SupportsShouldProcess)]
param([Parameter(Mandatory)][guid]$WindowsEspGuid)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $repo 'internal-boot/boot/build/shared-esp-qualification'
$manifest=Get-Content -LiteralPath (Join-Path $build 'manifest.json') -Raw | ConvertFrom-Json
if($WindowsEspGuid -eq [guid]::Empty -or [guid]$manifest.WindowsEspGuid -ne $WindowsEspGuid -or $manifest.CanUnlock -ne $false -or $manifest.Mode -ne 'shared-esp-release-qualification' -or $manifest.Visible -ne $true -or $manifest.AuditFileName -ne 'EVGA-SHARED-QUALIFICATION.EFI'){throw 'Audit build or ESP pin mismatch'}
$boot=Join-Path $repo 'internal-boot/boot'
foreach($pin in $manifest.Sources){
 $path=[IO.Path]::GetFullPath((Join-Path $boot $pin.Path))
 if(!$path.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Get-FileHash -LiteralPath $path).Hash -ine $pin.Sha256){throw 'Qualification source changed since build'}
}
$partition=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($partition.Count -ne 1){throw 'Expected exactly one pinned Windows system ESP'}
$volumes=@($partition[0] | Get-Volume -ErrorAction Stop)
if($volumes.Count -ne 1 -or $volumes[0].FileSystem -ne 'FAT32' -or !$volumes[0].Path){throw 'Expected accessible FAT32 system volume'}
$source=Join-Path $build 'BOOTX64.EFI'
if((Get-FileHash -LiteralPath $source).Hash -ine $manifest.EfiSha256){throw 'Preflight build hash mismatch; no write attempted'}
$root=$volumes[0].Path
$assetTarget=Get-AuditCheckedPath $root @('RTX3090-UNLOCK') $true
$assets=Get-AuditCheckedPath $root @('RTX3090-UNLOCK','ASSETS') $true
foreach($spec in (Get-Content -Raw -LiteralPath (Join-Path $repo 'internal-boot/unlock/assets.json') | ConvertFrom-Json)){
 $src=Get-AuditCheckedPath $assets @($spec.Name) $false
 if((Get-Item -LiteralPath $src).Length -ne $spec.Bytes -or (Get-FileHash -LiteralPath $src).Hash -ine $spec.Sha256){throw 'Existing ESP asset mismatch; preserve files'}
}
$priorMarker=Get-AuditCheckedPath $root @('RTX3090-UNLOCK','INTG01.ARMED') $false
$priorLog=Get-AuditCheckedPath $root @('RTX3090-UNLOCK','INTG01.TXT') $false
if((Get-FileHash -LiteralPath $priorMarker).Hash -ine 'e2fe1103ef03fe341df9b9b935ac13fd70cb6c0c792d4ce7c4228b2c248955c2' -or (Get-FileHash -LiteralPath $priorLog).Hash -ine 'f73b4001af51857f036460c3a6ee7139c3ffcea133f59b0b74d3e1eba7664d6c'){throw 'Expected reviewed successful INTG01 evidence; preserve files and report'}
$receipt=Join-Path $assetTarget 'QUAL01.ARMED'
if((Test-Path -LiteralPath (Join-Path $assetTarget 'QUAL01.TXT')) -or (Test-Path -LiteralPath (Join-Path $assetTarget 'QUAL01-GPU.ARMED'))){throw 'QUAL01 log already exists; preserve and report'}
if((Test-Path -LiteralPath $receipt) -or (Test-Path -LiteralPath (Join-Path $root 'EFI/EVGA-SHARED-QUALIFICATION.EFI'))){throw 'QUAL01 already staged or used; preserve and report'}
if($volumes[0].SizeRemaining -lt 16MB){throw 'Need 16 MiB free reserve; do not delete or resize'}
$windowsHash='2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'
$limineHash='a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462'
if($PSCmdlet.ShouldProcess('pinned Windows system ESP','Create EFI/EVGA-SHARED-QUALIFICATION.EFI without replacing existing files')){
 foreach($bootPin in @(@(@('EFI','Microsoft','Boot','bootmgfw.efi'),$windowsHash),@(@('EFI','limine','limine_x64.efi'),$limineHash))){
  $file=Get-AuditCheckedPath $root $bootPin[0] $false
  if((Get-FileHash -LiteralPath $file).Hash -ine $bootPin[1]){throw 'Boot file changed; no write attempted'}
 }
 $bytes=New-Object byte[] 512
 [Array]::Copy([Text.Encoding]::ASCII.GetBytes("PENDING QUAL01`r`n"),$bytes,16)
 $stream=$null
 try {$stream=[IO.File]::Open($receipt,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{if($stream){$stream.Dispose()}}
 if((Get-Item -LiteralPath $receipt).Length -ne 512 -or (Get-FileHash -LiteralPath $receipt).Hash -ine ([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash($bytes)).Replace('-',''))){throw 'Pending receipt verification failed; preserve it'}
 $target=Copy-SharedEspAudit -Root $volumes[0].Path -Source $source -ExpectedEfiHash $manifest.EfiSha256 -WindowsHash $windowsHash -LimineHash $limineHash -AuditFileName $manifest.AuditFileName
 Write-Output "Staged audit EFI at $target; existing boot file hashes verified. No boot entry changed or reboot performed."
}
