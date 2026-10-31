# Read-only firmware check before/after an ordinary restart. Local baseline/receipt only.
param([string]$StateDirectory='E:\UNLOCK\EVGA-PROFILE')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-firmware-lib.ps1')
. (Join-Path $PSScriptRoot 'evga-package-lib.ps1')
$config=Get-Content -LiteralPath (Join-Path $StateDirectory 'installation.json') -Raw | ConvertFrom-Json
if($config.Schema -cne 'evga-attended-install-v1' -or $config.Layout -cne 'SharedEsp'){throw 'Expected shared-ESP installation'}
$next=Get-EvgaProfileNextSlot $StateDirectory $config
$slot=$next-1
$attempt=Get-Content -LiteralPath (Join-Path $StateDirectory ('attempt-{0:D2}.json' -f $slot)) -Raw | ConvertFrom-Json
$result=Get-Content -LiteralPath (Join-Path $StateDirectory ('result-{0:D2}.json' -f $slot)) -Raw | ConvertFrom-Json
$savedName=if($result.CleanupCompleted){'cleanup-{0:D2}.json'}else{'cleanup-{0:D2}-separate-before.json'}
$saved=Get-Content -LiteralPath (Join-Path $StateDirectory ($savedName -f $slot)) -Raw | ConvertFrom-Json
$bootTime=(Get-CimInstance Win32_OperatingSystem).LastBootUpTime.ToUniversalTime().ToString('o')
if(-not ('SharedEspFirmware.Native' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'shared-esp-firmware-native.cs')}
$scope=[SharedEspFirmware.Native]::EnablePrivilege()
try{
 $read={param($name)$v=[SharedEspFirmware.Native]::Read($name);if($v){ConvertTo-FirmwareValue $v.Data $v.Attributes}}
 if($null -ne (& $read 'BootNext') -or $null -ne (& $read $attempt.OptionName)){throw 'Temporary option/BootNext present; do not restart for this test'}
 $null=Get-EvgaPreservedCleanupSnapshot $saved $read
 foreach($name in @('Boot0000','Boot0008','Boot000C')){
  $pin=@($saved.BootEntries | Where-Object Name -CEQ $name)
  if($pin.Count -ne 1 -or !(Test-FirmwareValueEqual (& $read $name) $pin[0].Value)){throw 'Reviewed normal boot entry changed'}
 }
 $baseline=Join-Path $StateDirectory ('normal-boot-{0:D2}-before.json' -f $slot)
 if(!(Test-Path -LiteralPath $baseline)){
  Write-EvgaPackageJson $baseline ([ordered]@{Schema='evga-profile-normal-boot-v1';AttemptSlot=$slot;LogSha256=$result.LogSha256;LastBootUtc=$bootTime})
  [ordered]@{phase='ready_for_normal_restart';slot=$slot;temporary_option_absent=$true;boot_next_clear=$true;firmware_written=$false;gpu_accessed=$false;next='Save work, keep USB unplugged, restart normally once without selecting any EFI test. Run this same script after Windows returns.'} | ConvertTo-Json
  return
 }
 $before=Get-Content -LiteralPath $baseline -Raw | ConvertFrom-Json
 if($before.Schema -cne 'evga-profile-normal-boot-v1' -or $before.AttemptSlot -ne $slot -or $before.LogSha256 -cne $result.LogSha256){throw 'Normal boot baseline differs'}
 if([DateTimeOffset]$bootTime -le [DateTimeOffset]$before.LastBootUtc){throw 'No newer Windows boot; use the requested ordinary restart, not EFI'}
 $current=& $read 'BootCurrent'
 if(!$current){throw 'BootCurrent missing'}
 $bytes=[Convert]::FromBase64String($current.DataBase64)
 if($bytes.Length -ne 2 -or [BitConverter]::ToUInt16($bytes,0) -notin @(0,8,12)){throw 'BootCurrent is not a reviewed normal entry'}
 $done=[ordered]@{schema='evga-profile-normal-boot-result-v1';slot=$slot;normal_restart_confirmed=$true;boot_current=('Boot{0:X4}' -f [BitConverter]::ToUInt16($bytes,0));temporary_option_absent=$true;boot_next_clear=$true;primary_entries_unchanged=$true;firmware_written=$false;gpu_accessed=$false}
 $receipt=Join-Path $StateDirectory ('normal-boot-{0:D2}-result.json' -f $slot)
 if(!(Test-Path -LiteralPath $receipt)){Write-EvgaPackageJson $receipt $done}
 $done | ConvertTo-Json
}finally{$scope.Dispose()}
