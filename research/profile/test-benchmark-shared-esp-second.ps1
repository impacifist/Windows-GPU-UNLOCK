# Package orchestration only; synthetic runner and executable hash. No GPU or firmware access.
param([string]$RunLabel='SULK02')
$ErrorActionPreference='Stop'
$fixture=Join-Path $PSScriptRoot ('build/benchmark-package-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory((Join-Path $fixture 'bin'))
Set-Content -LiteralPath (Join-Path $fixture 'bin/compute-benchmark-3090.exe') -Value 'not executable'
Set-Content -LiteralPath (Join-Path $fixture 'build-manifest.json') -Value '[{"fixture":true}]'
Set-Content -LiteralPath (Join-Path $fixture 'runtime-verification.json') -Value '{"fixture":true}'
Set-Content -LiteralPath (Join-Path $fixture 'run-benchmark.ps1') -Value @'
param($Card)
if($Card -cne '3090'){throw 'Wrong card'}
$out=Join-Path $PSScriptRoot 'results/synthetic-run'
[void][IO.Directory]::CreateDirectory($out)
Set-Content -LiteralPath (Join-Path $out 'comparison-complete.json') -Value '{"fixture":true}'
'@
$global:packageBadHash=$false
function Get-FileHash {param([string]$LiteralPath)
 if($LiteralPath -like '*compute-benchmark-3090.exe'){return [pscustomobject]@{Hash=$(if($global:packageBadHash){'0'*64}else{'919ebddfc504e4aed119659bcb6786e5766cf48b2852969e440e11b6e55ef538'})}}
 Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath
}
$entry=Join-Path $PSScriptRoot 'benchmark-shared-esp-second.ps1'
$result=& $entry -BenchmarkDirectory $fixture -RunLabel $RunLabel | ConvertFrom-Json
if(!$result.benchmark_zip.EndsWith(('-'+$RunLabel+'-benchmark.zip'))){throw 'Incorrect run label'}
if(!(Test-Path -LiteralPath $result.benchmark_zip) -or $result.firmware_changed){throw 'Package did not finish'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::OpenRead($result.benchmark_zip)
try {foreach($name in @('comparison-complete.json','build-manifest.json','runtime-verification.json')){if(@($zip.Entries | Where-Object {$_.Name -ceq $name}).Count -ne 1){throw "Missing packaged $name"}}}finally{$zip.Dispose()}
$global:packageBadHash=$true;$blocked=$false
try{& $entry -BenchmarkDirectory $fixture}catch{$blocked=$true}
if(!$blocked){throw 'Changed baseline binary accepted'}
$global:packageBadHash=$false;$blocked=$false
try{& $entry -BenchmarkDirectory $fixture}catch{$blocked=$true}
if(!$blocked){throw 'Reused run accepted as new result'}
'PASS benchmark packaging, provenance, changed-binary refusal and stale-run refusal. Synthetic runner only; no hardware accessed.'
