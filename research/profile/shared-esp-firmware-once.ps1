# One-time no-GPU audit via actual UEFI BootNext. Never writes BootOrder, BCD or ESP.
[CmdletBinding(SupportsShouldProcess)]
param([ValidateSet('Prepare','Status','Cleanup')][string]$Action='Prepare',
 [string]$StateDirectory=(Join-Path $env:USERPROFILE 'Documents/EVGA-ESP-ONCE01'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-firmware-lib.ps1')
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
$expectedGuid=[guid]'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'
$auditHash='f4cf33771fcc1e9c463a94561898457d0ce6ee7e4e17da5546c8a9752d373c92'
$statePath=[IO.Path]::GetFullPath($StateDirectory)
if($statePath -notmatch '^[A-Za-z]:\\'){throw 'Use a local drive directory outside the ESP for the recovery backup'}
for($ancestor=$statePath;$ancestor;){
 if(Test-Path -LiteralPath $ancestor){if(([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint)){throw 'Recovery backup path contains a reparse point'}}
 $nextAncestor=[IO.Path]::GetDirectoryName($ancestor.TrimEnd([char[]]'\/'));if($nextAncestor -eq $ancestor){break};$ancestor=$nextAncestor
}
$stateParts=@(Get-Partition -DriveLetter $statePath.Substring(0,1))
if($stateParts.Count -ne 1 -or ($stateParts[0].Guid -and [guid]$stateParts[0].Guid -eq $expectedGuid)){throw 'Recovery backup must be on an accessible local partition outside the ESP'}
if(-not ('SharedEspFirmware.Native' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'shared-esp-firmware-native.cs')}
$privilege=[SharedEspFirmware.Native]::EnablePrivilege()
try {
 $read={param($name) $v=[SharedEspFirmware.Native]::Read($name);if($null -ne $v){ConvertTo-FirmwareValue $v.Data $v.Attributes}}
 $write={param($name,$value) if($null -eq $value){[SharedEspFirmware.Native]::Write($name,$null,0)}else{[SharedEspFirmware.Native]::Write($name,[Convert]::FromBase64String($value.DataBase64),[uint32]$value.Attributes)}}
 $backup=Join-Path $statePath 'firmware-backup.json'
 if($Action -ne 'Prepare'){
  $state=Get-Content -LiteralPath $backup -Raw | ConvertFrom-Json
  if($state.Schema -ne 'shared-esp-audit-once-v1' -or [guid]$state.PartitionGuid -ne $expectedGuid -or $state.OptionNumber -lt 32256 -or $state.OptionNumber -gt 32511 -or $state.OptionName -cne ('Boot{0:X4}' -f [int]$state.OptionNumber) -or $null -ne $state.BootNext){throw 'Unsupported recovery backup; no firmware writes attempted'}
  $decoded=Read-SharedEspLoadOption $state.Option
  if($decoded.Path -cne '\EFI\EVGA-SHARED-AUDIT-VISIBLE.EFI' -or $decoded.PartitionGuid -ne $expectedGuid -or $state.Option.Attributes -ne 7){throw 'Recovery option is not the pinned no-GPU audit'}
  [byte[]]$oldOrder=[Convert]::FromBase64String($state.BootOrder.DataBase64)
  for($p=0;$p -lt $oldOrder.Length;$p+=2){if([BitConverter]::ToUInt16($oldOrder,$p) -eq $state.OptionNumber){throw 'Recovery option appears in original BootOrder; refuse deletion'}}
  if($Action -eq 'Cleanup' -and $PSCmdlet.ShouldProcess($state.OptionName,'Remove only the exact temporary audit option and its pending BootNext; retain backup')){Remove-OwnedSharedEspFirmware $state $read $write}
  Assert-SharedEspFirmwareSnapshot $state $read
  [ordered]@{action=$Action;boot_order_unchanged=$true;temporary_option_present=($null -ne (& $read $state.OptionName));temporary_option_matches=(Test-FirmwareValueEqual (& $read $state.OptionName) $state.Option);boot_next=(& $read 'BootNext');backup=$backup;gpu_transaction=$false} | ConvertTo-Json -Depth 4
  return
 }
 if(Test-Path -LiteralPath $statePath){throw 'State directory already exists. Use Status/Cleanup; do not overwrite the recovery backup'}
 $parts=@(Get-Partition | Where-Object {$_.Guid -and [guid]$_.Guid -eq $expectedGuid})
 if($parts.Count -ne 1 -or !$parts[0].IsSystem){throw 'Expected unique pinned Windows system ESP'}
 $part=$parts[0];$disk=Get-Disk -Number $part.DiskNumber;$volumes=@($part | Get-Volume)
 if($disk.PartitionStyle -ne 'GPT' -or $disk.BusType -eq 'USB' -or $volumes.Count -ne 1 -or $volumes[0].FileSystem -ne 'FAT32' -or !$volumes[0].Path){throw 'Expected internal GPT FAT32 ESP'}
 [uint64]$sector=$disk.LogicalSectorSize
 if($sector -notin @(512,4096) -or $part.Offset%$sector -or $part.Size%$sector){throw 'Unsupported ESP sector geometry'}
 foreach($pin in @(@(@('EFI','EVGA-SHARED-AUDIT-VISIBLE.EFI'),$auditHash),@(@('EFI','Microsoft','Boot','bootmgfw.efi'),'2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'),@(@('EFI','limine','limine_x64.efi'),'a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462'))){
  $file=Get-AuditCheckedPath $volumes[0].Path $pin[0] $false
  if((Get-FileHash -LiteralPath $file).Hash -ine $pin[1]){throw 'Existing audit/Windows/Limine file hash changed; nothing registered'}
 }
 $secureBoot=& $read 'SecureBoot'
 if($null -eq $secureBoot){throw 'Cannot establish Secure Boot state; no firmware writes attempted'}
 [byte[]]$secureBytes=[Convert]::FromBase64String($secureBoot.DataBase64)
 if($secureBytes.Length -ne 1 -or $secureBytes[0] -ne 0){throw 'Unsigned audit requires already-disabled Secure Boot; no setting was changed'}
 $order=& $read 'BootOrder'
 if($null -eq $order -or $order.Attributes -ne 7){throw 'Expected standard NV|BS|RT BootOrder attributes 7'}
 [byte[]]$orderBytes=[Convert]::FromBase64String($order.DataBase64)
 if($orderBytes.Length -lt 4 -or $orderBytes.Length%2){throw 'Expected BootOrder with Limine and Windows entries'}
 $entries=@();$numbers=@();$windows=$false
 for($p=0;$p -lt $orderBytes.Length;$p+=2){
  $number=[BitConverter]::ToUInt16($orderBytes,$p);if($number -in $numbers){throw 'Duplicate firmware BootOrder entry'};$numbers+=,$number
  $name='Boot{0:X4}' -f $number;$value=& $read $name
  if($null -eq $value){throw "BootOrder references missing $name"}
  $entries+=,[pscustomobject]@{Name=$name;Value=$value}
  try {$decoded=Read-SharedEspLoadOption $value}catch{if($p -eq 0){throw};continue}
  if($p -eq 0 -and (!$decoded.Active -or $decoded.Path -ine '\EFI\limine\limine_x64.efi' -or $decoded.PartitionGuid -ne $expectedGuid -or $decoded.PartitionNumber -ne $part.PartitionNumber)){throw 'First firmware BootOrder option is not the pinned active Limine entry; no writes attempted'}
  if($decoded.Active -and $decoded.Path -ieq '\EFI\Microsoft\Boot\bootmgfw.efi' -and $decoded.PartitionGuid -eq $expectedGuid -and $decoded.PartitionNumber -eq $part.PartitionNumber){$windows=$true}
 }
 if(!$windows){throw 'No active pinned Windows Boot Manager in BootOrder; no writes attempted'}
 if($null -ne (& $read 'BootNext')){throw 'BootNext already exists; do not replace another pending selection'}
 $chosen=$null
 for($n=0x7e00;$n -le 0x7eff;$n++){if($n -notin $numbers -and $null -eq (& $read ('Boot{0:X4}' -f $n))){$chosen=$n;break}}
 if($null -eq $chosen){throw 'No unused temporary firmware option slot in test range'}
 $operation=[guid]::NewGuid().ToString('N')
 $bytes=New-SharedEspLoadOption $expectedGuid ([uint32]$part.PartitionNumber) ([uint64]([decimal]$part.Offset/[decimal]$sector)) ([uint64]([decimal]$part.Size/[decimal]$sector)) ('EVGA shared ESP AUDIT ONCE '+$operation)
 $state=[ordered]@{Schema='shared-esp-audit-once-v1';Operation=$operation;PartitionGuid=$expectedGuid.ToString();AuditSha256=$auditHash;SecureBoot=$secureBoot;BootOrder=$order;BootEntries=$entries;BootNext=$null;BootCurrent=(& $read 'BootCurrent');OptionNumber=$chosen;OptionName=('Boot{0:X4}' -f $chosen);Option=(ConvertTo-FirmwareValue $bytes)}
 $save={param($s)
  [void][IO.Directory]::CreateDirectory($statePath)
  $data=[Text.Encoding]::UTF8.GetBytes(($s | ConvertTo-Json -Depth 8));$stream=$null
  try {$stream=[IO.File]::Open($backup,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$stream.Write($data,0,$data.Length);$stream.Flush($true)}finally{if($stream){$stream.Dispose()}}
  if([Convert]::ToBase64String([IO.File]::ReadAllBytes($backup)) -cne [Convert]::ToBase64String($data)){throw 'Recovery backup readback failed before firmware writes'}
 }
 $registered=$false
 if($PSCmdlet.ShouldProcess('UEFI firmware variables','Back up state, create temporary no-GPU audit Boot option and set one-time BootNext; never modify BootOrder')){Invoke-SharedEspFirmwareOnce $state $read $write $save;$registered=$true}
 [ordered]@{action='Prepare';registered=$registered;option=$state.OptionName;boot_next_one_time=$registered;boot_order_written=$false;limine_first_verified=$true;windows_entry_verified=$true;backup=$backup;automatic_reboot=$false;next=$(if($registered){'Remove storage USB and restart normally once. Photograph AUDIT ENTERED ON PINNED ESP and report Windows boot. Then run -Action Cleanup.'}else{'No firmware selection written. This is not a prepared test.'})} | ConvertTo-Json -Depth 4
}finally{$privilege.Dispose()}
