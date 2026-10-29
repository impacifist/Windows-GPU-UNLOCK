# GPL-2.0-only. RAM-only firmware mocks and compile-time admission tests.
param([string]$Clang='clang',[string]$VcVars)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'host.ps1')
Import-GuHostCompiler $VcVars
$boot=Join-Path $PSScriptRoot '../../internal-boot/boot'
Push-Location -LiteralPath $boot
try {
 New-Item -ItemType Directory -Path build/capture-tests/include -Force | Out-Null
 # Fallback only: an existing private configuration beside media.h wins.
 @('#define GU_INTERNAL_PARTITION_INITIALIZER {0,0,0,{0}}',
   '#define GU_WINDOWS_PARTITION_INITIALIZER {1,0,0,{0}}') | Set-Content build/capture-tests/include/platform-config.h -Encoding ascii
 $flags=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS',
   '-I','build/capture-tests/include','-I','../../deps/edk2/MdePkg/Include','-I','../../deps/edk2/MdePkg/Include/X64')
 $ad104=@('-DGU_DIAGNOSTIC_ONLY','-DGU_DIAGNOSTIC_PROBE','-DGU_PROBE_GENERIC','-DGU_PROBE_AD104_MSI','-DGU_USB_MEDIA')
 & $Clang @flags -Wno-unused-function @ad104 test_probe_ad104.c probe.c ../unlock/sha256.c -o build/capture-tests/ad104.exe
 if($LASTEXITCODE -ne 0){throw 'AD104 mocks compile failed'}
 & ./build/capture-tests/ad104.exe
 if($LASTEXITCODE -ne 0){throw 'AD104 mocks failed'}
 & $Clang @flags -Wno-unused-function @ad104 -DGU_PROBE_AD104_METADATA test_probe_ad104.c probe.c ../unlock/sha256.c -o build/capture-tests/metadata.exe
 if($LASTEXITCODE -ne 0){throw 'AD104 metadata mocks compile failed'}
 & ./build/capture-tests/metadata.exe
 if($LASTEXITCODE -ne 0){throw 'AD104 metadata mocks failed'}
 & $Clang @flags -Wno-unused-function @ad104 -DGU_PROBE_AD104_METADATA -DGU_PROBE_AD104_FIRMROM test_firmware_rom.c probe.c ../unlock/sha256.c -o build/capture-tests/firmrom.exe
 if($LASTEXITCODE -ne 0){throw 'Firmware ROM mocks compile failed'}
 & ./build/capture-tests/firmrom.exe
 if($LASTEXITCODE -ne 0){throw 'Firmware ROM mocks failed'}
 & $Clang @flags -Wno-unused-function @ad104 -DGU_PROBE_AD104_METADATA -DGU_PROBE_AD104_RESOURCES test_resources.c probe.c ../unlock/sha256.c -o build/capture-tests/resources.exe
 if($LASTEXITCODE -ne 0){throw 'Resource mocks compile failed'}
 & ./build/capture-tests/resources.exe
 if($LASTEXITCODE -ne 0){throw 'Resource mocks failed'}
 & $Clang @flags -Wno-unused-function test_ad104_preflight.c ../unlock/sha256.c -o build/capture-tests/preflight.exe
 if($LASTEXITCODE -ne 0){throw 'Preflight mocks compile failed'}
 & ./build/capture-tests/preflight.exe
 if($LASTEXITCODE -ne 0){throw 'Preflight mocks failed'}
 & $Clang @flags -Wno-unused-function test_output_observation.c ../unlock/sha256.c -o build/capture-tests/output.exe
 if($LASTEXITCODE -ne 0){throw 'Output mocks compile failed'}
 & ./build/capture-tests/output.exe
 if($LASTEXITCODE -ne 0){throw 'Output mocks failed'}
 & $Clang @flags -Wno-unused-function -DGU_AD104_DECODE_OFF_TEST test_decode_read.c ../unlock/sha256.c -o build/capture-tests/decode-off.exe
 if($LASTEXITCODE -ne 0){throw 'Decode-off compile failed'}
 & ./build/capture-tests/decode-off.exe
 if($LASTEXITCODE -ne 0){throw 'Decode-off mocks failed'}
 & $Clang @flags -Wno-unused-function -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST test_decode_read.c ../unlock/sha256.c -o build/capture-tests/delayed-read.exe
 if($LASTEXITCODE -ne 0){throw 'Delayed reads compile failed'}
 & ./build/capture-tests/delayed-read.exe
 if($LASTEXITCODE -ne 0){throw 'Delayed reads mocks failed'}
 & $Clang @flags -Wno-unused-function -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST -DGU_AD104_PROM_HEADER_TEST test_decode_read.c ../unlock/sha256.c -o build/capture-tests/prom-header.exe
 if($LASTEXITCODE -ne 0){throw 'PROM header compile failed'}
 & ./build/capture-tests/prom-header.exe
 if($LASTEXITCODE -ne 0){throw 'PROM header mocks failed'}
 & $Clang @flags -Wno-unused-function -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST -DGU_AD104_PROM_HEADER_TEST -DGU_AD104_PROM_FULL_TEST test_decode_read.c ../unlock/sha256.c -o build/capture-tests/prom-full.exe
 if($LASTEXITCODE -ne 0){throw 'Full PROM compile failed'}
 & ./build/capture-tests/prom-full.exe
 if($LASTEXITCODE -ne 0){throw 'Full PROM mocks failed'}
 & $Clang @flags -Wno-unused-function -DGU_FWSEC_AUDIT_TEST_FAULTS test_fwsec_audit.c fwsec-audit.c ../unlock/sha256.c -o build/capture-tests/fwsec-audit.exe
 if($LASTEXITCODE -ne 0){throw 'FWSEC audit compile failed'}
 & ./build/capture-tests/fwsec-audit.exe
 if($LASTEXITCODE -ne 0){throw 'FWSEC audit mocks failed'}
 & $Clang @flags -Wno-unused-function -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST -DGU_AD104_PROM_HEADER_TEST -DGU_AD104_PROM_FULL_TEST -DGU_AD104_FWSEC_AUDIT_TEST -DGU_AD104_FWSEC_REHEARSAL_TEST -DGU_AD104_FWSEC_SIGNATURE_TEST -DGU_AD104_FALCON_OBSERVE_TEST test_decode_read.c fwsec-audit.c ../unlock/sha256.c -o build/capture-tests/audit-recovery.exe
 if($LASTEXITCODE -ne 0){throw 'Audit recovery compile failed'}
 & ./build/capture-tests/audit-recovery.exe
 if($LASTEXITCODE -ne 0){throw 'Audit recovery mocks failed'}
 & $Clang @flags -Wno-unused-function -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST -DGU_AD104_PROM_HEADER_TEST -DGU_AD104_PROM_FULL_TEST -DGU_AD104_FWSEC_AUDIT_TEST -DGU_AD104_FWSEC_REHEARSAL_TEST -DGU_AD104_FWSEC_SIGNATURE_TEST -DGU_AD104_FALCON_OBSERVE_TEST -DGU_AD104_FALCON_MAILBOX_TEST test_decode_read.c fwsec-audit.c ../unlock/sha256.c -o build/capture-tests/mailbox-recovery.exe
 if($LASTEXITCODE -ne 0){throw 'Audit recovery compile failed'}
 & ./build/capture-tests/mailbox-recovery.exe
 if($LASTEXITCODE -ne 0){throw 'Audit recovery mocks failed'}
 & $Clang @flags -Wno-unused-function -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST -DGU_AD104_PROM_HEADER_TEST -DGU_AD104_PROM_FULL_TEST -DGU_AD104_FWSEC_AUDIT_TEST -DGU_AD104_FWSEC_REHEARSAL_TEST -DGU_AD104_FWSEC_SIGNATURE_TEST -DGU_AD104_FALCON_OBSERVE_TEST -DGU_AD104_FALCON_MAILBOX_TEST -DGU_AD104_FALCON_CAPS_TEST test_decode_read.c fwsec-audit.c ../unlock/sha256.c -o build/capture-tests/capability-recovery.exe
 if($LASTEXITCODE -ne 0){throw 'Audit recovery compile failed'}
 & ./build/capture-tests/capability-recovery.exe
 if($LASTEXITCODE -ne 0){throw 'Audit recovery mocks failed'}
 & $Clang @flags -Wno-unused-function -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST -DGU_AD104_PROM_HEADER_TEST -DGU_AD104_PROM_FULL_TEST -DGU_AD104_FWSEC_AUDIT_TEST -DGU_AD104_FWSEC_REHEARSAL_TEST -DGU_AD104_FWSEC_SIGNATURE_TEST -DGU_AD104_FALCON_OBSERVE_TEST -DGU_AD104_FALCON_MAILBOX_TEST -DGU_AD104_FALCON_CAPS_TEST -DGU_AD104_FALCON_CORE_TEST test_decode_read.c fwsec-audit.c ../unlock/sha256.c -o build/capture-tests/core-recovery.exe
 if($LASTEXITCODE -ne 0){throw 'Audit recovery compile failed'}
 & ./build/capture-tests/core-recovery.exe
 if($LASTEXITCODE -ne 0){throw 'Audit recovery mocks failed'}
 & $Clang @flags -Wno-unused-function -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST -DGU_AD104_PROM_HEADER_TEST -DGU_AD104_PROM_FULL_TEST -DGU_AD104_FWSEC_AUDIT_TEST -DGU_AD104_FWSEC_REHEARSAL_TEST -DGU_AD104_FWSEC_SIGNATURE_TEST -DGU_AD104_FALCON_OBSERVE_TEST -DGU_AD104_FALCON_MAILBOX_TEST -DGU_AD104_FALCON_CAPS_TEST -DGU_AD104_FALCON_CORE_TEST -DGU_AD104_FALCON_REFUSAL_TEST test_decode_read.c fwsec-audit.c ../unlock/sha256.c -o build/capture-tests/refusal-recovery.exe
 if($LASTEXITCODE -ne 0){throw 'Audit recovery compile failed'}
 & ./build/capture-tests/refusal-recovery.exe
 if($LASTEXITCODE -ne 0){throw 'Audit recovery mocks failed'}
 & $Clang @flags -Wno-unused-function -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST -DGU_AD104_PROM_HEADER_TEST -DGU_AD104_PROM_FULL_TEST -DGU_AD104_FWSEC_AUDIT_TEST -DGU_AD104_FWSEC_REHEARSAL_TEST -DGU_AD104_FWSEC_SIGNATURE_TEST -DGU_AD104_FALCON_OBSERVE_TEST -DGU_AD104_FALCON_MAILBOX_TEST -DGU_AD104_FALCON_CAPS_TEST -DGU_AD104_FALCON_CORE_TEST -DGU_AD104_FALCON_BCR_TEST test_decode_read.c fwsec-audit.c ../unlock/sha256.c -o build/capture-tests/bcr-recovery.exe
 if($LASTEXITCODE -ne 0){throw 'BCR subset compile failed'}
 & ./build/capture-tests/bcr-recovery.exe
 if($LASTEXITCODE -ne 0){throw 'BCR subset mocks failed'}
 $out=& $Clang @flags -Wno-unused-function -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST -DGU_AD104_PROM_HEADER_TEST -DGU_AD104_PROM_FULL_TEST -DGU_AD104_FWSEC_AUDIT_TEST -DGU_AD104_FWSEC_REHEARSAL_TEST -DGU_AD104_FWSEC_SIGNATURE_TEST -DGU_AD104_FALCON_OBSERVE_TEST -DGU_AD104_FALCON_MAILBOX_TEST -DGU_AD104_FALCON_CAPS_TEST -DGU_AD104_FALCON_CORE_TEST -DGU_AD104_FALCON_BCR_TEST -DGU_AD104_FALCON_REFUSAL_TEST -fsyntax-only test_decode_read.c 2>&1 | Out-String
 if($LASTEXITCODE -eq 0 -or $out -notmatch 'BCR observation cannot use'){throw 'BCR mode accepted refusal recovery'}
 $global:LASTEXITCODE=0
 'PASS BCR plus CPUCTL refusal recovery rejected'
 & $Clang @flags -Wno-unused-function -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST -DGU_AD104_PROM_HEADER_TEST -DGU_AD104_PROM_FULL_TEST -DGU_AD104_FWSEC_AUDIT_TEST -DGU_AD104_FWSEC_REHEARSAL_TEST -DGU_AD104_FWSEC_SIGNATURE_TEST -DGU_AD104_FALCON_OBSERVE_TEST -DGU_AD104_FALCON_MAILBOX_TEST -DGU_AD104_FALCON_CAPS_TEST -DGU_AD104_FALCON_CORE_TEST -DGU_AD104_ENTRY_INPUTS_TEST test_decode_read.c fwsec-audit.c ../unlock/sha256.c -o build/capture-tests/entry-inputs-recovery.exe
 if($LASTEXITCODE -ne 0){throw 'Entry input integration compile failed'}
 & ./build/capture-tests/entry-inputs-recovery.exe
 if($LASTEXITCODE -ne 0){throw 'Entry input integration failed'}
 & $Clang @flags test_entry_inputs.c -o build/capture-tests/entry-inputs.exe
 if($LASTEXITCODE -ne 0){throw 'Entry input arithmetic compile failed'}
 & ./build/capture-tests/entry-inputs.exe
 if($LASTEXITCODE -ne 0){throw 'Entry input arithmetic failed'}
 $out=& $Clang @flags -Wno-unused-function -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST -DGU_AD104_PROM_HEADER_TEST -DGU_AD104_PROM_FULL_TEST -DGU_AD104_FWSEC_AUDIT_TEST -DGU_AD104_FWSEC_REHEARSAL_TEST -DGU_AD104_FWSEC_SIGNATURE_TEST -DGU_AD104_FALCON_OBSERVE_TEST -DGU_AD104_FALCON_MAILBOX_TEST -DGU_AD104_FALCON_CAPS_TEST -DGU_AD104_FALCON_CORE_TEST -DGU_AD104_ENTRY_INPUTS_TEST -DGU_AD104_FALCON_BCR_TEST -fsyntax-only test_decode_read.c 2>&1 | Out-String
