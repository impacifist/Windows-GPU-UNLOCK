# GPL-2.0-only. Optional EDID/text/GOP lifecycle switches perform attended consumer mutations.
# Default: build the profile-less read-only USB probe (-DGU_PROBE_GENERIC) for a new machine.
# The probe records every NVIDIA display controller, the console/driver graph, the 1 MiB PROM
# (PROM.BIN) and loaded driver images (IMGnn.BIN) to the stick it booted from, then starts
# Windows. It compiles only as a diagnostic build: no unlock code, GPU writes, driver release,
# DMA or firmware variable writes. Builds files only.
param([Parameter(Mandatory,ParameterSetName='Gpt')][guid]$WindowsEspGuid,
 [Parameter(Mandatory,ParameterSetName='Mbr')][switch]$WindowsMbr,
 [switch]$Ad104Msi,[switch]$Ad104Metadata,[switch]$Ad104FirmwareRom,[switch]$Ad104Resources,[switch]$Ad104Preflight,[switch]$Ad104EdidTest,[switch]$Ad104OutputObservation,[switch]$Ad104PcdCapture,[switch]$Ad104TextTest,[switch]$Ad104GopTest,[switch]$Ad104ConsumersTest,[switch]$Ad104OwnerChildTest,[switch]$Ad104ParentTest,[switch]$Ad104DecodeReadTest,[switch]$Ad104DecodeOffTest,[switch]$Ad104DelayedReadTest,[switch]$Ad104PromHeaderTest,[switch]$Ad104PromFullTest,[switch]$Ad104FwsecAuditTest,[switch]$Ad104FwsecRehearsalTest,[switch]$Ad104FwsecSignatureTest,[switch]$Ad104FalconObserveTest,[switch]$Ad104FalconMailboxTest,[switch]$Ad104FalconCapsTest,[switch]$Ad104FalconCoreTest,[switch]$Ad104FalconRefusalTest,[switch]$Ad104FalconBcrTest,[switch]$Ad104EntryInputsTest,[string]$Clang='clang',[string]$LldLink='lld-link',[string]$Python='python',
 [ValidateRange(1,255)][int]$TargetBus=1)
