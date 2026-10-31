$ErrorActionPreference='Stop'
function Get-Partition {[pscustomobject]@{IsSystem=$true}}
function Get-Volume {param([Parameter(ValueFromPipeline)]$InputObject) process {[pscustomobject]@{FileSystem='FAT32';Size=10GB;SizeRemaining=7GB}}}
function bcdedit.exe {param($a,$b) $global:LASTEXITCODE=0;@('Firmware Boot Manager','displayorder {bootmgr}','Windows Boot Manager')}
$result=(& (Join-Path $PSScriptRoot 'inspect-shared-esp.ps1') | Out-String) | ConvertFrom-Json
if(!$result.read_only -or $result.system_volume.file_system -ne 'FAT32' -or $result.system_volume.free_bytes -ne 7GB -or $result.firmware_entries.Count -ne 3){throw 'Incorrect inspection result'}
function bcdedit.exe {$global:LASTEXITCODE=1; 'access denied'}
$refused=$false;try {& (Join-Path $PSScriptRoot 'inspect-shared-esp.ps1') | Out-Null}catch{$refused=$true}
if(!$refused){throw 'Firmware enumeration error was not reported'}
'Shared ESP inspection passed synthetic volume and firmware success/failure cases; no firmware accessed.'