if($LASTEXITCODE -eq 0 -or $out -notmatch 'Entry input audit excludes'){throw 'Invalid entry audit combination accepted'}
$global:LASTEXITCODE=0
 $out=& $Clang @flags -Wno-unused-function -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST -DGU_AD104_PROM_HEADER_TEST -DGU_AD104_PROM_FULL_TEST -DGU_AD104_FWSEC_AUDIT_TEST -DGU_AD104_FWSEC_REHEARSAL_TEST -DGU_AD104_FWSEC_SIGNATURE_TEST -DGU_AD104_FALCON_OBSERVE_TEST -DGU_AD104_FALCON_MAILBOX_TEST -DGU_AD104_FALCON_CAPS_TEST -DGU_AD104_FALCON_CORE_TEST -DGU_AD104_ENTRY_INPUTS_TEST -DGU_AD104_FALCON_REFUSAL_TEST -fsyntax-only test_decode_read.c 2>&1 | Out-String
if($LASTEXITCODE -eq 0 -or $out -notmatch 'Entry input audit excludes'){throw 'Invalid entry audit combination accepted'}
$global:LASTEXITCODE=0
 'PASS entry audit excludes both refused register modes'
 foreach($name in @('pcd_producer','text_display','text_lifecycle','gop_lifecycle','consumers_lifecycle','owner_child','owner_parent','decode_read')) {
  & $Clang @flags -Wno-unused-function "test_$name.c" ../unlock/sha256.c -o "build/capture-tests/$name.exe"
  if($LASTEXITCODE -ne 0){throw "$name mocks compile failed"}
  & "./build/capture-tests/$name.exe"
  if($LASTEXITCODE -ne 0){throw "$name mocks failed"}
 }
 & $Clang @flags -Wno-unused-function test_ad104_edid.c ../unlock/sha256.c -o build/capture-tests/edid.exe
 if($LASTEXITCODE -ne 0){throw 'EDID mocks compile failed'}
 & ./build/capture-tests/edid.exe
 if($LASTEXITCODE -ne 0){throw 'EDID mocks failed'}
 & $Clang @flags -DGU_DIAGNOSTIC_ONLY -DGU_USB_PROBE test_mbr_handoff.c ../unlock/sha256.c -o build/capture-tests/mbr.exe
 if($LASTEXITCODE -ne 0){throw 'MBR mocks compile failed'}
 & ./build/capture-tests/mbr.exe
 if($LASTEXITCODE -ne 0){throw 'MBR mocks failed'}
 & $Clang @flags -DGU_DIAGNOSTIC_ONLY -DGU_USB_PROBE test_edid_handoff.c ../unlock/sha256.c -o build/capture-tests/edid-handoff.exe
 if($LASTEXITCODE -ne 0){throw 'EDID handoff mocks compile failed'}
 & ./build/capture-tests/edid-handoff.exe
 if($LASTEXITCODE -ne 0){throw 'EDID handoff mocks failed'}
 & $Clang @flags -DGU_DIAGNOSTIC_ONLY -DGU_USB_PROBE -DGU_AD104_TEXT_TEST -DGU_AD104_DECODE_READ_TEST test_edid_handoff.c ../unlock/sha256.c -o build/capture-tests/decode-handoff.exe
 if($LASTEXITCODE -ne 0){throw 'Decode handoff compile failed'}
 & ./build/capture-tests/decode-handoff.exe
 if($LASTEXITCODE -ne 0){throw 'Decode handoff checks failed'}
 & $Clang @flags -DGU_DIAGNOSTIC_ONLY -DGU_USB_PROBE -DGU_AD104_TEXT_TEST -DGU_AD104_DECODE_READ_TEST -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST -DGU_AD104_PROM_HEADER_TEST -DGU_AD104_PROM_FULL_TEST test_edid_handoff.c ../unlock/sha256.c -o build/capture-tests/full-prom-handoff.exe
 if($LASTEXITCODE -ne 0){throw 'Full PROM handoff compile failed'}
 & ./build/capture-tests/full-prom-handoff.exe
 if($LASTEXITCODE -ne 0){throw 'Full PROM handoff checks failed'}
 & $Clang @flags -DGU_DIAGNOSTIC_ONLY -DGU_USB_PROBE -DGU_AD104_TEXT_TEST -DGU_AD104_DECODE_READ_TEST -DGU_AD104_DECODE_OFF_TEST -DGU_AD104_DELAYED_READ_TEST -DGU_AD104_PROM_HEADER_TEST -DGU_AD104_PROM_FULL_TEST -DGU_AD104_FALCON_REFUSAL_TEST test_edid_handoff.c ../unlock/sha256.c -o build/capture-tests/refusal-handoff.exe
 if($LASTEXITCODE -ne 0){throw 'Full PROM handoff compile failed'}
 & ./build/capture-tests/refusal-handoff.exe
 if($LASTEXITCODE -ne 0){throw 'Full PROM handoff checks failed'}
 & $Clang @flags -DGU_DIAGNOSTIC_ONLY -DGU_USB_PROBE -DGU_AD104_TEXT_TEST test_edid_handoff.c ../unlock/sha256.c -o build/capture-tests/text-handoff.exe
 if($LASTEXITCODE -ne 0){throw 'Text handoff mocks compile failed'}
 & ./build/capture-tests/text-handoff.exe
 if($LASTEXITCODE -ne 0){throw 'Text handoff mocks failed'}
 & $Clang @flags test_windows_media.c -o build/capture-tests/gpt.exe
 if($LASTEXITCODE -ne 0){throw 'GPT regression compile failed'}
 & ./build/capture-tests/gpt.exe
 if($LASTEXITCODE -ne 0){throw 'GPT regression failed'}
 $ErrorActionPreference='Continue'
 foreach($bad in @('-DGU_FULL_UNLOCK','-DGU_UEFI_PORT','-DGU_INTERNAL_BOOT','-DGU_HEADLESS_RESEARCH','-DGU_EXPERIMENTAL_3090','-DGU_CONSOLE_PREPARE','-DGU_AUTOMATIC_BOOT')) {
  $out=& $Clang @flags @ad104 $bad -fsyntax-only probe.c 2>&1 | Out-String
  if($LASTEXITCODE -eq 0 -or $out -notmatch 'AD104 capture is a separate read-only'){throw "AD104 compile guard failed: $bad"}
  $out=& $Clang @flags -DGU_DIAGNOSTIC_ONLY -DGU_USB_PROBE $bad -fsyntax-only test_mbr_handoff.c 2>&1 | Out-String
  if($LASTEXITCODE -eq 0 -or $out -notmatch 'MBR Windows volume selection is diagnostic-only'){throw "MBR compile guard failed: $bad"}
 }
 $out=& $Clang @flags -DGU_PROBE_AD104_MSI -fsyntax-only probe.c 2>&1 | Out-String
 if($LASTEXITCODE -eq 0 -or $out -notmatch 'AD104 capture is a separate read-only'){throw 'AD104 accepted outside diagnostic mode'}
 $out=& $Clang @flags -DGU_USB_PROBE -fsyntax-only test_mbr_handoff.c 2>&1 | Out-String
 if($LASTEXITCODE -eq 0 -or $out -notmatch 'MBR Windows volume selection is diagnostic-only'){throw 'MBR accepted outside diagnostic mode'}
 $ErrorActionPreference='Stop'
 $global:LASTEXITCODE=0
 $out=& $Clang @flags -DGU_PROBE_AD104_METADATA -fsyntax-only probe.c 2>&1 | Out-String
 if($LASTEXITCODE -eq 0 -or $out -notmatch 'AD104 metadata requires'){throw 'Metadata accepted without AD104 guard'}
 $global:LASTEXITCODE=0
 'PASS metadata-without-AD104 compile combination rejected'
 $out=& $Clang @flags -DGU_PROBE_AD104_FIRMROM -fsyntax-only probe.c 2>&1 | Out-String
 if($LASTEXITCODE -eq 0 -or $out -notmatch 'Firmware ROM capture requires'){throw 'Firmware ROM accepted without metadata guard'}
 $global:LASTEXITCODE=0
 'PASS firmware-ROM-without-metadata compile combination rejected'
 $out=& $Clang @flags -DGU_PROBE_AD104_RESOURCES -fsyntax-only probe.c 2>&1 | Out-String
 if($LASTEXITCODE -eq 0 -or $out -notmatch 'Resource observation requires'){throw 'Resources accepted without metadata guard'}
 $global:LASTEXITCODE=0
 'PASS resources-without-metadata compile combination rejected'
 $out=& $Clang @flags @ad104 -DGU_PROBE_AD104_METADATA -DGU_PROBE_AD104_RESOURCES -DGU_PROBE_AD104_FIRMROM -fsyntax-only probe.c 2>&1 | Out-String
 if($LASTEXITCODE -eq 0 -or $out -notmatch 'Resource observation is separate'){throw 'Resource and ROM modes combined'}
 $global:LASTEXITCODE=0
 'PASS combined resources/firmware-ROM compile combination rejected'
 $out=& $Clang @flags -DGU_PROBE_AD104_PREFLIGHT -fsyntax-only probe.c 2>&1 | Out-String
 if($LASTEXITCODE -eq 0 -or $out -notmatch 'AD104 preflight requires'){throw 'Preflight accepted without resource observation mode'}
 $global:LASTEXITCODE=0
 'PASS preflight-without-resources compile combination rejected'
 $out=& $Clang @flags -DGU_AD104_EDID_TEST -fsyntax-only probe.c 2>&1 | Out-String
 if($LASTEXITCODE -eq 0 -or $out -notmatch 'EDID test requires'){throw 'EDID test accepted without exact preflight and handoff'}
 $global:LASTEXITCODE=0
 'PASS EDID-without-preflight/handoff compile combination rejected'
 $out=& $Clang @flags -DGU_AD104_OUTPUT_OBSERVATION -fsyntax-only probe.c 2>&1 | Out-String
 if($LASTEXITCODE -eq 0 -or $out -notmatch 'Output observation requires'){throw 'Output observation accepted without preflight'}
 $global:LASTEXITCODE=0
 $out=& $Clang @flags @ad104 -DGU_PROBE_AD104_METADATA -DGU_PROBE_AD104_RESOURCES -DGU_PROBE_AD104_PREFLIGHT -DGU_DIAGNOSTIC_MBR -DGU_AD104_OUTPUT_OBSERVATION -DGU_AD104_EDID_TEST -fsyntax-only probe.c 2>&1 | Out-String
 if($LASTEXITCODE -eq 0 -or $out -notmatch 'Output observation requires'){throw 'Output observation accepted with mutation'}
 $global:LASTEXITCODE=0
 'PASS two invalid output-observation compile combinations rejected'
 foreach($bad in @('-DGU_AD104_PCD_CAPTURE','-DGU_AD104_TEXT_TEST')) {
  $out=& $Clang @flags $bad -fsyntax-only probe.c 2>&1 | Out-String
  if($LASTEXITCODE -eq 0 -or $out -notmatch 'PCD capture requires|Text test requires'){throw "Capture/text compile guard failed: $bad"}
  $global:LASTEXITCODE=0
 }
 'PASS two invalid PCD/text compile combinations rejected'
 $out=& $Clang @flags -DGU_AD104_GOP_TEST -fsyntax-only probe.c 2>&1 | Out-String
 if($LASTEXITCODE -eq 0 -or $out -notmatch 'GOP consumer test requires'){throw 'GOP test accepted without text lifecycle'}
 $global:LASTEXITCODE=0
 'PASS invalid GOP-without-text build combination rejected'
 foreach($bad in @('-DGU_AD104_CONSUMERS_TEST','-DGU_AD104_OWNER_CHILD_TEST','-DGU_AD104_PARENT_TEST','-DGU_AD104_DECODE_READ_TEST')) {
  $out=& $Clang @flags $bad -fsyntax-only probe.c 2>&1 | Out-String
  if($LASTEXITCODE -eq 0 -or $out -notmatch 'Consumers test requires|Owner child test requires|Parent test requires|Decode read test requires'){throw 'Advanced lifecycle compile dependency guard failed'}
  $global:LASTEXITCODE=0
 }
 'PASS consumers/owner-child dependency guards'
 'PASS 16 incompatible/non-diagnostic build combinations rejected'
} finally {Pop-Location}
