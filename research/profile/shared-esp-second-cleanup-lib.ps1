# Narrow exception for the exact SULK02 contraction reviewed on 2026-10-30.
function Remove-ReviewedSharedSecondOption($State,[scriptblock]$Read,[scriptblock]$Write,[scriptblock]$Save){
 if($State.OptionName -cne 'Boot7E00' -or $State.Option.Attributes -ne 7){throw 'Unreviewed temporary option'}
 $oldNames=@('Boot0000','Boot0008','Boot000C','Boot000D','Boot000E','Boot000F','Boot0010')
 $keep=@($oldNames[0..3]);$missing=@($oldNames[4..6])
 [byte[]]$oldBytes=[Convert]::FromBase64String($State.BootOrder.DataBase64)
 if($State.BootOrder.Attributes -ne 7 -or $oldBytes.Length -ne 14 -or $State.BootEntries.Count -ne 7){throw 'Unreviewed original order'}
 for($i=0;$i -lt 7;$i++){if(('Boot{0:X4}' -f [BitConverter]::ToUInt16($oldBytes,2*$i)) -cne $oldNames[$i] -or $State.BootEntries[$i].Name -cne $oldNames[$i]){throw 'Unreviewed original entries'}}
 $current=& $Read 'BootOrder'
 [byte[]]$expected=[byte[]](0,0,8,0,12,0,13,0)
 if(!(Test-FirmwareValueEqual $current (ConvertTo-FirmwareValue $expected))){throw 'Current order differs from reviewed four-entry order'}
 $snapshot=[pscustomobject]@{BootOrder=$current;BootEntries=@($State.BootEntries[0..3])}
 $verify={
  Assert-SharedEspFirmwareSnapshot $snapshot $Read
  foreach($name in $missing){if($null -ne (& $Read $name)){throw 'A missing fallback entry reappeared; stop'}}
  if($null -ne (& $Read 'BootNext')){throw 'Pending BootNext; stop'}
 }
 foreach($pass in 1..2){& $verify;if(!(Test-FirmwareValueEqual (& $Read $State.OptionName) $State.Option)){throw 'Temporary option changed; stop'}}
 & $Save ([pscustomobject]@{Schema='shared-esp-second-cleanup-v1';BootOrder=$current;BootEntries=$snapshot.BootEntries;AbsentEntries=$missing;OptionName=$State.OptionName;Option=$State.Option})
 & $verify
 if(!(Test-FirmwareValueEqual (& $Read $State.OptionName) $State.Option)){throw 'Temporary option raced; stop'}
 & $Write $State.OptionName $null
 if($null -ne (& $Read $State.OptionName)){throw 'Temporary option deletion did not persist'}
 & $verify
 [ordered]@{cleanup_completed=$true;temporary_option_present=$false;boot_next_clear=$true;boot_order_written=$false;preserved_entries=$keep;already_absent_entries=$missing;firmware_writes=1}
}
