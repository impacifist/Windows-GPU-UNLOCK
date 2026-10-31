# Offline real core replay with captured EVGA config/PROM and verified local assets.
param([string]$Clang='clang',[string]$VcVars='C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/VC/Auxiliary/Build/vcvars64.bat',
 [string]$Prom='', [string]$Assets='')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(!$Prom){$Prom=Join-Path $repo 'research/profile/build/evga3090-input/RTX3090-UNLOCK/PROM.BIN'}
if(!$Assets){$Assets=Join-Path $repo 'research/profile/build/evga-assets/output'}
if(!(Test-Path -LiteralPath $Prom) -or (Get-Item -LiteralPath $Prom).Length -ne 1048576 -or
 (Get-FileHash -LiteralPath $Prom -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'f1c7c6b205d9a5c0626af4f82e28cc377aa16cde8c25c61c77b4b5160b26e71f'){throw 'Captured EVGA PROM missing or mismatched'}
foreach($a in (Get-Content -Raw -LiteralPath (Join-Path $repo 'internal-boot/unlock/assets.json') | ConvertFrom-Json)) {
 $path=Join-Path $Assets $a.Name
 if(!(Test-Path -LiteralPath $path) -or (Get-Item -LiteralPath $path).Length -ne $a.Bytes -or
    (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $a.Sha256){throw "Asset mismatch: $($a.Name)"}
}
. (Join-Path $PSScriptRoot 'host.ps1');Import-GuHostCompiler $VcVars
$out=Join-Path $repo 'internal-boot/boot/build/evga-text-stop';New-Item -ItemType Directory -Path $out -Force | Out-Null
$captured=(Get-Content -Raw -LiteralPath (Join-Path $repo 'evidence/evga-3090-capture-review/sixteenth-efi-run-result.json') | ConvertFrom-Json).runtime_pci_config
if($captured.Count -ne 64){throw 'Expected 64 captured config DWORDs'}
$lines=for($i=0;$i -lt 64;$i+=4){'config[{0:x2}]={1}' -f ($i*4),(($captured[$i..($i+3)] | ForEach-Object {$_ -replace '^0x',''}) -join ' ')}
$config=Join-Path $out 'evga-captured-config.txt';$lines | Set-Content -LiteralPath $config -Encoding ascii
Push-Location (Join-Path $repo 'internal-boot/unlock')
try {
 $flags=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-D_CRT_SECURE_NO_WARNINGS','-D__KERNEL__','-DGU_UEFI_PORT','-DGU_PROFILE_EVGA_Z790_3090','-DGU_EVGA_TRANSACTION_DEVELOPMENT','-DGU_EXPERIMENTAL_3090','-DGU_ENTRY_LATCHED_SS1_FULL','-DGU_TARGET_PCI_ID=0x220410deU','-Icompat','-I../../deps/edk2/MdePkg/Include','-I../../deps/edk2/MdePkg/Include/X64')
 $src=@('tests/test_efi_transaction.c','port.c','compat.c','vendor/unlock_610.c','vendor/prep_610.c','vendor/layout_610.c','vendor/vbios_610.c','vendor/gfw_eval.c')
 $exe=Join-Path $out 'evga-real-core-host.exe';& $Clang @flags @src -o $exe
 if($LASTEXITCODE -ne 0){throw 'Core host compile failed'}
 & $exe $Prom $Assets $config
 if($LASTEXITCODE -ne 0){throw "Core replay failed: $LASTEXITCODE"}
} finally {Pop-Location}
