# GPL-2.0-only. Production PCI save/FLR/restore with RAM-only EFI services.
param([string]$Clang='clang',[string]$VcVars)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'host.ps1')
Import-GuHostCompiler $VcVars
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out=Join-Path $PSScriptRoot 'build/pci-layout'
[void][IO.Directory]::CreateDirectory($out)
Push-Location (Join-Path $repo 'internal-boot/unlock')
try {
 $flags=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS',
  '-DGU_HOST_WRITE_TEST','-I../../deps/edk2/MdePkg/Include','-I../../deps/edk2/MdePkg/Include/X64')
 foreach($profile in @('B650M_3090FE','B660M_3070TI','B660M_3090TI','X870_3090TI','B660PLUS_3060','H610M_3060')) {
  $exe=Join-Path $out ($profile+'.exe')
  & $Clang @flags "-DGU_PROFILE_$profile" tests/test_pci_layout.c checks.c port.c -o $exe
  if($LASTEXITCODE -ne 0){throw "PCI layout compile failed: $profile"}
  & $exe
  if($LASTEXITCODE -ne 0){throw "PCI layout failed: $profile"}
 }
} finally {Pop-Location}
