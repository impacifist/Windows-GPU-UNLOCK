# Read-only preview of a SULK02 firmware load option. Never registers/selects it.
param([Parameter(Mandatory)][guid]$WindowsEspGuid)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-firmware-lib.ps1')
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
if($WindowsEspGuid -ne [guid]'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'){throw 'Unreviewed ESP'}
$p=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($p.Count -ne 1){throw 'Expected unique system ESP'}
$d=Get-Disk -Number $p[0].DiskNumber;$v=@($p[0] | Get-Volume)
if($d.BusType -eq 'USB' -or $d.PartitionStyle -ne 'GPT' -or $v.Count -ne 1 -or $v[0].FileSystem -ne 'FAT32'){throw 'Expected internal GPT FAT32 ESP'}
$sector=[uint64]$d.LogicalSectorSize
if($sector -notin @(512,4096) -or $p[0].Offset%$sector -or $p[0].Size%$sector){throw 'Unsupported geometry'}
$launcher=Get-AuditCheckedPath $v[0].Path @('EFI','EVGA-SHARED-LAUNCH02.EFI') $false
$child=Get-AuditCheckedPath $v[0].Path @('EFI','EVGA-SHARED-UNLOCK02.EFI') $false
$build=Join-Path $PSScriptRoot '../../internal-boot/boot/build/shared-esp-unlock'
$m=Get-Content (Join-Path $build 'launcher-manifest.json') -Raw | ConvertFrom-Json
. (Join-Path $PSScriptRoot 'shared-esp-second-manifest-lib.ps1')
$childHash=(Get-FileHash -LiteralPath $child).Hash
$launcherHash=(Get-FileHash -LiteralPath $launcher).Hash
$checks=Get-SharedSecondManifestChecks $m $WindowsEspGuid $childHash $launcherHash
if(@($checks.Values | Where-Object {!$_}).Count){
 [ordered]@{read_only=$true;staged_payloads_match=$false;checks=$checks;manifest_mode=$m.Mode;manifest_child_name=$m.AuditFileName;staged_child_sha256=$childHash;expected_child_sha256=$m.AuditEfiSha256;staged_launcher_sha256=$launcherHash;expected_launcher_sha256=$m.EfiSha256;firmware_registered=$false;attempt_armed=$false;boot_authorized=$false} | ConvertTo-Json -Depth 5
 return
}
$value=ConvertTo-FirmwareValue (New-SharedEspLoadOption $WindowsEspGuid ([uint32]$p[0].PartitionNumber) ([uint64]([decimal]$p[0].Offset/$sector)) ([uint64]([decimal]$p[0].Size/$sector)) 'EVGA SULK02 development preview' '\EFI\EVGA-SHARED-LAUNCH02.EFI')
$decoded=Read-SharedEspLoadOption $value
if($decoded.Path -cne '\EFI\EVGA-SHARED-LAUNCH02.EFI' -or $decoded.PartitionGuid -ne $WindowsEspGuid -or $decoded.PartitionNumber -ne $p[0].PartitionNumber){throw 'Load option roundtrip failed'}
[ordered]@{read_only=$true;staged_payloads_match=$true;decoded_path=$decoded.Path;partition_matches=$true;firmware_registered=$false;attempt_armed=$false;boot_authorized=$false} | ConvertTo-Json
