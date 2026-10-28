# Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md.
param([switch]$PreflightOnly,[ValidateSet('3090','3090ti','3070ti','3060')][string]$Card='3090')
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$benchmarkNames = @('fp32_pedantic','fp16_tensor_fp32_accumulate','bf16_tensor_fp32_accumulate','tf32_tensor_fp32_accumulate')
# Reject overrides before executable/GPU queries; metadata inspection alone never runs GEMM.
if (![string]::IsNullOrEmpty($env:NVIDIA_TF32_OVERRIDE)) { throw 'Unset NVIDIA_TF32_OVERRIDE before running the four-mode benchmark' }
$exe = Join-Path $root "bin\compute-benchmark-$Card.exe"
$gpuName = @{'3090'='NVIDIA GeForce RTX 3090';'3090ti'='NVIDIA GeForce RTX 3090 Ti';'3070ti'='NVIDIA GeForce RTX 3070 Ti';'3060'='NVIDIA GeForce RTX 3060'}[$Card]
if (-not (Test-Path -LiteralPath $exe)) { throw 'Build the benchmark first' }
$manifest = Get-Content -LiteralPath (Join-Path $root 'build-manifest.json') -Raw | ConvertFrom-Json
$selectedArtifacts = 0
foreach ($artifact in $manifest) {
    if ((Get-FileHash -LiteralPath (Join-Path $root $artifact.Path) -Algorithm SHA256).Hash -ne $artifact.SHA256) { throw "Artifact changed: $($artifact.Path)" }
    if (($artifact.Path -replace '\\','/') -ieq "bin/compute-benchmark-$Card.exe") { $selectedArtifacts++ }
}
if ($selectedArtifacts -ne 1) { throw 'Build manifest must identify the selected benchmark executable exactly once' }
$inspect = & $exe --inspect-only
if ($LASTEXITCODE -ne 0) { throw 'Runtime metadata check failed' }
Write-Output $inspect
$runtime = $inspect | ConvertFrom-Json
if (($runtime.benchmark_protocol -isnot [int] -and $runtime.benchmark_protocol -isnot [long]) -or $runtime.benchmark_protocol -ne 2 -or $runtime.tf32_allowed -isnot [bool] -or !$runtime.tf32_allowed) { throw 'Rebuild the benchmark: four-mode TF32 protocol is required' }
if ($PreflightOnly) { Write-Output 'Preflight only: no GPU workload run'; exit 0 }
function Assert-GpuIdle {
    $rows = @(& nvidia-smi --query-gpu=name,pci.bus_id,driver_version,utilization.gpu,memory.used,temperature.gpu --format=csv,noheader,nounits)
    if ($LASTEXITCODE -ne 0 -or $rows.Count -ne 1) { throw 'Expected exactly one NVIDIA GPU' }
    $fields = $rows[0] -split ',' | ForEach-Object { $_.Trim() }
    if ($fields[0] -ne $gpuName -or $fields[1] -notmatch '^0+:01:00\.0$') { throw 'Unexpected GPU identity' }
    if ([int]$fields[3] -gt 5 -or [int]$fields[4] -gt 2048) { throw 'GPU is busy. Save and finish ComfyUI/other GPU jobs before benchmarking. Nothing was stopped.' }
    if ([int]$fields[5] -gt 65) { throw 'GPU is still hot. Let it cool before a comparable measurement.' }
    return $rows[0]
}
function Get-ComparisonSettings {
    $rows = @(& nvidia-smi --query-gpu=name,pci.bus_id,driver_version,power.limit,clocks.max.sm,clocks.max.memory --format=csv,noheader,nounits)
    if ($LASTEXITCODE -ne 0 -or $rows.Count -ne 1) { throw 'Could not capture comparison settings' }
    $fields = @($rows[0] -split ',' | ForEach-Object { $_.Trim() })
    if ($fields.Count -ne 6 -or $fields[0] -ne $gpuName -or $fields[1] -notmatch '^0+:01:00\.0$') { throw 'Unexpected comparison GPU identity' }
    return $fields
}
$initialState = Assert-GpuIdle
$settings = @(Get-ComparisonSettings)
$resultRoot = Join-Path $root ('results\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
if (Test-Path -LiteralPath $resultRoot) { throw 'Result directory exists' }
New-Item -ItemType Directory -Path $resultRoot | Out-Null
$initialState | Set-Content -LiteralPath (Join-Path $resultRoot 'gpu-before.csv') -Encoding utf8
# Snapshot provenance with the results, so a later rebuild cannot replace it.
# These values do not prove physical machine identity or unchanged overclock settings.
[pscustomobject]@{
    schema_version = 2
    benchmark_names = $benchmarkNames
    artifacts = @($manifest)
    runtime = $runtime
    gpu_settings = $settings
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $resultRoot 'comparison-context.json') -Encoding utf8
$measurements = @()
for ($trial = 1; $trial -le 3; $trial++) {
    # The initial check protects existing workloads. Trials themselves may leave transient utilization/heat.
    $json = & $exe 2> (Join-Path $resultRoot "trial-$trial.stderr.txt")
    if ($LASTEXITCODE -ne 0) { throw "Benchmark failed in trial $trial; keep partial evidence in $resultRoot" }
    $parsed = $json | ConvertFrom-Json
    if ($parsed.gpu -ne $gpuName -or $parsed.cuda_runtime -ne 12060 -or $parsed.cublas -ne 120600 -or $parsed.benchmarks.Count -ne 4 -or
        ($parsed.benchmark_protocol -isnot [int] -and $parsed.benchmark_protocol -isnot [long]) -or $parsed.benchmark_protocol -ne 2 -or $parsed.tf32_allowed -isnot [bool] -or !$parsed.tf32_allowed) { throw 'Unexpected benchmark metadata' }
    foreach ($name in $benchmarkNames) {
        $matched = @($parsed.benchmarks | Where-Object name -CEQ $name)
        if ($matched.Count -ne 1 -or $matched[0].reference_checked -isnot [bool] -or !$matched[0].reference_checked) { throw 'Missing, duplicate or unchecked benchmark mode' }
    }
    $json | Set-Content -LiteralPath (Join-Path $resultRoot "trial-$trial.json") -Encoding utf8
    $measurements += $parsed
    & nvidia-smi --query-gpu=timestamp,driver_version,clocks.sm,power.draw,temperature.gpu,clocks_event_reasons.sw_thermal_slowdown --format=csv |
        Set-Content -LiteralPath (Join-Path $resultRoot "trial-$trial-gpu-after.csv") -Encoding utf8
    if ($LASTEXITCODE -ne 0) { throw 'GPU telemetry failed; preserve partial results' }
    $currentSettings = @(Get-ComparisonSettings)
    if (($settings -join "`n") -cne ($currentSettings -join "`n")) { throw 'GPU comparison settings changed; preserve partial results' }
}
$summary = @()
foreach ($name in $benchmarkNames) {
    $values = @($measurements | ForEach-Object { $_.benchmarks | Where-Object name -EQ $name | ForEach-Object { [double]$_.gflops / 1000 } } | Sort-Object)
    $summary += [pscustomobject]@{Name=$name;MedianTflops=$values[1];MinTflops=$values[0];MaxTflops=$values[2]}
}
$summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $resultRoot 'summary.json') -Encoding utf8
# Written last: partial/failed runs must never qualify for offline comparison.
$completedFiles = @('comparison-context.json','trial-1.json','trial-2.json','trial-3.json') | ForEach-Object {
    [pscustomobject]@{Path=$_;SHA256=(Get-FileHash -LiteralPath (Join-Path $resultRoot $_) -Algorithm SHA256).Hash}
}
[pscustomobject]@{schema_version=1;files=@($completedFiles)} | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $resultRoot 'comparison-complete.json') -Encoding utf8
$summary | Format-Table -AutoSize
Write-Output "Three trials saved to $resultRoot. These are current-state measurements; no pre-unlock baseline or speedup is implied."
