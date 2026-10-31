# Offline C admission replay. The hash callback is mocked; use captured PROM hash separately.
param([string]$Clang='clang',[string]$LldLink='lld-link')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Push-Location $repo
try {
 $out='internal-boot/boot/build/evga-text-stop';New-Item -ItemType Directory -Path $out -Force | Out-Null
 $flags=@('-target','x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fno-builtin','-fno-stack-protector','-O1','-Wall','-Wextra','-Werror','-DGU_PROFILE_EVGA_Z790_3090','-DGU_EVGA_TRANSACTION_DEVELOPMENT','-I','internal-boot/unlock','-I','deps/edk2/MdePkg/Include','-I','deps/edk2/MdePkg/Include/X64')
 foreach($src in @('internal-boot/unlock/evga-admission.c','internal-boot/unlock/checks.c','internal-boot/unlock/tests/test_evga_admission.c','internal-boot/efi_runtime.c')) {
  $obj="$out/admission-"+[IO.Path]::GetFileNameWithoutExtension($src)+'.obj'
  & $Clang @flags -c $src -o $obj
  if($LASTEXITCODE -ne 0){throw "Compile failed: $src"}
 }
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/admission-test.exe" "$out/admission-evga-admission.obj" "$out/admission-checks.obj" "$out/admission-test_evga_admission.obj" "$out/admission-efi_runtime.obj"
 if($LASTEXITCODE -ne 0){throw 'Link failed'}
 & "./$out/admission-test.exe"
 if($LASTEXITCODE -ne 0){throw "Admission case failed: $LASTEXITCODE"}
 'Captured-config and mutated-state checks passed; PROM hash callback mocked; no hardware accessed.'
} finally {Pop-Location}
