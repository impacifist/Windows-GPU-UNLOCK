$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-second-payload-lib.ps1')
$dir=Join-Path $env:TEMP ('sulk02-copy-'+[guid]::NewGuid().ToString('N'));[void][IO.Directory]::CreateDirectory($dir)
try {
 $src=Join-Path $dir 'source';$dst=Join-Path $dir 'target';[IO.File]::WriteAllText($src,'fixture payload');$hash=(Get-FileHash $src).Hash
 Copy-SharedSecondPayload $src $dst $hash
 if((Get-FileHash $dst).Hash -ne $hash){throw 'Readback failed'}
 $blocked=$false;try{Copy-SharedSecondPayload $src $dst $hash}catch{$blocked=$true};if(!$blocked){throw 'Overwrite accepted'}
 $bad=Join-Path $dir 'bad';$blocked=$false;try{Copy-SharedSecondPayload $src $bad ('0'*64)}catch{$blocked=$true};if(!$blocked -or (Test-Path $bad)){throw 'Bad hash wrote target'}
 if((Get-FileHash $dst).Hash -ne $hash){throw 'Existing target changed'}
 'PASS payload copy, overwrite refusal, bad-hash refusal and preservation; temporary fixtures only'
}finally{Remove-Item -LiteralPath $src -Force;Remove-Item -LiteralPath $dst -Force;[IO.Directory]::Delete($dir)}
