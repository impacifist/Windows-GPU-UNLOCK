# GPL-2.0-only. Capture-free, RAM-only tests of PROM reads across service returns.
param([string]$Clang='clang',[string]$VcVars)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $repo 'research/profile/host.ps1')
Import-GuHostCompiler $VcVars
Push-Location -LiteralPath $PSScriptRoot
try {
 New-Item -ItemType Directory -Path build/prom-boundary/include -Force | Out-Null
 @('#define GU_INTERNAL_PARTITION_INITIALIZER {0}', '#define GU_WINDOWS_PARTITION_INITIALIZER {0}') |
   Set-Content -LiteralPath build/prom-boundary/include/platform-config.h -Encoding ascii
 $flags=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS',
   '-I','build/prom-boundary/include','-I','../../deps/edk2/MdePkg/Include','-I','../../deps/edk2/MdePkg/Include/X64')
 foreach($profile in @('B650M_3090FE','B660M_3070TI','X870_3090TI','B660PLUS_3060','H610M_3060','B760MK_3060')) {
  foreach($headless in @($false,$true)) {
   $mode=@();if($headless){$mode=@('-DGU_HEADLESS_RESEARCH','-DGU_SINGLE_RTX_CONSOLE','-DGU_FULL_UNLOCK','-DGU_AUTOMATIC_BOOT')}
   $exe="build/prom-boundary/test-$profile-$headless.exe"
   & $Clang @flags "-DGU_PROFILE_$profile" @mode test_prom_boundary.c console-flow.c ../unlock/checks.c ../unlock/sha256.c -o $exe
   if($LASTEXITCODE -ne 0){throw "PROM boundary test compile failed: $profile headless=$headless"}
   & "./$exe"
   if($LASTEXITCODE -ne 0){throw "PROM boundary tests failed: $profile headless=$headless"}
  }
 }
} finally {Pop-Location}
