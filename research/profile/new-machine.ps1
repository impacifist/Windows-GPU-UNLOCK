# GPL-2.0-only. One command for a new machine: probe -> profile -> test -> stick.
# Resumable: each run does the next step the stick and research/profile/local allow.
#   1. Too few probe captures on the stick: build and stage the read-only generic probe, then
#      stop. Boot the stick from the firmware boot menu after a full shutdown (twice), let
#      Windows start, and run this again.
#   2. Enough captures: copy them here, generate profile-local.h (refused outside
#      research/profile/proven.json), run test-profile.ps1, build the USB package.
#   3. With -StageUnlock only: copy that package and the assets to the stick.
# -CaptureOnly stops at collection, including on later runs with enough captures.
# Assets are required only when continuing to profile generation/host replay.
# Host execution changes files only: no partitioning, formatting, boot entries or firmware
# settings change. Booting the probe reads the GPU; booting unlock can mutate it. Neither is automatic.
param([Parameter(Mandatory)][ValidatePattern('^[D-Zd-z]$')][string]$Drive,[Parameter(Mandatory)][guid]$WindowsEspGuid,
 [string]$Assets,[switch]$StageUnlock,[switch]$CaptureOnly,
 [string]$Clang='clang',[string]$LldLink='lld-link',[string]$Python='python',[string]$VcVars,
 [ValidateRange(1,255)][int]$TargetBus)
$ErrorActionPreference='Stop'
if($CaptureOnly -and ($StageUnlock -or $Assets)){throw 'CaptureOnly cannot be combined with Assets or StageUnlock'}
if($StageUnlock -and !$Assets){throw 'StageUnlock requires -Assets; use -CaptureOnly for diagnostics without firmware assets'}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $repo 'internal-boot/unlock/local-profile.ps1')
. (Join-Path $PSScriptRoot 'host.ps1')
$part=Get-Partition -DriveLetter $Drive;$disk=Get-Disk -Number $part.DiskNumber;$vol=Get-Volume -DriveLetter $Drive
if($disk.BusType -ne 'USB' -or $vol.FileSystem -ne 'FAT32' -or $part.IsSystem -or $disk.IsBoot){throw "$($Drive): is not a FAT32 USB data stick"}
$usb="$($Drive.ToUpperInvariant()):";$dir=Join-Path $usb 'RTX3090-UNLOCK'
$need=(Get-Content -LiteralPath (Join-Path $PSScriptRoot 'proven.json') -Raw | ConvertFrom-Json).min_captures
function Captures {@(Get-ChildItem -LiteralPath $dir -Filter 'DIAG??.TXT' -File -ErrorAction SilentlyContinue | Where-Object {
 $t=[IO.File]::ReadAllText($_.FullName);$t.Contains('probe-pci-handles=') -and $t.Contains("capture-complete=1`r`n")})}

if(!(Test-Path -LiteralPath $dir) -or (Captures).Count -lt $need) {
 $probeOptions=@{}
 if($PSBoundParameters.ContainsKey('TargetBus')){$probeOptions.TargetBus=$TargetBus}
 & (Join-Path $PSScriptRoot 'build-probe.ps1') -WindowsEspGuid $WindowsEspGuid -Clang $Clang -LldLink $LldLink -Python $Python @probeOptions
 if(!$?){throw 'Probe build failed'}
 $m=Get-Content -LiteralPath (Join-Path $repo 'internal-boot/boot/build/probe-generic/manifest.json') -Raw | ConvertFrom-Json
 Assert-GuGenericProbeManifest $repo $m (Join-Path $repo 'internal-boot/boot/build/probe-generic/BOOTX64.EFI')
 $bootFile=Join-Path $usb 'EFI\BOOT\BOOTX64.EFI'
 $current=if(Test-Path -LiteralPath $bootFile){Get-GuSha256 $bootFile}else{''}
 if($current -ne $m.EfiSha256) {
  [void][IO.Directory]::CreateDirectory($dir)
  if($current){$bak=Join-Path $dir ('backup\BOOTX64-'+$current.Substring(0,12)+'.EFI');if(!(Test-Path -LiteralPath $bak)){[void][IO.Directory]::CreateDirectory((Split-Path $bak));Copy-Item -LiteralPath $bootFile -Destination $bak};"previous BOOTX64.EFI kept as $bak"}
  # Captures from another loader or BIOS must not mix with this probe's.
  $old=@(Get-ChildItem -LiteralPath $dir -File | Where-Object {$_.Name -match '^(DIAG\d\d\.TXT|IMG\d\d\.BIN|PROM\.BIN)$'})
  if($old.Count){$keep=Join-Path $dir ('old-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss'));[void][IO.Directory]::CreateDirectory($keep);$old | Move-Item -Destination $keep;"earlier capture files moved to $keep"}
  [void][IO.Directory]::CreateDirectory((Split-Path $bootFile))
  Assert-GuGenericProbeManifest $repo $m (Join-Path $repo 'internal-boot/boot/build/probe-generic/BOOTX64.EFI')
  Copy-Item -LiteralPath (Join-Path $repo 'internal-boot/boot/build/probe-generic/BOOTX64.EFI') -Destination $bootFile -Force
  if((Get-GuSha256 $bootFile) -ne $m.EfiSha256){throw 'Probe copy verification failed'}
  'READ-ONLY PROBE (research/profile/build-probe.ps1). Logs DIAGnn.TXT, PROM.BIN and IMGnn.BIN here, then starts Windows.' | Set-Content -LiteralPath (Join-Path $dir 'README.TXT') -Encoding ascii
  "Staged the read-only generic probe on $usb ($($m.EfiSha256))."
 }
 $have=(Captures).Count
 "Probe captures on the stick: $have of $need. Shut down fully (not Restart), boot the stick once from the firmware boot menu,"
 "let Windows start, and repeat until there are $need; then run this command again."
 exit 0
}

