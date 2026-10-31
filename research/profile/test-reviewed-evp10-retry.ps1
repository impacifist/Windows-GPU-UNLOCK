param([Parameter(Mandatory)][string]$PackageRoot)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test-evga-profile-manager.ps1') -PackageRoot $PackageRoot
$fixture=Join-Path $fixture 'reviewed-retry'
$f=Setup-ManagerFixture 'SharedEsp'
$null=& $manager -StateDirectory $f.State -Assets $f.Assets -PackageDirectory (Join-Path $PackageRoot 'SharedEsp-10')
$attempt=Get-Content -LiteralPath (Join-Path $f.State 'attempt-10.json') -Raw | ConvertFrom-Json
Simulate-PackageRun $attempt
Set-Content -LiteralPath (Join-Path $attempt.TargetRoot 'RTX3090-UNLOCK/EVP10.TXT') -Value 'synthetic reviewed failed log'
$failed=& $manager -Action Collect -StateDirectory $f.State | ConvertFrom-Json
if($failed.Success){throw 'Fixture should fail'}
$blocked=$false;try{Get-EvgaProfileNextSlot $f.State ([pscustomobject]@{SeedSlot=9}) | Out-Null}catch{$blocked=$true}
if(!$blocked){throw 'Generic failure bypass'}
$receipt=@{schema='reviewed-evp10-cleanup-v1';cleanup_completed=$true;failed_attempt_preserved=$true;usb_entry_preserved=$true;boot_order_written=$false;gpu_accessed=$false;rearm_authorized=$false}
Write-EvgaPackageJson (Join-Path $f.State 'reviewed-evp10-cleanup-result.json') $receipt
$global:reviewedFixtureExpected=@{'EVP10.TXT'='bbd5c27a727a9835f7ff886551bebf298509759e211bc3dda66c81158b96eaa5';'EVP10.ARMED'='7414f7145d718a42262f68840503473f597771cda90082a09d853616daa53264';'EVGA-UNLOCK10.ARMED'='c81ed25e0ed465df4a3813ac40781f6d0fb46f06fe899d4ed4fb6b4666c70aca'}
$global:reviewedFixtureOriginal=@{};foreach($name in $global:reviewedFixtureExpected.Keys){$global:reviewedFixtureOriginal[$name]=(Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath (Join-Path $attempt.TargetRoot ('RTX3090-UNLOCK/'+$name))).Hash}
$global:reviewedFixtureBaseHash=${function:Get-FileHash}
function Get-FileHash([string]$LiteralPath,[string]$Algorithm){
 $name=[IO.Path]::GetFileName($LiteralPath)
 if($global:reviewedFixtureExpected.ContainsKey($name) -and (Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath).Hash -ceq $global:reviewedFixtureOriginal[$name]){return [pscustomobject]@{Hash=$global:reviewedFixtureExpected[$name]}}
 & $global:reviewedFixtureBaseHash -LiteralPath $LiteralPath -Algorithm $Algorithm
}
if((Get-EvgaProfileNextSlot $f.State ([pscustomobject]@{SeedSlot=9}) -ReviewedEvp10Retry) -ne 11){throw 'Reviewed slot not 11'}
$log=Join-Path $attempt.TargetRoot 'RTX3090-UNLOCK/EVP10.TXT';$saved=[IO.File]::ReadAllBytes($log)
Set-Content -LiteralPath $log -Value 'modified'
$blocked=$false;try{Get-EvgaProfileNextSlot $f.State ([pscustomobject]@{SeedSlot=9}) -ReviewedEvp10Retry | Out-Null}catch{$blocked=$true}
if(!$blocked){throw 'Changed failure proof accepted'}
[IO.File]::WriteAllBytes($log,$saved)
$out=& $manager -Action Rearm -ReviewedEvp10Retry -StateDirectory $f.State -Assets $f.Assets -PackageDirectory (Join-Path $PackageRoot 'SharedEsp-11') | ConvertFrom-Json
if(!$out.prepared -or $out.slot -ne 11 -or !$out.requires_fresh_shift_g){throw 'Fresh retry not staged'}
$preserved=Get-Content -LiteralPath (Join-Path $f.State 'result-10.json') -Raw | ConvertFrom-Json
if($preserved.Success){throw 'Failure rewritten'}
$blocked=$false;try{Get-EvgaProfileNextSlot $f.State ([pscustomobject]@{SeedSlot=9}) -ReviewedEvp10Retry | Out-Null}catch{$blocked=$true}
if(!$blocked){throw 'Reviewed exception repeated after slot11'}
Write-Host 'PASS explicit reviewed fresh-slot11 path, unchanged failure, generic/modified/repeated refusal. Firmware/storage/hashes mocked; no hardware.'
