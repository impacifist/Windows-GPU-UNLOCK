param([Parameter(Mandatory)][string]$PackageRoot)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test-evga-profile-manager.ps1') -PackageRoot $PackageRoot
$fixture=Join-Path $fixture 'failure-inspection'
$f=Setup-ManagerFixture 'SharedEsp'
$null=& $manager -Action Install -StateDirectory $f.State -Assets $f.Assets -PackageDirectory (Join-Path $PackageRoot 'SharedEsp-10')
$attempt=Get-Content -LiteralPath (Join-Path $f.State 'attempt-10.json') -Raw | ConvertFrom-Json
Simulate-PackageRun $attempt
Set-Content -LiteralPath (Join-Path $attempt.TargetRoot 'RTX3090-UNLOCK/EVP10.TXT') -Value 'capture-complete=1'
[SharedEspFirmware.Native]::Values['Boot0009']=New-FixtureOption '\EFI\unreviewed.efi'
Set-FixtureValue 'BootOrder' ([byte[]](0,0,8,0,9,0))
$failed=& $manager -Action Collect -StateDirectory $f.State | ConvertFrom-Json
if($failed.Success -or $failed.CleanupCompleted){throw 'Failure fixture unexpectedly succeeded'}
$writes=[SharedEspFirmware.Native]::Writes.Count
$inspected=& (Join-Path $PSScriptRoot 'inspect-evga-package-failure.ps1') -StateDirectory $f.State | ConvertFrom-Json
if([SharedEspFirmware.Native]::Writes.Count -ne $writes -or $inspected.firmware_written -or $inspected.gpu_accessed){throw 'Inspection mutated firmware'}
$unpacked=$inspected.zip+'.contents';Expand-Archive -LiteralPath $inspected.zip -DestinationPath $unpacked
$report=Get-ChildItem -LiteralPath $unpacked -Recurse -Filter inspection.json | Get-Content -Raw | ConvertFrom-Json
$extra=@($report.current_entries | Where-Object name -eq 'Boot0009')
if($extra.Count -ne 1 -or $extra[0].previously_in_order -or !(Test-Path -LiteralPath $inspected.zip)){throw 'Missing unexpected-entry evidence'}
Write-Host 'PASS failed transaction/cleanup inspection produces evidence and identifies extra entry with no firmware writes; fake Native only.'
