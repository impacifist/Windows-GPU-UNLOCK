# RAM-only ordering/failure tests. No native bridge or hardware API is loaded.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-firmware-lib.ps1')
. (Join-Path $PSScriptRoot 'shared-esp-second-arm-lib.ps1')
$option=ConvertTo-FirmwareValue ([byte[]](11,12))
$state=[pscustomobject]@{BootOrder=(ConvertTo-FirmwareValue ([byte[]](0,0)));BootEntries=@([pscustomobject]@{Name='Boot0000';Value=$option});Option=$option;OptionName='Boot7E00';OptionNumber=32256;SecureBoot=(ConvertTo-FirmwareValue ([byte[]](0)) 6)}
$global:armChecks=0
function Check($ok,$why){$global:armChecks++;if(!$ok){throw $why}}
function Reset-Arm {
 $global:armMap=@{BootOrder=$state.BootOrder;Boot0000=$option;Boot7E00=$option;SecureBoot=$state.SecureBoot}
 $global:armCalls=@();$global:armMode='';$global:armVerifies=0
}
$read={param($name) $global:armMap[$name]}
$write={param($name,$value)
 $global:armCalls+='write-'+$name
 if($name -cne 'BootNext'){throw 'Unexpected write'}
 if($global:armMode -eq 'write-fail'){throw 'Injected write failure'}
 $global:armMap[$name]=$value
 if($global:armMode -eq 'readback-fail'){$global:armMap[$name]=$option}
}
$prepare={
 $global:armCalls+='prepare'
 if($global:armMode -eq 'prepare-fail'){throw 'Injected partial file write'}
 if($global:armMode -eq 'foreign-next'){$global:armMap.BootNext=$option}
 if($global:armMode -eq 'order-change'){$global:armMap.BootOrder=$option}
 if($global:armMode -eq 'option-change'){$global:armMap.Boot7E00=$null}
 if($global:armMode -eq 'secure-change'){$global:armMap.SecureBoot=$option}
}
$verify={
 $global:armVerifies++;$global:armCalls+='verify'
 if($global:armMode -eq 'verify-fail' -or ($global:armMode -eq 'post-verify-fail' -and $global:armVerifies -eq 2)){throw 'Injected file verification failure'}
}
Reset-Arm
Select-RegisteredSharedSecond $state $read $write $prepare $verify
Check (($global:armCalls -join ',') -ceq 'prepare,verify,write-BootNext,verify') 'BootNext was not the last mutation after verified preparation'
Check (Test-FirmwareValueEqual $global:armMap.BootNext (ConvertTo-FirmwareValue ([byte[]](0,126)))) 'Selected wrong option'
foreach($mode in @('pending','missing-option','prepare-fail','verify-fail','foreign-next','order-change','option-change','secure-change','write-fail','readback-fail','post-verify-fail')){
 Reset-Arm;$global:armMode=$mode
 if($mode -eq 'pending'){$global:armMap.BootNext=$option}
 if($mode -eq 'missing-option'){$global:armMap.Boot7E00=$null}
 $blocked=$false;try{Select-RegisteredSharedSecond $state $read $write $prepare $verify}catch{$blocked=$true}
 Check $blocked ('Accepted failure '+$mode)
 if($mode -notin @('write-fail','readback-fail','post-verify-fail')){Check ($global:armCalls -notcontains 'write-BootNext') ('Selected boot after '+$mode)}
 if($mode -in @('pending','missing-option')){Check (!$global:armCalls.Count) 'Prepared files after initial refusal'}
 if($mode -eq 'post-verify-fail'){Check ($null -ne $global:armMap.BootNext) 'Silently rolled back after failure'}
}
$temp=Join-Path $PSScriptRoot ('build/arm-file-'+[guid]::NewGuid().ToString('N'))
$bytes=New-Object byte[] 512;$prefix=[Text.Encoding]::ASCII.GetBytes("PENDING SULK02`r`n");[Array]::Copy($prefix,$bytes,$prefix.Length)
Write-SharedSecondNewFile $temp $bytes
Check (([IO.File]::ReadAllBytes($temp)).Length -eq 512) 'Receipt size differs'
$blocked=$false;try{Write-SharedSecondNewFile $temp ([byte[]](1))}catch{$blocked=$true}
Check ($blocked -and [Convert]::ToBase64String([IO.File]::ReadAllBytes($temp)) -ceq [Convert]::ToBase64String($bytes)) 'Existing receipt overwritten'
"PASS $global:armChecks arming order/failure/receipt checks. RAM-only; no native firmware API or GPU access."
