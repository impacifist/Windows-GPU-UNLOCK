param([string]$Clang='clang',[string]$LldLink='lld-link')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Push-Location (Join-Path $repo 'internal-boot/boot')
try {
 $out='build/shared-esp-integration'
 if(!(Test-Path "$out/platform-config.h")){throw 'Build the audit first'}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -fshort-wchar -O1 -Wall -Wextra -Werror -DGU_SHARED_ESP_INTEGRATION -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 -c test_shared_esp_integration_launcher.c -o "$out/test.obj"
 if($LASTEXITCODE -ne 0){throw 'Mock compile failed'}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 -c ../efi_runtime.c -o "$out/runtime-test.obj"
 if($LASTEXITCODE -ne 0){throw 'Mock runtime compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/test.exe" "$out/test.obj" "$out/runtime-test.obj"
 if($LASTEXITCODE -ne 0){throw 'Mock link failed'}
 & "./$out/test.exe"
 if($LASTEXITCODE -ne 0){throw "Mock case failed: $LASTEXITCODE"}
 'USB launcher ESP selection and handoff mocks passed ten cases; no hardware accessed.'
} finally {Pop-Location}
