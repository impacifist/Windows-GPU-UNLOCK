# GPL-2.0-only. Preserve captures and the previous USB EFI; never format or reboot.
[CmdletBinding(SupportsShouldProcess)]
param([Parameter(Mandatory)][ValidatePattern('^[A-Za-z]$')][string]$Drive,[switch]$AttemptUnlock)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $repo 'internal-boot/boot/build/evga-text-stop'
$source=Join-Path $build 'BOOTX64.EFI'
$manifest=Get-Content -LiteralPath (Join-Path $build 'manifest.json') -Raw | ConvertFrom-Json
if($manifest.Mode -notin @('evga3090-one-text-membership-stop','evga3090-two-text-stops','evga3090-three-consumer-stops','evga3090-four-consumer-stops','evga3090-six-consumer-stops','evga3090-nvidia-preflight','evga3090-nvidia-three-child-stop','evga3090-nvidia-parent-stop','evga3090-decode-probe','evga3090-state-probe','evga3090-platform-probe','evga3090-resources-probe','evga3090-experimental-unlock')){throw 'Wrong build mode'}
if(($manifest.Mode -eq 'evga3090-experimental-unlock') -ne [bool]$AttemptUnlock -or [bool]$manifest.CanUnlock -ne [bool]$AttemptUnlock){throw 'Explicit -AttemptUnlock must match the manifest unlock mode'}
$hash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
if($hash -cne $manifest.EfiSha256){throw 'EFI differs from build manifest; rebuild'}
$boot=Join-Path $repo 'internal-boot/boot'
foreach($pin in $manifest.Sources){
 $path=[IO.Path]::GetFullPath((Join-Path $boot $pin.Path))
 if(!$path.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Source pin outside repository'}
 if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pin.Sha256){throw "Source changed: $($pin.Path); rebuild"}
}
$system=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq [guid]$manifest.WindowsEspGuid})
if($system.Count -ne 1){throw 'Build Windows ESP pin does not match this machine'}
$letter=$Drive.ToUpperInvariant()
$volume=Get-Volume -DriveLetter $letter
$partition=@(Get-Partition -DriveLetter $letter)
if($volume.FileSystem -ne 'FAT32' -or $partition.Count -ne 1){throw 'Expected one FAT32 USB partition'}
$disk=Get-Disk -Number $partition[0].DiskNumber
if($disk.BusType -ne 'USB' -or $partition[0].IsSystem -or $partition[0].IsBoot){throw 'Target must be a USB data volume, not a Windows boot/system partition'}
$root=$letter+':\'
$captures=Join-Path $root 'RTX3090-UNLOCK'
if(!(Test-Path -LiteralPath (Join-Path $captures 'DIAG00.TXT')) -or !(Test-Path -LiteralPath (Join-Path $captures 'DIAG01.TXT'))){throw 'Original capture pair must remain on the USB'}
$attempt=if($manifest.Mode -eq 'evga3090-experimental-unlock'){'EVGA-UNLOCK.ARMED'}elseif($manifest.Mode -eq 'evga3090-resources-probe'){'EVGA-RESOURCES-PROBE.ARMED'}elseif($manifest.Mode -eq 'evga3090-platform-probe'){'EVGA-PLATFORM-PROBE.ARMED'}elseif($manifest.Mode -in @('evga3090-state-probe','evga3090-platform-probe','evga3090-resources-probe','evga3090-experimental-unlock')){'EVGA-STATE-PROBE.ARMED'}elseif($manifest.Mode -in @('evga3090-decode-probe','evga3090-state-probe','evga3090-platform-probe','evga3090-resources-probe','evga3090-experimental-unlock')){'EVGA-DECODE-PROBE.ARMED'}elseif($manifest.Mode -in @('evga3090-nvidia-parent-stop','evga3090-decode-probe','evga3090-state-probe','evga3090-platform-probe','evga3090-resources-probe','evga3090-experimental-unlock')){'EVGA-NVIDIA-PARENT.ARMED'}elseif($manifest.Mode -eq 'evga3090-nvidia-three-child-stop'){'EVGA-NVIDIA-CHILDREN.ARMED'}elseif($manifest.Mode -eq 'evga3090-nvidia-preflight'){'EVGA-NVIDIA-AUDIT.ARMED'}elseif($manifest.Mode -eq 'evga3090-six-consumer-stops'){'EVGA-EDID-SIBLINGS.ARMED'}elseif($manifest.Mode -eq 'evga3090-four-consumer-stops'){'EVGA-EDID-RELEASE.ARMED'}elseif($manifest.Mode -eq 'evga3090-three-consumer-stops'){'EVGA-GFX-RELEASE.ARMED'}elseif($manifest.Mode -eq 'evga3090-two-text-stops'){'EVGA-TEXT-RELEASE.ARMED'}else{'EVGA-TEXT-STOP.ARMED'}
if($AttemptUnlock){
 $prior=Join-Path $captures 'EVGA17.TXT'
 if(!(Test-Path -LiteralPath (Join-Path $captures 'EVGA-UNLOCK.ARMED')) -or !(Test-Path -LiteralPath $prior) -or (Get-FileHash -LiteralPath $prior -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'fb0dd12b5eacc5bd7e289be04827aa1b15f9ce94bf768e63fabcd98b39bedb46'){throw 'Expected original unlock marker and reviewed EVGA17; preserve evidence'}
 $second=Join-Path $captures 'EVGA18.TXT'
 if(!(Test-Path -LiteralPath (Join-Path $captures 'EVGA-UNLOCK2.ARMED')) -or !(Test-Path -LiteralPath $second) -or (Get-FileHash -LiteralPath $second -Algorithm SHA256).Hash.ToLowerInvariant() -ne '7f2c070f80d680d49997ee990b99aefbe1040707db2a40f59bd5a16aa43244a4'){throw 'Expected second unlock marker and reviewed EVGA18; preserve evidence'}
 $third=Join-Path $captures 'EVGA19.TXT'
 if(!(Test-Path -LiteralPath (Join-Path $captures 'EVGA-UNLOCK3.ARMED')) -or !(Test-Path -LiteralPath $third) -or (Get-FileHash -LiteralPath $third -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'dfd6a0e0e58b8bc794e87a6c1b0e774ac674d5b483985ba36439b53fbc597537'){throw 'Expected third unlock marker and reviewed EVGA19; preserve evidence'}
 $slot=[int]$manifest.AttemptSlot
 if($slot -lt 4 -or $slot -gt 99){throw 'Attempt slot is invalid'}
 if($slot -ge 5){
  $fourth=Join-Path $captures 'EVGA20.TXT'
  if(!(Test-Path -LiteralPath (Join-Path $captures 'EVGA-UNLOCK4.ARMED')) -or !(Test-Path -LiteralPath $fourth) -or (Get-FileHash -LiteralPath $fourth -Algorithm SHA256).Hash.ToLowerInvariant() -ne '0d793ac0913e13f1aa786026743aa33ddcb6b5fe79fe0975a1269b5c378b9053'){throw 'Expected fourth unlock marker and reviewed EVGA20; preserve evidence'}
 }
 if($slot -ge 6){
  $previous=$slot-1
  if(!(Test-Path -LiteralPath (Join-Path $captures "EVGA-UNLOCK$previous.ARMED"))){throw 'Previous attempt marker missing; preserve evidence'}
  $logs=@(Get-ChildItem -LiteralPath $captures -File -Filter 'EVGA*.TXT' | Where-Object {$_.Name -match '^EVGA\d\d\.TXT$'} | Sort-Object Name)
  if(!$logs.Count){throw 'Previous attempt log missing; preserve evidence'}
  $last=Get-Content -LiteralPath $logs[-1].FullName -Raw
  $slotEvent=('evga-unlock-slot-{0:D2} status=0x0000000000000000' -f $previous)
  if(!$last.Contains($slotEvent) -or !$last.Contains('evga-unlock-transaction-return status=0x0000000000000000') -or !$last.Contains('evga-unlock-assets-released status=0x0000000000000000') -or !$last.Contains('evga-text-stop-ready-windows status=0x0000000000000000') -or ([regex]::Matches($last,'capture-complete=1').Count -lt 2)){throw 'Previous attempt did not record complete successful cleanup; preserve evidence'}
 }
 for($later=$slot+1;$later -le 99;$later++){if(Test-Path -LiteralPath (Join-Path $captures "EVGA-UNLOCK$later.ARMED")){throw 'Later attempt marker exists; preserve evidence'}}
 $attempt="EVGA-UNLOCK$slot.ARMED"
}
if(Test-Path -LiteralPath (Join-Path $captures $attempt)){throw 'An attempt is already armed; preserve evidence instead of repeating'}
if($manifest.Mode -in @('evga3090-two-text-stops','evga3090-three-consumer-stops','evga3090-four-consumer-stops','evga3090-six-consumer-stops','evga3090-nvidia-preflight','evga3090-nvidia-three-child-stop','evga3090-nvidia-parent-stop','evga3090-decode-probe','evga3090-state-probe','evga3090-platform-probe','evga3090-resources-probe','evga3090-experimental-unlock')) {
 $baseline=Join-Path $captures 'EVGA05.TXT'
 if(!(Test-Path -LiteralPath $baseline) -or (Get-FileHash -LiteralPath $baseline -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'cbbfad6aadb858cdc6311f656c819e6707e52f274e8e0959df4348c51fbbec1a'){throw 'Expected reviewed EVGA05 baseline; preserve captures and send the error'}
}
if($manifest.Mode -in @('evga3090-three-consumer-stops','evga3090-four-consumer-stops','evga3090-six-consumer-stops','evga3090-nvidia-preflight','evga3090-nvidia-three-child-stop','evga3090-nvidia-parent-stop','evga3090-decode-probe','evga3090-state-probe','evga3090-platform-probe','evga3090-resources-probe','evga3090-experimental-unlock')) {
 $baseline=Join-Path $captures 'EVGA06.TXT'
 if(!(Test-Path -LiteralPath $baseline) -or (Get-FileHash -LiteralPath $baseline -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'cf7fb3897910da7f05d17cc08a70d8595de89398bea85f5e287e9e3c64c5d89f'){throw 'Expected reviewed EVGA06 baseline; preserve captures and send the error'}
}
if($manifest.Mode -in @('evga3090-four-consumer-stops','evga3090-six-consumer-stops','evga3090-nvidia-preflight','evga3090-nvidia-three-child-stop','evga3090-nvidia-parent-stop','evga3090-decode-probe','evga3090-state-probe','evga3090-platform-probe','evga3090-resources-probe','evga3090-experimental-unlock')) {
 $baseline=Join-Path $captures 'EVGA07.TXT'
 if(!(Test-Path -LiteralPath $baseline) -or (Get-FileHash -LiteralPath $baseline -Algorithm SHA256).Hash.ToLowerInvariant() -ne '4fd72a6ad44025d31909900acde4db086ca56c7d678084df258c585936c42818'){throw 'Expected reviewed EVGA07 baseline; preserve captures and send the error'}
}
if($manifest.Mode -in @('evga3090-six-consumer-stops','evga3090-nvidia-preflight','evga3090-nvidia-three-child-stop','evga3090-nvidia-parent-stop','evga3090-decode-probe','evga3090-state-probe','evga3090-platform-probe','evga3090-resources-probe','evga3090-experimental-unlock')) {
 $baseline=Join-Path $captures 'EVGA08.TXT'
 if(!(Test-Path -LiteralPath $baseline) -or (Get-FileHash -LiteralPath $baseline -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'a7f683fc21550e1ebaa381119ab2feba908f37a0dbefbcc27a3f50f973d03747'){throw 'Expected reviewed EVGA08 baseline; preserve captures and send the error'}
}
if($manifest.Mode -in @('evga3090-nvidia-preflight','evga3090-nvidia-three-child-stop','evga3090-nvidia-parent-stop','evga3090-decode-probe','evga3090-state-probe','evga3090-platform-probe','evga3090-resources-probe','evga3090-experimental-unlock')) {
 $baseline=Join-Path $captures 'EVGA09.TXT'
 if(!(Test-Path -LiteralPath $baseline) -or (Get-FileHash -LiteralPath $baseline -Algorithm SHA256).Hash.ToLowerInvariant() -ne '0385147bd6396028fb9669877b33494f42df76a31b7123ca0294515da8e2c6d1'){throw 'Expected reviewed EVGA09 baseline; preserve captures and send the error'}
}
if($manifest.Mode -in @('evga3090-nvidia-three-child-stop','evga3090-nvidia-parent-stop','evga3090-decode-probe','evga3090-state-probe','evga3090-platform-probe','evga3090-resources-probe','evga3090-experimental-unlock')) {
 $baseline=Join-Path $captures 'EVGA10.TXT'
 if(!(Test-Path -LiteralPath $baseline) -or (Get-FileHash -LiteralPath $baseline -Algorithm SHA256).Hash.ToLowerInvariant() -ne '16083085694eb60ee6ca2bf2522b9297b35e989efcf3582c57beccbcc51c7c83'){throw 'Expected reviewed EVGA10 baseline; preserve captures and send the error'}
}
if($manifest.Mode -in @('evga3090-nvidia-parent-stop','evga3090-decode-probe','evga3090-state-probe','evga3090-platform-probe','evga3090-resources-probe','evga3090-experimental-unlock')) {
 $baseline=Join-Path $captures 'EVGA11.TXT'
 if(!(Test-Path -LiteralPath $baseline) -or (Get-FileHash -LiteralPath $baseline -Algorithm SHA256).Hash.ToLowerInvariant() -ne '927fa507875f37b43d067fe7aa2848b0c635d8abbd9a059803371bf44cdee240'){throw 'Expected reviewed EVGA11 baseline; preserve captures and send the error'}
}
if($manifest.Mode -in @('evga3090-decode-probe','evga3090-state-probe','evga3090-platform-probe','evga3090-resources-probe','evga3090-experimental-unlock')) {
 $baseline=Join-Path $captures 'EVGA12.TXT'
 if(!(Test-Path -LiteralPath $baseline) -or (Get-FileHash -LiteralPath $baseline -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'cafbcb9a6fd0b20ba1eb68502e6cfc464c28bf1bee1e5cb72fa65b712fb46869'){throw 'Expected reviewed EVGA12 baseline; preserve evidence'}
}
if($manifest.Mode -in @('evga3090-state-probe','evga3090-platform-probe','evga3090-resources-probe','evga3090-experimental-unlock')) {
 $baseline=Join-Path $captures 'EVGA13.TXT'
 if(!(Test-Path -LiteralPath $baseline) -or (Get-FileHash -LiteralPath $baseline -Algorithm SHA256).Hash.ToLowerInvariant() -ne '4c381605ffb0a25e07414388758d6bb2cb542b4ab4e04e34610d8fd4a9ec3864'){throw 'Expected reviewed EVGA13 baseline; preserve evidence'}
}
if($manifest.Mode -in @('evga3090-platform-probe','evga3090-resources-probe','evga3090-experimental-unlock')) {
 $baseline=Join-Path $captures 'EVGA14.TXT'
 if(!(Test-Path -LiteralPath $baseline) -or (Get-FileHash -LiteralPath $baseline -Algorithm SHA256).Hash.ToLowerInvariant() -ne '2c6b400998dae1d7d77d943b0d2a5cf7c410507d633a58000d269aebedede24c'){throw 'Expected reviewed EVGA14 baseline; preserve evidence'}
}
$assetCopies=@()
if($manifest.Mode -in @('evga3090-resources-probe','evga3090-experimental-unlock')) {
 $baseline=Join-Path $captures 'EVGA15.TXT'
 if(!(Test-Path -LiteralPath $baseline) -or (Get-FileHash -LiteralPath $baseline -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'de5721c91a1a84f81f7d9a06279c654d5660ddc198f861bb53b418b2f37ea372'){throw 'Expected reviewed EVGA15; preserve evidence'}
 if($manifest.Mode -eq 'evga3090-experimental-unlock') {
  $latest=Join-Path $captures 'EVGA16.TXT'
  if(!(Test-Path -LiteralPath $latest) -or (Get-FileHash -LiteralPath $latest -Algorithm SHA256).Hash.ToLowerInvariant() -ne '2537c7c9b15911c69a91770467e1e161ffd7d9f362485faf9ac6645fcfd9e01e'){throw 'Expected reviewed EVGA16; preserve evidence'}
 }
 $assetSource=Join-Path $repo 'research/profile/build/evga-assets/output'
 foreach($asset in (Get-Content -Raw -LiteralPath (Join-Path $repo 'internal-boot/unlock/assets.json') | ConvertFrom-Json)) {
  $src=Join-Path $assetSource $asset.Name;$dst=Join-Path $captures ('ASSETS/'+$asset.Name)
  if(!(Test-Path -LiteralPath $src) -or (Get-Item -LiteralPath $src).Length -ne $asset.Bytes -or (Get-FileHash -LiteralPath $src).Hash.ToLowerInvariant() -ne $asset.Sha256){throw "Asset source mismatch: $($asset.Name)"}
  if(Test-Path -LiteralPath $dst){if((Get-Item -LiteralPath $dst).Length -ne $asset.Bytes -or (Get-FileHash -LiteralPath $dst).Hash.ToLowerInvariant() -ne $asset.Sha256){throw 'Existing USB asset mismatch; preserve and report'}}
  else {$assetCopies+=@{Source=$src;Destination=$dst;Hash=$asset.Sha256}}
 }
 foreach($name in @('NVIDIA-LICENSE','NOTICE.md')) {
  $src=Join-Path $assetSource $name;$dst=Join-Path $captures ('ASSETS/'+$name)
  if(!(Test-Path -LiteralPath $src)){throw "Missing $name"}
  $expected=(Get-FileHash -LiteralPath $src).Hash
  if(Test-Path -LiteralPath $dst){if((Get-FileHash -LiteralPath $dst).Hash -ne $expected){throw 'Existing license/notice differs; preserve and report'}}
  else {$assetCopies+=@{Source=$src;Destination=$dst;Hash=$expected}}
 }
}
$destination=Join-Path $root 'EFI/BOOT/BOOTX64.EFI'
if(!(Test-Path -LiteralPath $destination)){throw 'Expected the existing capture EFI on this USB'}
$backup=Join-Path $root 'EFI/BOOT/BOOTX64.before-evga-text-stop.efi'
if(Test-Path -LiteralPath $backup){
 $found=$false
 for($i=1;$i -le 99;$i++){
  $candidate=Join-Path $root ('EFI/BOOT/BOOTX64.before-evga-text-stop.{0:D2}.efi' -f $i)
  if(!(Test-Path -LiteralPath $candidate)){$backup=$candidate;$found=$true;break}
 }
 if(!$found){throw 'All backup slots are occupied; preserve them and review USB state'}
}
$description=if($AttemptUnlock){'Back up existing EFI and stage the experimental GPU unlock attempt (writes, DMA, FLR)'}else{'Back up existing EFI and stage the selected EVGA diagnostic'}
if($PSCmdlet.ShouldProcess($destination,$description)) {
 foreach($copy in $assetCopies){
  New-Item -ItemType Directory -Path (Split-Path -Parent $copy.Destination) -Force | Out-Null
  Copy-Item -LiteralPath $copy.Source -Destination $copy.Destination
  if((Get-FileHash -LiteralPath $copy.Destination).Hash.ToLowerInvariant() -ne $copy.Hash.ToLowerInvariant()){throw 'USB asset copy hash mismatch; do not boot'}
 }
 Copy-Item -LiteralPath $destination -Destination $backup
 if((Get-FileHash -LiteralPath $destination).Hash -cne (Get-FileHash -LiteralPath $backup).Hash){throw 'Backup verification failed'}
 Copy-Item -LiteralPath $source -Destination $destination -Force
 if((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant() -cne $hash){throw 'Staged EFI hash mismatch; do not boot'}
 "Staged $destination ($hash). Previous capture EFI saved as $backup. Captures preserved. No reboot performed."
}
