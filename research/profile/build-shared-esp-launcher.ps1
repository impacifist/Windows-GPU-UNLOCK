# GPL-2.0-only. Build launcher only; no USB or firmware changes.
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[string]$Clang='clang',[string]$LldLink='lld-link',[string]$Python='python')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
& (Join-Path $PSScriptRoot 'build-shared-esp-audit.ps1') -WindowsEspGuid $WindowsEspGuid -Clang $Clang -LldLink $LldLink -Python $Python
Push-Location (Join-Path $repo 'internal-boot/boot')
try {
 $out='build/shared-esp-audit'
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -DGU_SHARED_ESP_AUDIT -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 -c shared-esp-launcher.c -o "$out/launcher.obj"
 if($LASTEXITCODE -ne 0){throw 'Launcher compilation failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf "/map:$out/LAUNCHER.map" "/out:$out/LAUNCHER.EFI" "$out/launcher.obj"
 if($LASTEXITCODE -ne 0){throw 'Launcher link failed'}
 & $Python -I ../unlock/verify_pe.py "$out/LAUNCHER.EFI" "$out/LAUNCHER.map" --output "$out/launcher-pe-verification.json"
 if($LASTEXITCODE -ne 0){throw 'Launcher PE verification failed'}
 [ordered]@{Mode='shared-esp-usb-launcher';AuditFileName='EVGA-SHARED-AUDIT-VISIBLE.EFI';AuditEfiSha256=(Get-FileHash "$out/AUDIT.EFI").Hash;WindowsEspGuid=$WindowsEspGuid.ToString();EfiSha256=(Get-FileHash "$out/LAUNCHER.EFI").Hash;LauncherSourceSha256=(Get-FileHash 'shared-esp-launcher.c').Hash;AuditSourceSha256=(Get-FileHash 'shared-esp-audit.c').Hash;MediaSha256=(Get-FileHash 'media.h').Hash} | ConvertTo-Json | Set-Content "$out/launcher-manifest.json" -Encoding utf8
 'Launcher built; no USB or firmware changes.'
} finally {Pop-Location}
