# GPL-2.0-only. Read-only storage inventory; does not mount or modify partitions.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$espType='{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}'
$disks=@(Get-Disk)
$partitions=@(Get-Partition)
$volumes=@(Get-Volume)
$result=@(foreach($disk in $disks){
 $parts=@(foreach($part in $partitions | Where-Object {$_.DiskNumber -eq $disk.Number}){
  $letter=if($part.DriveLetter){[string]$part.DriveLetter}else{$null}
  $matchingVolumes=@(if($letter){$volumes | Where-Object {[string]$_.DriveLetter -eq $letter}})
  $filesystem=if($matchingVolumes.Count -eq 1 -and $matchingVolumes[0].FileSystem){[string]$matchingVolumes[0].FileSystem}else{$null}
  $type=if($part.GptType){([string]$part.GptType).ToLowerInvariant()}else{$null}
  [ordered]@{partition_number=$part.PartitionNumber;size_bytes=[long]$part.Size;gpt_type=$type;is_system=[bool]$part.IsSystem;is_boot=[bool]$part.IsBoot;drive_letter=$letter;file_system=$filesystem;
   candidate_dedicated_esp=([string]$disk.BusType -ne 'USB' -and $type -eq $espType -and !$part.IsSystem -and !$part.IsBoot -and $part.Size -ge 1GB -and $filesystem -eq 'FAT32')}
 })
 [ordered]@{disk_number=$disk.Number;bus_type=[string]$disk.BusType;partition_style=[string]$disk.PartitionStyle;size_bytes=[long]$disk.Size;partitions=$parts}
})
[ordered]@{schema='evga-internal-storage-inventory-v1';read_only=$true;hardware_access='Windows storage inventory only';candidate_definition='non-USB GPT ESP, at least 1 GiB, FAT32, accessible drive letter, not system or boot';windows_system_partition_count=@($partitions | Where-Object {$_.IsSystem}).Count;disks=$result} | ConvertTo-Json -Depth 6
