# Host-only checks of the shared-ESP Prepare admission branch. No hardware access.
param([string]$Clang='clang',[string]$LldLink='lld-link')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'));Push-Location $repo
try {
 $out='internal-boot/boot/build/shared-esp-audit';New-Item -ItemType Directory -Force $out | Out-Null
 if(!(Test-Path 'internal-boot/boot/build/evga-text-stop/platform-config.h')){throw 'Build the EVGA image first to configure the ESP GUID'}
 $includes=@('-I','deps/edk2/MdePkg/Include','-I','deps/edk2/MdePkg/Include/X64')
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -fshort-wchar -ffunction-sections -O1 -Wall -Wextra -Werror @includes -c internal-boot/boot/test_shared_esp_prepare.c -o "$out/prepare-test.obj"
 if($LASTEXITCODE -ne 0){throw 'Prepare mock compile failed'}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror @includes -c internal-boot/efi_runtime.c -o "$out/prepare-runtime.obj"
 if($LASTEXITCODE -ne 0){throw 'Runtime compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 /opt:ref "/out:$out/prepare-test.exe" "$out/prepare-test.obj" "$out/prepare-runtime.obj"
 if($LASTEXITCODE -ne 0){throw 'Prepare mock link failed'}
 & "./$out/prepare-test.exe"
 if($LASTEXITCODE -ne 0){throw "Prepare mock failure $LASTEXITCODE"}
 'Shared ESP Prepare mock passed five admission cases; no hardware accessed.'
}finally{Pop-Location}
