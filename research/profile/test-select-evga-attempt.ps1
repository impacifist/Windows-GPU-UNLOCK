$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'select-evga-attempt.ps1')
function ExpectRefusal([string[]]$names){$failed=$false;try {Get-EvgaNextAttemptSlot -MarkerNames $names | Out-Null}catch{$failed=$true};if(!$failed){throw 'Expected marker refusal'}}
if((Get-EvgaNextAttemptSlot -MarkerNames @('EVGA-UNLOCK.ARMED','EVGA-UNLOCK2.ARMED','EVGA-UNLOCK3.ARMED','EVGA-UNLOCK4.ARMED','EVGA-UNLOCK5.ARMED','EVGA-UNLOCK6.ARMED')) -ne 7){throw 'Wrong next slot'}
if((Get-EvgaNextAttemptSlot -MarkerNames @('EVGA-UNLOCK4.ARMED')) -ne 5){throw 'Wrong initial slot'}
ExpectRefusal @('EVGA-UNLOCK3.ARMED')
ExpectRefusal @('EVGA-UNLOCK4.ARMED','EVGA-UNLOCK6.ARMED')
ExpectRefusal @('EVGA-UNLOCK4.ARMED','EVGA-UNLOCK04.ARMED')
ExpectRefusal @('EVGA-UNLOCK4.ARMED','EVGA-UNLOCK5.ARMED','evga-unlock5.armed')
ExpectRefusal @('EVGA-UNLOCK4.ARMED','EVGA-UNLOCKoops.ARMED')
$full=4..99 | ForEach-Object {"EVGA-UNLOCK$_.ARMED"}
ExpectRefusal $full
'Next-slot selection passed eight synthetic cases; no hardware accessed.'
