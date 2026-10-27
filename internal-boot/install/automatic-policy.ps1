# Mutations are serialized; any failed step/verification invokes recovery once.
function Resolve-NewBootCopy {
 param([string[]]$BeforeIds,[hashtable]$AfterEntries,[string]$Source,[string]$Name)
 $new=@($AfterEntries.Keys | Where-Object {$BeforeIds -notcontains $_})
 if($new.Count -ne 1){throw 'Cannot attribute one new boot copy; inspect firmware backup'}
 $id=$new[0];$text=$AfterEntries[$id]
 if($text -notmatch ('(?m)^description\s+'+[regex]::Escape($Name)+'\s*$')){throw 'New entry description differs'}
 $zh=[string]::Join('',@([char]0x6807,[char]0x8bc6,[char]0x7b26))
 $strip="(?m)^((?:identifier|$zh)|description)[^\r\n]*(\r?\n|$)"
 if(($text -replace $strip,'') -cne ($Source -replace $strip,'')){throw 'New entry is not the expected boot-manager copy'}
 return $id
}
function Invoke-RecoveryActions {
 param([scriptblock[]]$Actions)
 $failures=@()
 foreach($action in $Actions){try{& $action}catch{$failures+=$_.Exception.Message}}
 if($failures.Count){throw ('Recovery incomplete: '+($failures -join '; '))}
}
function Invoke-AutomaticTransition {
 param([scriptblock[]]$Steps,[scriptblock]$Verify,[scriptblock]$Rollback)
 try {
  foreach($step in $Steps){& $step}
  & $Verify
 }catch{
  $original=$_
  try{& $Rollback}catch{Write-Warning ('Automatic boot recovery failed: '+$_.Exception.Message)}
  throw $original
 }
}
