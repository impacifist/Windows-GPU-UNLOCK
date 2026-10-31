# Direct UEFI variables, not the Windows BCD store. No call executes at import.
function ConvertTo-FirmwareValue([byte[]]$Data,[uint32]$Attributes=7){
 [pscustomobject]@{Attributes=$Attributes;DataBase64=[Convert]::ToBase64String($Data)}
}
function Test-FirmwareValueEqual($A,$B){
 if($null -eq $A -or $null -eq $B){return $null -eq $A -and $null -eq $B}
 return [uint32]$A.Attributes -eq [uint32]$B.Attributes -and $A.DataBase64 -ceq $B.DataBase64
}
function New-SharedEspLoadOption([guid]$PartitionGuid,[uint32]$PartitionNumber,[uint64]$StartLba,[uint64]$SizeLba,[string]$Description,[string]$FilePath='\EFI\EVGA-SHARED-AUDIT-VISIBLE.EFI'){
 if($PartitionGuid -eq [guid]::Empty -or !$PartitionNumber -or !$SizeLba -or $Description.Contains([char]0)){throw 'Invalid EFI load option input'}
 if($FilePath -cnotin @('\EFI\EVGA-SHARED-AUDIT-VISIBLE.EFI','\EFI\EVGA-SHARED-LAUNCH02.EFI') -and $FilePath -cnotmatch '^\\EFI\\WindowsGPUUnlock\\EVP(?:0[8-9]|[1-9][0-9])\\UNLOCK\.EFI$'){throw 'Unsupported firmware launch path'}
 $path=[Text.Encoding]::Unicode.GetBytes($FilePath+[char]0)
 $stream=New-Object IO.MemoryStream;$w=New-Object IO.BinaryWriter($stream)
 try {
  $w.Write([uint32]1);$w.Write([uint16](42+4+$path.Length+4));$w.Write([Text.Encoding]::Unicode.GetBytes($Description+[char]0))
  $w.Write([byte]4);$w.Write([byte]1);$w.Write([uint16]42);$w.Write($PartitionNumber);$w.Write($StartLba);$w.Write($SizeLba);$w.Write($PartitionGuid.ToByteArray());$w.Write([byte]2);$w.Write([byte]2)
  $w.Write([byte]4);$w.Write([byte]4);$w.Write([uint16](4+$path.Length));$w.Write($path)
  $w.Write([byte]127);$w.Write([byte]255);$w.Write([uint16]4);$w.Flush()
  return ,$stream.ToArray()
 }finally{$w.Dispose();$stream.Dispose()}
}
function Read-SharedEspLoadOption($Value){
 [byte[]]$b=[Convert]::FromBase64String($Value.DataBase64)
 if($b.Length -lt 10){throw 'Truncated firmware load option'}
 $length=[BitConverter]::ToUInt16($b,4);$start=6
 while($start+1 -lt $b.Length -and ($b[$start] -ne 0 -or $b[$start+1] -ne 0)){$start+=2}
 $start+=2;$end=$start+$length
 if($start -gt $b.Length -or $end -gt $b.Length){throw 'Malformed firmware load option'}
 $file='';$guid=$null;$number=$null;$ended=$false
 for($p=$start;$p+4 -le $end;){
  $n=[BitConverter]::ToUInt16($b,$p+2);if($n -lt 4 -or $p+$n -gt $end){throw 'Malformed firmware device path'}
  if($b[$p] -eq 4 -and $b[$p+1] -eq 1){
   if($n -ne 42 -or $b[$p+40] -ne 2 -or $b[$p+41] -ne 2 -or $guid){throw 'Expected one GPT hard-drive node'}
   $number=[BitConverter]::ToUInt32($b,$p+4);$g=New-Object byte[] 16;[Array]::Copy($b,$p+24,$g,0,16);$guid=[guid]::new($g)
  }
  if($b[$p] -eq 4 -and $b[$p+1] -eq 4){if(($n-4)%2){throw 'Odd file path length'};$file+=[Text.Encoding]::Unicode.GetString($b,$p+4,$n-4).TrimEnd([char]0)}
  if($b[$p] -eq 127){if($b[$p+1] -ne 255 -or $n -ne 4){throw 'Unsupported multi-instance path'};$ended=$true;break}
  $p+=$n
 }
 if(!$ended){throw 'Missing firmware device-path end'}
 [pscustomobject]@{Active=([BitConverter]::ToUInt32($b,0) -band 1) -ne 0;Path=$file;PartitionGuid=$guid;PartitionNumber=$number}
}
function Assert-SharedEspFirmwareSnapshot($State,[scriptblock]$Read){
 if(!(Test-FirmwareValueEqual (& $Read 'BootOrder') $State.BootOrder)){throw 'BootOrder changed; preserve backup and stop'}
 foreach($entry in $State.BootEntries){if(!(Test-FirmwareValueEqual (& $Read $entry.Name) $entry.Value)){throw "Existing $($entry.Name) changed; preserve backup and stop"}}
}
function Remove-OwnedSharedEspFirmware($State,[scriptblock]$Read,[scriptblock]$Write){
 Assert-SharedEspFirmwareSnapshot $State $Read
 $next=& $Read 'BootNext';$ownedNext=ConvertTo-FirmwareValue ([BitConverter]::GetBytes([uint16]$State.OptionNumber))
 if(Test-FirmwareValueEqual $next $ownedNext){
  if(!(Test-FirmwareValueEqual (& $Read 'BootNext') $ownedNext)){throw 'BootNext raced during cleanup; nothing deleted'}
  & $Write 'BootNext' $null
  if($null -ne (& $Read 'BootNext')){throw 'BootNext deletion did not persist; preserve state'}
 }
 # A different pending BootNext belongs to someone else and is left intact.
 $currentNext=& $Read 'BootNext'
 if($null -ne $currentNext){
  [byte[]]$pending=[Convert]::FromBase64String($currentNext.DataBase64)
  if($pending.Length -eq 2 -and [BitConverter]::ToUInt16($pending,0) -eq $State.OptionNumber){throw 'BootNext still references the test option with unexpected state; refuse deletion'}
 }
 Assert-SharedEspFirmwareSnapshot $State $Read
 $option=& $Read $State.OptionName
 if($null -ne $option){
  if(!(Test-FirmwareValueEqual $option $State.Option)){throw 'Temporary Boot option differs; refusing deletion'}
  if(!(Test-FirmwareValueEqual (& $Read $State.OptionName) $State.Option)){throw 'Boot option raced during cleanup; nothing deleted'}
  & $Write $State.OptionName $null
  if($null -ne (& $Read $State.OptionName)){throw 'Temporary Boot option deletion did not persist'}
 }
 Assert-SharedEspFirmwareSnapshot $State $Read
}
function Invoke-SharedEspFirmwareOnce($State,[scriptblock]$Read,[scriptblock]$Write,[scriptblock]$Save){
 Assert-SharedEspFirmwareSnapshot $State $Read
 if($null -ne (& $Read 'BootNext')){throw 'A BootNext selection already exists; no firmware writes attempted'}
 if($null -ne (& $Read $State.OptionName)){throw 'Chosen Boot option is no longer unused'}
 & $Save $State # Durable immutable backup before the first firmware write.
 $attempted=$false
 try {
  Assert-SharedEspFirmwareSnapshot $State $Read
  if($null -ne (& $Read 'BootNext') -or $null -ne (& $Read $State.OptionName)){throw 'Firmware state changed before registration'}
  $attempted=$true;& $Write $State.OptionName $State.Option
  if(!(Test-FirmwareValueEqual (& $Read $State.OptionName) $State.Option)){throw 'Boot option readback differs'}
  Assert-SharedEspFirmwareSnapshot $State $Read
  if($null -ne (& $Read 'BootNext')){throw 'Another BootNext appeared; refusing to replace it'}
  & $Write 'BootNext' (ConvertTo-FirmwareValue ([BitConverter]::GetBytes([uint16]$State.OptionNumber)))
  if(!(Test-FirmwareValueEqual (& $Read 'BootNext') (ConvertTo-FirmwareValue ([BitConverter]::GetBytes([uint16]$State.OptionNumber))))){throw 'BootNext readback differs'}
  if(!(Test-FirmwareValueEqual (& $Read $State.OptionName) $State.Option)){throw 'Boot option changed after selection'}
  Assert-SharedEspFirmwareSnapshot $State $Read
 }catch{
  $original=$_.Exception.Message
  if($attempted){try {Remove-OwnedSharedEspFirmware $State $Read $Write}catch{throw "$original. Rollback refused/failed: $($_.Exception.Message). Preserve backup and do not reboot."}}
  throw "$original. Owned temporary variables were rolled back where safe; preserve backup and do not reboot."
 }
}

# Register only: no BootNext selection. Backup precedes the first write.
function Register-SharedEspFirmwareOption($State,[scriptblock]$Read,[scriptblock]$Write,[scriptblock]$Save){
 Assert-SharedEspFirmwareSnapshot $State $Read
 if($null -ne (& $Read 'BootNext')){throw 'Pending BootNext; registration refused'}
 if($null -ne (& $Read $State.OptionName)){throw 'Registration option already exists'}
 & $Save $State
 Assert-SharedEspFirmwareSnapshot $State $Read
 if($null -ne (& $Read 'BootNext') -or $null -ne (& $Read $State.OptionName)){throw 'State changed after backup; no registration attempted'}
 & $Write $State.OptionName $State.Option
 if(!(Test-FirmwareValueEqual (& $Read $State.OptionName) $State.Option)){throw 'Registration readback failed; preserve backup and partial state'}
 Assert-SharedEspFirmwareSnapshot $State $Read
 if($null -ne (& $Read 'BootNext')){throw 'BootNext appeared during registration; preserve backup and partial state'}
}
