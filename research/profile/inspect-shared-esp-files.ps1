# GPL-2.0-only. Read selected known ESP paths through an existing volume path.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$systemPartitions=@(Get-Partition | Where-Object {$_.IsSystem})
if($systemPartitions.Count -ne 1){throw 'Expected exactly one system partition; no changes made'}
$systemVolumes=@($systemPartitions[0] | Get-Volume -ErrorAction Stop)
if($systemVolumes.Count -ne 1 -or !$systemVolumes[0].Path){throw 'System volume has no accessible volume path; no changes made'}
$volume=$systemVolumes[0]
if([string]$volume.FileSystem -ne 'FAT32'){throw 'System volume is not reported as FAT32; no changes made'}
$root=[string]$volume.Path
try {$rootAttributes=[IO.File]::GetAttributes($root)}catch{throw "Cannot read the existing system volume path; no changes made: $($_.Exception.Message)"}
if(!(($rootAttributes -band [IO.FileAttributes]::Directory) -eq [IO.FileAttributes]::Directory)){throw 'System volume path is not a directory'}
function Inspect-Entry([string]$relative,[bool]$expectedDirectory){
 $path=[IO.Path]::Combine($root,$relative)
 try {$attributes=[IO.File]::GetAttributes($path)}
 catch [IO.FileNotFoundException] {return [ordered]@{path=$relative;exists=$false}}
 catch [IO.DirectoryNotFoundException] {return [ordered]@{path=$relative;exists=$false}}
 catch {throw "Cannot inspect $relative; no changes made: $($_.Exception.Message)"}
 $isDirectory=(($attributes -band [IO.FileAttributes]::Directory) -eq [IO.FileAttributes]::Directory)
 if($isDirectory -ne $expectedDirectory){throw "Unexpected object type at $relative; no changes made"}
 if(($attributes -band [IO.FileAttributes]::ReparsePoint) -eq [IO.FileAttributes]::ReparsePoint){throw "Reparse point at $relative; no changes made"}
 if($isDirectory){return [ordered]@{path=$relative;exists=$true;kind='directory'}}
 $info=Get-Item -LiteralPath $path -Force -ErrorAction Stop
 return [ordered]@{path=$relative;exists=$true;kind='file';size_bytes=[long]$info.Length;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256 -ErrorAction Stop).Hash.ToLowerInvariant()}
}
$entries=@((Inspect-Entry 'EFI\Microsoft\Boot\bootmgfw.efi' $false),(Inspect-Entry 'EFI\limine\limine_x64.efi' $false),(Inspect-Entry 'EFI\WindowsGPUUnlock' $true),(Inspect-Entry 'RTX3090-UNLOCK' $true))
[ordered]@{schema='evga-shared-esp-path-inspection-v1';read_only=$true;mounted_or_assigned_letter=$false;file_system=[string]$volume.FileSystem;entries=$entries} | ConvertTo-Json -Depth 5
