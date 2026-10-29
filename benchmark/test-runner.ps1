# Host-only runner integration: synthetic managed executable and mocked nvidia-smi.
# Run with Windows PowerShell 5.1. No CUDA, real DLL loading or GPU access.
$ErrorActionPreference = 'Stop'
$testRoot = Join-Path $PSScriptRoot ('build\runner-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$stub = Join-Path $testRoot 'stub.exe'
Add-Type -OutputAssembly $stub -OutputType ConsoleApplication -TypeDefinition @'
using System;
public class BenchmarkStub {
    public static int Main(string[] args) {
        System.IO.File.AppendAllText(Environment.GetEnvironmentVariable("GPU_UNLOCK_TEST_EXE_LOG"), "called\n");
        Console.WriteLine(Environment.GetEnvironmentVariable(
            args.Length == 1 && args[0] == "--inspect-only" ?
            "GPU_UNLOCK_TEST_INSPECT" : "GPU_UNLOCK_TEST_TRIAL"));
        return 0;
    }
}
'@
$oldInspect = $env:GPU_UNLOCK_TEST_INSPECT
$oldTrial = $env:GPU_UNLOCK_TEST_TRIAL
$oldExeLog = $env:GPU_UNLOCK_TEST_EXE_LOG
$oldTf32Override = $env:NVIDIA_TF32_OVERRIDE
$oldTestState = Get-Variable -Name GpuUnlockRunnerTestState -Scope Global -ErrorAction SilentlyContinue
$global:GpuUnlockRunnerTestState = @{Calls=0;SettingsCalls=0;Scenario=''}
function nvidia-smi {
    $global:GpuUnlockRunnerTestState.Calls++
    Set-Variable -Name LASTEXITCODE -Scope 1 -Value 0
    # PowerShell functions receive comma-separated native arguments as arrays.
    $query = ($args | ForEach-Object { $_ }) -join ' '
    $gpu = $global:GpuUnlockRunnerTestState.Gpu
    if ($query -like '*power.limit*') {
        $global:GpuUnlockRunnerTestState.SettingsCalls++
        $power = '350.00'
        if ($global:GpuUnlockRunnerTestState.Scenario -eq 'settings-change' -and $global:GpuUnlockRunnerTestState.SettingsCalls -gt 1) { $power = '300.00' }
        if ($global:GpuUnlockRunnerTestState.Scenario -eq 'wrong-settings-gpu') { $gpu = 'NVIDIA GeForce RTX 4090' }
        return "$gpu, 00000000:01:00.0, 617.14, $power, 2100, 9751"
    }
    if ($query -like '*utilization.gpu*') {
        if ($global:GpuUnlockRunnerTestState.Scenario -eq 'wrong-gpu') { $gpu = 'NVIDIA GeForce RTX 4090' }
        return "$gpu, 00000000:01:00.0, 617.14, 0, 100, 40"
    }
    if ($global:GpuUnlockRunnerTestState.Scenario -eq 'telemetry-failure') { throw 'Synthetic telemetry failure' }
    return 'synthetic telemetry'
}
try {
    $baseInspect = '{"benchmark_protocol":2,"tf32_allowed":true,"cuda_runtime":12060,"cublas_major":12,"cublas_minor":6,"cublas_patch":0,"gpu_workload_run":false}'
    $benchmarks = foreach ($name in @('fp32_pedantic','fp16_tensor_fp32_accumulate','bf16_tensor_fp32_accumulate','tf32_tensor_fp32_accumulate')) {
        [pscustomobject]@{name=$name;n=4096;samples=30;warmup_seconds=2;median_ms=2;min_ms=1.9;max_ms=2.1;gflops=(2.0*4096*4096*4096/2e6);reference_checked=$true}
    }
    $cards = @{'3090'='NVIDIA GeForce RTX 3090';'3090ti'='NVIDIA GeForce RTX 3090 Ti';'3070ti'='NVIDIA GeForce RTX 3070 Ti';'3060'='NVIDIA GeForce RTX 3060'}
    foreach ($card in @('3090','3090ti','3070ti','3060')) {
    foreach ($scenario in @('preflight','success','telemetry-failure','settings-change','wrong-gpu','wrong-settings-gpu','missing-selected-hash','duplicate-selected-hash',
        'override-blocked','override-other-blocked','old-inspect','inspect-flag-string','inspect-flag-false','inspect-protocol-string',
        'missing-tf32','duplicate-mode','trial-flag-false','trial-flag-string','trial-flag-missing','trial-protocol-string')) {
        $env:NVIDIA_TF32_OVERRIDE = $null
        $env:GPU_UNLOCK_TEST_INSPECT = $baseInspect
        if ($scenario -eq 'override-blocked') { $env:NVIDIA_TF32_OVERRIDE = '0' }
        if ($scenario -eq 'override-other-blocked') { $env:NVIDIA_TF32_OVERRIDE = '1' }
        $inspection = $baseInspect | ConvertFrom-Json
        if ($scenario -eq 'old-inspect') { $inspection.PSObject.Properties.Remove('benchmark_protocol'); $inspection.PSObject.Properties.Remove('tf32_allowed') }
        if ($scenario -eq 'inspect-flag-string') { $inspection.tf32_allowed = 'true' }
        if ($scenario -eq 'inspect-flag-false') { $inspection.tf32_allowed = $false }
        if ($scenario -eq 'inspect-protocol-string') { $inspection.benchmark_protocol = '2' }
        $env:GPU_UNLOCK_TEST_INSPECT = $inspection | ConvertTo-Json -Compress
        $global:GpuUnlockRunnerTestState.Scenario = $scenario
        $global:GpuUnlockRunnerTestState.Gpu = $cards[$card]
        $global:GpuUnlockRunnerTestState.Calls = 0
        $global:GpuUnlockRunnerTestState.SettingsCalls = 0
        $trialRecord = [pscustomobject]@{benchmark_protocol=2;tf32_allowed=$true;gpu=$cards[$card];pci='0000:01:00.0';compute_capability='8.6';cuda_runtime=12060;cuda_driver_api=13000;cublas=120600;benchmarks=@($benchmarks)}
        if ($scenario -eq 'missing-tf32') { $trialRecord.benchmarks = @($benchmarks | Select-Object -First 3) }
        if ($scenario -eq 'duplicate-mode') { $trialRecord.benchmarks = @($benchmarks[0],$benchmarks[1],$benchmarks[2],$benchmarks[0]) }
        if ($scenario -eq 'trial-flag-false') { $trialRecord.tf32_allowed = $false }
        if ($scenario -eq 'trial-flag-string') { $trialRecord.tf32_allowed = 'true' }
        if ($scenario -eq 'trial-flag-missing') { $trialRecord.PSObject.Properties.Remove('tf32_allowed') }
        if ($scenario -eq 'trial-protocol-string') { $trialRecord.benchmark_protocol = '2' }
        $env:GPU_UNLOCK_TEST_TRIAL = $trialRecord | ConvertTo-Json -Depth 5 -Compress
        $caseRoot = Join-Path $testRoot "$card-$scenario"
        New-Item -ItemType Directory -Path (Join-Path $caseRoot 'bin') | Out-Null
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'run-benchmark.ps1') -Destination $caseRoot
        $env:GPU_UNLOCK_TEST_EXE_LOG = Join-Path $caseRoot 'stub-calls.txt'
        $relativeExe = "bin\compute-benchmark-$card.exe"
        Copy-Item -LiteralPath $stub -Destination (Join-Path $caseRoot $relativeExe)
        'not a DLL; hash fixture only' | Set-Content -LiteralPath (Join-Path $caseRoot 'bin\cublas64_12.dll') -Encoding utf8
        $manifest = @(foreach ($relative in @($relativeExe,'bin\cublas64_12.dll')) {
            [pscustomobject]@{Path=$relative;SHA256=(Get-FileHash -LiteralPath (Join-Path $caseRoot $relative)).Hash}
        })
        if ($scenario -eq 'missing-selected-hash') { $manifest = @($manifest[1]) }
        if ($scenario -eq 'duplicate-selected-hash') { $manifest += $manifest[0] }
        $manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $caseRoot 'build-manifest.json') -Encoding utf8
        $failed = $false
        # Exercise the default 3090 invocation as well as explicit other cards.
        $cardArguments = @{}
        if ($card -ne '3090') { $cardArguments.Card = $card }
        try {
            if ($scenario -eq 'preflight') { & (Join-Path $caseRoot 'run-benchmark.ps1') -PreflightOnly @cardArguments | Out-Null }
            else { & (Join-Path $caseRoot 'run-benchmark.ps1') @cardArguments | Out-Null }
        } catch {
            $failed = $true
            if ($scenario -in @('preflight','success')) { throw }
        }
        if ($scenario -eq 'preflight') {
            if ($global:GpuUnlockRunnerTestState.Calls -ne 0 -or (Test-Path (Join-Path $caseRoot 'results'))) { throw 'Preflight touched GPU queries or results' }
        } elseif ($scenario -eq 'success') {
            if ($failed) { throw 'Synthetic success failed' }
            $result = @(Get-ChildItem -LiteralPath (Join-Path $caseRoot 'results') -Directory)
            if ($result.Count -ne 1) { throw 'Expected one result directory' }
            $summary = Get-Content -LiteralPath (Join-Path $result[0].FullName 'summary.json') -Raw | ConvertFrom-Json
            if ($summary.Count -ne 4 -or @($summary | Where-Object Name -CEQ 'tf32_tensor_fp32_accumulate').Count -ne 1) { throw 'TF32 missing from four-mode summary' }
            # Exercise the real Python reader against PowerShell-produced BOM JSON.
            & python -c 'import sys; from pathlib import Path; sys.path.insert(0, sys.argv[1]); from compare_results import load_run; load_run(Path(sys.argv[2]))' $PSScriptRoot $result[0].FullName
            if ($LASTEXITCODE -ne 0) { throw 'PowerShell evidence is not accepted by Python reader' }
        } else {
            if (-not $failed) { throw "Expected failure: $scenario" }
            if (Test-Path (Join-Path $caseRoot 'results')) {
                if (Get-ChildItem -LiteralPath (Join-Path $caseRoot 'results') -Recurse -Filter comparison-complete.json) { throw 'Failed run produced completion record' }
            }
            if ($scenario -in @('missing-selected-hash','duplicate-selected-hash','override-blocked','override-other-blocked')) {
                if (Test-Path $env:GPU_UNLOCK_TEST_EXE_LOG) { throw 'Unidentified executable was run' }
                if ($global:GpuUnlockRunnerTestState.Calls) { throw 'Manifest failure reached GPU queries' }
            }
            if ($scenario -in @('old-inspect','inspect-flag-string','inspect-flag-false','inspect-protocol-string') -and $global:GpuUnlockRunnerTestState.Calls) { throw 'Invalid TF32 inspection reached GPU queries' }
        }
        Write-Output "PASS $card $scenario (synthetic host only)"
    }
    }
    Write-Output "Evidence: $testRoot"
} finally {
    $env:GPU_UNLOCK_TEST_INSPECT = $oldInspect
    $env:GPU_UNLOCK_TEST_TRIAL = $oldTrial
    $env:GPU_UNLOCK_TEST_EXE_LOG = $oldExeLog
    $env:NVIDIA_TF32_OVERRIDE = $oldTf32Override
    if ($null -ne $oldTestState) { $global:GpuUnlockRunnerTestState = $oldTestState.Value }
    else { Remove-Variable -Name GpuUnlockRunnerTestState -Scope Global }
}
