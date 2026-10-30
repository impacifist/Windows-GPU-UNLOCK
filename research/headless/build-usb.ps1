# GPL-2.0-only. Headless wrapper/child package for a USB stick (-DGU_USB_MEDIA).
# Same exact-profile child and hash contract as build-unlock.ps1, but the wrapper and
# child accept only the USB device they were loaded from instead of a pinned internal
# ESP GUID. Builds files only: no staging, boot entries or firmware settings change.
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[string]$Clang='clang',[string]$LldLink='lld-link',[string]$Python='python',
 [ValidateSet('b650m-3090fe','b660m-3070ti','x870-3090ti','b660plus-3060','h610m-3060','local')][string]$HardwareProfile='b650m-3090fe')
$profiles=@{
 'b650m-3090fe'=@{Defines=@('-DGU_PROFILE_B650M_3090FE');Name='ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE'}
 'b660m-3070ti'=@{Defines=@('-DGU_PROFILE_B660M_3070TI');Name='ASUS-TUF-GAMING-B660M-E-D4-4003-RTX3070TI'}
 'x870-3090ti'=@{Defines=@('-DGU_PROFILE_X870_3090TI');Name='GIGABYTE-X870-AORUS-ELITE-WIFI7-F13d-RTX3090TI'}
 'h610m-3060'=@{Defines=@('-DGU_PROFILE_H610M_3060');Name='GIGABYTE-H610M-H-FC-RTX3060-12GB'}
 'b660plus-3060'=@{Defines=@('-DGU_PROFILE_B660PLUS_3060');Name='ASUS-PRIME-B660-PLUS-D4-3010-RTX3060'}
}
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$boot=Join-Path $repo 'internal-boot/boot'
$unlock=Join-Path $repo 'internal-boot/unlock'
. (Join-Path $unlock 'local-profile.ps1')
# 'local': this machine's generated profile; refused here and again in build.ps1 without passing evidence.
if($HardwareProfile -eq 'local'){
 $local=Get-LocalProfile $repo;$localEvidence=Assert-LocalProfileEvidence $repo
 $localEvidenceSha=Get-GuSha256 (Join-Path $repo 'research/profile/local/test-results.json')
 $hw=@{Defines=@($local.Define);Name=$local.Name}
}
else{$hw=$profiles[$HardwareProfile]}
function GuidC([guid]$Value){
 $b=$Value.ToByteArray();$tail=($b[8..15] | ForEach-Object {'0x{0:x2}' -f $_}) -join ','
 return ('{{0x{0:x8},0x{1:x4},0x{2:x4},{{{3}}}}}' -f [BitConverter]::ToUInt32($b,0),[BitConverter]::ToUInt16($b,4),[BitConverter]::ToUInt16($b,6),$tail)
}
$windows=@(Get-Partition | Where-Object {$_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($windows.Count -ne 1 -or !$windows[0].IsSystem){throw 'WindowsEspGuid must identify the current system EFI partition'}
$config=Join-Path $boot 'platform-config.h';$marker='/* Local USB configuration (build-usb.ps1). Internal partition unused under GU_USB_MEDIA. */'
if((Test-Path -LiteralPath $config) -and (Get-Content -LiteralPath $config -TotalCount 1) -ne $marker){throw 'platform-config.h belongs to an internal Configure.ps1 setup; move it aside before a USB build'}
@($marker,('#define GU_INTERNAL_PARTITION_INITIALIZER '+(GuidC ([guid]::Empty))),('#define GU_WINDOWS_PARTITION_INITIALIZER '+(GuidC $WindowsEspGuid))) | Set-Content -LiteralPath $config -Encoding ascii
& (Join-Path $unlock 'build.ps1') -ExperimentalUnlock -StockProfile entryLatchedSs1Full -InternalBoot -AutomaticBoot -HeadlessResearch -UsbMedia -HardwareProfile $HardwareProfile -Clang $Clang -LldLink $LldLink -Python $Python
if(!$?){throw 'Headless USB child build failed'}
$childDir=Join-Path $unlock 'build/experimental/entryLatchedSs1Full/internal/automatic/headless-research/usb'
$child=Join-Path $childDir 'RTX3090-EXPERIMENTAL-entryLatchedSs1Full-INTERNAL-AUTO-HEADLESS-RESEARCH-USB.EFI'
$cm=Get-Content (Join-Path $childDir 'build-manifest.json') -Raw | ConvertFrom-Json
$hash=(Get-FileHash -LiteralPath $child).Hash.ToLowerInvariant()
if($cm.Mode -ne 'headless-research-automatic-internal-exact-card-unlock' -or $cm.EfiSha256 -ne $hash -or $cm.StockProfile -ne 'entryLatchedSs1Full' -or !$cm.CanArm -or
   $cm.HardwareProfile -ne $hw.Name -or $cm.Defines -notcontains '-DGU_USB_MEDIA'){throw 'Headless USB child contract mismatch'}
Push-Location -LiteralPath $boot
try {
 [void][IO.Directory]::CreateDirectory((Join-Path $PWD 'build/headless'))
 @('/* Local headless child contract; GPL-2.0-only. */',('#define GU_UNLOCK_SHA256 "'+$hash+'"'),('#define GU_UNLOCK_BYTES '+(Get-Item $child).Length)) | Set-Content build/headless/child-contract.h -Encoding ascii
 $sources=@('handoff.c','full-release.c','full-flow.c','diagnostic.c','../unlock/checks.c','../unlock/sha256.c','../efi_runtime.c')
 $objects=@()
 foreach($source in $sources) {
  $obj='build/headless/'+[IO.Path]::GetFileNameWithoutExtension($source)+'.obj'
  & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -DGU_HEADLESS_RESEARCH -DGU_SINGLE_RTX_CONSOLE -DGU_FULL_UNLOCK -DGU_AUTOMATIC_BOOT -DGU_USB_MEDIA @($hw.Defines) -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 -c $source -o $obj
  if($LASTEXITCODE -ne 0){throw "Headless USB wrapper compile failed: $source"};$objects+=$obj
 }
 & $LldLink /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /map:build/headless/HEADLESS.map /out:build/headless/HEADLESS.EFI @objects
 if($LASTEXITCODE -ne 0){throw 'Headless USB wrapper link failed'}
 & $Python ../unlock/verify_pe.py build/headless/HEADLESS.EFI build/headless/HEADLESS.map --output build/headless/pe-verification.json
 if($LASTEXITCODE -ne 0){throw 'Headless USB wrapper PE verification failed'}
 $usbManifest=[ordered]@{Format=2;HardwareProfile=$hw.Name;Mode='single-rtx-headless-unlock-then-windows-usb';CanUnlock=$true;RequiresHumanKey=$false;HardwareTested=$false;
  ChildSha256=$hash;ChildBytes=(Get-Item $child).Length;EfiSha256=(Get-FileHash -LiteralPath build/headless/HEADLESS.EFI).Hash.ToLowerInvariant();EfiBytes=(Get-Item build/headless/HEADLESS.EFI).Length;
  BuiltUtc=[DateTime]::UtcNow.ToString('o')}
 if($HardwareProfile -eq 'local'){
  $currentEvidence=Assert-LocalProfileEvidence $repo
  if($currentEvidence.TreeSha256 -cne $localEvidence.TreeSha256 -or
     (Get-LocalProfile $repo).HeaderSha256 -cne $local.HeaderSha256 -or
     (Get-GuSha256 (Join-Path $repo 'research/profile/local/test-results.json')) -cne $localEvidenceSha){throw 'Local build inputs or evidence changed while packaging; rerun tests and rebuild'}
  $usbManifest.LocalProfileHeaderSha256=$local.HeaderSha256
  $usbManifest.LocalProfileTreeSha256=$localEvidence.TreeSha256
  $usbManifest.LocalProfileEvidenceSha256=$localEvidenceSha
 }
 $usbManifest | ConvertTo-Json | Set-Content build/headless/usb-manifest.json -Encoding utf8
 Get-Content build/headless/usb-manifest.json
} finally {Pop-Location}
