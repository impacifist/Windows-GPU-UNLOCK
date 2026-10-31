param([Parameter(Mandatory)][string]$PackageRoot)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test-evga-default-rearm.ps1') -PackageRoot $PackageRoot
$a=Get-Content -LiteralPath (Join-Path $f.State 'attempt-12.json') -Raw | ConvertFrom-Json;Simulate-PackageRun $a
$null=& $manager -Action Collect -StateDirectory $f.State
$rpath=Join-Path $f.State 'result-12.json';$r=Get-Content -LiteralPath $rpath -Raw | ConvertFrom-Json
$r.LogSha256='ba15d7ef5ffd98ff195bab474f00d541d3b4f492d21831c4ee7e242c078f9db1';$r.GateSha256='3e7d5d8f12e408f37c557abecba929dec9b8680c7c1eda4c6058a7a6891e0e40'
$r | ConvertTo-Json | Set-Content -LiteralPath $rpath
@{schema='evga-profile-normal-boot-result-v1';slot=11;normal_restart_confirmed=$true;temporary_option_absent=$true;boot_next_clear=$true;primary_entries_unchanged=$true} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $f.State 'normal-boot-11-result.json')
$global:handoffBaseHash=${function:Get-FileHash}
function Get-FileHash([string]$LiteralPath,[string]$Algorithm){
 switch -CaseSensitive ([IO.Path]::GetFileName($LiteralPath)){
  'EVP12.TXT' {return [pscustomobject]@{Hash='ba15d7ef5ffd98ff195bab474f00d541d3b4f492d21831c4ee7e242c078f9db1'}}
  'EVP12.ARMED' {return [pscustomobject]@{Hash='3e7d5d8f12e408f37c557abecba929dec9b8680c7c1eda4c6058a7a6891e0e40'}}
 }
 & $global:handoffBaseHash -LiteralPath $LiteralPath -Algorithm $Algorithm
}
$entry=Join-Path $PSScriptRoot 'export-evga-profile-handoff.ps1'
Set-FixtureValue 'BootNext' ([byte[]](8,0))
$blocked=$false;try{& $entry -StateDirectory $f.State | Out-Null}catch{$blocked=$true}
if(!$blocked){throw 'Pending boot accepted for final handoff'}
[void][SharedEspFirmware.Native]::Values.Remove('BootNext')
$writes=[SharedEspFirmware.Native]::Writes.Count
$out=& $entry -StateDirectory $f.State | ConvertFrom-Json
if(!(Test-Path -LiteralPath $out.acceptance_zip) -or $out.firmware_written -or $out.gpu_accessed -or [SharedEspFirmware.Native]::Writes.Count -ne $writes){throw 'Handoff failed/wrote firmware'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::OpenRead($out.acceptance_zip)
try{
 if(@($zip.Entries | Where-Object {$_.Name -match '^(Boot|attempt-|UNLOCK|.*\.BIN)'}).Count){throw 'Private firmware/payload included'}
 if(@($zip.Entries | Where-Object Name -eq 'acceptance.json').Count -ne 1){throw 'Missing final receipt'}
}finally{$zip.Dispose()}
Write-Host 'PASS final accepted-state export, pending-boot refusal, sanitized inventory and no firmware writes. Fake firmware/storage/hash fixture.'
