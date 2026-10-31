# Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md.
param([string]$VcVars,[switch]$AllowUnsupportedCompiler,[ValidateSet('3090','3090ti','3070ti')][string]$Card='3090')
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$nvcc = Join-Path $root 'packages\cuda_nvcc\cuda_nvcc-windows-x86_64-12.6.20-archive'
$runtime = Join-Path $root 'packages\cuda_cudart\cuda_cudart-windows-x86_64-12.6.37-archive'
$cccl = Join-Path $root 'packages\cuda_cccl\cuda_cccl-windows-x86_64-12.6.37-archive'
$blas = Join-Path $root 'packages\libcublas\libcublas-windows-x86_64-12.6.0.22-archive'
$vcvars = $VcVars
if ([string]::IsNullOrWhiteSpace($vcvars)) {
    # CUDA 12.6 supports VS2019; never select a newer installation just because it is latest.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'vswhere missing; supply -VcVars for a supported C++ toolchain' }
    $installation = @(& $vswhere -latest -products '*' -version '[16.0,17.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
    if ($LASTEXITCODE -ne 0 -or $installation.Count -ne 1 -or [string]::IsNullOrWhiteSpace($installation[0])) {
        throw 'VS2019 C++ x64 tools not found; supply -VcVars for a supported toolchain. No compiler guard was bypassed.'
    }
    $vcvars = Join-Path $installation[0] 'VC\Auxiliary\Build\vcvars64.bat'
}
if (-not (Test-Path -LiteralPath $vcvars)) { throw "C++ environment script missing: $vcvars" }
Write-Output "Using C++ environment: $vcvars"
$bin = Join-Path $root 'bin'
New-Item -ItemType Directory -Force -Path $bin | Out-Null
# Import vcvars only into this child PowerShell process, never the user/machine environment.
$vcLines = & $env:ComSpec /d /c "`"$vcvars`" >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'vcvars64 failed' }
foreach ($line in $vcLines) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
}
# CUDA 12.6 nvcc predates MSVC 14.4x (VS 2022 17.14); opt in explicitly when building with it.
$compat = @(if ($AllowUnsupportedCompiler) { '-allow-unsupported-compiler' })
& (Join-Path $nvcc 'bin\nvcc.exe') @compat -std=c++17 -O3 -arch=sm_86 -Xcompiler /utf-8 `
    "-I$runtime\include" "-I$cccl\include" "-I$blas\include" `
    "-L$runtime\lib\x64" "-L$blas\lib\x64" `
    (Join-Path $root "compute-benchmark-$Card.cu") -lcublas -o (Join-Path $bin "compute-benchmark-$Card.exe")
if ($LASTEXITCODE -ne 0) { throw "nvcc failed: $LASTEXITCODE" }
Get-ChildItem -LiteralPath (Join-Path $blas 'bin') -Filter '*.dll' | Copy-Item -Destination $bin
$inspect = & (Join-Path $bin "compute-benchmark-$Card.exe") --inspect-only
if ($LASTEXITCODE -ne 0) { throw 'Metadata-only binary check failed' }
$versions = $inspect | ConvertFrom-Json
if ($versions.cuda_runtime -ne 12060 -or $versions.cublas_major -ne 12 -or $versions.cublas_minor -ne 6 -or $versions.cublas_patch -ne 0) { throw 'Unexpected CUDA/cuBLAS version' }
if (($versions.benchmark_protocol -isnot [int] -and $versions.benchmark_protocol -isnot [long]) -or $versions.benchmark_protocol -ne 2 -or $versions.tf32_allowed -isnot [bool]) { throw 'Unexpected four-mode TF32 metadata protocol' }
$inspect | Set-Content -LiteralPath (Join-Path $root 'runtime-verification.json') -Encoding utf8
$selectedArtifactNames = @("compute-benchmark-$Card.exe","compute-benchmark-$Card.exp","compute-benchmark-$Card.lib")
$artifactPaths = @((Join-Path $root "compute-benchmark-$Card.cu"),(Join-Path $root 'upstream-compute-benchmark.cu')) + @(Get-ChildItem -LiteralPath $bin -File | Where-Object { $_.Name -in $selectedArtifactNames -or $_.Extension -ieq '.dll' } | Select-Object -ExpandProperty FullName)
$artifactPaths | ForEach-Object { $f = Get-Item -LiteralPath $_; [pscustomobject]@{Path=$f.FullName.Substring($root.Length+1);Bytes=$f.Length;SHA256=(Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash} } |
    ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $root 'build-manifest.json') -Encoding utf8
Write-Output $inspect
Write-Output 'Build and metadata-only check passed. No GEMM workload was run.'
