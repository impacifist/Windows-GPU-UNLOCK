param([Parameter(Mandatory)][string]$PackageRoot)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test-evga-profile-manager.ps1') -PackageRoot $PackageRoot
$fixture=Join-Path $fixture 'normal-boot'
$f=Setup-ManagerFixture 'SharedEsp'
[SharedEspFirmware.Native]::Values['Boot000C']=New-FixtureOption '\EFI\Microsoft\Boot\bootmgfw.efi'
Set-FixtureValue 'BootOrder' ([byte[]](0,0,8,0,12,0))
$null=& $manager -StateDirectory $f.State -Assets $f.Assets -PackageDirectory (Join-Path $PackageRoot 'SharedEsp-10')
$a=Get-Content -LiteralPath (Join-Path $f.State 'attempt-10.json') -Raw | ConvertFrom-Json;Simulate-PackageRun $a
$null=& $manager -Action Collect -StateDirectory $f.State
$global:normalProfileBoot=[datetime]'2026-10-30T12:00:00Z'
function Get-CimInstance {param($ClassName)if($ClassName -cne 'Win32_OperatingSystem'){throw 'Unexpected CIM query'};[pscustomobject]@{LastBootUpTime=$global:normalProfileBoot}}
$script=Join-Path $PSScriptRoot 'confirm-evga-profile-normal-boot.ps1'
$writes=[SharedEspFirmware.Native]::Writes.Count
$ready=& $script -StateDirectory $f.State | ConvertFrom-Json
if($ready.phase -cne 'ready_for_normal_restart'){throw 'Missing ready phase'}
$blocked=$false;try{& $script -StateDirectory $f.State | Out-Null}catch{$blocked=$true}
if(!$blocked){throw 'Same Windows session accepted'}
$global:normalProfileBoot=$global:normalProfileBoot.AddHours(1)
Set-FixtureValue 'BootCurrent' ([byte[]](0,0))
$done=& $script -StateDirectory $f.State | ConvertFrom-Json
if(!$done.normal_restart_confirmed -or $done.boot_current -cne 'Boot0000' -or $done.firmware_written -or $done.gpu_accessed){throw 'Normal restart not confirmed'}
Set-FixtureValue 'BootCurrent' ([byte[]](0,126))
$blocked=$false;try{& $script -StateDirectory $f.State | Out-Null}catch{$blocked=$true}
if(!$blocked){throw 'Temporary BootCurrent accepted'}
if([SharedEspFirmware.Native]::Writes.Count -ne $writes){throw 'Script wrote firmware'}
Write-Host 'PASS normal-boot baseline, same-session refusal, newer normal entry, temporary-current refusal; no script firmware writes. Fake Native and OS boot time.'
