# SPDX-License-Identifier: GPL-2.0-only
# Added 2026-10-26: compile the ordinary baseline without executing its output.
[CmdletBinding()]
param(
    [string]$VcVarsPath,
    [ValidatePattern('^14\.(2[0-9]|3[0-9])(\.\d+)?$')][string]$MsvcToolset,
    [switch]$CheckOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
. (Join-Path $root 'package-tools.ps1')
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $root '..\..'))
$lock = Read-BaselineDependencyLock (Join-Path $root 'packages-lock.json')
$vswhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = $null
if (-not $VcVarsPath -and (Test-Path -LiteralPath $vswhere)) {
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0) { throw 'Visual Studio discovery failed.' }
    if ($installation) {
        $installation = [string]$installation
        $VcVarsPath = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
    }
}
if ($VcVarsPath) {
    $VcVarsPath = [IO.Path]::GetFullPath($VcVarsPath)
    if ($VcVarsPath -match '["%&|<>!^]') { throw 'Unsupported characters in compiler setup path.' }
}
$compilerPresent = [bool]($VcVarsPath -and (Test-Path -LiteralPath $VcVarsPath -PathType Leaf))
$toolsets = @()
if ($installation) {
    $toolsRoot = Join-Path $installation 'VC\Tools\MSVC'
    if (Test-Path -LiteralPath $toolsRoot) { $toolsets = @(Get-ChildItem -LiteralPath $toolsRoot -Directory | Select-Object -ExpandProperty Name) }
}
$documented = @($toolsets | Where-Object { $_ -match '^14\.(2[0-9]|3[0-9])\.' })
$componentPaths = @{}
$packagesPresent = $true
foreach ($item in $lock.components) {
    $path = Join-Path (Join-Path (Join-Path $root 'packages') $item.name) ([IO.Path]::GetFileNameWithoutExtension($item.relative_path))
    $componentPaths[$item.name] = $path
    if (-not (Test-Path -LiteralPath $path -PathType Container)) { $packagesPresent = $false }
}
if ($CheckOnly) {
    [pscustomobject]@{
        benchmark_protocol=3;cuda_toolkit=$lock.cuda_toolkit
        compiler_setup_present=$compilerPresent
        enumerated_msvc_toolsets=$toolsets
        documented_msvc_toolsets=$documented
        project_packages_present=$packagesPresent
        ambient_compiler_override_names=@(Get-BaselineCompilerOverrides)
        compilation_attempted=$false;gpu_workload_run=$false
        note='CUDA 12.6 lists MSVC 192x/193x; custom compiler paths require an actual environment/version check during build.'
    } | ConvertTo-Json -Depth 4
    return
}
if (-not $compilerPresent) { throw 'Provide an existing compatible x64 compiler setup with -VcVarsPath.' }
if (-not $packagesPresent) { throw 'Pinned project-local dependencies are absent; review setup.ps1 before requesting downloads.' }
$receiptPath = Join-Path $root 'packages-manifest.json'
if (-not (Test-Path -LiteralPath $receiptPath)) { throw 'Verified dependency receipt is missing.' }
$receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
if ($receipt.schema_version -ne 2 -or $receipt.cuda_toolkit -ne '12.6.0' -or @($receipt.components).Count -ne 4) { throw 'Unexpected dependency receipt; verify setup again.' }
foreach ($item in $lock.components) {
    $record = @($receipt.components | Where-Object { $_.component -ceq $item.name })
    if ($record.Count -ne 1 -or $record[0].sha256 -cne $item.sha256 -or $record[0].version -cne $item.version -or $record[0].archive -cne $item.relative_path -or $record[0].bytes -ne $item.bytes) { throw 'Receipt and dependency lock differ.' }
    $archive = Join-Path (Join-Path $root 'downloads') ([IO.Path]::GetFileName($item.relative_path))
    Assert-BaselineArchive -ArchivePath $archive -Component $item
    $index = Get-BaselineArchiveIndex -ArchivePath $archive -ArchiveRoot ([IO.Path]::GetFileNameWithoutExtension($item.relative_path))
    Assert-BaselineExtractedTree -Directory (Join-Path (Join-Path $root 'packages') $item.name) -Index $index
    if ($record[0].tree_sha256 -cne $index.tree_sha256 -or $record[0].file_count -ne @($index.files).Count -or $record[0].directory_count -ne @($index.directories).Count) { throw 'Receipt and verified extracted tree differ.' }
}
Assert-BaselineCleanCompilerEnvironment
$originalEnvironment = [Environment]::GetEnvironmentVariables('Process')
try {
    $selection = if ($MsvcToolset) { ' -vcvars_ver=' + $MsvcToolset } else { '' }
    $command = '""' + $VcVarsPath + '"' + $selection + ' >nul && set"'
    $lines = & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
    foreach ($line in $lines) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') }
    }
    Assert-BaselineCleanCompilerEnvironment
    if ($env:VCToolsVersion -notmatch '^14\.(2[0-9]|3[0-9])\.' -or $env:VSCMD_ARG_TGT_ARCH -ne 'x64' -or $env:VSCMD_ARG_HOST_ARCH -ne 'x64') {
        throw 'This CUDA 12.6 build requires a reviewed x64 MSVC 14.2x/14.3x toolset. No unsupported-compiler override is available.'
    }
    $compilerCommand = Get-Command cl.exe -CommandType Application -ErrorAction Stop
    $expectedCompiler = Join-Path $env:VCToolsInstallDir 'bin\Hostx64\x64\cl.exe'
    $compiler = Get-BaselineCompilerDescriptor -CompilerPath $compilerCommand.Source -ExpectedPath $expectedCompiler -ToolsetVersion $env:VCToolsVersion
    $nvcc = Join-Path $componentPaths.cuda_nvcc 'bin\nvcc.exe'
    $runtime = $componentPaths.cuda_cudart
    $cccl = $componentPaths.cuda_cccl
    $blas = $componentPaths.libcublas
    if (-not (Test-Path -LiteralPath $nvcc)) { throw 'Pinned NVCC is missing.' }
    $sourcePaths = @('benchmark/rtx3080/baseline.cu','benchmark/rtx3080/numerics.hpp','benchmark/rtx3080/result_protocol.py','benchmark/rtx3080/run.py','benchmark/rtx3080/build.ps1','benchmark/rtx3080/setup.ps1','benchmark/rtx3080/packages-lock.json','benchmark/rtx3080/package-tools.ps1')
    $sources = @(Get-BaselineSourceRecords -RepositoryRoot $repositoryRoot -Paths $sourcePaths)
    $commit = [string](& git -C $repositoryRoot rev-parse HEAD)
    if ($LASTEXITCODE -ne 0 -or $commit -cnotmatch '^[a-f0-9]{40}$') { throw 'Source commit could not be identified.' }
    $statusLines = @(& git -C $repositoryRoot status --porcelain)
    if ($LASTEXITCODE -ne 0) { throw 'Source working-tree state could not be identified.' }
    $dirty = [bool]$statusLines.Count
    $buildDirectory = Join-Path $root ('build\native-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $buildDirectory | Out-Null
    $output = Join-Path $buildDirectory 'rtx3080-baseline.exe'
    $flags = @('-std=c++17','-O3','-arch=sm_86','--cudart=static','-Xcompiler','/EHsc')
    $arguments = $flags + @('-ccbin',$compiler.compiler_directory,("-I" + $runtime + '\include'),("-I" + $cccl + '\include'),("-I" + $blas + '\include'),("-L" + $runtime + '\lib\x64'),("-L" + $blas + '\lib\x64'),(Join-Path $root 'baseline.cu'),'-lcublas','-o',$output)
    Push-Location $buildDirectory
    try {
        & $nvcc @arguments
        if ($LASTEXITCODE -ne 0) { throw 'CUDA compilation failed; no manifest published.' }
    } finally { Pop-Location }
    $currentCommit = [string](& git -C $repositoryRoot rev-parse HEAD)
    if ($LASTEXITCODE -ne 0) { throw 'Source verification failed; no artifacts or manifest published.' }
    Assert-BaselineSourceSnapshot -RepositoryRoot $repositoryRoot -Records $sources -ExpectedCommit $commit -CurrentCommit $currentCommit
    $bin = Join-Path $root 'bin'
    New-Item -ItemType Directory -Force -Path $bin | Out-Null
    Copy-Item -LiteralPath $output -Destination (Join-Path $bin 'rtx3080-baseline.exe')
    foreach ($dll in @('cublas64_12.dll','cublasLt64_12.dll')) { Copy-Item -LiteralPath (Join-Path (Join-Path $blas 'bin') $dll) -Destination (Join-Path $bin $dll) }
    $artifacts = foreach ($relative in @('bin/rtx3080-baseline.exe','bin/cublas64_12.dll','bin/cublasLt64_12.dll')) {
        [pscustomobject]@{path=$relative;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root $relative)).Hash.ToLowerInvariant()}
    }
    $manifest = [pscustomobject]@{
        schema_version=1;benchmark_protocol=3;source_commit=$commit;source_dirty=$dirty
        toolchain=[pscustomobject]@{cuda_toolkit='12.6.0';nvcc='12.6.20';cublas='12.6.0.22';msvc_toolset=$env:VCToolsVersion;msvc_compiler_version=$compiler.msvc_compiler_version;msvc_compiler_sha256=$compiler.msvc_compiler_sha256;target_arch='sm_86';compiler_options=$flags}
        artifacts=@($artifacts);sources=@($sources)
    }
    $manifestPath = Join-Path $root 'build-manifest.json'
    Write-BaselineAtomicJson -Path $manifestPath -Value $manifest
    Write-Output 'CUDA baseline compiled; artifact/source manifest written last. Native output was not executed.'
} finally {
    foreach ($key in @([Environment]::GetEnvironmentVariables('Process').Keys)) {
        if (-not $originalEnvironment.Contains($key)) { [Environment]::SetEnvironmentVariable($key,$null,'Process') }
    }
    foreach ($key in $originalEnvironment.Keys) { [Environment]::SetEnvironmentVariable($key,[string]$originalEnvironment[$key],'Process') }
}
