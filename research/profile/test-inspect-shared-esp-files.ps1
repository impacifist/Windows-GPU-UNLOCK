$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$testRoot=Join-Path $repo ('research/profile/build/esp-inspect-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $testRoot 'EFI/Microsoft/Boot'),(Join-Path $testRoot 'EFI/limine') -Force | Out-Null
Set-Content -LiteralPath (Join-Path $testRoot 'EFI/Microsoft/Boot/bootmgfw.efi') -Value 'windows fixture'
Set-Content -LiteralPath (Join-Path $testRoot 'EFI/limine/limine_x64.efi') -Value 'linux fixture'
function Get-Partition {[pscustomobject]@{IsSystem=$true}}
function Get-Volume {param([Parameter(ValueFromPipeline)]$InputObject) process {[pscustomobject]@{FileSystem='FAT32';Path=$testRoot}}}
$out=(& (Join-Path $PSScriptRoot 'inspect-shared-esp-files.ps1') | Out-String) | ConvertFrom-Json
if(!$out.read_only -or $out.mounted_or_assigned_letter -or $out.entries.Count -ne 4){throw 'Incorrect output structure'}
if(!$out.entries[0].exists -or !$out.entries[1].exists -or $out.entries[2].exists -or $out.entries[3].exists){throw 'Incorrect path status'}
if($out.entries[0].sha256 -ne (Get-FileHash -LiteralPath (Join-Path $testRoot 'EFI/Microsoft/Boot/bootmgfw.efi')).Hash.ToLowerInvariant()){throw 'Incorrect file hash'}
New-Item -ItemType Directory -Path (Join-Path $testRoot 'EFI/WindowsGPUUnlock') | Out-Null
$out=(& (Join-Path $PSScriptRoot 'inspect-shared-esp-files.ps1') | Out-String) | ConvertFrom-Json
if(!$out.entries[2].exists -or $out.entries[2].kind -ne 'directory'){throw 'Existing target was not detected'}
'Shared ESP path inspection passed fixture file, hash, missing and existing directory cases; no ESP accessed.'
