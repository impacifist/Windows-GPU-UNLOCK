# Run the existing reviewed benchmark and package its output. No firmware cleanup or reboot.
param([string]$BenchmarkDirectory=(Join-Path $PSScriptRoot '../../benchmark'),[ValidatePattern('^(SULK02|EVP[0-9]{2})$')][string]$RunLabel='SULK02')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($BenchmarkDirectory)
$exe=Join-Path $root 'bin/compute-benchmark-3090.exe'
if((Get-FileHash -LiteralPath $exe).Hash -ine '919ebddfc504e4aed119659bcb6786e5766cf48b2852969e440e11b6e55ef538'){throw 'Benchmark binary differs from reviewed baseline; do not rebuild. Send this error.'}
foreach($name in @('build-manifest.json','runtime-verification.json')){if(!(Test-Path -LiteralPath (Join-Path $root $name))){throw "Missing $name; preserve files and report"}}
$results=Join-Path $root 'results'
$before=@(if(Test-Path -LiteralPath $results){Get-ChildItem -LiteralPath $results -Directory | ForEach-Object {$_.FullName}})
& (Join-Path $root 'run-benchmark.ps1') -Card 3090
$new=@(Get-ChildItem -LiteralPath $results -Directory | Where-Object {$_.FullName -notin $before})
if($new.Count -ne 1 -or !(Test-Path -LiteralPath (Join-Path $new[0].FullName 'comparison-complete.json'))){throw 'No unique completed benchmark result. Preserve partial output and report; do not rerun automatically.'}
$run=$new[0].FullName
foreach($name in @('build-manifest.json','runtime-verification.json')){
 $source=Join-Path $root $name;$target=Join-Path $run $name
 if(Test-Path -LiteralPath $target){throw 'Provenance target already exists; preserve result'}
 $inputStream=$null;$outputStream=$null
 try {$inputStream=[IO.File]::OpenRead($source);$outputStream=[IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$inputStream.CopyTo($outputStream);$outputStream.Flush($true)}finally{if($outputStream){$outputStream.Dispose()};if($inputStream){$inputStream.Dispose()}}
 if((Get-FileHash -LiteralPath $source).Hash -ine (Get-FileHash -LiteralPath $target).Hash){throw 'Provenance copy differs; preserve output'}
}
$zip=$run+'-'+$RunLabel+'-benchmark.zip'
Compress-Archive -LiteralPath $run -DestinationPath $zip -ErrorAction Stop
[ordered]@{benchmark_zip=$zip;firmware_changed=$false;next=('Send this ZIP. Do not run Cancel, rerun EFI or reboot for this step. Report if Windows has rebooted since '+$RunLabel+'.')} | ConvertTo-Json