if($CaptureOnly) {
 "Capture-only complete: $((Captures).Count) complete logs found in $dir; hardware compatibility is not assessed."
 'No profile generation, asset replay, unlock package build or unlock staging was attempted. Keep the original files for offline review.'
 exit 0
}
if(!$Assets){throw 'Captures are ready. Use -CaptureOnly to stop at collection, or supply -Assets to explicitly continue to profile generation and host replay'}

$selectedCaptures=@(Captures)
if($PSBoundParameters.ContainsKey('TargetBus')) {
 Assert-GuProbeTargetBus @($selectedCaptures | ForEach-Object {$_.FullName}) $TargetBus
}
"Using $($selectedCaptures.Count) probe captures from $dir."
$capture=Join-Path $repo 'research/profile/local/capture'
if(Test-Path -LiteralPath $capture) {
 # Preserve original captures when resuming; never recursively delete hardware evidence.
 $sourceItem=Get-Item -LiteralPath $capture -Force
 $parent=[IO.Path]::GetFullPath((Join-Path $repo 'research/profile/local'))
 if(!$sourceItem.PSIsContainer -or ($sourceItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
    [IO.Path]::GetDirectoryName($sourceItem.FullName) -ne $parent){throw 'Capture archive source is not the expected local directory'}
 $archive=Join-Path $parent ('capture-archive-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N'))
 if([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($archive)) -ne $parent){throw 'Invalid capture archive destination'}
 Move-Item -LiteralPath $sourceItem.FullName -Destination $archive
 "Previous local captures preserved in $archive"
}
[void][IO.Directory]::CreateDirectory($capture)
# Complete probe captures only: an interrupted boot's partial log is left on the stick.
Captures | Copy-Item -Destination $capture
Get-ChildItem -LiteralPath $dir -File | Where-Object {$_.Name -match '^(IMG\d\d\.BIN|PROM\.BIN)$'} | Copy-Item -Destination $capture
Import-GuHostCompiler $VcVars
$locator=Build-GuFwsecLocate $repo $Clang
& $Python -I (Join-Path $PSScriptRoot 'make-profile.py') $capture $locator
if($LASTEXITCODE -ne 0){throw 'No local profile was generated (see the refusal above); this machine is outside the proven envelope or its captures disagree'}
& (Join-Path $PSScriptRoot 'test-profile.ps1') -Assets $Assets -Clang $Clang -Python $Python -VcVars $VcVars
if(!$?){throw 'Local profile host tests failed'}
& (Join-Path $repo 'research/headless/build-usb.ps1') -WindowsEspGuid $WindowsEspGuid -HardwareProfile local -Clang $Clang -LldLink $LldLink -Python $Python
if(!$?){throw 'USB package build failed'}
if(!$StageUnlock) {
 "Host tests passed and the USB unlock package is built for $((Get-LocalProfile $repo).Name). Nothing was staged."
 'This machine has no hardware evidence yet. Read docs/NEW-MACHINE.en.md, then rerun with -StageUnlock to copy it to the stick.'
 exit 0
}
& (Join-Path $repo 'research/headless/stage-usb.ps1') -Drive $Drive -Assets $Assets
if(!$?){throw 'Staging failed'}
