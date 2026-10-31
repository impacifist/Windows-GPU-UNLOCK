# Simulated Windows timestamps and RAM-native variables; never reboots or accesses firmware.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test-shared-esp-second-cleanup-entry.ps1')
$global:normalBootTime=[datetime]'2026-10-30T10:00:00Z'
function Get-CimInstance {param($ClassName) if($ClassName -cne 'Win32_OperatingSystem'){throw 'Unexpected query'};[pscustomobject]@{LastBootUpTime=$global:normalBootTime}}
$entry=Join-Path $PSScriptRoot 'confirm-shared-esp-normal-boot.ps1'
$writes=[SharedEspFirmware.Native]::Writes.Count
$first=& $entry -StateDirectory $cleanupState | ConvertFrom-Json
if($first.phase -cne 'ready_for_normal_restart'){throw 'Baseline not recorded'}
$blocked=$false;try{& $entry -StateDirectory $cleanupState}catch{$blocked=$true}
if(!$blocked){throw 'Same Windows boot accepted'}
$global:normalBootTime=[datetime]'2026-10-30T11:00:00Z'
Set-FixtureValue 'BootCurrent' ([byte[]](0,0)) 6
$result=& $entry -StateDirectory $cleanupState | ConvertFrom-Json
if(!$result.normal_restart_confirmed -or $result.firmware_written -or !$result.primary_entries_unchanged){throw 'Normal boot result incorrect'}
Set-FixtureValue 'BootCurrent' ([byte[]](0,126)) 6
$blocked=$false;try{& $entry -StateDirectory $cleanupState}catch{$blocked=$true}
if(!$blocked){throw 'Test boot accepted as normal boot'}
Set-FixtureValue 'BootCurrent' ([byte[]](0,0)) 6
Set-FixtureValue 'BootNext' ([byte[]](8,0))
$blocked=$false;try{& $entry -StateDirectory $cleanupState}catch{$blocked=$true}
if(!$blocked -or [SharedEspFirmware.Native]::Writes.Count -ne $writes){throw 'Pending request accepted or firmware written'}
'PASS ordinary restart confirmation: baseline, same-boot refusal, normal entry, test-entry refusal, pending-request refusal; zero firmware writes. RAM-native/CIM mocks only.'
