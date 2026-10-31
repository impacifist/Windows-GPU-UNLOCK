# Fresh process only. Real cleanup entry with the no-P/Invoke registration fixture.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test-shared-esp-second-register.ps1')
foreach($name in @('Boot0008','Boot000C','Boot000D','Boot000E','Boot000F','Boot0010')){[SharedEspFirmware.Native]::Values[$name]=[SharedEspFirmware.Native]::Values['Boot0001']}
Set-FixtureValue 'BootOrder' ([byte[]](0,0,8,0,12,0,13,0,14,0,15,0,16,0))
$cleanupState=Join-Path $fixture 'cleanup-current'
$prepared=& (Join-Path $PSScriptRoot 'register-shared-esp-second.ps1') -StateDirectory $cleanupState | ConvertFrom-Json
if(!$prepared.registered){throw 'Fixture registration failed'}
foreach($name in @('Boot000E','Boot000F','Boot0010')){[void][SharedEspFirmware.Native]::Values.Remove($name)}
Set-FixtureValue 'BootOrder' ([byte[]](0,0,8,0,12,0,13,0))
$before=[SharedEspFirmware.Native]::Writes.Count
$entry=Join-Path $PSScriptRoot 'cleanup-shared-esp-second.ps1'
& $entry -StateDirectory $cleanupState -WhatIf
if([SharedEspFirmware.Native]::Writes.Count -ne $before -or (Test-Path (Join-Path $cleanupState 'cleanup-sulk02.json'))){throw 'WhatIf mutated state'}
$result=& $entry -StateDirectory $cleanupState | ConvertFrom-Json
if(!$result.cleanup_completed -or $result.temporary_option_present -or $result.boot_order_written -or [SharedEspFirmware.Native]::Writes.Count -ne $before+1 -or [SharedEspFirmware.Native]::Writes[$before] -cne 'Boot7E00'){throw 'Incorrect real-entry cleanup'}
foreach($name in @('firmware-backup.json','cleanup-sulk02.json')){if(!(Test-Path (Join-Path $cleanupState $name))){throw 'Backup missing'}}
$blocked=$false;try{& $entry -StateDirectory $cleanupState}catch{$blocked=$true}
if(!$blocked -or [SharedEspFirmware.Native]::Writes.Count -ne $before+1){throw 'Cleanup replay accepted'}
'PASS real cleanup entry: WhatIf, durable snapshot, exact deletion with attributes 7, preserved backup and replay refusal. RAM-native only.'
