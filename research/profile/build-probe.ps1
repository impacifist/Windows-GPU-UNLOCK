# ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md.
# GPL-2.0-only. Build the profile-less read-only USB probe (-DGU_PROBE_GENERIC) for a new machine.
# The probe records every NVIDIA display controller, the console/driver graph, the 1 MiB PROM
# (PROM.BIN) and loaded driver images (IMGnn.BIN) to the stick it booted from, then starts
# Windows. It compiles only as a diagnostic build: no unlock code, GPU writes, driver release,
# DMA or firmware variable writes. Builds files only.
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[string]$Clang='clang',[string]$LldLink='lld-link',[string]$Python='python',
      [ValidateRange(1,255)][int]$TargetBus,[switch]$Asrock3060)
$ErrorActionPreference='Stop'
if($Asrock3060 -and (!$PSBoundParameters.ContainsKey('TargetBus') -or $TargetBus -ne 7)){throw 'ASRock diagnostic requires PCI bus 7'}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$boot=Join-Path $repo 'internal-boot/boot'
function GuidC([guid]$Value){
 $b=$Value.ToByteArray();$tail=($b[8..15] | ForEach-Object {'0x{0:x2}' -f $_}) -join ','
 return ('{{0x{0:x8},0x{1:x4},0x{2:x4},{{{3}}}}}' -f [BitConverter]::ToUInt32($b,0),[BitConverter]::ToUInt16($b,4),[BitConverter]::ToUInt16($b,6),$tail)
}
$windows=@(Get-Partition | Where-Object {$_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($windows.Count -ne 1 -or !$windows[0].IsSystem){throw 'WindowsEspGuid must identify the current system EFI partition'}
# Same local USB configuration as build-usb.ps1, so the two builds share it.
$config=Join-Path $boot 'platform-config.h';$marker='/* Local USB configuration (build-usb.ps1). Internal partition unused under GU_USB_MEDIA. */'
if((Test-Path -LiteralPath $config) -and (Get-Content -LiteralPath $config -TotalCount 1) -ne $marker){throw 'platform-config.h belongs to an internal Configure.ps1 setup; move it aside before a USB build'}
@($marker,('#define GU_INTERNAL_PARTITION_INITIALIZER '+(GuidC ([guid]::Empty))),('#define GU_WINDOWS_PARTITION_INITIALIZER '+(GuidC $WindowsEspGuid))) | Set-Content -LiteralPath $config -Encoding ascii
$generated=Join-Path $boot 'generated.h'
if(!(Test-Path -LiteralPath $generated)){'/* Observation build: GU_DIAGNOSTIC_ONLY excludes all unlock loading. */' | Set-Content -LiteralPath $generated -Encoding ascii}
Push-Location -LiteralPath $boot
try {
 $out='build/probe-generic'
 New-Item -ItemType Directory -Path $out -Force | Out-Null
 $sources=@('handoff.c','diagnostic.c','../efi_runtime.c','probe.c','../unlock/sha256.c')
 $defines=@('-DGU_DIAGNOSTIC_ONLY','-DGU_DIAGNOSTIC_PROBE','-DGU_PROBE_GENERIC','-DGU_USB_PROBE')
 if($Asrock3060){$defines+=@('-DGU_PROBE_ASROCK_3060','-DGU_PROBE_DUMP_PROM','-DGU_USB_MEDIA')}
 $busMode='auto'
 if($PSBoundParameters.ContainsKey('TargetBus')){
  $defines+=('-DGU_PROBE_BUS=0x{0:x}U' -f $TargetBus);$busMode='explicit'
 } else {$defines+='-DGU_PROBE_AUTO_BUS'}
 $objects=@()
 foreach($source in $sources) {
  $obj="$out/"+[IO.Path]::GetFileNameWithoutExtension($source)+'.obj'
  & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror @defines -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 -c $source -o $obj
  if($LASTEXITCODE -ne 0){throw "Compile failed: $source"}
  $objects+=$obj
 }
 & $LldLink /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf "/map:$out/PROBE.map" "/out:$out/BOOTX64.EFI" @objects
 if($LASTEXITCODE -ne 0){throw 'Probe link failed'}
 & $Python -I ../unlock/verify_pe.py "$out/BOOTX64.EFI" "$out/PROBE.map" --output "$out/pe-verification.json"
 if($LASTEXITCODE -ne 0){throw 'Probe PE verification failed'}
 [ordered]@{Mode='generic-read-only-probe-then-windows-usb';CanUnlock=$false;CanDetach=$false;WritesFirmwareVariables=$false;HardwareTested=$false;Defines=$defines
  ProbeBusMode=$busMode;TargetBus=$(if($busMode -eq 'explicit'){$TargetBus}else{$null})
  EfiSha256=(Get-FileHash -LiteralPath "$out/BOOTX64.EFI").Hash.ToLowerInvariant();BuiltUtc=[DateTime]::UtcNow.ToString('o')
  Sources=@(($sources+@('diagnostic.h','media.h','probe.h','asrock-probe.h','../unlock/machine-profile.h','../unlock/gpu-family.h','../unlock/firmware-profile.h')) | ForEach-Object {[ordered]@{Path=$_;Sha256=(Get-FileHash -LiteralPath $_).Hash.ToLowerInvariant()}})
 } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$out/manifest.json" -Encoding utf8
 "Built generic read-only probe $out/BOOTX64.EFI; hardware untested."
} finally {Pop-Location}
