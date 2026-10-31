# Read/copy only the two known INTG01 evidence files. Never mounts or writes ESP.
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
$parts=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($WindowsEspGuid -eq [guid]::Empty -or $parts.Count -ne 1){throw 'Expected exact pinned system ESP'}
$volumes=@($parts[0] | Get-Volume)
if($volumes.Count -ne 1 -or $volumes[0].FileSystem -ne 'FAT32' -or !$volumes[0].Path){throw 'Expected FAT32 system ESP'}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Output directory already exists; preserve it and choose a new directory'}
$marker=Get-AuditCheckedPath $volumes[0].Path @('RTX3090-UNLOCK','INTG01.ARMED') $false
$log=Get-AuditCheckedPath $volumes[0].Path @('RTX3090-UNLOCK','INTG01.TXT') $false
if((Get-Item -LiteralPath $marker).Length -ne 512 -or (Get-Item -LiteralPath $log).Length -gt 9MB){throw 'Unexpected evidence size; preserve files and report'}
$output=[IO.Path]::GetFullPath($OutputDirectory)
$espRoot=$volumes[0].Path.TrimEnd([char[]]'\/')
if($output -ieq $espRoot -or $output.StartsWith($espRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must be outside the ESP'}
[void][IO.Directory]::CreateDirectory($output)
foreach($source in @($marker,$log)){
 $target=Join-Path $output ([IO.Path]::GetFileName($source));$i=$null;$o=$null
 try {$i=[IO.File]::OpenRead($source);$o=[IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$i.CopyTo($o);$o.Flush($true)}finally{if($o){$o.Dispose()};if($i){$i.Dispose()}}
 if((Get-FileHash -LiteralPath $source).Hash -ine (Get-FileHash -LiteralPath $target).Hash){throw 'Evidence copy differs; preserve both and report'}
}
$text=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($marker)).TrimEnd([char]0)
$lines=[IO.File]::ReadAllLines($log)
[ordered]@{schema='evga-shared-esp-intg01';esp_read_only=$true;output=$output;marker_text=$text;events=@($lines | Where-Object {$_ -match '^intg01-|^capture-complete='});windows_handoff_proven=$false;files=@(foreach($name in @('INTG01.ARMED','INTG01.TXT')){Get-FileHash -LiteralPath (Join-Path $output $name) | Select-Object Path,Hash})} | ConvertTo-Json -Depth 4
