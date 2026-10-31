# Delete only the reviewed SULK02 temporary option; never restores or writes BootOrder.
[CmdletBinding(SupportsShouldProcess)]
param([string]$StateDirectory='E:\UNLOCK\ESP-SECOND02')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-firmware-lib.ps1')
. (Join-Path $PSScriptRoot 'shared-esp-second-arm-lib.ps1')
. (Join-Path $PSScriptRoot 'shared-esp-second-cleanup-lib.ps1')
$dir=[IO.Path]::GetFullPath($StateDirectory)
if($dir -notmatch '^[A-Za-z]:\\'){throw 'Expected local recovery directory'}
$part=@(Get-Partition -DriveLetter $dir.Substring(0,1))
if($part.Count -ne 1 -or ($part[0].Guid -and [guid]$part[0].Guid -eq [guid]'c36c3e86-02aa-4f76-af1b-96e327f9d1c1')){throw 'Recovery snapshot must be outside the ESP'}
for($p=$dir;$p;){if(([IO.File]::GetAttributes($p) -band [IO.FileAttributes]::ReparsePoint)){throw 'Recovery path contains a reparse point'};$parent=[IO.Path]::GetDirectoryName($p.TrimEnd([char[]]'\/'));if($parent -eq $p){break};$p=$parent}
$backup=Join-Path $dir 'firmware-backup.json'
$state=Get-Content -LiteralPath $backup -Raw | ConvertFrom-Json
if($state.Schema -cne 'shared-esp-second-register-v1' -or $state.OptionName -cne 'Boot7E00' -or $state.OptionNumber -ne 32256 -or $state.Option.Attributes -ne 7 -or $null -ne $state.BootNext){throw 'Unexpected SULK02 backup'}
$decoded=Read-SharedEspLoadOption $state.Option
if($decoded.Path -cne '\EFI\EVGA-SHARED-LAUNCH02.EFI' -or $decoded.PartitionGuid -ne [guid]'c36c3e86-02aa-4f76-af1b-96e327f9d1c1' -or !$decoded.Active){throw 'Unexpected SULK02 option'}
$record=Join-Path $dir 'cleanup-sulk02.json'
if(Test-Path -LiteralPath $record){throw 'Cleanup already has state; preserve it and report rather than retry'}
$backupHash=(Get-FileHash -LiteralPath $backup).Hash
if(-not ('SharedEspFirmware.Native' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'shared-esp-firmware-native.cs')}
$scope=[SharedEspFirmware.Native]::EnablePrivilege()
try {
 $read={param($name) $v=[SharedEspFirmware.Native]::Read($name);if($v){ConvertTo-FirmwareValue $v.Data $v.Attributes}}
 $write={param($name,$value) if($name -cne 'Boot7E00' -or $null -ne $value){throw 'Unexpected cleanup write'};[SharedEspFirmware.Native]::Write($name,$null,[uint32]$state.Option.Attributes)}
 $save={param($snapshot)
  if((Get-FileHash -LiteralPath $backup).Hash -ine $backupHash){throw 'Original backup changed; no deletion attempted'}
  Write-SharedSecondNewFile $record ([Text.Encoding]::UTF8.GetBytes(($snapshot | ConvertTo-Json -Depth 8)))
 }
 if($PSCmdlet.ShouldProcess('Boot7E00','Save current state, then delete only the exact SULK02 temporary option; keep current BootOrder and all evidence')){
  $result=Remove-ReviewedSharedSecondOption $state $read $write $save
  $result['next']='Cleanup finished. Send this JSON; no reboot or further test is required by this command. Keep all backups and evidence.'
  $result | ConvertTo-Json -Depth 5
 }
}finally{$scope.Dispose()}
