$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-unlock-progression.ps1')
$base=@{UsbMarkers=@('EVGA-UNLOCK.ARMED')+@(2..8 | ForEach-Object {"EVGA-UNLOCK$_.ARMED"});EspMarkers=@('EVGA-UNLOCK8.ARMED');LogSha256='c0cb1b121fbe24610e84dfdaee52da512856af39a65768a8441012e4236e5f23';ReservationSha256='2da4ba31c1e3793b4c76999002775feb241be768dc82ae8cf55515503db66842'}
if((Get-ReviewedSharedUnlockNextSlot @base) -ne 9){throw 'Reviewed progression refused'}
$count=1
foreach($kind in @('log','reservation','future','missing','replay')){
 $case=$base.Clone()
 switch($kind){
 'log' {$case.LogSha256='0'*64}
 'reservation' {$case.ReservationSha256='0'*64}
 'future' {$case.UsbMarkers=$base.UsbMarkers+@('EVGA-UNLOCK9.ARMED')}
 'missing' {$case.UsbMarkers=@($base.UsbMarkers | Where-Object {$_ -ne 'EVGA-UNLOCK7.ARMED'})}
 'replay' {$case.EspMarkers=@('EVGA-UNLOCK8.ARMED','EVGA-UNLOCK9.ARMED')}
 }
 $blocked=$false;try{Get-ReviewedSharedUnlockNextSlot @case | Out-Null}catch{$blocked=$true}
 if(!$blocked){throw "Accepted invalid $kind history"};$count++
}
"PASS $count reviewed-history progression cases; no storage or firmware access"
