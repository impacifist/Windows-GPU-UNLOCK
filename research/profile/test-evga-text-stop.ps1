param([string]$Clang='clang',[string]$LldLink='lld-link')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Push-Location $repo
try {
 $out='internal-boot/boot/build/evga-text-stop';New-Item -ItemType Directory -Force $out | Out-Null
 foreach($source in @('internal-boot/boot/test_evga_text_stop.c','internal-boot/boot/test_evga_gop_capture.c','internal-boot/efi_runtime.c')) {
  $obj="$out/"+[IO.Path]::GetFileNameWithoutExtension($source)+'.host.obj'
  & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 -c $source -o $obj
  if($LASTEXITCODE -ne 0){throw "Host compile failed: $source"}
 }
 foreach($name in @('test_evga_text_stop','test_evga_gop_capture')) {
  & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/$name.exe" "$out/$name.host.obj" "$out/efi_runtime.host.obj"
  if($LASTEXITCODE -ne 0){throw "Host link failed: $name"}
  & "./$out/$name.exe"
  if($LASTEXITCODE -ne 0){throw "Host scenario failed: $name/$LASTEXITCODE"}
 }
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -DGU_EVGA_RELEASE_TEXT -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 -c internal-boot/boot/test_evga_text_stop.c -o "$out/release.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Combined release host compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/release-test.exe" "$out/release.host.obj" "$out/efi_runtime.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Combined release host link failed'}
 & "./$out/release-test.exe"
 if($LASTEXITCODE -ne 0){throw "Combined release scenario failed: $LASTEXITCODE"}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -DGU_EVGA_RELEASE_TEXT -DGU_EVGA_RELEASE_GRAPHICS -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 -c internal-boot/boot/test_evga_text_stop.c -o "$out/graphics.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Graphics release host compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/graphics-test.exe" "$out/graphics.host.obj" "$out/efi_runtime.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Graphics release host link failed'}
 & "./$out/graphics-test.exe"
 if($LASTEXITCODE -ne 0){throw "Graphics release scenario failed: $LASTEXITCODE"}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -DGU_EVGA_RELEASE_TEXT -DGU_EVGA_RELEASE_GRAPHICS -DGU_EVGA_RELEASE_EDID -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 -c internal-boot/boot/test_evga_text_stop.c -o "$out/edid.host.obj"
 if($LASTEXITCODE -ne 0){throw 'EDID host compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/edid-test.exe" "$out/edid.host.obj" "$out/efi_runtime.host.obj"
 if($LASTEXITCODE -ne 0){throw 'EDID host link failed'}
 & "./$out/edid-test.exe"
 if($LASTEXITCODE -ne 0){throw "EDID scenario failed: $LASTEXITCODE"}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -DGU_EVGA_RELEASE_TEXT -DGU_EVGA_RELEASE_GRAPHICS -DGU_EVGA_RELEASE_EDID -DGU_EVGA_RELEASE_EDID_SIBLINGS -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 -c internal-boot/boot/test_evga_text_stop.c -o "$out/siblings.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Sibling host compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/siblings-test.exe" "$out/siblings.host.obj" "$out/efi_runtime.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Sibling host link failed'}
 & "./$out/siblings-test.exe"
 if($LASTEXITCODE -ne 0){throw "Sibling scenario failed: $LASTEXITCODE"}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -DGU_EVGA_RELEASE_TEXT -DGU_EVGA_RELEASE_GRAPHICS -DGU_EVGA_RELEASE_EDID -DGU_EVGA_RELEASE_EDID_SIBLINGS -DGU_EVGA_AUDIT_NVIDIA -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 -c internal-boot/boot/test_evga_text_stop.c -o "$out/nvidia.host.obj"
 if($LASTEXITCODE -ne 0){throw 'NVIDIA audit host compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/nvidia-test.exe" "$out/nvidia.host.obj" "$out/efi_runtime.host.obj"
 if($LASTEXITCODE -ne 0){throw 'NVIDIA audit host link failed'}
 & "./$out/nvidia-test.exe"
 if($LASTEXITCODE -ne 0){throw "NVIDIA audit scenario failed: $LASTEXITCODE"}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -DGU_EVGA_RELEASE_TEXT -DGU_EVGA_RELEASE_GRAPHICS -DGU_EVGA_RELEASE_EDID -DGU_EVGA_RELEASE_EDID_SIBLINGS -DGU_EVGA_AUDIT_NVIDIA -DGU_EVGA_STOP_NVIDIA_CHILDREN -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 -c internal-boot/boot/test_evga_text_stop.c -o "$out/nvidia-children.host.obj"
 if($LASTEXITCODE -ne 0){throw 'NVIDIA children host compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/nvidia-children-test.exe" "$out/nvidia-children.host.obj" "$out/efi_runtime.host.obj"
 if($LASTEXITCODE -ne 0){throw 'NVIDIA children host link failed'}
 & "./$out/nvidia-children-test.exe"
 if($LASTEXITCODE -ne 0){throw "NVIDIA children scenario failed: $LASTEXITCODE"}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -DGU_EVGA_RELEASE_TEXT -DGU_EVGA_RELEASE_GRAPHICS -DGU_EVGA_RELEASE_EDID -DGU_EVGA_RELEASE_EDID_SIBLINGS -DGU_EVGA_AUDIT_NVIDIA -DGU_EVGA_STOP_NVIDIA_CHILDREN -DGU_EVGA_STOP_NVIDIA_PARENT -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 -c internal-boot/boot/test_evga_text_stop.c -o "$out/nvidia-parent.host.obj"
 if($LASTEXITCODE -ne 0){throw 'NVIDIA parent host compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/nvidia-parent-test.exe" "$out/nvidia-parent.host.obj" "$out/efi_runtime.host.obj"
 if($LASTEXITCODE -ne 0){throw 'NVIDIA parent host link failed'}
 & "./$out/nvidia-parent-test.exe"
 if($LASTEXITCODE -ne 0){throw "NVIDIA parent scenario failed: $LASTEXITCODE"}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -DGU_EVGA_RELEASE_TEXT -DGU_EVGA_RELEASE_GRAPHICS -DGU_EVGA_RELEASE_EDID -DGU_EVGA_RELEASE_EDID_SIBLINGS -DGU_EVGA_AUDIT_NVIDIA -DGU_EVGA_STOP_NVIDIA_CHILDREN -DGU_EVGA_STOP_NVIDIA_PARENT -DGU_EVGA_PROBE_DECODE -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 -c internal-boot/boot/test_evga_text_stop.c -o "$out/decode-probe.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Decode probe host compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/decode-probe-test.exe" "$out/decode-probe.host.obj" "$out/efi_runtime.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Decode probe host link failed'}
 & "./$out/decode-probe-test.exe"
 if($LASTEXITCODE -ne 0){throw "Decode probe scenario failed: $LASTEXITCODE"}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -DGU_EVGA_RELEASE_TEXT -DGU_EVGA_RELEASE_GRAPHICS -DGU_EVGA_RELEASE_EDID -DGU_EVGA_RELEASE_EDID_SIBLINGS -DGU_EVGA_AUDIT_NVIDIA -DGU_EVGA_STOP_NVIDIA_CHILDREN -DGU_EVGA_STOP_NVIDIA_PARENT -DGU_EVGA_PROBE_DECODE -DGU_EVGA_PROBE_STATE -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 -c internal-boot/boot/test_evga_text_stop.c -o "$out/state-probe.host.obj"
 if($LASTEXITCODE -ne 0){throw 'State probe host compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/state-probe-test.exe" "$out/state-probe.host.obj" "$out/efi_runtime.host.obj"
 if($LASTEXITCODE -ne 0){throw 'State probe host link failed'}
 & "./$out/state-probe-test.exe"
 if($LASTEXITCODE -ne 0){throw "State probe scenario failed: $LASTEXITCODE"}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -O1 -Wall -Wextra -Werror -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 -c internal-boot/unlock/checks.c -o "$out/shared-checks.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Shared config checks compile failed'}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -DGU_EVGA_RELEASE_TEXT -DGU_EVGA_RELEASE_GRAPHICS -DGU_EVGA_RELEASE_EDID -DGU_EVGA_RELEASE_EDID_SIBLINGS -DGU_EVGA_AUDIT_NVIDIA -DGU_EVGA_STOP_NVIDIA_CHILDREN -DGU_EVGA_STOP_NVIDIA_PARENT -DGU_EVGA_PROBE_DECODE -DGU_EVGA_PROBE_STATE -DGU_EVGA_PROBE_PLATFORM -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 -c internal-boot/boot/test_evga_text_stop.c -o "$out/platform-probe.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Platform probe host compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/platform-probe-test.exe" "$out/platform-probe.host.obj" "$out/efi_runtime.host.obj" "$out/shared-checks.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Platform probe host link failed'}
 & "./$out/platform-probe-test.exe"
 if($LASTEXITCODE -ne 0){throw "Platform probe scenario failed: $LASTEXITCODE"}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -DGU_EVGA_RELEASE_TEXT -DGU_EVGA_RELEASE_GRAPHICS -DGU_EVGA_RELEASE_EDID -DGU_EVGA_RELEASE_EDID_SIBLINGS -DGU_EVGA_AUDIT_NVIDIA -DGU_EVGA_STOP_NVIDIA_CHILDREN -DGU_EVGA_STOP_NVIDIA_PARENT -DGU_EVGA_PROBE_DECODE -DGU_EVGA_PROBE_STATE -DGU_EVGA_PROBE_PLATFORM -DGU_EVGA_PROBE_RESOURCES -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 -c internal-boot/boot/test_evga_text_stop.c -o "$out/resource-integration.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Resource integration compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/resource-integration-test.exe" "$out/resource-integration.host.obj" "$out/efi_runtime.host.obj" "$out/shared-checks.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Resource integration link failed'}
 & "./$out/resource-integration-test.exe"
 if($LASTEXITCODE -ne 0){throw "Resource integration failed: $LASTEXITCODE"}
 '6 resource integration checkpoints passed (mock acquisition callback).'
 $resourceFlags=@('-target','x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fno-builtin','-fno-stack-protector','-O1','-Wall','-Wextra','-Werror','-I','deps/edk2/MdePkg/Include','-I','deps/edk2/MdePkg/Include/X64')
 & $Clang @resourceFlags -DGU_HEADLESS_RESEARCH -DGU_OWNERSHIP_CLAIM_ONLY -c internal-boot/unlock/ownership.c -o "$out/ownership.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Shared ownership compile failed'}
 & $Clang @resourceFlags -c internal-boot/unlock/runtime.c -o "$out/resource-runtime.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Resource runtime compile failed'}
 & $Clang @resourceFlags -c internal-boot/boot/test_evga_resources.c -o "$out/resources.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Resources test compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/resources-test.exe" "$out/resources.host.obj" "$out/ownership.host.obj" "$out/resource-runtime.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Resources test link failed'}
 & "./$out/resources-test.exe"
 if($LASTEXITCODE -ne 0){throw "Resources scenario failed: $LASTEXITCODE"}
 & $Clang @resourceFlags -c internal-boot/unlock/sha256.c -o "$out/asset-sha.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Asset SHA compile failed'}
 & $Clang @resourceFlags -c internal-boot/boot/test_asset_reader.c -o "$out/asset-reader.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Asset reader test compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/asset-reader-test.exe" "$out/asset-reader.host.obj" "$out/asset-sha.host.obj" "$out/resource-runtime.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Asset reader test link failed'}
 & "./$out/asset-reader-test.exe"
 if($LASTEXITCODE -ne 0){throw "Asset reader scenario failed: $LASTEXITCODE"}
 '6 real SHA256 reader vector/refusal scenarios passed (mock EFI file/pool).'
 '13 shared asset/ownership resource scenarios passed (synthetic hash callback, no firmware executed).'
 $unlockFlags=@('-target','x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fno-builtin','-fshort-wchar','-O1','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-D__KERNEL__','-DGU_UEFI_PORT','-DGU_EVGA_UNLOCK','-DGU_EVGA_STOP_ONLY','-DGU_EVGA_RELEASE_TEXT','-DGU_EVGA_RELEASE_GRAPHICS','-DGU_EVGA_RELEASE_EDID','-DGU_EVGA_RELEASE_EDID_SIBLINGS','-DGU_EVGA_AUDIT_NVIDIA','-DGU_EVGA_STOP_NVIDIA_CHILDREN','-DGU_EVGA_STOP_NVIDIA_PARENT','-DGU_EVGA_PROBE_DECODE','-DGU_EVGA_PROBE_STATE','-DGU_EVGA_PROBE_PLATFORM','-DGU_EVGA_PROBE_RESOURCES','-DGU_PROFILE_EVGA_Z790_3090','-DGU_EVGA_TRANSACTION_DEVELOPMENT','-DGU_EXPERIMENTAL_3090','-DGU_ENTRY_LATCHED_SS1_FULL','-DGU_TARGET_PCI_ID=0x220410deU','-I','internal-boot/unlock','-I','internal-boot/unlock/compat','-I','deps/edk2/MdePkg/Include','-I','deps/edk2/MdePkg/Include/X64')
 & $Clang @unlockFlags -c internal-boot/boot/test_evga_text_stop.c -o "$out/unlock-wrapper-test.obj"
 if($LASTEXITCODE -ne 0){throw 'Unlock wrapper mock compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/unlock-wrapper-test.exe" "$out/unlock-wrapper-test.obj" "$out/shared-checks.host.obj" "$out/resource-runtime.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Unlock wrapper mock link failed'}
 & "./$out/unlock-wrapper-test.exe"
 if($LASTEXITCODE -ne 0){throw "Unlock wrapper mock scenario failed: $LASTEXITCODE"}
 foreach($slot in @(5,10)){
  & $Clang @unlockFlags "-DGU_EVGA_ATTEMPT_SLOT=$slot" -c internal-boot/boot/test_evga_text_stop.c -o "$out/unlock-slot-test.obj"
  if($LASTEXITCODE -ne 0){throw "Unlock slot $slot mock compile failed"}
  & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/unlock-slot-test.exe" "$out/unlock-slot-test.obj" "$out/shared-checks.host.obj" "$out/resource-runtime.host.obj"
  if($LASTEXITCODE -ne 0){throw "Unlock slot $slot mock link failed"}
  & "./$out/unlock-slot-test.exe"
  if($LASTEXITCODE -ne 0){throw "Unlock slot $slot mock scenario failed: $LASTEXITCODE"}
 }
 & $Clang @unlockFlags -Wno-unused-function -c internal-boot/boot/test_evga_retained_assets.c -o "$out/retained-assets-test.obj"
 if($LASTEXITCODE -ne 0){throw 'Retained assets mock compile failed'}
 & $Clang @unlockFlags -c internal-boot/unlock/format.c -o "$out/retained-format.obj"
 if($LASTEXITCODE -ne 0){throw 'Retained logger compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/retained-assets-test.exe" "$out/retained-assets-test.obj" "$out/retained-format.obj" "$out/resource-runtime.host.obj"
 if($LASTEXITCODE -ne 0){throw 'Retained assets mock link failed'}
 & "./$out/retained-assets-test.exe"
 if($LASTEXITCODE -ne 0){throw "Retained assets case failed: $LASTEXITCODE"}
 '10 retained-asset success and cleanup checks passed; files/hash mocked.'
 '10 combined unlock wrapper/cleanup cases passed; transaction mocked.'
 '25 membership host checkpoints, 7 combined-release checkpoints, 8 graphics-release checkpoints, 12 EDID checkpoints, 13 sibling checkpoints, 11 NVIDIA audit checkpoints, 11 NVIDIA child checkpoints, 11 NVIDIA parent checkpoints, 8 decode-probe checkpoints, 6 state-probe checkpoints, 5 platform-probe checkpoints and 9 capture regression cases passed (mock protocols, hashes and Stop; no captured firmware executed).'
} finally {Pop-Location}
