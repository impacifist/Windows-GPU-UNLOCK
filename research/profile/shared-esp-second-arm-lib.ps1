# One-shot arming primitives. Importing this file performs no operations.
function Write-SharedSecondNewFile([string]$Path,[byte[]]$Data){
 $stream=$null
 try {$stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$stream.Write($Data,0,$Data.Length);$stream.Flush($true)}finally{if($stream){$stream.Dispose()}}
 if([Convert]::ToBase64String([IO.File]::ReadAllBytes($Path)) -cne [Convert]::ToBase64String($Data)){throw 'New file readback differs; preserve partial state'}
}
function Assert-RegisteredSharedSecond($State,[scriptblock]$Read){
 Assert-SharedEspFirmwareSnapshot $State $Read
 if(!(Test-FirmwareValueEqual (& $Read $State.OptionName) $State.Option)){throw 'Registered option differs; stop'}
 if(!(Test-FirmwareValueEqual (& $Read 'SecureBoot') $State.SecureBoot)){throw 'Secure Boot state changed; stop'}
}
function Select-RegisteredSharedSecond($State,[scriptblock]$Read,[scriptblock]$Write,[scriptblock]$Prepare,[scriptblock]$Verify){
 Assert-RegisteredSharedSecond $State $Read
 if($null -ne (& $Read 'BootNext')){throw 'Pending BootNext; nothing armed'}
 & $Prepare # Immutable local journal, original USB reservation, then ESP receipt.
 & $Verify
 Assert-RegisteredSharedSecond $State $Read
 if($null -ne (& $Read 'BootNext')){throw 'BootNext appeared; preserve armed files and do not reboot'}
 $next=ConvertTo-FirmwareValue ([BitConverter]::GetBytes([uint16]$State.OptionNumber))
 & $Write 'BootNext' $next # Last mutation. Never writes BootOrder or the existing option.
 if(!(Test-FirmwareValueEqual (& $Read 'BootNext') $next)){throw 'BootNext readback differs; preserve state and do not reboot'}
 Assert-RegisteredSharedSecond $State $Read
 & $Verify
}
