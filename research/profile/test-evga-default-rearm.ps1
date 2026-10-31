param([Parameter(Mandatory)][string]$PackageRoot)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test-evga-profile-manager.ps1') -PackageRoot $PackageRoot
$fixture=Join-Path $fixture 'default-rearm'
$f=Setup-ManagerFixture 'SharedEsp'
$null=& $manager -StateDirectory $f.State -Assets $f.Assets -PackageDirectory (Join-Path $PackageRoot 'SharedEsp-10')
foreach($slot in @(10,11)){
 $a=Get-Content -LiteralPath (Join-Path $f.State ('attempt-{0:D2}.json' -f $slot)) -Raw | ConvertFrom-Json
 Simulate-PackageRun $a
 $done=& $manager -Action Collect -StateDirectory $f.State | ConvertFrom-Json
 if(!$done.Success -or !$done.CleanupCompleted){throw 'Fixture collection failed'}
 $out=& $manager -Action Rearm -StateDirectory $f.State -Assets $f.Assets -PackageDirectory (Join-Path $PackageRoot ('SharedEsp-'+($slot+1))) | ConvertFrom-Json
 if(!$out.prepared -or $out.slot -ne $slot+1 -or !$out.full_shutdown_required -or !$out.requires_fresh_shift_g -or $out.automatic_reboot){throw 'Default rearm output wrong'}
}
foreach($slot in @(10,11)){
 $r=Get-Content -LiteralPath (Join-Path $f.State ('result-{0:D2}.json' -f $slot)) -Raw | ConvertFrom-Json
 if(!$r.Success){throw 'Prior result changed'}
}
$blocked=$false;try{Get-EvgaProfileNextSlot $f.State ([pscustomobject]@{SeedSlot=9}) | Out-Null}catch{$blocked=$true}
if(!$blocked){throw 'Unrun slot12 rearmed'}
Write-Host 'PASS default rearm through slot12, fresh receipts, full-shutdown guidance, prior results preserved and pending-slot refusal. Firmware/storage mocked.'
