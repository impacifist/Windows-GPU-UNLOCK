# SPDX-License-Identifier: GPL-2.0-only
# Added 2026-10-26: isolated, verified dependencies for the ordinary CUDA baseline.
[CmdletBinding()]
param([switch]$Download)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = $PSScriptRoot
. (Join-Path $root 'package-tools.ps1')
$lock = Read-BaselineDependencyLock (Join-Path $root 'packages-lock.json')
$downloads = Join-Path $root 'downloads'
$packages = Join-Path $root 'packages'
if (-not $Download) {
    $status = foreach ($item in $lock.components) {
        $folder = Join-Path (Join-Path $packages $item.name) ([IO.Path]::GetFileNameWithoutExtension($item.relative_path))
        [pscustomobject]@{component=$item.name;version=$item.version;extracted_directory_present=(Test-Path -LiteralPath $folder -PathType Container)}
    }
    [pscustomobject]@{download_requested=$false;gpu_workload_run=$false;components=@($status)} | ConvertTo-Json -Depth 5
    return
}
New-Item -ItemType Directory -Force -Path $downloads,$packages | Out-Null
$manifestPath = Join-Path $downloads 'redistrib_12.6.0.json'
if (-not (Test-Path -LiteralPath $manifestPath)) {
    $partial = $manifestPath + '.partial-' + [guid]::NewGuid().ToString('N')
    Invoke-WebRequest -UseBasicParsing -Uri $lock.manifest_url -OutFile $partial -TimeoutSec 60
    if ((Get-BaselineFileSha256 $partial) -cne $lock.manifest_sha256) { throw 'Official manifest differs from the reviewed lock.' }
    [IO.File]::Move($partial,$manifestPath)
}
if ((Get-BaselineFileSha256 $manifestPath) -cne $lock.manifest_sha256) { throw 'Stored manifest hash mismatch.' }
$official = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$records = @()
foreach ($item in $lock.components) {
    $published = $official.($item.name).'windows-x86_64'
    if ($published.sha256 -cne $item.sha256 -or [long]$published.size -ne $item.bytes -or $published.relative_path -cne $item.relative_path) {
        throw 'Lock and official manifest disagree.'
    }
    $archive = Join-Path $downloads ([IO.Path]::GetFileName($item.relative_path))
    if (-not (Test-Path -LiteralPath $archive)) {
        $partial = $archive + '.partial-' + [guid]::NewGuid().ToString('N')
        Write-Output ('Downloading official component: ' + $item.name + ' ' + $item.version)
        Invoke-WebRequest -UseBasicParsing -Uri ('https://developer.download.nvidia.com/compute/cuda/redist/' + $item.relative_path) -OutFile $partial -TimeoutSec 300
        Assert-BaselineArchive -ArchivePath $partial -Component $item
        [IO.File]::Move($partial,$archive)
    }
    Assert-BaselineArchive -ArchivePath $archive -Component $item
    $archiveRoot = [IO.Path]::GetFileNameWithoutExtension($item.relative_path)
    $index = Get-BaselineArchiveIndex -ArchivePath $archive -ArchiveRoot $archiveRoot
    $destination = Join-Path $packages $item.name
    $created = Install-BaselineVerifiedArchive -ArchivePath $archive -Destination $destination -Index $index
    $action = if ($created) { 'Extracted and verified: ' } else { 'Reused verified component: ' }
    Write-Output ($action + $item.name)
    $records += [pscustomobject]@{
        component=$item.name;version=$item.version;archive=$item.relative_path
        sha256=$item.sha256;bytes=$item.bytes;tree_sha256=$index.tree_sha256
        file_count=@($index.files).Count;directory_count=@($index.directories).Count
    }
}
$receipt = [pscustomobject]@{schema_version=2;cuda_toolkit=$lock.cuda_toolkit;components=$records}
Write-BaselineAtomicJson -Path (Join-Path $root 'packages-manifest.json') -Value $receipt
Write-Output 'Pinned components verified locally; receipt published last. No installer, driver change or GPU workload was run.'
