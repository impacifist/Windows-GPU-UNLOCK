# Offline qualification of the reviewed first shared-ESP transaction.
. (Join-Path $PSScriptRoot 'select-evga-attempt.ps1')
function Get-ReviewedSharedUnlockNextSlot {
 param([Parameter(Mandatory)][string[]]$UsbMarkers,[Parameter(Mandatory)][string[]]$EspMarkers,[Parameter(Mandatory)][string]$LogSha256,[Parameter(Mandatory)][string]$ReservationSha256)
 if($LogSha256 -ine 'c0cb1b121fbe24610e84dfdaee52da512856af39a65768a8441012e4236e5f23' -or $ReservationSha256 -ine '2da4ba31c1e3793b4c76999002775feb241be768dc82ae8cf55515503db66842'){throw 'Shared transaction evidence differs from reviewed hashes'}
 $next=Get-EvgaNextAttemptSlot -MarkerNames $UsbMarkers
 if($next -ne 9){throw 'Only reviewed shared slot 8 progression is supported'}
 if(@($EspMarkers).Count -ne 1 -or $EspMarkers[0] -cne 'EVGA-UNLOCK8.ARMED'){throw 'Unexpected ESP runtime marker history'}
 return 9
}
