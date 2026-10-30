# GPL-2.0-only. RAM-only captured-configuration model, without firmware assets.
param([string]$Clang='clang',[string]$VcVars)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'host.ps1')
Import-GuHostCompiler $VcVars
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out=Join-Path $PSScriptRoot 'build/capture-model'
[void][IO.Directory]::CreateDirectory($out)
Push-Location (Join-Path $repo 'internal-boot/unlock')
try {
 $flags=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function',
  '-D_CRT_SECURE_NO_WARNINGS','-D__KERNEL__','-DGU_UEFI_PORT','-DGU_EXPERIMENTAL_3090','-DGU_ENTRY_LATCHED_SS1_FULL',
  '-Icompat','-I../../deps/edk2/MdePkg/Include','-I../../deps/edk2/MdePkg/Include/X64')
 $sources=@('tests/test_capture_model.c','checks.c','port.c','compat.c','vendor/unlock_610.c',
  'vendor/prep_610.c','vendor/layout_610.c','vendor/vbios_610.c','vendor/gfw_eval.c')
 # These profiles have pinned FWSEC fixtures in the retained transaction test.
 # Do not bypass that requirement for profiles without a transaction fixture.
 foreach($profile in @('B650M_3090FE','X870_3090TI','B660PLUS_3060','H610M_3060')) {
  $exe=Join-Path $out ($profile+'.exe')
  & $Clang @flags "-DGU_PROFILE_$profile" @sources -o $exe
  if($LASTEXITCODE -ne 0){throw "Capture model compile failed: $profile"}
  & $exe
  if($LASTEXITCODE -ne 0){throw "Capture model failed: $profile"}
 }
} finally {Pop-Location}
