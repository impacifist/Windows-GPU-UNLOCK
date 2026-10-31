# GPL-2.0-only. Compile only. Does not stage USB, change boot configuration or execute EFI.
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[string]$Clang='clang',[string]$LldLink='lld-link',[string]$Python='python')
$ProbeResources=$true;$AttemptUnlock=$false;$AttemptSlot=4
$ErrorActionPreference='Stop'
if($AttemptUnlock){$ProbeResources=$true}
if($ProbeResources){$ProbePlatform=$true}
if($ProbePlatform){$ProbeState=$true}
if($ProbeState){$ProbeDecode=$true}
if($ProbeDecode){$StopNvidiaParent=$true}
if($StopNvidiaParent){$StopNvidiaChildren=$true}
if($StopNvidiaChildren){$AuditNvidia=$true}
if($AuditNvidia){$ReleaseEdidSiblings=$true}
if($ReleaseEdidSiblings){$ReleaseEdid=$true}
if($ReleaseEdid){$ReleaseGraphics=$true}
if($ReleaseGraphics){$ReleaseVirtualText=$true}
if($WindowsEspGuid -eq [guid]::Empty){throw 'An exact Windows ESP GUID is required'}
if(!$AttemptUnlock -and $AttemptSlot -ne 4){throw 'AttemptSlot requires -AttemptUnlock'}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Push-Location (Join-Path $repo 'internal-boot/boot')
try {
 $out='build/shared-esp-qualification';New-Item -ItemType Directory -Force $out | Out-Null
 $b=$WindowsEspGuid.ToByteArray();$tail=($b[8..15] | ForEach-Object {'0x{0:x2}' -f $_}) -join ','
 $guidC='{{0x{0:x8},0x{1:x4},0x{2:x4},{{{3}}}}}' -f [BitConverter]::ToUInt32($b,0),[BitConverter]::ToUInt16($b,4),[BitConverter]::ToUInt16($b,6),$tail
 @('#define GU_INTERNAL_PARTITION_INITIALIZER {0,0,0,{0,0,0,0,0,0,0,0}}',('#define GU_WINDOWS_PARTITION_INITIALIZER '+$guidC)) | Set-Content "$out/platform-config.h" -Encoding ascii
 $sources=@('shared-esp-qualification.c','evga-text-stop.c','diagnostic.c','../efi_runtime.c','../unlock/sha256.c')
 if($ProbePlatform){$sources+='../unlock/checks.c'}
 if($ProbeResources){$sources=@($sources | Where-Object {$_ -ne '../efi_runtime.c'})+@('evga-resources.c','../unlock/ownership.c','../unlock/runtime.c')}
 if($AttemptUnlock){$sources+=@('../unlock/format.c','../unlock/port.c','../unlock/compat.c','../unlock/evga-admission.c','../unlock/evga-transaction.c','../unlock/vendor/unlock_610.c','../unlock/vendor/prep_610.c','../unlock/vendor/layout_610.c','../unlock/vendor/vbios_610.c','../unlock/vendor/gfw_eval.c')}
 $defines=@('-DGU_DIAGNOSTIC_ONLY','-DGU_EVGA_SHARED_ESP_ADMISSION','-DGU_SHARED_ESP_QUAL','-DGU_EVGA_STOP_ONLY');$objects=@();if($AttemptUnlock){$defines+=@('-DGU_EVGA_UNLOCK','-DGU_PROFILE_EVGA_Z790_3090','-DGU_EVGA_TRANSACTION_DEVELOPMENT','-DGU_EXPERIMENTAL_3090','-DGU_ENTRY_LATCHED_SS1_FULL','-DGU_UEFI_PORT','-D__KERNEL__','-DGU_TARGET_PCI_ID=0x220410deU')};if($ProbeResources){$defines+='-DGU_EVGA_PROBE_RESOURCES'};if($ProbePlatform){$defines+='-DGU_EVGA_PROBE_PLATFORM'};if($ProbeState){$defines+='-DGU_EVGA_PROBE_STATE'};if($ProbeDecode){$defines+='-DGU_EVGA_PROBE_DECODE'};if($ReleaseVirtualText){$defines+='-DGU_EVGA_RELEASE_TEXT'};if($ReleaseGraphics){$defines+='-DGU_EVGA_RELEASE_GRAPHICS'};if($ReleaseEdid){$defines+='-DGU_EVGA_RELEASE_EDID'};if($ReleaseEdidSiblings){$defines+='-DGU_EVGA_RELEASE_EDID_SIBLINGS'};if($AuditNvidia){$defines+='-DGU_EVGA_AUDIT_NVIDIA'};if($StopNvidiaChildren){$defines+='-DGU_EVGA_STOP_NVIDIA_CHILDREN'};if($StopNvidiaParent){$defines+='-DGU_EVGA_STOP_NVIDIA_PARENT'}
 if($AttemptUnlock){$defines+=('-DGU_EVGA_ATTEMPT_SLOT='+$AttemptSlot)}
 foreach($source in $sources) {
  $sourceDefines=@();if($source -eq '../unlock/ownership.c'){$sourceDefines+=@('-DGU_HEADLESS_RESEARCH','-DGU_OWNERSHIP_CLAIM_ONLY')}
  $obj="$out/"+[IO.Path]::GetFileNameWithoutExtension($source)+'.obj'
  & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -ffunction-sections -fdata-sections -Os -Wall -Wextra -Werror -Wno-unused-parameter @defines @sourceDefines -I ../unlock/compat -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 -c $source -o $obj
  if($LASTEXITCODE -ne 0){throw "Compile failed: $source"};$objects+=$obj
 }
 & $LldLink /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf "/map:$out/EVGA.map" "/out:$out/BOOTX64.EFI" @objects
 if($LASTEXITCODE -ne 0){throw 'Link failed'}
 & $Python -I ../unlock/verify_pe.py "$out/BOOTX64.EFI" "$out/EVGA.map" --output "$out/pe-verification.json"
 if($LASTEXITCODE -ne 0){throw 'PE verification failed'}
 [ordered]@{AuditFileName='EVGA-SHARED-QUALIFICATION.EFI';Visible=$true;Mode=$(if($AttemptUnlock){'evga3090-experimental-unlock'}elseif($ProbeResources){'shared-esp-release-qualification'}elseif($ProbePlatform){'evga3090-platform-probe'}elseif($ProbeState){'evga3090-state-probe'}elseif($ProbeDecode){'evga3090-decode-probe'}elseif($StopNvidiaParent){'evga3090-nvidia-parent-stop'}elseif($StopNvidiaChildren){'evga3090-nvidia-three-child-stop'}elseif($AuditNvidia){'evga3090-nvidia-preflight'}elseif($ReleaseEdidSiblings){'evga3090-six-consumer-stops'}elseif($ReleaseEdid){'evga3090-four-consumer-stops'}elseif($ReleaseGraphics){'evga3090-three-consumer-stops'}elseif($ReleaseVirtualText){'evga3090-two-text-stops'}else{'evga3090-one-text-membership-stop'});HardwareTested=$false;CanUnlock=[bool]$AttemptUnlock;CanStopTextMembership=$true;WindowsEspGuid=$WindowsEspGuid.ToString();Staged=$false
 AttemptSlot=$(if($AttemptUnlock){$AttemptSlot}else{$null});EfiSha256=(Get-FileHash "$out/BOOTX64.EFI" -Algorithm SHA256).Hash.ToLowerInvariant();Sources=@(($sources+$(if($AttemptUnlock){@(Get-ChildItem '../unlock','../../deps/edk2/MdePkg/Include' -Filter '*.h' -File -Recurse | ForEach-Object {Resolve-Path -Relative $_.FullName})}else{@()})+@('handoff.c','evga-text-stop.h','evga-resources.h','../unlock/asset-reader.h','../unlock/assets.json','../unlock/evga-admission.h','../unlock/evga-transaction.h','../unlock/ownership.h','../unlock/runtime.h','media.h','../unlock/checks.h','../unlock/port.h','../unlock/machine-profile.h','../unlock/gpu-family.h','../unlock/firmware-profile.h',"$out/platform-config.h")) | ForEach-Object {[ordered]@{Path=$_;Sha256=(Get-FileHash $_ -Algorithm SHA256).Hash.ToLowerInvariant()}})} | ConvertTo-Json -Depth 5 | Set-Content "$out/manifest.json" -Encoding utf8
 $guide=if($AttemptUnlock){'NEXT-UNLOCK-ATTEMPT.en.md'}elseif($ProbeResources){'SHARED-ESP-QUALIFICATION.en.md'}elseif($ProbePlatform){'NEXT-PLATFORM-PROBE.en.md'}elseif($ProbeState){'NEXT-STATE-PROBE.en.md'}elseif($ProbeDecode){'NEXT-DECODE-PROBE.en.md'}elseif($StopNvidiaParent){'NEXT-NVIDIA-PARENT.en.md'}elseif($StopNvidiaChildren){'NEXT-NVIDIA-CHILDREN.en.md'}elseif($AuditNvidia){'NEXT-NVIDIA-AUDIT.en.md'}elseif($ReleaseEdidSiblings){'NEXT-EDID-SIBLINGS.en.md'}elseif($ReleaseEdid){'NEXT-EDID-RELEASE.en.md'}elseif($ReleaseGraphics){'NEXT-GRAPHICS-RELEASE.en.md'}elseif($ReleaseVirtualText){'NEXT-TEXT-RELEASE.en.md'}else{'NEXT-EFI-TEST.en.md'}
 Write-Output "Compiled $out/BOOTX64.EFI. NOT STAGED. Read $guide for risk and staging/run instructions; no separate confirmation reply is required."
} finally {Pop-Location}
