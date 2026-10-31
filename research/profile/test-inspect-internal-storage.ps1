$ErrorActionPreference='Stop'
function Get-Disk {@([pscustomobject]@{Number=0;BusType='NVMe';PartitionStyle='GPT';Size=200GB},[pscustomobject]@{Number=1;BusType='USB';PartitionStyle='GPT';Size=32GB})}
function Get-Partition {@([pscustomobject]@{DiskNumber=0;PartitionNumber=1;Size=100MB;GptType='{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}';IsSystem=$true;IsBoot=$false;DriveLetter=$null},[pscustomobject]@{DiskNumber=0;PartitionNumber=2;Size=2GB;GptType='{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}';IsSystem=$false;IsBoot=$false;DriveLetter='U'},[pscustomobject]@{DiskNumber=1;PartitionNumber=1;Size=4GB;GptType='{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}';IsSystem=$false;IsBoot=$false;DriveLetter='D'})}
function Get-Volume {@([pscustomobject]@{DriveLetter='U';FileSystem='FAT32'},[pscustomobject]@{DriveLetter='D';FileSystem='FAT32'})}
$out=(& (Join-Path $PSScriptRoot 'inspect-internal-storage.ps1') | Out-String) | ConvertFrom-Json
if(!$out.read_only -or $out.windows_system_partition_count -ne 1){throw 'Wrong inventory metadata'}
if($out.disks.Count -ne 2 -or $out.disks[0].partitions.Count -ne 2){throw 'Wrong disk inventory'}
if($out.disks[0].partitions[0].candidate_dedicated_esp -or !$out.disks[0].partitions[1].candidate_dedicated_esp -or $out.disks[1].partitions[0].candidate_dedicated_esp){throw 'Wrong candidate classification'}
'Read-only storage inventory passed synthetic system, dedicated and USB ESP cases.'
