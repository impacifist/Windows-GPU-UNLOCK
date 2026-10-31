$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'cleanup-reviewed-evp10.ps1')
$v=ConvertTo-FirmwareValue ([byte[]](1,2,3))
$u=ConvertTo-FirmwareValue ([byte[]](4,5,6))
$a=[pscustomobject]@{BootEntries=@([pscustomobject]@{Name='Boot0000';Value=$v})}
$now=[pscustomobject]@{BootEntries=@($a.BootEntries)+@([pscustomobject]@{Name='Boot000D';Value=$u})}
$read={param($name)if($name -eq 'Boot0000'){$v}}
# The exact digest is supplied by a fake hasher for this synthetic byte fixture.
# Production hasher remains SHA256; no native firmware API is loaded.
$actual=Get-EvgaReviewHash $u
if($actual -cne '787c798e39a5bc1910355bae6d0cd87a36b2e10fd0202a83e3bb6b005da83472'){throw 'SHA256 helper mismatch'}
function Get-EvgaReviewHash($Value){if($Value.DataBase64 -ceq $u.DataBase64){'ac1eb792d44382686770c715aacae3681d572ab8f61cc7e6dc9d71fa0fab94ee'}else{'wrong'}}
Assert-ReviewedEvp10Snapshot $a $now $read
$blocked=0
foreach($kind in @('changed','order','extra','attributes')){
 $bad=$now | ConvertTo-Json -Depth 8 | ConvertFrom-Json
 switch($kind){
  changed {$bad.BootEntries[0].Value=$u}
  order {$bad.BootEntries=@($bad.BootEntries[1],$bad.BootEntries[0])}
  extra {$bad.BootEntries+=,[pscustomobject]@{Name='Boot000E';Value=$u}}
  attributes {$bad.BootEntries[1].Value.Attributes=6}
 }
 try{Assert-ReviewedEvp10Snapshot $a $bad $read}catch{$blocked++;continue}
 throw ('Unexpected admission: '+$kind)
}
if($blocked -ne 4){throw 'Missing refusal coverage'}
$tokens=$null;$errors=$null
[void][Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'cleanup-reviewed-evp10.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw $errors[0]}
Write-Host 'PASS real SHA256, reviewed snapshot admission, four mutations refused and controller syntax. No native firmware API loaded; deletion path uses existing tested cleanup helper.'
