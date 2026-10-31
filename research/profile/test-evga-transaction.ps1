# Development-only adapter replay. EFI protocols and unlock core are mocked.
param([string]$Clang='clang',[string]$LldLink='lld-link')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Push-Location $repo
try {
 $out='internal-boot/boot/build/evga-text-stop';New-Item -ItemType Directory -Path $out -Force | Out-Null
 $flags=@('-target','x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fno-builtin','-fno-stack-protector','-fshort-wchar','-O1','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-D__KERNEL__','-D__STDC_HOSTED__=1','-Wno-builtin-macro-redefined','-DGU_PROFILE_EVGA_Z790_3090','-DGU_EVGA_TRANSACTION_DEVELOPMENT','-DGU_EVGA_TEST_ONLY','-DGU_EXPERIMENTAL_3090','-DGU_ENTRY_LATCHED_SS1_FULL','-DGU_TARGET_PCI_ID=0x220410deU','-I','internal-boot/unlock','-I','internal-boot/unlock/compat','-I','deps/edk2/MdePkg/Include','-I','deps/edk2/MdePkg/Include/X64')
 $sources=@('internal-boot/unlock/tests/test_evga_transaction.c','internal-boot/unlock/evga-transaction.c','internal-boot/unlock/evga-admission.c','internal-boot/unlock/checks.c','internal-boot/unlock/runtime.c');$objects=@()
 foreach($src in $sources){$obj="$out/adapter-"+[IO.Path]::GetFileNameWithoutExtension($src)+'.obj';& $Clang @flags -c $src -o $obj;if($LASTEXITCODE -ne 0){throw "Compile failed: $src"};$objects+=$obj}
 & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 "/out:$out/adapter-test.exe" @objects
 if($LASTEXITCODE -ne 0){throw 'Link failed'}
 & "./$out/adapter-test.exe"
 if($LASTEXITCODE -ne 0){throw "Adapter scenario failed: $LASTEXITCODE"}
 'Synthetic adapter success, refusal, one-shot and containment scenarios passed; hardware, PROM hash and vendor transaction mocked.'
} finally {Pop-Location}
