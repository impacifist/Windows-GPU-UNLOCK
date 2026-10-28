# GPL-2.0-only. Fixture-free host tests for the read-only pre-release cold check and the full-flow gates.
param([string]$Clang='clang',[string]$VcVars='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat',
 [ValidateSet('b650m-3090fe','b660m-3070ti','b660m-3090ti','x870-3090ti','b660plus-3060','h610m-3060','b550-3060','local')][string]$HardwareProfile='b650m-3090fe')
$profiles=@{'b650m-3090fe'=@('-DGU_PROFILE_B650M_3090FE');'b660m-3070ti'=@('-DGU_PROFILE_B660M_3070TI');'b660m-3090ti'=@('-DGU_PROFILE_B660M_3090TI');'x870-3090ti'=@('-DGU_PROFILE_X870_3090TI');'b660plus-3060'=@('-DGU_PROFILE_B660PLUS_3060');'h610m-3060'=@('-DGU_PROFILE_H610M_3060');'b550-3060'=@('-DGU_PROFILE_B550_3060');'local'=@('-DGU_PROFILE_LOCAL')}
$ErrorActionPreference='Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
 $lines=& cmd.exe /d /c "call `"$VcVars`" >nul && set"
 if($LASTEXITCODE -ne 0){throw 'MSVC environment failed'}
 foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process')}}
 New-Item -ItemType Directory -Path build/cold/include -Force | Out-Null
 # Host tests need no partition identity: a zero-GUID header unless a local one exists beside media.h.
 @('/* Host-test placeholder; never used for an EFI build. */','#define GU_INTERNAL_PARTITION_INITIALIZER {0,0,0,{0,0,0,0,0,0,0,0}}','#define GU_WINDOWS_PARTITION_INITIALIZER {0,0,0,{0,0,0,0,0,0,0,0}}') | Set-Content build/cold/include/platform-config.h -Encoding ascii
 $flags=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS','-I','build/cold/include','-I','../../deps/edk2/MdePkg/Include','-I','../../deps/edk2/MdePkg/Include/X64')
 & $Clang @flags @($profiles[$HardwareProfile]) -DGU_HEADLESS_RESEARCH -DGU_SINGLE_RTX_CONSOLE -DGU_FULL_UNLOCK -DGU_AUTOMATIC_BOOT test_cold_precheck.c console-flow.c ../unlock/checks.c ../unlock/sha256.c -o build/cold/test-cold-precheck.exe
 if($LASTEXITCODE -ne 0){throw 'Cold pre-check tests compile failed'}
 & ./build/cold/test-cold-precheck.exe
 if($LASTEXITCODE -ne 0){throw 'Cold pre-check tests failed'}
 & $Clang @flags test_full_flow.c full-flow.c -o build/cold/test-full-flow.exe
 if($LASTEXITCODE -ne 0){throw 'Full flow tests compile failed'}
 & ./build/cold/test-full-flow.exe
 if($LASTEXITCODE -ne 0){throw 'Full flow tests failed'}
} finally {Pop-Location}
