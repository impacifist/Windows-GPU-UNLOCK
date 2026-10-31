# GPL-2.0-only. Read-only Windows storage and firmware boot-entry inspection.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$systemPartitions=@(Get-Partition | Where-Object {$_.IsSystem})
if($systemPartitions.Count -ne 1){throw 'Expected exactly one Windows system partition; no changes made'}
$systemVolumes=@($systemPartitions[0] | Get-Volume -ErrorAction Stop)
if($systemVolumes.Count -ne 1){throw 'Expected exactly one system volume; no changes made'}
$sysVolume=$systemVolumes[0]
$bootEntries=@(& bcdedit.exe /enum firmware 2>&1)
if($LASTEXITCODE -ne 0){throw "Read-only firmware entry enumeration failed (exit $LASTEXITCODE): $($bootEntries -join ' ')"}
[ordered]@{
 schema='evga-shared-esp-inspection-v1'
 read_only=$true
 system_volume=[ordered]@{file_system=[string]$sysVolume.FileSystem;size_bytes=[long]$sysVolume.Size;free_bytes=[long]$sysVolume.SizeRemaining}
 firmware_entries=@($bootEntries | ForEach-Object {[string]$_})
} | ConvertTo-Json -Depth 5
