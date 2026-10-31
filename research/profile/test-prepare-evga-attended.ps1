$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$testRoot=Join-Path $repo ('research/profile/build/prepare-fixture-'+[guid]::NewGuid().ToString('N'))
if(Get-PSDrive Q -ErrorAction SilentlyContinue){throw 'Q already used'}
New-Item -ItemType Directory -Path (Join-Path $testRoot 'RTX3090-UNLOCK') -Force | Out-Null
New-PSDrive -Name Q -PSProvider FileSystem -Root $testRoot | Out-Null
try {
 function Get-Partition {param($DriveLetter) [pscustomobject]@{DiskNumber=999;IsSystem=$false;IsBoot=$false}}
 function Get-Volume {param($DriveLetter) [pscustomobject]@{FileSystem='FAT32'}}
 function Get-Disk {param($Number) [pscustomobject]@{BusType='USB'}}
 foreach($i in 4..6){Set-Content -LiteralPath (Join-Path $testRoot "RTX3090-UNLOCK/EVGA-UNLOCK$i.ARMED") -Value 'fixture'}
 $before=@(Get-ChildItem -LiteralPath $testRoot -File -Recurse | ForEach-Object {$_.FullName})
 $out=@(& (Join-Path $PSScriptRoot 'prepare-evga-attended.ps1') -Drive Q -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1' -WhatIf)
 if(-not (($out -join "`n") -match 'Next attended attempt: slot 7')){throw 'Wrong selected slot'}
 $after=@(Get-ChildItem -LiteralPath $testRoot -File -Recurse | ForEach-Object {$_.FullName})
 if(Compare-Object $before $after){throw 'WhatIf changed fixture files'}
 $scripts=Join-Path $testRoot 'scripts';New-Item -ItemType Directory -Path $scripts | Out-Null
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'prepare-evga-attended.ps1'),(Join-Path $PSScriptRoot 'select-evga-attempt.ps1') -Destination $scripts
 @'
param($WindowsEspGuid,$Clang,$LldLink,$Python,[switch]$AttemptUnlock,$AttemptSlot)
if(!$AttemptUnlock -or $AttemptSlot -ne 7){throw 'Unexpected build arguments'}
if($env:EVGA_FIXTURE_FAIL_BUILD -eq '1'){throw 'Synthetic build failure'}
Set-Content -LiteralPath (Join-Path $PSScriptRoot 'build-called') -Value $AttemptSlot
'@ | Set-Content -LiteralPath (Join-Path $scripts 'build-evga-text-stop.ps1') -Encoding ascii
 @'
[CmdletBinding(SupportsShouldProcess)]
param($Drive,[switch]$AttemptUnlock)
if($Drive -ne 'Q' -or !$AttemptUnlock){throw 'Unexpected stage arguments'}
Set-Content -LiteralPath (Join-Path $PSScriptRoot 'stage-called') -Value $Drive
'@ | Set-Content -LiteralPath (Join-Path $scripts 'stage-evga-text-stop.ps1') -Encoding ascii
 $env:EVGA_FIXTURE_FAIL_BUILD='1'
 $refused=$false;try {& (Join-Path $scripts 'prepare-evga-attended.ps1') -Drive Q -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'}catch{$refused=$true}
 if(!$refused -or (Test-Path -LiteralPath (Join-Path $scripts 'stage-called'))){throw 'Build failure did not stop staging'}
 $env:EVGA_FIXTURE_FAIL_BUILD='0'
 & (Join-Path $scripts 'prepare-evga-attended.ps1') -Drive Q -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'
 if(!(Test-Path -LiteralPath (Join-Path $scripts 'build-called')) -or !(Test-Path -LiteralPath (Join-Path $scripts 'stage-called'))){throw 'Wrapper did not call build and stage'}
 'Attended preparation passed WhatIf, slot/argument dispatch and build-failure refusal; storage and child scripts mocked.'
} finally {Remove-Item Env:EVGA_FIXTURE_FAIL_BUILD -ErrorAction SilentlyContinue;Remove-PSDrive Q}