$ErrorActionPreference='Stop'
if($Ad104Metadata -and !$Ad104Msi){throw 'Ad104Metadata requires Ad104Msi'}
if($Ad104FirmwareRom -and !$Ad104Metadata){throw 'Ad104FirmwareRom requires Ad104Metadata'}
if($Ad104Resources -and (!$Ad104Metadata -or $Ad104FirmwareRom)){throw 'Ad104Resources requires metadata mode without firmware ROM capture'}
if($Ad104Preflight -and !$Ad104Resources){throw 'Ad104Preflight requires Ad104Resources'}
if($Ad104EdidTest -and (!$Ad104Preflight -or !$WindowsMbr)){throw 'Ad104EdidTest requires Ad104Preflight and WindowsMbr'}
if($Ad104OutputObservation -and (!$Ad104Preflight -or $Ad104EdidTest)){throw 'Output observation requires preflight without EDID mutation'}
if($Ad104PcdCapture -and !$Ad104OutputObservation){throw 'PCD capture requires output observation'}
if($Ad104TextTest -and (!$Ad104PcdCapture -or !$WindowsMbr -or $Ad104EdidTest)){throw 'Text test requires PCD capture and WindowsMbr without EDID test'}
if($Ad104GopTest -and !$Ad104TextTest){throw 'GOP test requires text test'}
if($Ad104ConsumersTest -and !$Ad104GopTest){throw 'Consumers test requires GOP test'}
if($Ad104OwnerChildTest -and !$Ad104ConsumersTest){throw 'Owner child test requires consumers test'}
if($Ad104ParentTest -and !$Ad104OwnerChildTest){throw 'Parent test requires owner child test'}
if($Ad104EntryInputsTest -and (!$Ad104FalconCoreTest -or $Ad104FalconBcrTest -or $Ad104FalconRefusalTest)){throw 'Entry input audit requires core mode without refused-register modes'}
if($Ad104FalconBcrTest -and (!$Ad104FalconCoreTest -or $Ad104FalconRefusalTest)){throw 'BCR subset requires core-state mode without refusal recovery'}
if($Ad104FalconRefusalTest -and !$Ad104FalconCoreTest){throw 'Refusal recovery requires core-state observation'}
if($Ad104FalconCoreTest -and !$Ad104FalconCapsTest){throw 'Falcon core state requires capability observations'}
if($Ad104FalconCapsTest -and !$Ad104FalconMailboxTest){throw 'Falcon capabilities requires mailbox subset'}
if($Ad104FalconMailboxTest -and !$Ad104FalconObserveTest){throw 'Mailbox subset requires Falcon observation'}
if($Ad104FalconObserveTest -and !$Ad104FwsecSignatureTest){throw 'Falcon observation requires sampled signature test'}
if($Ad104FwsecSignatureTest -and !$Ad104FwsecRehearsalTest){throw 'Sampled signature requires RAM rehearsal'}
if($Ad104FwsecRehearsalTest -and !$Ad104FwsecAuditTest){throw 'FWSEC rehearsal requires observation audit'}
if($Ad104FwsecAuditTest -and !$Ad104PromFullTest){throw 'FWSEC audit requires full PROM capture'}
if($Ad104PromFullTest -and !$Ad104PromHeaderTest){throw 'Full PROM requires header and recovery tests'}
if($Ad104PromHeaderTest -and !$Ad104DelayedReadTest){throw 'PROM header test requires delayed read recovery'}
if($Ad104DelayedReadTest -and !$Ad104DecodeOffTest){throw 'Delayed reads require decode off recovery'}
if($Ad104DecodeOffTest -and !$Ad104DecodeReadTest){throw 'Decode off test requires decode read test'}
if($Ad104DecodeReadTest -and !$Ad104ParentTest){throw 'Decode read test requires parent test'}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$boot=Join-Path $repo 'internal-boot/boot'
function GuidC([guid]$Value){
 $b=$Value.ToByteArray();$tail=($b[8..15] | ForEach-Object {'0x{0:x2}' -f $_}) -join ','
 return ('{{0x{0:x8},0x{1:x4},0x{2:x4},{{{3}}}}}' -f [BitConverter]::ToUInt32($b,0),[BitConverter]::ToUInt16($b,4),[BitConverter]::ToUInt16($b,6),$tail)
}
$mbrConfig=@()
if($WindowsMbr) {
 $windows=@(Get-Partition | Where-Object IsSystem)
 if($windows.Count -ne 1){throw 'Expected one current Windows system boot partition'}
 $part=$windows[0];$disk=Get-Disk -Number $part.DiskNumber
 if($disk.PartitionStyle -ne 'MBR' -or !$disk.Signature -or $disk.BusType -eq 'USB' -or
    $disk.LogicalSectorSize -ne 512 -or !$part.IsActive -or $part.MbrType -ne 7 -or
    !$part.Size -or $part.Offset%512 -or $part.Size%512){throw 'Unreviewed MBR boot partition geometry'}
 if((Get-ComputerInfo -Property BiosFirmwareType).BiosFirmwareType.ToString() -ne 'Uefi'){throw 'UEFI Windows boot required'}
 $volume=Get-Volume -Partition $part
 if(!$volume.Path){throw 'System boot volume path unavailable'}
 $manager=Join-Path $volume.Path 'EFI\Microsoft\Boot\bootmgfw.efi'
 $managerFile=Get-Item -LiteralPath $manager -ErrorAction Stop
 if(!$managerFile.Length -or $managerFile.Length -gt 8MB){throw 'Unexpected boot-manager size'}
 $managerHash=(Get-FileHash -LiteralPath $manager -Algorithm SHA256 -ErrorAction Stop).Hash.ToLowerInvariant()
 $mbrConfig=@('#ifdef GU_DIAGNOSTIC_MBR',
  ('#define GU_WINDOWS_MBR_SIGNATURE 0x{0:x8}U' -f [uint32]$disk.Signature),
  ('#define GU_WINDOWS_MBR_PARTITION {0}U' -f $part.PartitionNumber),
  ('#define GU_WINDOWS_MBR_START {0}ULL' -f [uint64]($part.Offset/512)),
  ('#define GU_WINDOWS_MBR_SIZE {0}ULL' -f [uint64]($part.Size/512)),
  ('#define GU_WINDOWS_BOOT_BYTES {0}U' -f $managerFile.Length),
  ('#define GU_WINDOWS_BOOT_SHA256 "'+$managerHash+'"'),'#endif')
 # Deliberately unused GUID in the MBR diagnostic. Never a discovered ESP GUID.
 $WindowsEspGuid=[guid]'ffffffff-ffff-ffff-ffff-ffffffffffff'
} else {
 $windows=@(Get-Partition | Where-Object {$_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
 if($windows.Count -ne 1 -or !$windows[0].IsSystem){throw 'WindowsEspGuid must identify the current system EFI partition'}
}
# Same local USB configuration as build-usb.ps1, so the two builds share it.
$config=Join-Path $boot 'platform-config.h';$marker='/* Local USB configuration (build-usb.ps1). Internal partition unused under GU_USB_MEDIA. */'
if((Test-Path -LiteralPath $config) -and (Get-Content -LiteralPath $config -TotalCount 1) -ne $marker){throw 'platform-config.h belongs to an internal Configure.ps1 setup; move it aside before a USB build'}
(@($marker,('#define GU_INTERNAL_PARTITION_INITIALIZER '+(GuidC ([guid]::Empty))),('#define GU_WINDOWS_PARTITION_INITIALIZER '+(GuidC $WindowsEspGuid)))+$mbrConfig) | Set-Content -LiteralPath $config -Encoding ascii
$generated=Join-Path $boot 'generated.h'
if(!(Test-Path -LiteralPath $generated)){'/* Observation build: GU_DIAGNOSTIC_ONLY excludes all unlock loading. */' | Set-Content -LiteralPath $generated -Encoding ascii}
Push-Location -LiteralPath $boot
try {
 $out='build/probe-generic'
 if($Ad104Msi){$out='build/probe-ad104-msi'}
 if($Ad104Metadata){$out='build/probe-ad104-metadata'}
 if($Ad104FirmwareRom){$out='build/probe-ad104-firmrom'}
 if($Ad104Resources){$out='build/probe-ad104-resources'}
 if($Ad104Preflight){$out='build/probe-ad104-preflight'}
 if($Ad104EdidTest){$out='build/probe-ad104-edid'}
 if($Ad104OutputObservation){$out='build/probe-ad104-output'}
 if($Ad104PcdCapture){$out='build/probe-ad104-pcd'}
 if($Ad104TextTest){$out='build/probe-ad104-text'}
 if($Ad104EntryInputsTest){$out='build/probe-ad104-entry-inputs'}
 elseif($Ad104FalconBcrTest){$out='build/probe-ad104-falcon-bcr'}
 elseif($Ad104FalconRefusalTest){$out='build/probe-ad104-falcon-refusal'}
 elseif($Ad104FalconCoreTest){$out='build/probe-ad104-falcon-core'}
 elseif($Ad104FalconCapsTest){$out='build/probe-ad104-falcon-caps'}
 elseif($Ad104FalconMailboxTest){$out='build/probe-ad104-falcon-mailbox'}
 elseif($Ad104FalconObserveTest){$out='build/probe-ad104-falcon-observe'}
 elseif($Ad104FwsecSignatureTest){$out='build/probe-ad104-fwsec-signature'}
 elseif($Ad104FwsecRehearsalTest){$out='build/probe-ad104-fwsec-rehearsal'}
 elseif($Ad104FwsecAuditTest){$out='build/probe-ad104-fwsec-audit'}
 elseif($Ad104PromFullTest){$out='build/probe-ad104-prom-full'}
 elseif($Ad104PromHeaderTest){$out='build/probe-ad104-prom-header'}
 elseif($Ad104DelayedReadTest){$out='build/probe-ad104-delayed-read'}
 elseif($Ad104DecodeOffTest){$out='build/probe-ad104-decode-off'}
 elseif($Ad104DecodeReadTest){$out='build/probe-ad104-decode-read'}
 elseif($Ad104ParentTest){$out='build/probe-ad104-parent'}
 elseif($Ad104OwnerChildTest){$out='build/probe-ad104-owner-child'}
 elseif($Ad104ConsumersTest){$out='build/probe-ad104-consumers'}
 elseif($Ad104GopTest){$out='build/probe-ad104-gop'}
 New-Item -ItemType Directory -Path $out -Force | Out-Null
 $sources=@('handoff.c','diagnostic.c','../efi_runtime.c','probe.c','../unlock/sha256.c')
 $auditDeps=@()
 if($Ad104FwsecAuditTest){$sources+='fwsec-audit.c';$auditDeps=@('fwsec-audit.h','../unlock/vendor/vbios_610.c','../unlock/vendor/vbios_610.h','../unlock/vendor/diagnostic.h','audit-include/errno.h','audit-include/string.h')}
 $defines=@('-DGU_DIAGNOSTIC_ONLY','-DGU_DIAGNOSTIC_PROBE','-DGU_PROBE_GENERIC','-DGU_USB_PROBE')
 if($Ad104Msi){$defines+=@('-DGU_PROBE_AD104_MSI','-DGU_USB_MEDIA')}
 if($Ad104Metadata){$defines+='-DGU_PROBE_AD104_METADATA'}
 if($Ad104FirmwareRom){$defines+='-DGU_PROBE_AD104_FIRMROM'}
 if($Ad104Resources){$defines+='-DGU_PROBE_AD104_RESOURCES'}
 if($Ad104Preflight){$defines+='-DGU_PROBE_AD104_PREFLIGHT'}
 if($Ad104EdidTest){$defines+='-DGU_AD104_EDID_TEST'}
 if($Ad104OutputObservation){$defines+='-DGU_AD104_OUTPUT_OBSERVATION'}
 if($Ad104PcdCapture){$defines+='-DGU_AD104_PCD_CAPTURE'}
 if($Ad104TextTest){$defines+='-DGU_AD104_TEXT_TEST'}
 if($Ad104DecodeReadTest){$defines+='-DGU_AD104_DECODE_READ_TEST'}
 if($Ad104DecodeOffTest){$defines+='-DGU_AD104_DECODE_OFF_TEST'}
 if($Ad104DelayedReadTest){$defines+='-DGU_AD104_DELAYED_READ_TEST'}
 if($Ad104PromHeaderTest){$defines+='-DGU_AD104_PROM_HEADER_TEST'}
 if($Ad104PromFullTest){$defines+='-DGU_AD104_PROM_FULL_TEST'}
 if($Ad104EntryInputsTest){$defines+='-DGU_AD104_ENTRY_INPUTS_TEST'}
 if($Ad104FalconBcrTest){$defines+='-DGU_AD104_FALCON_BCR_TEST'}
 if($Ad104FalconRefusalTest){$defines+='-DGU_AD104_FALCON_REFUSAL_TEST'}
 if($Ad104FalconCoreTest){$defines+='-DGU_AD104_FALCON_CORE_TEST'}
 if($Ad104FalconCapsTest){$defines+='-DGU_AD104_FALCON_CAPS_TEST'}
 if($Ad104FalconMailboxTest){$defines+='-DGU_AD104_FALCON_MAILBOX_TEST'}
 if($Ad104FalconObserveTest){$defines+='-DGU_AD104_FALCON_OBSERVE_TEST'}
 if($Ad104FwsecSignatureTest){$defines+='-DGU_AD104_FWSEC_SIGNATURE_TEST'}
 if($Ad104FwsecRehearsalTest){$defines+='-DGU_AD104_FWSEC_REHEARSAL_TEST'}
 if($Ad104FwsecAuditTest){$defines+='-DGU_AD104_FWSEC_AUDIT_TEST'}
 if($Ad104ParentTest){$defines+='-DGU_AD104_PARENT_TEST'}
 if($Ad104OwnerChildTest){$defines+='-DGU_AD104_OWNER_CHILD_TEST'}
 if($Ad104ConsumersTest){$defines+='-DGU_AD104_CONSUMERS_TEST'}
 if($Ad104GopTest){$defines+='-DGU_AD104_GOP_TEST'}
 if($WindowsMbr){$defines+='-DGU_DIAGNOSTIC_MBR';if(!$Ad104Msi){$defines+='-DGU_USB_MEDIA'}}
 $busMode='auto'
 if($PSBoundParameters.ContainsKey('TargetBus') -or $Ad104Msi){
  $defines+=('-DGU_PROBE_BUS=0x{0:x}U' -f $TargetBus);$busMode='explicit'
 } else {$defines+='-DGU_PROBE_AUTO_BUS'}
 $objects=@()
 foreach($source in $sources) {
  $obj="$out/"+[IO.Path]::GetFileNameWithoutExtension($source)+'.obj'
  $parserIncludes=@();if($source -eq 'fwsec-audit.c'){$parserIncludes=@('-I','audit-include')}
  & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -ffunction-sections -Os -Wall -Wextra -Werror @defines @parserIncludes -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 -c $source -o $obj
  if($LASTEXITCODE -ne 0){throw "Compile failed: $source"}
  $objects+=$obj
 }
 & $LldLink /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf "/map:$out/PROBE.map" "/out:$out/BOOTX64.EFI" @objects
 if($LASTEXITCODE -ne 0){throw 'Probe link failed'}
 & $Python -I ../unlock/verify_pe.py "$out/BOOTX64.EFI" "$out/PROBE.map" --output "$out/pe-verification.json"
 if($LASTEXITCODE -ne 0){throw 'Probe PE verification failed'}
 [ordered]@{Mode=if($Ad104EntryInputsTest){'ad104-msi-measured-entry-input-audit-test'}elseif($Ad104FalconBcrTest){'ad104-msi-bcr-only-core-observation-test'}elseif($Ad104FalconRefusalTest){'ad104-msi-known-refusal-recovery-test'}elseif($Ad104FalconCoreTest){'ad104-msi-gsp-core-state-observation-test'}elseif($Ad104FalconCapsTest){'ad104-msi-gsp-capability-observation-test'}elseif($Ad104FalconMailboxTest){'ad104-msi-mailbox-only-observation-test'}elseif($Ad104FalconObserveTest){'ad104-msi-gsp-falcon-observation-test'}elseif($Ad104FwsecSignatureTest){'ad104-msi-sampled-signature-ram-test'}elseif($Ad104FwsecRehearsalTest){'ad104-msi-ram-only-fwsec-rehearsal-test'}elseif($Ad104FwsecAuditTest){'ad104-msi-observation-only-fwsec-audit-test'}elseif($Ad104PromFullTest){'ad104-msi-full-prom-capture-test'}elseif($Ad104PromHeaderTest){'ad104-msi-bounded-prom-header-test'}elseif($Ad104DelayedReadTest){'ad104-msi-delayed-register-stability-test'}elseif($Ad104DecodeOffTest){'ad104-msi-capture-then-decode-off-test'}elseif($Ad104DecodeReadTest){'ad104-msi-limited-decode-register-read-test'}elseif($Ad104ParentTest){'ad104-msi-direct-parent-stop-test'}elseif($Ad104OwnerChildTest){'ad104-msi-direct-owner-child-test'}elseif($Ad104ConsumersTest){'ad104-msi-three-consumer-test'}elseif($Ad104GopTest){'ad104-msi-text-then-gop-consumer-test'}elseif($Ad104TextTest){'ad104-msi-pcd-capture-then-one-shot-text-test'}elseif($Ad104PcdCapture){'ad104-msi-pcd-producer-capture'}elseif($Ad104OutputObservation){'ad104-msi-output-and-pcd-observation'}elseif($Ad104EdidTest){'ad104-msi-one-shot-edid-lifecycle-test'}elseif($Ad104Preflight){'ad104-msi-runtime-preflight-observation-then-windows-usb'}elseif($Ad104Resources){'ad104-msi-resource-observation-then-windows-usb'}elseif($Ad104FirmwareRom){'ad104-msi-firmware-rom-buffer-then-windows-usb'}elseif($Ad104Metadata){'ad104-msi-metadata-only-then-windows-usb'}elseif($Ad104Msi){'ad104-msi-read-only-probe-then-windows-usb'}else{'generic-read-only-probe-then-windows-usb'};
   CanQueryBarResources=[bool]$Ad104Resources;CanQueryPcdBoolean=[bool]$Ad104OutputObservation;CanCapturePcdProducer=[bool]$Ad104PcdCapture;CanReleaseGopConsumer=[bool]$Ad104GopTest;CanReleasePciOwner=[bool]$Ad104ParentTest;DriverMayDisablePciAttributes=[bool]$Ad104ParentTest;CanRestoreDecode=$false;CanStopOwnerChildDirectly=[bool]$Ad104OwnerChildTest;CanReleaseAllDisplayConsumers=[bool]$Ad104ConsumersTest;
   CanReadGpuMmio=([bool]$Ad104DecodeReadTest -or !$Ad104Metadata);CanEnableLimitedDecode=[bool]$Ad104DecodeReadTest;CanDisableLimitedDecode=[bool]$Ad104DecodeOffTest;FwsecObservationOnly=[bool]$Ad104FwsecAuditTest;FwsecRamRehearsal=[bool]$Ad104FwsecRehearsalTest;FwsecSampledSignature=[bool]$Ad104FwsecSignatureTest;FalconObservation=[bool]$Ad104FalconObserveTest;FalconMailboxOnly=([bool]$Ad104FalconMailboxTest -and !$Ad104FalconCapsTest);FalconCapabilities=[bool]$Ad104FalconCapsTest;FalconCoreState=[bool]$Ad104FalconCoreTest;FalconBcrOnly=[bool]$Ad104FalconBcrTest;EntryInputAudit=[bool]$Ad104EntryInputsTest;CanReadBcr=([bool]$Ad104FalconCoreTest -and !$Ad104EntryInputsTest);CanReadRiscvCpuCtl=([bool]$Ad104FalconCoreTest -and !$Ad104FalconBcrTest -and !$Ad104EntryInputsTest);CanRecoverKnownObservationRefusal=[bool]$Ad104FalconRefusalTest;RecoveryRegisterReads=if($Ad104FalconRefusalTest){14}else{0};RefusalPathAdditionalReads=if($Ad104FalconRefusalTest){23}else{0};CanResetGpu=$false;CanReadDmaControl=([bool]$Ad104FalconObserveTest -and !$Ad104FalconMailboxTest);AdditionalMmioReads=if($Ad104EntryInputsTest){24}elseif($Ad104FalconBcrTest){26}elseif($Ad104FalconCoreTest){28}elseif($Ad104FalconCapsTest){22}elseif($Ad104FalconMailboxTest){16}elseif($Ad104FalconObserveTest){18}else{0};CanExecuteFirmware=$false;PromCaptureBytes=if($Ad104PromFullTest){1048576}else{0};PromHeaderBytes=if($Ad104PromHeaderTest){512}else{0};InterBatchDelayUs=if($Ad104DelayedReadTest){1000000}else{0};CanDereferenceRomImage=[bool]$Ad104FirmwareRom;
  WindowsBootIdentity=if($WindowsMbr){'exact-mbr-extent-and-boot-manager-hash'}else{'gpt-partition-guid'};
  CanUnlock=$false;CanDetach=([bool]$Ad104EdidTest -or [bool]$Ad104TextTest);WritesFirmwareVariables=$false;HardwareTested=$false;Defines=$defines
  ProbeBusMode=$busMode;TargetBus=$(if($busMode -eq 'explicit'){$TargetBus}else{$null})
  EfiSha256=(Get-FileHash -LiteralPath "$out/BOOTX64.EFI").Hash.ToLowerInvariant();BuiltUtc=[DateTime]::UtcNow.ToString('o')
   Sources=@(($sources+$auditDeps+@('diagnostic.h','media.h','probe.h','firmware-rom.h','ad104-preflight.h','ad104-edid.h','output-observation.h','pcd-producer.h','text-display.h','gop-step.h','edid-step.h','owner-child.h','owner-parent.h','parent-branch.h','decode-read.h','prom-full.h','falcon-observe.h','falcon-refusal.h','entry-inputs.h','platform-config.h','../unlock/machine-profile.h','../unlock/gpu-family.h','../unlock/firmware-profile.h')) | ForEach-Object {[ordered]@{Path=$_;Sha256=(Get-FileHash -LiteralPath $_).Hash.ToLowerInvariant()}})
 } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$out/manifest.json" -Encoding utf8
 "Built $out/BOOTX64.EFI; CanDetach=$([bool]$Ad104EdidTest -or [bool]$Ad104TextTest); hardware untested."
} finally {Pop-Location}
