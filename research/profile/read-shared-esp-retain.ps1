# Read one known receipt without mounting, writing, deleting or changing boot entries.
param([Parameter(Mandatory)][guid]$WindowsEspGuid)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
$parts=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($WindowsEspGuid -eq [guid]::Empty -or $parts.Count -ne 1){throw 'Expected exact pinned system ESP'}
$volumes=@($parts[0] | Get-Volume)
if($volumes.Count -ne 1 -or $volumes[0].FileSystem -ne 'FAT32' -or !$volumes[0].Path){throw 'Expected FAT32 system ESP'}
$file=Get-AuditCheckedPath $volumes[0].Path @('RTX3090-UNLOCK','RETAIN01.TXT') $false
if((Get-Item -LiteralPath $file).Length -ne 512){throw 'Receipt is not 512 bytes; preserve it and report'}
$bytes=[IO.File]::ReadAllBytes($file)
$text=[Text.Encoding]::ASCII.GetString($bytes).TrimEnd([char]0)
[ordered]@{schema='evga-shared-esp-retain01';read_only=$true;bytes=$bytes.Length;sha256=(Get-FileHash -LiteralPath $file).Hash;text=$text;windows_handoff_proven=$false} | ConvertTo-Json
