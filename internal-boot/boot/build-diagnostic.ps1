param([string]$Clang='clang',[string]$LldLink='lld-link',[string]$Python='python',
 [ValidateSet('b650m-3090fe','b660m-3070ti','b760m-3080lhr','x870-3090ti','b660plus-3060','h610m-3060','probe-h610m-3060')][string]$HardwareProfile='b650m-3090fe',[switch]$UsbMedia,[switch]$DumpProm)
$profiles=@{'b650m-3090fe'=@();'b660m-3070ti'=@('-DGU_PROFILE_B660M_3070TI');'b760m-3080lhr'=@('-DGU_PROFILE_B760M_3080LHR');'x870-3090ti'=@('-DGU_PROFILE_X870_3090TI');'b660plus-3060'=@('-DGU_PROFILE_B660PLUS_3060');'h610m-3060'=@('-DGU_PROFILE_H610M_3060');'probe-h610m-3060'=@('-DGU_PROFILE_PROBE_H610M_3060')}
$defines=@($profiles[$HardwareProfile]);if($UsbMedia){$defines+='-DGU_USB_MEDIA'}
if($DumpProm){if(!$UsbMedia){throw '-DumpProm writes the PROM to the observation stick; it requires -UsbMedia'};$defines+='-DGU_PROBE_DUMP_PROM'}
$ErrorActionPreference='Stop'
$HardwareProfile=$HardwareProfile.ToLowerInvariant()
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $repo 'research/headless/observe-manifest.ps1')
$state=if($UsbMedia){Get-ObservationState $repo}else{$null}
Push-Location -LiteralPath $PSScriptRoot
try {
 New-Item -ItemType Directory -Path build/diagnostic -Force | Out-Null
 $sources=@('handoff.c','diagnostic.c','../efi_runtime.c','probe.c','../unlock/sha256.c')
 $objects=@()
 foreach($source in $sources) {
  $obj='build/diagnostic/'+[IO.Path]::GetFileNameWithoutExtension($source)+'.obj'
  & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -DGU_DIAGNOSTIC_ONLY -DGU_DIAGNOSTIC_PROBE @defines -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 -c $source -o $obj
  if($LASTEXITCODE -ne 0){throw "Compile failed: $source"}
  $objects+=$obj
 }
 & $LldLink /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /map:build/diagnostic/DIAGNOSTIC.map /out:build/diagnostic/DIAGNOSTIC.EFI @objects
 if($LASTEXITCODE -ne 0){throw 'Diagnostic link failed'}
 & $Python ../unlock/verify_pe.py build/diagnostic/DIAGNOSTIC.EFI build/diagnostic/DIAGNOSTIC.map --output build/diagnostic/pe-verification.json
 if($LASTEXITCODE -ne 0){throw 'Diagnostic PE verification failed'}
 $sourceManifest=@(($sources+@('diagnostic.h','media.h','probe.h','../unlock/machine-profile.h')) | ForEach-Object {
  [ordered]@{Path=$_;Sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash}
 })
 if($UsbMedia){
  $after=Get-ObservationState $repo
  if($state.TreeSha256 -cne $after.TreeSha256 -or $state.PlatformConfigSha256 -cne $after.PlatformConfigSha256){throw 'Observation inputs changed while compiling; rebuild'}
 }
 $manifest=[ordered]@{
  Format=1
  HardwareTested=$false;Mode='graph-capture-then-normal-windows';CanUnlock=$false
  CanDetach=$false;WritesFirmwareVariables=$false
  HardwareProfile=$HardwareProfile;UsbMedia=[bool]$UsbMedia;DumpsProm=[bool]$DumpProm;Defines=$defines
  EfiSha256=(Get-FileHash -LiteralPath build/diagnostic/DIAGNOSTIC.EFI -Algorithm SHA256).Hash.ToLowerInvariant()
  BuiltUtc=[DateTime]::UtcNow.ToString('o');Sources=$sourceManifest
 }
 if($UsbMedia){$manifest.Inputs=$state.Inputs;$manifest.TreeSha256=$state.TreeSha256;$manifest.PlatformConfigSha256=$state.PlatformConfigSha256}
 $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath build/diagnostic/manifest.json -Encoding utf8
} finally {Pop-Location}
