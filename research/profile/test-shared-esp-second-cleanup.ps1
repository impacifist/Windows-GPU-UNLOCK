# RAM-only exact-state cleanup and failure coverage; no native bridge loaded.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-firmware-lib.ps1')
. (Join-Path $PSScriptRoot 'shared-esp-second-cleanup-lib.ps1')
$names=@('Boot0000','Boot0008','Boot000C','Boot000D','Boot000E','Boot000F','Boot0010')
$option=ConvertTo-FirmwareValue ([byte[]](99,98))
$state=[pscustomobject]@{OptionName='Boot7E00';Option=$option;BootOrder=(ConvertTo-FirmwareValue ([byte[]](0,0,8,0,12,0,13,0,14,0,15,0,16,0)));BootEntries=@($names | ForEach-Object {[pscustomobject]@{Name=$_;Value=$option}})}
$global:cleanupChecks=0
function Check($ok,$why){$global:cleanupChecks++;if(!$ok){throw $why}}
function Reset-Cleanup {
 $global:cleanMap=@{BootOrder=(ConvertTo-FirmwareValue ([byte[]](0,0,8,0,12,0,13,0)));Boot7E00=$option}
 foreach($name in $names[0..3]){$global:cleanMap[$name]=$option}
 $global:cleanWrites=@();$global:cleanMode='';$global:cleanSaved=0
}
$read={param($name) $global:cleanMap[$name]}
$write={param($name,$value)
 if($name -cne 'Boot7E00' -or $null -ne $value){throw 'Unexpected write'}
 $global:cleanWrites+=,$name
 if($global:cleanMode -eq 'delete-fail'){throw 'Injected deletion failure'}
 if($global:cleanMode -ne 'retained'){[void]$global:cleanMap.Remove($name)}
 if($global:cleanMode -eq 'post-drift'){$global:cleanMap.BootOrder=$option}
}
$save={param($snapshot)
 $global:cleanSaved++
 if($global:cleanMode -eq 'save-fail'){throw 'Injected backup failure'}
 if($global:cleanMode -eq 'race-next'){$global:cleanMap.BootNext=$option}
 if($global:cleanMode -eq 'race-option'){$global:cleanMap.Boot7E00=$null}
}
Reset-Cleanup
$ok=Remove-ReviewedSharedSecondOption $state $read $write $save
Check ($ok.cleanup_completed -and $global:cleanWrites.Count -eq 1 -and $global:cleanSaved -eq 1 -and !$global:cleanMap.ContainsKey('Boot7E00')) 'Exact cleanup failed'
Check (Test-FirmwareValueEqual $global:cleanMap.BootOrder (ConvertTo-FirmwareValue ([byte[]](0,0,8,0,12,0,13,0)))) 'BootOrder changed'
foreach($mode in @('order','entry','fallback','pending','option','save-fail','race-next','race-option','delete-fail','retained','post-drift')){
 Reset-Cleanup;$global:cleanMode=$mode
 switch($mode){
  'order' {$global:cleanMap.BootOrder=$option}
  'entry' {$global:cleanMap.Boot0008=$null}
  'fallback' {$global:cleanMap.Boot000E=$option}
  'pending' {$global:cleanMap.BootNext=$option}
  'option' {$global:cleanMap.Boot7E00=$null}
 }
 $blocked=$false;try{Remove-ReviewedSharedSecondOption $state $read $write $save | Out-Null}catch{$blocked=$true}
 Check $blocked ('Accepted '+$mode)
 if($mode -notin @('delete-fail','retained','post-drift')){Check (!$global:cleanWrites.Count) ('Deleted after '+$mode)}
}
"PASS $global:cleanupChecks exact cleanup/failure checks. RAM-only; no firmware or GPU access."
