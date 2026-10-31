# GPL-2.0-only. Determine the next attended attempt without modifying media.
function Get-EvgaNextAttemptSlot {
 param([Parameter(Mandatory)][string[]]$MarkerNames)
 $seen=@{}
 foreach($name in $MarkerNames){
  if($name -match '^EVGA-UNLOCK([0-9]+)\.ARMED$'){
   $digits=$Matches[1]
   if($digits -notmatch '^[1-9][0-9]?$'){throw 'Invalid numbered unlock marker; preserve evidence'}
   $slot=[int]$digits
   if($seen.ContainsKey($slot)){throw 'Duplicate numbered unlock marker; preserve evidence'}
   $seen[$slot]=$true
  } elseif($name -like 'EVGA-UNLOCK*.ARMED' -and $name -ine 'EVGA-UNLOCK.ARMED'){
   throw 'Unexpected unlock marker; preserve evidence'
  }
 }
 if(!$seen.ContainsKey(4)){throw 'Expected reviewed slot 4 marker; preserve evidence'}
 $highest=($seen.Keys | Measure-Object -Maximum).Maximum
 for($i=4;$i -le $highest;$i++){if(!$seen.ContainsKey($i)){throw 'Gap in unlock markers; preserve evidence'}}
 if($highest -ge 99){throw 'All numbered attempt slots are exhausted'}
 return [int]($highest+1)
}
