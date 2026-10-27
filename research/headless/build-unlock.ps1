# GPL-2.0-only. Isolated exact-platform headless unlock wrapper/child package.
param([string]$Clang='clang',[string]$LldLink='lld-link',[string]$Python='python',[ValidateSet('b650m-3090fe','b660m-3070ti','b760m-3080lhr')][string]$HardwareProfile='b650m-3090fe')
$hwDefine=switch($HardwareProfile){
 'b660m-3070ti'{'-DGU_PROFILE_B660M_3070TI'}
 'b760m-3080lhr'{'-DGU_PROFILE_B760M_3080LHR'}
 default{'-DGU_PROFILE_B650M_3090FE'}}
$hwName=switch($HardwareProfile){
 'b660m-3070ti'{'ASUS-TUF-GAMING-B660M-E-D4-4003-RTX3070TI'}
 'b760m-3080lhr'{'MAXSUN-MS-TERMINATOR-B760M-D4-H7.8G-RTX3080LHR'}
 default{'ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE'}}
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $repo 'internal-boot/install/headless-package.ps1')
$null=Assert-HeadlessLocalConfig $repo
$beforeInputs=Get-HeadlessInputInventory $repo
$unlock=Join-Path $repo 'internal-boot/unlock'
& (Join-Path $unlock 'build.ps1') -ExperimentalUnlock -StockProfile entryLatchedSs1Full -InternalBoot -AutomaticBoot -HeadlessResearch -Clang $Clang -LldLink $LldLink -Python $Python -HardwareProfile $HardwareProfile
if(!$?){throw 'Headless child build failed'}
$child=Join-Path $unlock 'build/experimental/entryLatchedSs1Full/internal/automatic/headless-research/RTX3090-EXPERIMENTAL-entryLatchedSs1Full-INTERNAL-AUTO-HEADLESS-RESEARCH.EFI'
$cm=Get-Content (Join-Path (Split-Path $child) 'build-manifest.json') -Raw | ConvertFrom-Json
$hash=(Get-FileHash -LiteralPath $child).Hash.ToLowerInvariant()
if($cm.Mode -ne 'headless-research-automatic-internal-exact-card-unlock' -or $cm.EfiSha256 -ne $hash -or $cm.StockProfile -ne 'entryLatchedSs1Full' -or !$cm.CanArm){throw 'Headless child contract mismatch'}
Push-Location (Join-Path $repo 'internal-boot/boot')
try {
 [void][IO.Directory]::CreateDirectory((Join-Path $PWD 'build/headless'))
 @('/* Local headless child contract; GPL-2.0-only. */',('#define GU_UNLOCK_SHA256 "'+$hash+'"'),('#define GU_UNLOCK_BYTES '+(Get-Item $child).Length)) | Set-Content build/headless/child-contract.h -Encoding ascii
 $sources=@('handoff.c','full-release.c','full-flow.c','diagnostic.c','../unlock/checks.c','../unlock/sha256.c','../efi_runtime.c')
 $objects=@()
 foreach($source in $sources) {
  $obj='build/headless/'+[IO.Path]::GetFileNameWithoutExtension($source)+'.obj'
  & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -DGU_HEADLESS_RESEARCH -DGU_SINGLE_RTX_CONSOLE -DGU_FULL_UNLOCK -DGU_AUTOMATIC_BOOT $hwDefine -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 -c $source -o $obj
  if($LASTEXITCODE -ne 0){throw "Headless wrapper compile failed: $source"};$objects+=$obj
 }
 & $LldLink /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /map:build/headless/HEADLESS.map /out:build/headless/HEADLESS.EFI @objects
 if($LASTEXITCODE -ne 0){throw 'Headless wrapper link failed'}
 & $Python ../unlock/verify_pe.py build/headless/HEADLESS.EFI build/headless/HEADLESS.map --output build/headless/pe-verification.json
 if($LASTEXITCODE -ne 0){throw 'Headless wrapper PE verification failed'}
 $afterInputs=Get-HeadlessInputInventory $repo
 if(($beforeInputs | ConvertTo-Json -Depth 4 -Compress) -cne ($afterInputs | ConvertTo-Json -Depth 4 -Compress)){throw 'Inputs changed while building; no package manifest will be published'}
 [ordered]@{Format=2;HardwareProfile=$hwName;Mode='single-rtx-headless-unlock-then-windows';CanUnlock=$true;RequiresHumanKey=$false;HardwareTested=$false;ChildSha256=$hash;ChildBytes=(Get-Item $child).Length;ChildManifestSha256=(Get-FileHash -LiteralPath (Join-Path (Split-Path $child) 'build-manifest.json')).Hash;EfiSha256=(Get-FileHash -LiteralPath build/headless/HEADLESS.EFI).Hash;EfiBytes=(Get-Item build/headless/HEADLESS.EFI).Length;Inputs=(Get-HeadlessInputInventory $repo -WithContract);BuiltUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json -Depth 6 | Set-Content build/headless/manifest.json -Encoding utf8
 $null=Assert-HeadlessPackage $repo
} finally {Pop-Location}
