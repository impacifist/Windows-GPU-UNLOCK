# Fresh process only. Reuse the no-P/Invoke native fixture, then execute the real run script.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test-shared-esp-second-register.ps1')
$realScripts=$PSScriptRoot
$scripts=Join-Path $fixture 'run/research/profile'
$assetsDir=Join-Path $fixture 'run/internal-boot/unlock'
$global:runUsb=Join-Path $fixture 'usb'
[void][IO.Directory]::CreateDirectory($scripts)
[void][IO.Directory]::CreateDirectory($assetsDir)
[void][IO.Directory]::CreateDirectory((Join-Path $global:runUsb 'RTX3090-UNLOCK'))
foreach($name in @('run-shared-esp-second.ps1','register-shared-esp-second.ps1','shared-esp-firmware-lib.ps1','shared-esp-audit-stage-lib.ps1','shared-esp-second-arm-lib.ps1','read-shared-esp-unlock.ps1')){Copy-Item -LiteralPath (Join-Path $realScripts $name) -Destination (Join-Path $scripts $name)}
Copy-Item -LiteralPath (Join-Path $realScripts '../../internal-boot/unlock/assets.json') -Destination (Join-Path $assetsDir 'assets.json')
# Reconciliation was independently exercised on contributor data; this fixture isolates orchestration.
Set-Content -LiteralPath (Join-Path $scripts 'inspect-shared-unlock-reconciliation.ps1') -Value '[pscustomobject]@{reviewed_next_slot=9;checks=[pscustomobject]@{fixture_history=$true}} | ConvertTo-Json -Depth 3'
$global:runAssets=Get-Content (Join-Path $assetsDir 'assets.json') -Raw | ConvertFrom-Json
[void][IO.Directory]::CreateDirectory((Join-Path $global:entryEsp 'RTX3090-UNLOCK/ASSETS'))
foreach($spec in $global:runAssets){Set-Content -LiteralPath (Join-Path $global:entryEsp ('RTX3090-UNLOCK/ASSETS/'+$spec.Name)) -Value 'fixture'}
foreach($name in @('SULK01.TXT','SULK01.ARMED','EVGA-UNLOCK8.ARMED')){Set-Content -LiteralPath (Join-Path $global:entryEsp ('RTX3090-UNLOCK/'+$name)) -Value 'prior fixture'}
Set-Content -LiteralPath (Join-Path $global:runUsb 'RTX3090-UNLOCK/EVGA-UNLOCK8.ARMED') -Value 'prior reservation fixture'
function Get-Partition {param([string]$DriveLetter)
 if($DriveLetter){[pscustomobject]@{Guid='11111111-2222-3333-4444-555555555555';DiskNumber=13;IsSystem=$false;IsBoot=$false}}
 else{[pscustomobject]@{Guid='c36c3e86-02aa-4f76-af1b-96e327f9d1c1';IsSystem=$true;DiskNumber=12;PartitionNumber=1;Offset=[uint64]1048576;Size=[uint64]1073741824}}
}
function Get-Disk {param($Number) [pscustomobject]@{PartitionStyle='GPT';BusType=$(if($Number -eq 13){'USB'}else{'NVMe'});LogicalSectorSize=512}}
function Get-Volume {param([Parameter(ValueFromPipeline)]$InputObject,[string]$DriveLetter) process {[pscustomobject]@{FileSystem='FAT32';Path=$(if($DriveLetter){$global:runUsb}else{$global:entryEsp});SizeRemaining=1GB}}}
function Get-FileHash {param([string]$LiteralPath)
 switch([IO.Path]::GetFileName($LiteralPath)){
  'EVGA-SHARED-LAUNCH02.EFI' {return [pscustomobject]@{Hash='258a93fdf0eb526331f25b562c13a36176980ecc594825c8b4c48b0ec5604bec'}}
  'EVGA-SHARED-UNLOCK02.EFI' {return [pscustomobject]@{Hash='a15bade7806747f5cec76c411278d6d797d6f932f5d532eaf283add02d6409bf'}}
  'bootmgfw.efi' {return [pscustomobject]@{Hash='2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'}}
  'limine_x64.efi' {return [pscustomobject]@{Hash='a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462'}}
 }
 $spec=@($global:runAssets | Where-Object {$_.Name -ceq [IO.Path]::GetFileName($LiteralPath)})
 if($spec.Count){return [pscustomobject]@{Hash=$spec[0].Sha256}}
 Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath
}
function Get-Item {param([string]$LiteralPath)
 $spec=@($global:runAssets | Where-Object {$_.Name -ceq [IO.Path]::GetFileName($LiteralPath)})
 if($spec.Count){return [pscustomobject]@{Length=$spec[0].Bytes}}
 Microsoft.PowerShell.Management\Get-Item -LiteralPath $LiteralPath
}
$run=Join-Path $scripts 'run-shared-esp-second.ps1'
$runState=Join-Path $fixture 'run-state'
$registered=& (Join-Path $scripts 'register-shared-esp-second.ps1') -StateDirectory $runState | ConvertFrom-Json
if(!$registered.registered){throw 'Fixture registration failed'}
$before=[SharedEspFirmware.Native]::Writes.Count
$plan=& $run -StateDirectory $runState -Drive D -WhatIf | ConvertFrom-Json
if($plan.armed -or [SharedEspFirmware.Native]::Writes.Count -ne $before -or (Test-Path (Join-Path $runState 'sulk02-arm.json'))){throw 'WhatIf changed state'}
$result=& $run -StateDirectory $runState -Drive D | ConvertFrom-Json
if(!$result.armed -or !$result.boot_next_selected -or $result.gpu_executed -or $result.automatic_reboot){throw 'Arming result incorrect'}
if([SharedEspFirmware.Native]::Writes.Count -ne $before+1 -or [SharedEspFirmware.Native]::Writes[$before] -cne 'BootNext'){throw 'Run script wrote an unrelated firmware setting'}
$receipt=Join-Path $global:entryEsp 'RTX3090-UNLOCK/SULK02.ARMED'
$reserved=Join-Path $global:runUsb 'RTX3090-UNLOCK/EVGA-UNLOCK9.ARMED'
if(([IO.File]::ReadAllBytes($receipt)).Length -ne 512 -or !(Test-Path $reserved)){throw 'Receipt/reservation missing'}
$blocked=$false;try{& $run -StateDirectory $runState -Drive D}catch{$blocked=$true}
if(!$blocked -or [SharedEspFirmware.Native]::Writes.Count -ne $before+1){throw 'Arming replay accepted'}
# Simulate firmware consuming BootNext and the EFI producing evidence; no EFI executes.
[void][SharedEspFirmware.Native]::Values.Remove('BootNext')
Set-Content -LiteralPath (Join-Path $global:entryEsp 'RTX3090-UNLOCK/SULK02.TXT') -Value "evga-unlock-slot-09 status=0x0000000000000000`ncapture-complete=1"
Set-Content -LiteralPath (Join-Path $global:entryEsp 'RTX3090-UNLOCK/EVGA-UNLOCK9.ARMED') -Value 'synthetic runtime marker'
$collected=& $run -Action Collect -StateDirectory $runState | ConvertFrom-Json
if(!(Test-Path $collected.evidence_zip) -or $collected.cleanup.temporary_option_present -or [SharedEspFirmware.Native]::Values.ContainsKey('BootNext')){throw 'Collection/cleanup failed'}
$summary=Get-Content -LiteralPath (Join-Path ($collected.evidence_zip -replace '\.zip$','') 'summary.json') -Raw | ConvertFrom-Json
if($summary.schema -cne 'evga-shared-esp-sulk02' -or $summary.windows_handoff_proven -or !$summary.gpu_marker_present){throw 'Collected wrong run or inferred hardware proof'}
if(!(Test-Path $receipt) -or !(Test-Path $reserved) -or !(Test-Path (Join-Path $runState 'firmware-backup.json'))){throw 'Cleanup deleted persistent evidence'}
$saved=Get-Content -LiteralPath (Join-Path $runState 'firmware-backup.json') -Raw | ConvertFrom-Json
Set-FixtureValue 'Boot7E00' ([Convert]::FromBase64String($saved.Option.DataBase64))
Set-FixtureValue 'BootOrder' ([byte[]](0,0,1,0,2,0))
$refused=& $run -Action Collect -StateDirectory $runState | ConvertFrom-Json
if(!$refused.cleanup_error -or !(Test-Path $refused.evidence_zip) -or ![SharedEspFirmware.Native]::Values.ContainsKey('Boot7E00')){throw 'Cleanup refusal lost ZIP or changed firmware'}
Set-FixtureValue 'BootOrder' ([byte[]](0,0,1,0))
$canceled=& $run -Action Cancel -StateDirectory $runState | ConvertFrom-Json
if($canceled.temporary_option_present){throw 'Cancel failed'}
[IO.File]::WriteAllBytes((Join-Path $global:entryEsp 'RTX3090-UNLOCK/SULK01.ARMED'),(New-Object byte[] 512))
$old=& (Join-Path $scripts 'read-shared-esp-unlock.ps1') -WindowsEspGuid $guid -AttemptSlot 8 -OutputDirectory (Join-Path $fixture 'old-reader') | ConvertFrom-Json
if($old.schema -cne 'evga-shared-esp-sulk01' -or @($old.files | Where-Object {$_.Path -like '*SULK02*'}).Count){throw 'Default evidence reader no longer isolates SULK01'}
'PASS real SULK02 script: registration, WhatIf, reservation/receipt, BootNext-last selection, replay refusal, collection/ZIP and cleanup. Native/inventory/hashes/history are mocked; no hardware executed.'
