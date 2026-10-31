# Fixture tests: children are synthetic scripts and no physical device is touched.
$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility
$fixture=Join-Path $env:TEMP ('sulk-wrapper-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$global:SulkCalls=[Collections.Generic.List[string]]::new()
$global:SulkFail=''
try {
 foreach($name in @('prepare-shared-esp-unlock.ps1','prepare-shared-esp-unlock-stage.ps1','finish-shared-esp-unlock.ps1')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $fixture $name)}
 [IO.File]::WriteAllText((Join-Path $fixture 'get-shared-esp-unlock-slot.ps1'),"param(`$Drive)`nreturn 8`n")
 [IO.File]::WriteAllText((Join-Path $fixture 'build-shared-esp-unlock-launcher.ps1'),"param(`$WindowsEspGuid,`$AttemptSlot,`$Clang,`$LldLink,`$Python)`n`$global:SulkCalls.Add('build'); if(`$global:SulkFail -eq 'build'){throw 'injected'} `n")
 foreach($name in @('test-shared-esp-unlock-gate.ps1','test-shared-esp-unlock-prepare.ps1','test-shared-esp-unlock-launcher.ps1')){[IO.File]::WriteAllText((Join-Path $fixture $name),"param(`$Clang,`$LldLink)`n`$global:SulkCalls.Add('$name'); if(`$global:SulkFail -eq '$name'){throw 'injected'} `n")}
 [IO.File]::WriteAllText((Join-Path $fixture 'stage-shared-esp-unlock.ps1'),"[CmdletBinding(SupportsShouldProcess)]`nparam(`$WindowsEspGuid,`$Drive)`nif(`$PSCmdlet.ShouldProcess('test','test')){`$global:SulkCalls.Add('esp')} `n")
 [IO.File]::WriteAllText((Join-Path $fixture 'stage-shared-esp-unlock-launcher.ps1'),"[CmdletBinding(SupportsShouldProcess)]`nparam(`$WindowsEspGuid,`$Drive,[switch]`$ReuseExisting)`nif(`$PSCmdlet.ShouldProcess('test','test')){`$global:SulkCalls.Add('usb')} `n")
 [IO.File]::WriteAllText((Join-Path $fixture 'read-shared-esp-unlock.ps1'),"param(`$WindowsEspGuid,`$AttemptSlot,`$OutputDirectory)`n`$global:SulkCalls.Add('read'); if(`$global:SulkFail -eq 'read'){throw 'injected'} `n")
 [IO.File]::WriteAllText((Join-Path $fixture 'restore-shared-esp-unlock-launcher.ps1'),"param(`$Drive,`$OriginalSha256)`n`$global:SulkCalls.Add('restore') `n")
 function Get-FileHash {param($LiteralPath,$Algorithm) [pscustomobject]@{Hash=('A'*64)}}
 function Join-Path {param($Path,$ChildPath) if($Path -eq 'X:\'){return 'mock-boot'};Microsoft.PowerShell.Management\Join-Path -Path $Path -ChildPath $ChildPath}
 $args=@{WindowsEspGuid=[guid]'c36c3e86-02aa-4f76-af1b-96e327f9d1c1';Drive='X'}
 & (Join-Path $fixture 'prepare-shared-esp-unlock.ps1') @args
 if(($global:SulkCalls -join ',') -ne 'build,test-shared-esp-unlock-gate.ps1,test-shared-esp-unlock-prepare.ps1,test-shared-esp-unlock-launcher.ps1'){throw 'Unexpected build-only calls'}
 $global:SulkCalls.Clear();$global:SulkFail='test-shared-esp-unlock-gate.ps1';$failed=$false
 try {& (Join-Path $fixture 'prepare-shared-esp-unlock.ps1') @args -Stage}catch{$failed=$true}
 if(!$failed -or $global:SulkCalls.Contains('esp') -or $global:SulkCalls.Contains('usb')){throw 'Failure did not prevent staging'}
 $global:SulkCalls.Clear();$global:SulkFail=''
 & (Join-Path $fixture 'prepare-shared-esp-unlock.ps1') @args -Stage
 if(($global:SulkCalls -join ',') -ne 'build,test-shared-esp-unlock-gate.ps1,test-shared-esp-unlock-prepare.ps1,test-shared-esp-unlock-launcher.ps1,esp,usb'){throw 'Unexpected stage sequence'}
 $global:SulkCalls.Clear()
 & (Join-Path $fixture 'prepare-shared-esp-unlock-stage.ps1') @args -WhatIf
 if($global:SulkCalls.Count -ne 0){throw 'WhatIf performed staging'}
 $global:SulkCalls.Clear();$global:SulkFail='read';$failed=$false
 try {& (Join-Path $fixture 'finish-shared-esp-unlock.ps1') @args -AttemptSlot 8 -OriginalSha256 ('A'*64) -OutputDirectory 'fixture'}catch{$failed=$true}
 if(!$failed -or $global:SulkCalls.Contains('restore')){throw 'Read failure did not stop restore'}
 $global:SulkCalls.Clear();$global:SulkFail=''
 & (Join-Path $fixture 'finish-shared-esp-unlock.ps1') @args -AttemptSlot 8 -OriginalSha256 ('A'*64) -OutputDirectory 'fixture'
 if(($global:SulkCalls -join ',') -ne 'read,restore'){throw 'Unexpected finish sequence'}
 'Wrapper fixtures passed: build, fail-stop, stage order, dry run and finish; no hardware accessed.'
} finally {
 $resolved=[IO.Path]::GetFullPath($fixture);$temp=[IO.Path]::GetFullPath($env:TEMP).TrimEnd('\')+'\'
 if(!$resolved.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected fixture path'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
