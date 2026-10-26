# GPL-2.0-only. Synthetic helper tests only: no real config, registry or firmware.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'headless-package.ps1')
$script:Checks=0
$testRoot=Join-Path $PSScriptRoot ('build/headless-tests/'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($testRoot)
$childRelative='internal-boot/unlock/build/experimental/entryLatchedSs1Full/internal/automatic/headless-research/RTX3090-EXPERIMENTAL-entryLatchedSs1Full-INTERNAL-AUTO-HEADLESS-RESEARCH.EFI'
$packageRelative='internal-boot/boot/build/headless/manifest.json'
$childManifestRelative=(Split-Path $childRelative -Parent)+'/build-manifest.json'

function Check([bool]$Condition,[string]$Name){
 $script:Checks++
 if(!$Condition){throw ('FAIL: '+$Name)}
}
function Reject([string]$Name,[scriptblock]$Action,[string]$Message){
 $caught=$false
 try{& $Action | Out-Null}catch{
  $caught=$true
  if($_.Exception.Message -notmatch $Message){throw ('FAIL: '+$Name+' reached unexpected failure: '+$_.Exception.Message)}
 }
 Check $caught $Name
}
function FixturePath([string]$Fixture,[string]$Relative){
 $p=[IO.Path]::GetFullPath((Join-Path $Fixture $Relative))
 $prefix=[IO.Path]::GetFullPath($testRoot).TrimEnd([char]92)+[IO.Path]::DirectorySeparatorChar
 if(!$p.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Fixture path escapes ignored test root'}
 return $p
}
function Put([string]$Fixture,[string]$Relative,[string]$Text){
 $p=FixturePath $Fixture $Relative
 [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($p))
 [IO.File]::WriteAllText($p,$Text,[Text.UTF8Encoding]::new($false))
}
function Hash([string]$Fixture,[string]$Relative){return (Get-FileHash -LiteralPath (FixturePath $Fixture $Relative) -Algorithm SHA256).Hash.ToLowerInvariant()}
function ReadJson([string]$Fixture,[string]$Relative){return Get-Content -LiteralPath (FixturePath $Fixture $Relative) -Raw | ConvertFrom-Json}
function PutJson([string]$Fixture,[string]$Relative,$Value){Put $Fixture $Relative (ConvertTo-Json -InputObject $Value -Depth 10)}
function SealInputs([string]$Fixture){
 $m=ReadJson $Fixture $packageRelative
 $m.Inputs=@(Get-HeadlessInputInventory $Fixture -WithContract)
 PutJson $Fixture $packageRelative $m
}
function SealChildManifest([string]$Fixture){
 $m=ReadJson $Fixture $packageRelative
 $m.ChildManifestSha256=Hash $Fixture $childManifestRelative
 PutJson $Fixture $packageRelative $m
}
function NewFixture([string]$Name){
 $f=Join-Path $testRoot $Name
 $header=@'
/* Synthetic partitions, never a real machine configuration. */
#define GU_INTERNAL_PARTITION_INITIALIZER {0x11111111,0x2222,0x3333,{0x44,0x44,0x55,0x55,0x55,0x55,0x55,0x55}}
#define GU_WINDOWS_PARTITION_INITIALIZER {0xaaaaaaaa,0xbbbb,0xcccc,{0xdd,0xdd,0xee,0xee,0xee,0xee,0xee,0xee}}
'@
 Put $f 'internal-boot/boot/platform-config.h' $header
 Put $f 'internal-boot/boot/media.h' '/* synthetic boot include */'
 Put $f 'internal-boot/boot/handoff.c' '/* synthetic wrapper source */'
 Put $f 'internal-boot/unlock/app.c' '/* synthetic child source */'
 Put $f 'internal-boot/unlock/port.h' '/* synthetic child header */'
 Put $f 'internal-boot/install/build.ps1' '# synthetic build script'
 Put $f 'deps/edk2/MdePkg/Include/Uefi.h' '/* synthetic external ABI include */'
 Put $f 'deps/edk2/MdePkg/Include/Protocol/PciIo.h' '/* synthetic PCI protocol include */'
 Put $f 'research/headless/build-unlock.ps1' '# synthetic package builder'
 $cfg=[ordered]@{
  UnlockDriveLetter='Z';UnlockPartitionGuid='{11111111-2222-3333-4444-555555555555}'
  WindowsPartitionGuid='{aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee}'
  UnlockDiskUniqueId='SYNTHETIC-ONLY';UnlockOffset=1048576;UnlockSize=1GB
  GeneratedHeaderSha256=(Hash $f 'internal-boot/boot/platform-config.h')
 }
 PutJson $f 'internal-boot/local-config.json' $cfg
 $assets=@('booter_load_ga102.bin','v67-ga102-610.43.03.bin','gsp_ga10x.bin','gsp_boot_ga102.bin','gsp_boot_desc_ga102.bin') | ForEach-Object {
  [ordered]@{Name=$_;Bytes=1;Sha256=('a'*64)}
 }
 PutJson $f 'internal-boot/unlock/assets.json' @($assets)
 Put $f $childRelative 'SYNTHETIC CHILD BYTES: not an EFI application'
 Put $f 'internal-boot/boot/build/headless/HEADLESS.EFI' 'SYNTHETIC WRAPPER BYTES: not an EFI application'
 $childHash=Hash $f $childRelative
 $childSize=(Get-Item -LiteralPath (FixturePath $f $childRelative)).Length
 Put $f 'internal-boot/boot/build/headless/child-contract.h' ("#define GU_UNLOCK_SHA256 `"$childHash`"`n#define GU_UNLOCK_BYTES $childSize`n")
 $cm=[ordered]@{
  Mode='headless-research-automatic-internal-exact-card-unlock';StockProfile='entryLatchedSs1Full';EfiSha256=$childHash
  Sources=@([ordered]@{Path='app.c';Sha256=(Hash $f 'internal-boot/unlock/app.c')})
  Headers=@([ordered]@{Path='port.h';Sha256=(Hash $f 'internal-boot/unlock/port.h')})
 }
 PutJson $f $childManifestRelative $cm
 $m=[ordered]@{
  Format=2;HardwareProfile='ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE'
  Mode='single-rtx-headless-unlock-then-windows';CanUnlock=$true;RequiresHumanKey=$false
  EfiSha256=(Hash $f 'internal-boot/boot/build/headless/HEADLESS.EFI')
  EfiBytes=(Get-Item -LiteralPath (FixturePath $f 'internal-boot/boot/build/headless/HEADLESS.EFI')).Length
  ChildSha256=$childHash;ChildBytes=$childSize;ChildManifestSha256=(Hash $f $childManifestRelative)
  Inputs=@(Get-HeadlessInputInventory $f -WithContract)
 }
 PutJson $f $packageRelative $m
 return $f
}

# These tests validate coherent provenance, not executable PE contents or hardware.
$z690=Get-HeadlessProfileIdentity 'ASUS-ROG-STRIX-Z690-I-4601-RTX3090TI'
Check ($z690.Board -ceq 'ROG STRIX Z690-I GAMING WIFI' -and $z690.Bios -ceq '4601') 'Z690-I identity pins board and BIOS'
$z690Fixture=NewFixture 'z690i'
$z690Manifest=ReadJson $z690Fixture $packageRelative
$z690Manifest.HardwareProfile='ASUS-ROG-STRIX-Z690-I-4601-RTX3090TI'
PutJson $z690Fixture $packageRelative $z690Manifest
Check ((Assert-HeadlessPackage $z690Fixture).HardwareProfile -ceq $z690Manifest.HardwareProfile) 'coherent Z690-I internal package accepted'
$normal=NewFixture 'normal'
$m=Assert-HeadlessPackage $normal
Check ($m.Format -eq 2 -and $m.ChildBytes -gt 0) 'complete synthetic package accepted'
Check ((Get-HeadlessAssetSpecs $normal).Count -eq 5) 'five valid unique asset contracts accepted'
$reordered=NewFixture 'reordered-inputs'
$reorderedManifest=ReadJson $reordered $packageRelative
[array]::Reverse($reorderedManifest.Inputs)
PutJson $reordered $packageRelative $reorderedManifest
Check ((Assert-HeadlessPackage $reordered).Format -eq 2) 'cross-platform inventory ordering does not change package identity'
$duplicate=NewFixture 'duplicate-input'
$duplicateManifest=ReadJson $duplicate $packageRelative
$duplicateManifest.Inputs[0]=$duplicateManifest.Inputs[1]
PutJson $duplicate $packageRelative $duplicateManifest
Reject 'duplicate input cannot replace a required source' {Assert-HeadlessPackage $duplicate} 'Build input changed'
$inventory=@(Get-HeadlessInputInventory $normal -WithContract)
foreach($required in @('internal-boot/boot/platform-config.h','internal-boot/boot/media.h','internal-boot/local-config.json','internal-boot/unlock/assets.json','deps/edk2/MdePkg/Include/Uefi.h','deps/edk2/MdePkg/Include/Protocol/PciIo.h','internal-boot/boot/build/headless/child-contract.h')){
 Check ($inventory.Path -contains $required) ('inventory includes '+$required)
}
Check ((Get-HeadlessGuidInitializer ([guid]'01234567-89ab-cdef-0123-456789abcdef')) -ceq '{0x01234567,0x89ab,0xcdef,{0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef}}') 'GUID initializer preserves component endianness'
Put $normal 'internal-boot/boot/build/unrelated/ignored.h' 'ignored generated file'
Check ((Assert-HeadlessPackage $normal).EfiSha256 -eq $m.EfiSha256) 'unrelated ignored build header does not become a source input'

foreach($item in @(
 @('source-drift','internal-boot/unlock/app.c'),
 @('boot-header-drift','internal-boot/boot/media.h'),
 @('external-include-drift','deps/edk2/MdePkg/Include/Protocol/PciIo.h'),
 @('assets-drift','internal-boot/unlock/assets.json'),
 @('builder-drift','research/headless/build-unlock.ps1')
)){
 $f=NewFixture $item[0]
 Put $f $item[1] 'changed after build'
 Reject $item[0] {Assert-HeadlessPackage $f} 'Build input changed'
}
$f=NewFixture 'include-added'
Put $f 'deps/edk2/MdePkg/Include/NewProtocol.h' 'new external include'
Reject 'added external include changes complete input set' {Assert-HeadlessPackage $f} 'Build source set changed'
$f=NewFixture 'include-deleted'
Remove-Item -LiteralPath (FixturePath $f 'deps/edk2/MdePkg/Include/Protocol/PciIo.h') -ErrorAction Stop
Reject 'deleted external include changes complete input set' {Assert-HeadlessPackage $f} 'Build source set changed'
$f=NewFixture 'header-hash-mismatch'
Put $f 'internal-boot/boot/platform-config.h' 'changed generated header'
Reject 'generated header hash mismatch' {Assert-HeadlessLocalConfig $f} 'header/config contract'
$f=NewFixture 'header-semantics-mismatch'
$cfg=ReadJson $f 'internal-boot/local-config.json';$cfg.UnlockPartitionGuid='{99999999-2222-3333-4444-555555555555}'
PutJson $f 'internal-boot/local-config.json' $cfg
Reject 'header hash alone cannot authorize a different configured partition' {Assert-HeadlessLocalConfig $f} 'partition header differs'
$f=NewFixture 'equal-partitions'
$cfg=ReadJson $f 'internal-boot/local-config.json';$cfg.WindowsPartitionGuid=$cfg.UnlockPartitionGuid
PutJson $f 'internal-boot/local-config.json' $cfg
Reject 'Windows and unlock partition must differ' {Assert-HeadlessLocalConfig $f} 'header/config contract'

foreach($item in @(@('child-bytes-drift',$childRelative),@('wrapper-bytes-drift','internal-boot/boot/build/headless/HEADLESS.EFI'))){
 $f=NewFixture $item[0];Put $f $item[1] 'different binary bytes'
 Reject $item[0] {Assert-HeadlessPackage $f} 'EFI/child manifest mismatch'
}
$f=NewFixture 'child-manifest-drift'
Put $f $childManifestRelative '{}'
Reject 'child manifest itself is hash-bound' {Assert-HeadlessPackage $f} 'EFI/child manifest mismatch'
$f=NewFixture 'child-source-claim-mismatch'
$cm=ReadJson $f $childManifestRelative;$cm.Sources[0].Sha256='0'*64
PutJson $f $childManifestRelative $cm;SealChildManifest $f
Reject 'child source manifest claims are checked independently' {Assert-HeadlessPackage $f} 'Child source/header provenance mismatch'
$f=NewFixture 'child-header-claim-mismatch'
$cm=ReadJson $f $childManifestRelative;$cm.Headers[0].Sha256='0'*64
PutJson $f $childManifestRelative $cm;SealChildManifest $f
Reject 'child header manifest claims are checked independently' {Assert-HeadlessPackage $f} 'Child source/header provenance mismatch'
$f=NewFixture 'child-source-escape'
$cm=ReadJson $f $childManifestRelative;$cm.Sources[0].Path='../boot/handoff.c'
PutJson $f $childManifestRelative $cm;SealChildManifest $f
Reject 'child manifest cannot escape its source root' {Assert-HeadlessPackage $f} 'Child source/header provenance mismatch'
$f=NewFixture 'wrong-child-contract'
Put $f 'internal-boot/boot/build/headless/child-contract.h' '#define GU_UNLOCK_SHA256 "wrong-child"'
SealInputs $f
Reject 'inventory-consistent contract must still match the verified child' {Assert-HeadlessPackage $f} 'Wrapper child contract mismatch'
$f=NewFixture 'wrong-profile'
$m=ReadJson $f $packageRelative;$m.HardwareProfile='UNSUPPORTED'
PutJson $f $packageRelative $m
Reject 'unsupported hardware profile' {Assert-HeadlessPackage $f} 'Unexpected headless package profile'
$f=NewFixture 'wrong-child-profile'
$cm=ReadJson $f $childManifestRelative;$cm.StockProfile='entryLatchedSs1Preserve'
PutJson $f $childManifestRelative $cm;SealChildManifest $f
Reject 'child full profile cannot silently become preserve profile' {Assert-HeadlessPackage $f} 'EFI/child manifest mismatch'

foreach($kind in @('empty','duplicate','wrong-name','zero-length','bad-hash')){
 $f=NewFixture ('assets-'+$kind)
 $assets=@(ReadJson $f 'internal-boot/unlock/assets.json')
 switch($kind){
  'empty' {$assets=@()}
  'duplicate' {$assets[1].Name=$assets[0].Name}
  'wrong-name' {$assets[0].Name='../unexpected.bin'}
  'zero-length' {$assets[0].Bytes=0}
  'bad-hash' {$assets[0].Sha256='not-a-sha256'}
 }
 PutJson $f 'internal-boot/unlock/assets.json' @($assets)
 Reject ('invalid asset contract: '+$kind) {Get-HeadlessAssetSpecs $f} 'firmware asset'
}

$headless='{11111111-2222-3333-4444-555555555555}'
$existingAuto='{22222222-3333-4444-5555-666666666666}'
$uefi='{33333333-4444-5555-6666-777777777777}'
$diagnostic='{44444444-5555-6666-7777-888888888888}'
$original=@($existingAuto,'{bootmgr}',$uefi,$headless,$diagnostic)
$expected=@($headless,'{bootmgr}',$existingAuto,$uefi,$diagnostic)
$promoted=@(Get-HeadlessPromotionOrder $original $headless)
Check (($promoted -join ',') -ceq ($expected -join ',')) 'promotion puts Windows second even when another automatic entry was first'
Check (($original -join ',') -ceq (@($existingAuto,'{bootmgr}',$uefi,$headless,$diagnostic) -join ',')) 'promotion does not mutate its input order'
foreach($bad in @(@($headless,$existingAuto),@('{bootmgr}',$existingAuto),@('{bootmgr}',$headless,$headless))){
 Reject 'invalid/missing/duplicate promotion identifiers rejected' {Get-HeadlessPromotionOrder $bad $headless} 'Invalid headless promotion input'
}
Reject 'non-GUID promotion target rejected' {Get-HeadlessPromotionOrder $expected 'not-an-id'} 'Invalid headless promotion input'

$durable=FixturePath $normal 'durable.json'
$first=@{Mode='registered';HiberbootBefore=@{Exists=$false;Value=$null};Message="first`nrecord"}
Write-HeadlessJson $durable $first
$read=Get-Content -LiteralPath $durable -Raw | ConvertFrom-Json
Check ($read.Mode -eq 'registered' -and !$read.HiberbootBefore.Exists -and $read.Message -eq "first`nrecord") 'durable JSON initial creation and nested/null/newline readback'
$second=@{Mode='preparing-automatic';HiberbootBefore=@{Exists=$true;Value=1};Message='replacement'}
Write-HeadlessJson $durable $second
$read=Get-Content -LiteralPath $durable -Raw | ConvertFrom-Json
Check ($read.Mode -eq 'preparing-automatic' -and $read.HiberbootBefore.Exists -and $read.HiberbootBefore.Value -eq 1 -and $read.Message -eq 'replacement') 'durable JSON replaces existing evidence with complete readable recovery plan'
Check (@(Get-ChildItem -LiteralPath (Split-Path $durable) -File -Filter 'durable.json.pending-*').Count -eq 0) 'successful durable creation/replacement consumes pending file'

Write-Output "PASS headless package helper: $script:Checks checks; synthetic ignored fixtures only; no registry/BCD/GPU calls. Fixtures: $testRoot"
