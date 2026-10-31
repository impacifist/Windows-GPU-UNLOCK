# Christian MSI3090 saved-capture test. No EFI execution or hardware access.
param([string]$Captures='G:\RTX3090-UNLOCK',[string]$Python='python')
$ErrorActionPreference='Stop'
$out=Join-Path $PSScriptRoot ('build/christian-offline-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($out)
foreach($test in @('inspect-evga3090-topology.py','inspect-evga3090-console-code.py')){
 $lines=& $Python -I (Join-Path $PSScriptRoot $test) $Captures
 if($LASTEXITCODE -ne 0){throw "Saved-capture check failed: $test. Send this error; do not boot an unlock EFI."}
 $json=$lines -join "`n";$parsed=$json | ConvertFrom-Json
 if($parsed.results.Count -ne 2 -or $parsed.error){throw 'Expected exactly two complete captures'}
 [IO.File]::WriteAllText((Join-Path $out ($test+'.json')),$json,[Text.UTF8Encoding]::new($false))
}
$zip=$out+'.zip';Compress-Archive -LiteralPath $out -DestinationPath $zip
[ordered]@{evidence_zip=$zip;hardware_accessed=$false;firmware_written=$false;unlock_proven=$false;next='Attach this ZIP. No reboot or EFI test. Report any hardware/BIOS changes since the captures.'} | ConvertTo-Json
