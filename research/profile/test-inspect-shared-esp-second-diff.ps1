# Real read-only diagnostic against the no-P/Invoke registration fixture.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test-shared-esp-second-register.ps1')
$stateDir=Join-Path $fixture 'diff-state'
$prepared=& (Join-Path $PSScriptRoot 'register-shared-esp-second.ps1') -StateDirectory $stateDir | ConvertFrom-Json
if(!$prepared.registered){throw 'Fixture registration failed'}
$before=[SharedEspFirmware.Native]::Writes.Count
Set-FixtureValue 'BootOrder' ([byte[]](0,0,1,0,2,0))
[SharedEspFirmware.Native]::Values['Boot0002']=New-FixtureOption '\EFI\Microsoft\Boot\bootmgfw.efi'
$entry=Join-Path $PSScriptRoot 'inspect-shared-esp-firmware-diff.ps1'
$diff=& $entry -StateDirectory $stateDir | ConvertFrom-Json
if(!$diff.read_only -or !$diff.temporary_option_matches_backup -or $diff.added_entries.Count -ne 1 -or $diff.added_entries[0].name -cne 'Boot0002' -or $null -ne $diff.boot_next){throw 'Added entry diagnostic incorrect'}
[void][SharedEspFirmware.Native]::Values.Remove('Boot0001')
$diff=& $entry -StateDirectory $stateDir | ConvertFrom-Json
$missing=@($diff.existing_entries | Where-Object name -CEQ 'Boot0001')
if($missing.Count -ne 1 -or $missing[0].current_present -or $missing[0].matches_backup){throw 'Missing entry not reported'}
if([SharedEspFirmware.Native]::Writes.Count -ne $before){throw 'Diagnostic wrote firmware'}
'PASS SULK02 firmware diff: added/missing entries and owned-option state; zero writes. RAM-native fixture only.'
