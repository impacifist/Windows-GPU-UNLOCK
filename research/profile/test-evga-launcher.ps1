$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'launch-evga-profile.ps1')
$arg=Get-EvgaLauncherArguments 'E:\folder with spaces\launch-evga-profile.ps1' 'Rearm'
if($arg -cne '-NoProfile -File "E:\folder with spaces\launch-evga-profile.ps1" -Action Rearm -Elevated'){throw 'Quoted path/arguments differ'}
$blocked=0
foreach($pair in @(@('E:\bad"path.ps1','Rearm'),@('E:\good.ps1','Erase'))){try{Get-EvgaLauncherArguments $pair[0] $pair[1] | Out-Null}catch{$blocked++;continue};throw 'Invalid launch accepted'}
foreach($name in @('prepare-evga-unlock.cmd','collect-evga-unlock.cmd')){
 $text=Get-Content -LiteralPath (Join-Path $PSScriptRoot $name) -Raw
 if($text -notmatch '%~dp0launch-evga-profile.ps1' -or $text -match 'ExecutionPolicy|shutdown'){throw 'Unexpected command wrapper'}
}
Write-Host 'PASS launcher argument quoting/refusals and local command wrappers. No UAC, manager, GPU or firmware action executed; double-click behavior remains untested live.'
