# Read-only history qualification. The USB reservation deliberately blocks later USB attempts.
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
. (Join-Path $PSScriptRoot 'select-evga-attempt.ps1')
function Get-SharedUnlockHistory([string]$UsbRoot){
 $capture=Get-AuditCheckedPath $UsbRoot @('RTX3090-UNLOCK') $true
 $markers=@(Get-ChildItem -LiteralPath $capture -File -Filter 'EVGA-UNLOCK*.ARMED')
 $next=Get-EvgaNextAttemptSlot -MarkerNames @($markers | ForEach-Object {$_.Name})
 foreach($name in @('EVGA-UNLOCK.ARMED','EVGA-UNLOCK2.ARMED','EVGA-UNLOCK3.ARMED')){
  [void](Get-AuditCheckedPath $capture @($name) $false)
 }
 $pins=@{
  'EVGA17.TXT'='fb0dd12b5eacc5bd7e289be04827aa1b15f9ce94bf768e63fabcd98b39bedb46'
  'EVGA18.TXT'='7f2c070f80d680d49997ee990b99aefbe1040707db2a40f59bd5a16aa43244a4'
  'EVGA19.TXT'='dfd6a0e0e58b8bc794e87a6c1b0e774ac674d5b483985ba36439b53fbc597537'
  'EVGA20.TXT'='0d793ac0913e13f1aa786026743aa33ddcb6b5fe79fe0975a1269b5c378b9053'
 }
 foreach($name in $pins.Keys){
  $file=Get-AuditCheckedPath $capture @($name) $false
  if((Get-FileHash -LiteralPath $file).Hash -ine $pins[$name]){throw "Original USB history differs: $name"}
 }
 $logs=@(Get-ChildItem -LiteralPath $capture -File -Filter 'EVGA*.TXT' | Where-Object {$_.Name -match '^EVGA\d\d\.TXT$'} | Sort-Object Name)
 if(!$logs.Count){throw 'No original USB attempt log'}
 $latest=Get-AuditCheckedPath $capture @($logs[-1].Name) $false
 if((Get-Item -LiteralPath $latest).Length -gt 9MB){throw 'Unexpected prior USB log size'}
 $text=[IO.File]::ReadAllText($latest)
 foreach($event in @(('evga-unlock-slot-{0:D2}' -f ($next-1)),'evga-unlock-transaction-return','evga-unlock-assets-released','evga-text-stop-ready-windows')){
  if(!$text.Contains($event+' status=0x0000000000000000')){throw "Latest USB attempt lacks successful $event; preserve history"}
 }
 if(([regex]::Matches($text,'capture-complete=1').Count) -lt 2){throw 'Latest USB attempt lacks complete captures'}
 $files=@($markers | ForEach-Object {Get-AuditCheckedPath $capture @($_.Name) $false})+@($latest)
 $snapshots=@($files | ForEach-Object {[pscustomobject]@{Path=$_;Name=[IO.Path]::GetFileName($_);Hash=(Get-FileHash -LiteralPath $_).Hash;Bytes=(Get-Item -LiteralPath $_).Length}})
 [pscustomobject]@{NextSlot=$next;Capture=$capture;Files=$snapshots;Bytes=($snapshots | Measure-Object Bytes -Sum).Sum}
}
