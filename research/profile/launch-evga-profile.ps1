# Interactive convenience launcher; retains manager guards, UAC and no automatic reboot.
[CmdletBinding()]
param([ValidateSet('Rearm','Collect','Cleanup','Status')][string]$Action='Rearm',[switch]$Elevated)
$ErrorActionPreference='Stop'
function Get-EvgaLauncherArguments([string]$Script,[string]$Operation){
 if($Script.Contains('"') -or $Operation -cnotin @('Rearm','Collect','Cleanup','Status')){throw 'Invalid launcher argument'}
 return '-NoProfile -File "'+$Script+'" -Action '+$Operation+' -Elevated'
}
if($MyInvocation.InvocationName -eq '.'){return}
try{
 $admin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
 if(!$admin){
  if($Elevated){throw 'Elevation did not grant administrator rights'}
  $child=Start-Process -FilePath (Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe') -Verb RunAs -ArgumentList (Get-EvgaLauncherArguments $PSCommandPath $Action) -Wait -PassThru
  exit $child.ExitCode
 }
 & (Join-Path $PSScriptRoot 'manage-evga-profile.ps1') -Action $Action
 Write-Host 'Keep the output above. No shutdown or reboot was performed by this launcher.'
 [void](Read-Host 'Press Enter to close')
}catch{
 Write-Host ('Stopped: '+$_.Exception.Message) -ForegroundColor Red
 Write-Host 'Preserve the existing state and send the error. Do not retry or delete markers to bypass a refusal.'
 [void](Read-Host 'Press Enter to close')
 exit 1
}
