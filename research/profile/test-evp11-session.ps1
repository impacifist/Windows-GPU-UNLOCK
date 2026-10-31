param([Parameter(Mandatory)][string]$PackageRoot)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test-evga-profile-manager.ps1') -PackageRoot $PackageRoot
$fixture=Join-Path $fixture 'session-check'
$f=Setup-ManagerFixture 'SharedEsp'
$null=& $manager -StateDirectory $f.State -Assets $f.Assets -PackageDirectory (Join-Path $PackageRoot 'SharedEsp-10')
$a=Get-Content -LiteralPath (Join-Path $f.State 'attempt-10.json') -Raw | ConvertFrom-Json;Simulate-PackageRun $a
$null=& $manager -Action Collect -StateDirectory $f.State
$null=& $manager -Action Rearm -StateDirectory $f.State -Assets $f.Assets -PackageDirectory (Join-Path $PackageRoot 'SharedEsp-11')
$a=Get-Content -LiteralPath (Join-Path $f.State 'attempt-11.json') -Raw | ConvertFrom-Json;Simulate-PackageRun $a
$null=& $manager -Action Collect -StateDirectory $f.State
$bench=Join-Path $fixture 'benchmark';[void][IO.Directory]::CreateDirectory((Join-Path $bench 'bin'))
Set-Content -LiteralPath (Join-Path $bench 'bin/compute-benchmark-3090.exe') -Value 'fake'
foreach($name in @('build-manifest.json','runtime-verification.json')){Set-Content -LiteralPath (Join-Path $bench $name) -Value '{}'}
Set-Content -LiteralPath (Join-Path $bench 'run-benchmark.ps1') -Value @'
param($Card)
$out=Join-Path $PSScriptRoot 'results/synthetic';[void][IO.Directory]::CreateDirectory($out)
Set-Content -LiteralPath (Join-Path $out 'comparison-complete.json') -Value '{}'
'@
$global:sessionBaseHash=${function:Get-FileHash}
function Get-FileHash([string]$LiteralPath,[string]$Algorithm){
 if([IO.Path]::GetFileName($LiteralPath) -ceq 'EVP11.TXT'){return [pscustomobject]@{Hash='386a2b17d30aa2eb18e36fd8d742e84518be69e2093214a863f40c3191b5f768'}}
 if($LiteralPath -like '*compute-benchmark-3090.exe'){return [pscustomobject]@{Hash='919ebddfc504e4aed119659bcb6786e5766cf48b2852969e440e11b6e55ef538'}}
 & $global:sessionBaseHash -LiteralPath $LiteralPath -Algorithm $Algorithm
}
$writes=[SharedEspFirmware.Native]::Writes.Count
$output=& (Join-Path $PSScriptRoot 'check-evp11-session.ps1') -StateDirectory $f.State -BenchmarkDirectory $bench
$joined=$output -join "`n"
if($joined -notmatch 'EVP11-benchmark.zip' -or $joined -notmatch 'inspection-' -or [SharedEspFirmware.Native]::Writes.Count -ne $writes){throw 'Combined session check failed/mutated firmware'}
Write-Host 'PASS combined successful EVP11 evidence guard, read-only firmware inspection and labelled synthetic benchmark ZIP. No native hardware access.'
