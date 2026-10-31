# Read-only cross-volume attempt reconciliation; does not grant staging eligibility.
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[ValidatePattern('^[A-Za-z]$')][string]$Drive='D')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
$parts=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($parts.Count -ne 1){throw 'Expected one pinned system ESP'}
$vol=@($parts[0] | Get-Volume);if($vol.Count -ne 1 -or $vol[0].FileSystem -ne 'FAT32' -or !$vol[0].Path){throw 'Expected accessible FAT32 ESP'}
$root=$vol[0].Path
$log=Get-AuditCheckedPath $root @('RTX3090-UNLOCK','SULK01.TXT') $false
if((Get-Item -LiteralPath $log).Length -gt 9MB){throw 'Unexpected log size'}
$hash=(Get-FileHash -LiteralPath $log).Hash
$text=[IO.File]::ReadAllText($log)
$checks=[ordered]@{}
foreach($event in @('evga-unlock-slot-08','evga-unlock-transaction-return','evga-unlock-assets-released','evga-text-stop-ready-windows')){$checks[$event]=$text.Contains($event+' status=0x0000000000000000')}
$checks['two_complete_captures']=([regex]::Matches($text,'capture-complete=1').Count -ge 2)
$checks['reviewed_log_hash']=($hash -ieq 'c0cb1b121fbe24610e84dfdaee52da512856af39a65768a8441012e4236e5f23')
$usb=Get-AuditCheckedPath ($Drive.ToUpperInvariant()+':\') @('RTX3090-UNLOCK') $true
$res=Get-AuditCheckedPath $usb @('EVGA-UNLOCK8.ARMED') $false
if((Get-Item -LiteralPath $res).Length -gt 4096){throw 'Unexpected reservation size'}
$expected="RESERVED FOR SHARED ESP SULK01 SLOT 8. NOT EXECUTION PROOF. DO NOT DELETE OR RETRY.`r`n"
$checks['usb_reservation_matches']=([IO.File]::ReadAllText($res) -ceq $expected)
. (Join-Path $PSScriptRoot 'shared-unlock-progression.ps1')
$usbMarkers=@(Get-ChildItem -LiteralPath $usb -File -Filter 'EVGA-UNLOCK*.ARMED' | ForEach-Object {$_.Name})
$espCapture=Get-AuditCheckedPath $root @('RTX3090-UNLOCK') $true
$espMarkers=@(Get-ChildItem -LiteralPath $espCapture -File -Filter 'EVGA-UNLOCK*.ARMED' | ForEach-Object {$_.Name})
$qualifiedNext=Get-ReviewedSharedUnlockNextSlot -UsbMarkers $usbMarkers -EspMarkers $espMarkers -LogSha256 $hash -ReservationSha256 (Get-FileHash -LiteralPath $res).Hash
[ordered]@{reviewed_next_slot=$qualifiedNext;read_only=$true;slot=8;checks=$checks;esp_log_sha256=$hash;usb_reservation_sha256=(Get-FileHash -LiteralPath $res).Hash;staging_eligible=$false;next_attempt_authorized=$false} | ConvertTo-Json -Depth 5
