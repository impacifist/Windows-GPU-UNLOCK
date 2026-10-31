# One ordinary-restart confirmation. No firmware writes, EFI execution or automatic reboot.
param([string]$StateDirectory='E:\UNLOCK\ESP-SECOND02')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-firmware-lib.ps1')
. (Join-Path $PSScriptRoot 'shared-esp-second-arm-lib.ps1')
$dir=[IO.Path]::GetFullPath($StateDirectory)
if($dir -notmatch '^[A-Za-z]:\\'){throw 'Expected local recovery directory'}
for($p=$dir;$p;){if(([IO.File]::GetAttributes($p) -band [IO.FileAttributes]::ReparsePoint)){throw 'Recovery path contains a reparse point'};$parent=[IO.Path]::GetDirectoryName($p.TrimEnd([char[]]'\/'));if($parent -eq $p){break};$p=$parent}
$saved=Get-Content -LiteralPath (Join-Path $dir 'cleanup-sulk02.json') -Raw | ConvertFrom-Json
if($saved.Schema -cne 'shared-esp-second-cleanup-v1' -or $saved.OptionName -cne 'Boot7E00'){throw 'Expected successful SULK02 cleanup snapshot'}
$bootTime=(Get-CimInstance Win32_OperatingSystem).LastBootUpTime.ToUniversalTime().ToString('o')
if(-not ('SharedEspFirmware.Native' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'shared-esp-firmware-native.cs')}
$scope=[SharedEspFirmware.Native]::EnablePrivilege()
try {
 $read={param($name) $v=[SharedEspFirmware.Native]::Read($name);if($v){ConvertTo-FirmwareValue $v.Data $v.Attributes}}
 if($null -ne (& $read 'BootNext') -or $null -ne (& $read 'Boot7E00')){throw 'Temporary option or BootNext is present; preserve state and report'}
 $order=& $read 'BootOrder'
 if(!$order -or $order.Attributes -ne 7){throw 'Unexpected BootOrder'}
 [byte[]]$bytes=[Convert]::FromBase64String($order.DataBase64)
 if($bytes.Length -lt 6 -or $bytes.Length%2){throw 'Unexpected boot-order length'}
 $names=@('Boot0000','Boot0008','Boot000C')
 for($i=0;$i -lt 3;$i++){
  if(('Boot{0:X4}' -f [BitConverter]::ToUInt16($bytes,2*$i)) -cne $names[$i]){throw 'Primary Limine/Windows order changed'}
  $pin=@($saved.BootEntries | Where-Object Name -CEQ $names[$i])
  if($pin.Count -ne 1 -or !(Test-FirmwareValueEqual (& $read $names[$i]) $pin[0].Value)){throw 'Primary boot entry changed'}
 }
 $baseline=Join-Path $dir 'normal-boot-baseline.json'
 if(!(Test-Path -LiteralPath $baseline)){
  Write-SharedSecondNewFile $baseline ([Text.Encoding]::UTF8.GetBytes(([ordered]@{Schema='sulk02-normal-boot-v1';LastBootUtc=$bootTime} | ConvertTo-Json)))
  [ordered]@{phase='ready_for_normal_restart';temporary_option_absent=$true;boot_next_clear=$true;firmware_written=$false;next='Save work, keep USB unplugged and restart normally once. Do not select an EFI test. After Windows returns, run this same command again.'} | ConvertTo-Json
  return
 }
 $before=Get-Content -LiteralPath $baseline -Raw | ConvertFrom-Json
 if($before.Schema -cne 'sulk02-normal-boot-v1'){throw 'Unexpected normal-boot baseline'}
 if([DateTimeOffset]$bootTime -le [DateTimeOffset]$before.LastBootUtc){throw 'No newer Windows boot recorded yet. Do not repeat EFI; use one ordinary restart, then rerun this command.'}
 $current=& $read 'BootCurrent'
 if(!$current){throw 'BootCurrent unavailable; report without retrying'}
 [byte[]]$currentBytes=[Convert]::FromBase64String($current.DataBase64)
 if($currentBytes.Length -ne 2 -or [BitConverter]::ToUInt16($currentBytes,0) -notin @(0,8,12)){throw 'BootCurrent is not a normal reviewed Limine/Windows entry'}
 [ordered]@{normal_restart_confirmed=$true;boot_current=('Boot{0:X4}' -f [BitConverter]::ToUInt16($currentBytes,0));temporary_option_absent=$true;boot_next_clear=$true;primary_entries_unchanged=$true;firmware_written=$false;gpu_accessed=$false;next='Send this JSON. No further reboot or unlock test requested. Preserve backups and evidence.'} | ConvertTo-Json
}finally{$scope.Dispose()}
