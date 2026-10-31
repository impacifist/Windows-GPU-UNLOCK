# RAM-only independent firmware-variable mocks. Never enables privilege or calls native firmware APIs.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-firmware-lib.ps1')
Add-Type -Path (Join-Path $PSScriptRoot 'shared-esp-firmware-native.cs')
$global:fwChecks=0
function Check($condition,$message){$global:fwChecks++;if(!$condition){throw $message}}
$guid=[guid]'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'
$option=ConvertTo-FirmwareValue (New-SharedEspLoadOption $guid 1 2048 2097152 'EVGA audit test vector')
$decoded=Read-SharedEspLoadOption $option
Check ($decoded.Path -ceq '\EFI\EVGA-SHARED-AUDIT-VISIBLE.EFI' -and $decoded.Active -and $decoded.PartitionGuid -eq $guid -and $decoded.PartitionNumber -eq 1) 'EFI load option decode failed'
$state=[pscustomobject]@{BootOrder=(ConvertTo-FirmwareValue ([byte[]](0,0,1,0)));BootEntries=@([pscustomobject]@{Name='Boot0000';Value=(ConvertTo-FirmwareValue ([byte[]](11,12)))},[pscustomobject]@{Name='Boot0001';Value=(ConvertTo-FirmwareValue ([byte[]](21,22)))});OptionNumber=32256;OptionName='Boot7E00';Option=$option}
$global:fwMap=@{};$global:fwWrites=@();$global:fwSaved=0;$global:fwMode='';$global:fwReads=@{}
$foreign=ConvertTo-FirmwareValue ([byte[]](3,0))
function Reset-Fake {
 $global:fwMap=@{BootOrder=$state.BootOrder;Boot0000=$state.BootEntries[0].Value;Boot0001=$state.BootEntries[1].Value}
 $global:fwWrites=@();$global:fwSaved=0;$global:fwMode='';$global:fwReads=@{}
}
$read={param($name)
 if(!$global:fwReads.ContainsKey($name)){$global:fwReads[$name]=0};$global:fwReads[$name]++
 if($global:fwMode -eq 'next-delete-race' -and $name -eq 'BootNext' -and $global:fwReads[$name] -eq 2){$global:fwMap.BootNext=$foreign}
 if($global:fwMode -eq 'option-delete-race' -and $name -eq 'Boot7E00' -and $global:fwReads[$name] -eq 3){$global:fwMap.Boot7E00=ConvertTo-FirmwareValue ([byte[]](99,98))}
 $global:fwMap[$name]
}
$write={param($name,$value)
 if($name -eq 'BootOrder' -or $name -in @('Boot0000','Boot0001')){throw 'Attempted to write an existing boot setting'}
 $global:fwWrites+=,[pscustomobject]@{Name=$name;Delete=($null -eq $value)}
 if($name -eq 'BootNext' -and $null -ne $value -and $global:fwMode -in @('next-fail','option-delete-race')){throw 'Injected BootNext write failure'}
 if($null -eq $value){$global:fwMap.Remove($name)}else{$global:fwMap[$name]=$value}
 if($name -eq 'Boot7E00' -and $null -ne $value){
  if($global:fwMode -eq 'foreign-next'){$global:fwMap.BootNext=$foreign}
  if($global:fwMode -eq 'order-change'){$global:fwMap.BootOrder=ConvertTo-FirmwareValue ([byte[]](1,0,0,0))}
 }
}
$save={param($value) $global:fwSaved++;if($global:fwMode -eq 'backup-fail'){throw 'Injected backup failure'}}
Reset-Fake
Invoke-SharedEspFirmwareOnce $state $read $write $save
Check ($global:fwSaved -eq 1 -and (Test-FirmwareValueEqual $global:fwMap.BootOrder $state.BootOrder) -and $global:fwMap.ContainsKey('Boot7E00')) 'Successful registration changed existing state'
Check (Test-FirmwareValueEqual $global:fwMap.BootNext (ConvertTo-FirmwareValue ([byte[]](0,126)))) 'BootNext was not the expected little-endian UINT16 with attributes 7'
$global:fwMap.Remove('BootNext') # Simulated firmware consumes BootNext before entering the audit.
Remove-OwnedSharedEspFirmware $state $read $write
Check (!$global:fwMap.ContainsKey('Boot7E00') -and $global:fwMap.ContainsKey('Boot0000')) 'Post-boot cleanup changed existing entries'
Reset-Fake;$global:fwMap.BootNext=$foreign
$blocked=$false;try{Invoke-SharedEspFirmwareOnce $state $read $write $save}catch{$blocked=$true}
Check ($blocked -and !$global:fwWrites.Count -and !$global:fwSaved) 'Existing BootNext was not refused before backup/write'
foreach($mode in @('backup-fail','next-fail','foreign-next','order-change','option-delete-race')){
 Reset-Fake;$global:fwMode=$mode;$blocked=$false
 try{Invoke-SharedEspFirmwareOnce $state $read $write $save}catch{$blocked=$true}
 Check $blocked ('Failure was not reported: '+$mode)
 if($mode -eq 'backup-fail'){Check (!$global:fwWrites.Count) 'Backup failure allowed firmware writes'}
 if($mode -eq 'next-fail'){Check (!$global:fwMap.ContainsKey('Boot7E00') -and !$global:fwMap.ContainsKey('BootNext')) 'Owned rollback did not remove temporary option'}
 if($mode -eq 'foreign-next'){Check ((Test-FirmwareValueEqual $global:fwMap.BootNext $foreign) -and !$global:fwMap.ContainsKey('Boot7E00')) 'Rollback deleted foreign BootNext'}
 if($mode -eq 'order-change'){Check ($global:fwMap.ContainsKey('Boot7E00') -and !(Test-FirmwareValueEqual $global:fwMap.BootOrder $state.BootOrder)) 'Rollback overwrote foreign BootOrder change'}
 if($mode -eq 'option-delete-race'){Check ((Test-FirmwareValueEqual $global:fwMap.Boot7E00 (ConvertTo-FirmwareValue ([byte[]](99,98))))) 'Rollback deleted changed temporary option'}
}
Reset-Fake;$global:fwMap.Boot7E00=$state.Option;$global:fwMap.BootNext=ConvertTo-FirmwareValue ([byte[]](0,126));$global:fwMode='next-delete-race'
$blocked=$false;try{Remove-OwnedSharedEspFirmware $state $read $write}catch{$blocked=$true}
Check ($blocked -and (Test-FirmwareValueEqual $global:fwMap.BootNext $foreign) -and !$global:fwWrites.Count) 'Cleanup raced and deleted a foreign BootNext'
Reset-Fake;$global:fwMap.Boot7E00=$state.Option;$global:fwMap.BootNext=ConvertTo-FirmwareValue ([byte[]](0,126)) 6
$blocked=$false;try{Remove-OwnedSharedEspFirmware $state $read $write}catch{$blocked=$true}
Check ($blocked -and !$global:fwWrites.Count) 'Cleanup ignored changed BootNext attributes'
Reset-Fake
Register-SharedEspFirmwareOption $state $read $write $save
Check ($global:fwSaved -eq 1 -and $global:fwWrites.Count -eq 1 -and $global:fwWrites[0].Name -eq 'Boot7E00' -and !$global:fwMap.ContainsKey('BootNext')) 'Register-only wrote a boot selection'
foreach($mode in @('pending','existing','backup-fail','order-change','foreign-next')){
 Reset-Fake
 if($mode -eq 'pending'){$global:fwMap.BootNext=$foreign}
 elseif($mode -eq 'existing'){$global:fwMap.Boot7E00=$state.Option}
 else{$global:fwMode=$mode}
 $blocked=$false;try{Register-SharedEspFirmwareOption $state $read $write $save}catch{$blocked=$true}
 Check $blocked ('Register-only accepted '+$mode)
 if($mode -in @('pending','existing','backup-fail')){Check (!$global:fwWrites.Count) 'Register-only wrote after a precondition refusal'}
 Check (@($global:fwWrites | Where-Object {$_.Name -ne 'Boot7E00'}).Count -eq 0) 'Register-only wrote an unrelated variable'
}
$tokens=$null;$errors=$null;[void][Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'shared-esp-firmware-once.ps1'),[ref]$tokens,[ref]$errors)
Check (!$errors.Count) 'Preparation script has syntax errors'
$out=Join-Path $PSScriptRoot 'build/firmware-once-vector.bin';[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($out));[IO.File]::WriteAllBytes($out,[Convert]::FromBase64String($option.DataBase64))
"PASS $global:fwChecks binary/state-machine checks; native bridge compiled only. No native firmware API or hardware was accessed. Vector: $out"
