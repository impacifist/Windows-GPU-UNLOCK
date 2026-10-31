# GPL-2.0-only. Builds a no-GPU, no-write EFI. Never stages or changes boot entries.
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[string]$Clang='clang',[string]$LldLink='lld-link',[string]$Python='python')
$ErrorActionPreference='Stop'
if($WindowsEspGuid -eq [guid]::Empty){throw 'An exact Windows ESP GUID is required'}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Push-Location (Join-Path $repo 'internal-boot/boot')
try {
 $out='build/shared-esp-retain';New-Item -ItemType Directory -Path $out -Force | Out-Null
 $b=$WindowsEspGuid.ToByteArray();$tail=($b[8..15] | ForEach-Object {'0x{0:x2}' -f $_}) -join ','
 $guidC='{{0x{0:x8},0x{1:x4},0x{2:x4},{{{3}}}}}' -f [BitConverter]::ToUInt32($b,0),[BitConverter]::ToUInt16($b,4),[BitConverter]::ToUInt16($b,6),$tail
 @(('#define GU_INTERNAL_PARTITION_INITIALIZER '+$guidC),('#define GU_WINDOWS_PARTITION_INITIALIZER '+$guidC)) | Set-Content "$out/platform-config.h" -Encoding ascii
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -DGU_SHARED_ESP_RETAIN -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 -c shared-esp-retain.c -o "$out/audit.obj"
 if($LASTEXITCODE -ne 0){throw 'EFI compile failed'}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -Os -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 -c ../unlock/sha256.c -o "$out/sha256.obj"
 if($LASTEXITCODE -ne 0){throw "SHA compile failed"}
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -Os -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 -c ../efi_runtime.c -o "$out/runtime.obj"
 if($LASTEXITCODE -ne 0){throw 'Runtime compile failed'}
 & $LldLink /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf "/map:$out/AUDIT.map" "/out:$out/AUDIT.EFI" "$out/audit.obj" "$out/sha256.obj" "$out/runtime.obj"
 if($LASTEXITCODE -ne 0){throw 'EFI link failed'}
 & $Python -I ../unlock/verify_pe.py "$out/AUDIT.EFI" "$out/AUDIT.map" --output "$out/pe-verification.json"
 if($LASTEXITCODE -ne 0){throw 'EFI PE verification failed'}
 [ordered]@{Mode='shared-esp-no-gpu-retain';AuditFileName='EVGA-SHARED-RETAIN.EFI';Visible=$true;HardwareTested=$false;Staged=$false;WindowsEspGuid=$WindowsEspGuid.ToString();EfiSha256=(Get-FileHash "$out/AUDIT.EFI" -Algorithm SHA256).Hash.ToLowerInvariant();SourceSha256=(Get-FileHash 'shared-esp-retain.c' -Algorithm SHA256).Hash.ToLowerInvariant();BaseAuditSha256=(Get-FileHash 'shared-esp-audit.c').Hash;Sha256SourceSha256=(Get-FileHash '../unlock/sha256.c').Hash;AssetsSourceSha256=(Get-FileHash 'shared-esp-assets.c').Hash;RuntimeSha256=(Get-FileHash '../efi_runtime.c').Hash;MediaSha256=(Get-FileHash 'media.h' -Algorithm SHA256).Hash.ToLowerInvariant()} | ConvertTo-Json | Set-Content "$out/manifest.json" -Encoding utf8
 Write-Output 'Built shared ESP audit EFI; not staged, registered or boot-tested.'
} finally {Pop-Location}
