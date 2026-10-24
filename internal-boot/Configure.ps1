# GPL-2.0-only. Read-only disk inspection; writes only ignored local build config.
param([Parameter(Mandatory)][ValidatePattern('^[D-Zd-z]$')][string]$UnlockDriveLetter,[Parameter(Mandatory)][guid]$WindowsEspGuid,[ValidateSet('b650m-3090fe','b660m-3070ti')][string]$HardwareProfile='b650m-3090fe')
$ErrorActionPreference='Stop'
$unlock=Get-Partition -DriveLetter $UnlockDriveLetter
$windows=@(Get-Partition | Where-Object {$_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
$uv=$unlock | Get-Volume
if($windows.Count -ne 1 -or !$windows[0].IsSystem){throw 'WindowsEspGuid must identify the current system EFI partition'}
$wv=$windows[0] | Get-Volume
if($unlock.IsSystem -or [guid]$unlock.Guid -eq $WindowsEspGuid -or $unlock.GptType -ne '{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}' -or $unlock.Size -lt 1GB -or $uv.FileSystem -ne 'FAT32' -or !$uv.Path -or !$wv.Path){throw 'Use a separate existing FAT32 EFI partition, at least 1 GiB; never the Windows ESP'}
if(!(Test-Path -LiteralPath (Join-Path $wv.Path 'EFI\Microsoft\Boot\bootmgfw.efi'))){throw 'Windows Boot Manager missing from selected ESP'}
. (Join-Path $PSScriptRoot 'install/headless-package.ps1')
$hwName=if($HardwareProfile -eq 'b660m-3070ti'){'ASUS-TUF-GAMING-B660M-E-D4-4003-RTX3070TI'}else{'ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE'}
$identity=Get-HeadlessProfileIdentity $hwName
$board=Get-CimInstance Win32_BaseBoard;$bios=Get-CimInstance Win32_BIOS
if($board.Product -ne $identity.Board -or $bios.SMBIOSBIOSVersion -ne $identity.Bios){throw ('This driver profile is restricted to '+$identity.Board+' BIOS '+$identity.Bios)}
function GuidC([guid]$Value){
 $b=$Value.ToByteArray();$tail=($b[8..15] | ForEach-Object {'0x{0:x2}' -f $_}) -join ','
 return ('{{0x{0:x8},0x{1:x4},0x{2:x4},{{{3}}}}}' -f [BitConverter]::ToUInt32($b,0),[BitConverter]::ToUInt16($b,4),[BitConverter]::ToUInt16($b,6),$tail)
}
$header=@('/* Local partition configuration; never distribute this file. */',('#define GU_INTERNAL_PARTITION_INITIALIZER '+(GuidC ([guid]$unlock.Guid))),('#define GU_WINDOWS_PARTITION_INITIALIZER '+(GuidC $WindowsEspGuid)))
$header | Set-Content (Join-Path $PSScriptRoot 'boot/platform-config.h') -Encoding ascii
$disk=Get-Disk -Number $unlock.DiskNumber
[ordered]@{UnlockDriveLetter=$UnlockDriveLetter.ToUpperInvariant();UnlockPartitionGuid=[string]$unlock.Guid;WindowsPartitionGuid=[string]$windows[0].Guid;UnlockDiskUniqueId=$disk.UniqueId;UnlockOffset=$unlock.Offset;UnlockSize=$unlock.Size;GeneratedHeaderSha256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'boot/platform-config.h')).Hash;ConfiguredUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content (Join-Path $PSScriptRoot 'local-config.json') -Encoding utf8
Write-Output 'Local profile configured. No disk layout, EFI files or boot settings changed.'
