param([string]$Clang='clang',[string]$LldLink='lld-link')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'));Push-Location $repo
try {
 $out='internal-boot/boot/build/shared-esp-qualification'
 $flags=@('-target','x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fno-builtin','-fno-stack-protector','-O1','-Wall','-Wextra','-Werror','-I','deps/edk2/MdePkg/Include','-I','deps/edk2/MdePkg/Include/X64')
 $defines=@('-DGU_SHARED_ESP_QUAL','-DGU_EVGA_RELEASE_TEXT','-DGU_EVGA_RELEASE_GRAPHICS','-DGU_EVGA_RELEASE_EDID','-DGU_EVGA_RELEASE_EDID_SIBLINGS','-DGU_EVGA_AUDIT_NVIDIA','-DGU_EVGA_STOP_NVIDIA_CHILDREN','-DGU_EVGA_STOP_NVIDIA_PARENT','-DGU_EVGA_PROBE_DECODE','-DGU_EVGA_PROBE_STATE','-DGU_EVGA_PROBE_PLATFORM','-DGU_EVGA_PROBE_RESOURCES')
 & $Clang @flags @defines -c internal-boot/boot/test_evga_text_stop.c -o "$out/resources-mock.obj"
 if($LASTEXITCODE -ne 0){throw 'Resources mock compile failed'}
 foreach($unit in @(@('internal-boot/efi_runtime.c','runtime-mock'),@('internal-boot/unlock/checks.c','checks-mock'))){
  & $Clang @flags -c $unit[0] -o "$out/$($unit[1]).obj"
  if($LASTEXITCODE -ne 0){throw 'Mock dependency compile failed'}
 }
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/resources-mock.exe" "$out/resources-mock.obj" "$out/runtime-mock.obj" "$out/checks-mock.obj"
 if($LASTEXITCODE -ne 0){throw 'Resources mock link failed'}
 & "./$out/resources-mock.exe"
 if($LASTEXITCODE -ne 0){throw "Resources mock failed $LASTEXITCODE"}
 'QUAL01 release/resources wrapper checkpoints passed; firmware, Stop and hardware callbacks mocked.'
} finally {Pop-Location}
