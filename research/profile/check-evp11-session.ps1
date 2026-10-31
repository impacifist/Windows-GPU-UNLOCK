# One Windows session check: preserve results, inspect boot entries, run reviewed benchmark.
param([string]$StateDirectory='E:\UNLOCK\EVGA-PROFILE',[string]$BenchmarkDirectory=(Join-Path $PSScriptRoot '../../benchmark'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'evga-package-lib.ps1')
$attempt=Get-Content -LiteralPath (Join-Path $StateDirectory 'attempt-11.json') -Raw | ConvertFrom-Json
$result=Get-Content -LiteralPath (Join-Path $StateDirectory 'result-11.json') -Raw | ConvertFrom-Json
if($attempt.AttemptSlot -ne 11 -or $attempt.Run -cne 'EVP11' -or $result.AttemptSlot -ne 11 -or $result.Success -ne $true){throw 'Expected successful collected EVP11'}
$proof=Assert-EvgaPackageResult $attempt.TargetRoot 11 'EVP11'
if($proof.LogSha256 -cne '386a2b17d30aa2eb18e36fd8d742e84518be69e2093214a863f40c3191b5f768' -or $proof.GateSha256 -cne $result.GateSha256 -or $proof.GpuMarkerSha256 -cne $result.GpuMarkerSha256){throw 'Reviewed EVP11 evidence differs'}
$zip=Join-Path $StateDirectory 'EVP11-results.zip'
if(!(Test-Path -LiteralPath $zip)){
 $folder=Join-Path $StateDirectory 'EVP11-results'
 if(!(Test-Path -LiteralPath $folder -PathType Container)){throw 'Missing collected result directory; preserve state and report'}
 Compress-Archive -LiteralPath $folder -DestinationPath $zip -ErrorAction Stop
}
& (Join-Path $PSScriptRoot 'inspect-evga-package-failure.ps1') -StateDirectory $StateDirectory
& (Join-Path $PSScriptRoot 'benchmark-shared-esp-second.ps1') -BenchmarkDirectory $BenchmarkDirectory -RunLabel EVP11
