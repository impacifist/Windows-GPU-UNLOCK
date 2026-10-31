param([Parameter(Mandatory)][string]$PackageRoot)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test-evga-profile-manager.ps1') -PackageRoot $PackageRoot
$digest='ac1eb792d44382686770c715aacae3681d572ab8f61cc7e6dc9d71fa0fab94ee'
if(!(Test-EvgaReviewedUsbOrderRemoval 'Boot000D' 7 $digest $false $false) -or !(Test-EvgaReviewedUsbOrderRemoval 'Boot000D' 7 $digest $true $true)){throw 'Reviewed removal not accepted'}
if((Test-EvgaReviewedUsbOrderRemoval 'Boot000D' 7 $digest $true $false) -or (Test-EvgaReviewedUsbOrderRemoval 'Boot000E' 7 $digest $false $false) -or (Test-EvgaReviewedUsbOrderRemoval 'Boot000D' 6 $digest $false $false) -or (Test-EvgaReviewedUsbOrderRemoval 'Boot000D' 7 ('0'*64) $false $false)){throw 'Unreviewed removal accepted'}
$fixture=Join-Path $fixture 'separate-cleanup'
$f=Setup-ManagerFixture 'SharedEsp'
$null=& $manager -StateDirectory $f.State -Assets $f.Assets -PackageDirectory (Join-Path $PackageRoot 'SharedEsp-10')
$a=Get-Content -LiteralPath (Join-Path $f.State 'attempt-10.json') -Raw | ConvertFrom-Json;Simulate-PackageRun $a
[SharedEspFirmware.Native]::Values['Boot0009']=New-FixtureOption '\EFI\unreviewed.efi'
Set-FixtureValue 'BootOrder' ([byte[]](0,0,8,0,9,0))
$r=& $manager -Action Collect -StateDirectory $f.State | ConvertFrom-Json
if(!$r.Success -or $r.CleanupCompleted){throw 'Expected successful transaction and refused cleanup'}
$original=(Get-FileHash -LiteralPath (Join-Path $f.State 'result-10.json')).Hash
$blocked=$false;try{Get-EvgaProfileNextSlot $f.State ([pscustomobject]@{SeedSlot=9}) | Out-Null}catch{$blocked=$true}
if(!$blocked){throw 'Uncleaned run rearmed'}
# Restore the synthetic fixture, not real firmware, before the separate action.
[void][SharedEspFirmware.Native]::Values.Remove('Boot0009');Set-FixtureValue 'BootOrder' ([byte[]](0,0,8,0))
$writes=[SharedEspFirmware.Native]::Writes.Count
$what=& $manager -Action Cleanup -StateDirectory $f.State -WhatIf
if([SharedEspFirmware.Native]::Writes.Count -ne $writes){throw 'WhatIf altered firmware'}
$done=& $manager -Action Cleanup -StateDirectory $f.State | ConvertFrom-Json
if(!$done.CleanupCompleted -or !$done.OriginalResultPreserved -or $done.BootOrderWritten -or $done.GpuAccessed){throw 'Separate cleanup failed'}
if((Get-FileHash -LiteralPath (Join-Path $f.State 'result-10.json')).Hash -cne $original){throw 'Original collection result changed'}
if((Get-EvgaProfileNextSlot $f.State ([pscustomobject]@{SeedSlot=9})) -ne 11){throw 'Verified cleanup not recognized'}
$blocked=$false;try{& $manager -Action Cleanup -StateDirectory $f.State | Out-Null}catch{$blocked=$true}
if(!$blocked){throw 'Duplicate cleanup accepted'}
Write-Host 'PASS reviewed removable-order policy, separate cleanup/WhatIf, immutable result, cleanup receipt rearm and repeat refusal. Fake Native only.'
