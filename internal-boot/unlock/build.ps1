# Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md.
param(
 [string]$Clang='clang',
 [string]$LldLink='lld-link',
 [string]$Python='python',
 [switch]$ExperimentalUnlock,
 [switch]$InternalBoot,
 [switch]$AutomaticBoot,
 [switch]$HeadlessResearch,
 [switch]$UsbMedia,
 [ValidateSet('original03100062','observed03000062','entryLatchedSs1Preserve','entryLatchedSs1Full')][string]$StockProfile,
 [ValidateSet('b650m-3090fe','b660m-3070ti','b660m-3090ti','x870-3090ti','b660plus-3060','h610m-3060','b550-3060','local')][string]$HardwareProfile='b650m-3090fe'
)
# Reviewed single-machine profiles; mirrors internal-boot/unlock/machine-profile.h.
# A profile is fixed provenance: it selects constants compiled into the EFI,
# never a user-editable switch. 'local' is this machine's generated profile-local.h,
# write-eligible only with matching passing evidence (local-profile.ps1).
$hwTable=@{
 'b650m-3090fe'=[ordered]@{Define='-DGU_PROFILE_B650M_3090FE';Name='ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE';TargetPci='220410de';Subsystem='145410de';Chip='0xb72000a1';Prom='8ea297373e9e5d3600ff41ad9fae88f2d14c98c017d100669f7333981da673b4';Memory='0x6000';HeadlessOnly=$false}
 'b660m-3070ti'=[ordered]@{Define='-DGU_PROFILE_B660M_3070TI';Name='ASUS-TUF-GAMING-B660M-E-D4-4003-RTX3070TI';TargetPci='248210de';Subsystem='50521462';Chip='0xb74000a1';Prom='f429aa1d5f0f18d11ed9a19503683a674fe3541ae77e959df8e732652c2b4df0';Memory='0x2000';HeadlessOnly=$true}
 'b660m-3090ti'=[ordered]@{Define='-DGU_PROFILE_B660M_3090TI';Name='ASUS-TUF-GAMING-B660M-E-D4-4003-RTX3090TI';TargetPci='220310de';Subsystem='13017377';Chip='0xb72000a1';Prom='22b77e844a215bf52d745e0b10c6698ec3d9479f8350e833247bd5d77430bf44';Memory='0x5ff4';HeadlessOnly=$true}
 'h610m-3060'=[ordered]@{Define='-DGU_PROFILE_H610M_3060';Name='GIGABYTE-H610M-H-FC-RTX3060-12GB';TargetPci='250410de';Subsystem='40961458';Chip='0xb76000a1';Prom='727dd8332f028783d3de2f81a707e4d9448c3238db6180271f3a8e9c03a808c9';Memory='0x3000';HeadlessOnly=$true;UsbOnly=$true}
 'x870-3090ti'=[ordered]@{Define='-DGU_PROFILE_X870_3090TI';Name='GIGABYTE-X870-AORUS-ELITE-WIFI7-F13d-RTX3090TI';TargetPci='220310de';Subsystem='88701043';Chip='0xb72000a1';Prom='943565975885ee71a7e861265984c785f564f35272c32395ee7410b353d9bbc4';Memory='0x5ff4';HeadlessOnly=$true}
 'b550-3060'=[ordered]@{Define='-DGU_PROFILE_B550_3060';Name='GIGABYTE-B550-AORUS-ELITE-V2-F13-RTX3060-12GB';TargetPci='248710de';Subsystem='248710de';Chip='0xb74000a1';Prom='2fdefb4b02f64022e9ade1adf9f1aa3fd8aa4a2a609c59bdcb46c4e1f4292439';Memory='0x3000';HeadlessOnly=$true;UsbOnly=$true}
 'b660plus-3060'=[ordered]@{Define='-DGU_PROFILE_B660PLUS_3060';Name='ASUS-PRIME-B660-PLUS-D4-3010-RTX3060';TargetPci='248710de';Subsystem='153010de';Chip='0xb74000a1';Prom='7cc9018e80c63e4aec79c71cd4ed6bdeb5e147aeeb22b21b96efe90887ba8806';Memory='0x3000';HeadlessOnly=$true;UsbOnly=$true}
}
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'local-profile.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$hw=if($HardwareProfile -eq 'local'){Get-LocalProfile $repo}else{$hwTable[$HardwareProfile]}
$evidence=$null
Push-Location -LiteralPath $PSScriptRoot
try {
 if(!(Test-Path -LiteralPath $PSScriptRoot)){throw 'Missing output parent'}
 if($ExperimentalUnlock -and !$PSBoundParameters.ContainsKey('StockProfile')){throw '-StockProfile is required with -ExperimentalUnlock'}
 if(!$ExperimentalUnlock -and $PSBoundParameters.ContainsKey('StockProfile')){throw '-StockProfile is only valid with -ExperimentalUnlock'}
 $out=if($ExperimentalUnlock){"build/experimental/$StockProfile"}else{'build'}
 $name=if($ExperimentalUnlock){"RTX3090-EXPERIMENTAL-$StockProfile"}else{'RTX3090-PREFLIGHT'}
 $defines=@()
 if($HeadlessResearch -and (!$AutomaticBoot -or !$InternalBoot -or !$ExperimentalUnlock -or $StockProfile -ne 'entryLatchedSs1Full')){throw 'Headless research requires automatic internal exact full profile'}
 if($hw.HeadlessOnly -and $ExperimentalUnlock -and !$HeadlessResearch){throw ('Profile '+$HardwareProfile+' only ships a headless write path; non-headless builds are diagnostic-only')}
 if($hw.UsbOnly -and $ExperimentalUnlock -and !$UsbMedia){throw ('Profile '+$HardwareProfile+' only ships the headless USB write path')}
 if($HardwareProfile -eq 'local' -and $ExperimentalUnlock){$evidence=Assert-LocalProfileEvidence $repo}
 if($AutomaticBoot -and (!$InternalBoot -or !$ExperimentalUnlock -or $StockProfile -ne 'entryLatchedSs1Full')){throw 'Automatic boot requires internal full experimental profile'}
 if($InternalBoot) {
  if(!$ExperimentalUnlock -or $StockProfile -ne 'entryLatchedSs1Full'){throw 'Internal boot requires the validated full experimental profile'}
  $out+='/internal';$name+='-INTERNAL';$defines+='-DGU_INTERNAL_BOOT'
 }
 if($AutomaticBoot){$out+='/automatic';$name+='-AUTO';$defines+='-DGU_AUTOMATIC_BOOT'}
 if($HeadlessResearch){$out+='/headless-research';$name+='-HEADLESS-RESEARCH';$defines+='-DGU_HEADLESS_RESEARCH'}
 if($UsbMedia -and !$HeadlessResearch){throw 'USB media is only wired for the headless research child'}
 if($UsbMedia){$out+='/usb';$name+='-USB';$defines+='-DGU_USB_MEDIA'}
  if($ExperimentalUnlock){$defines+= '-DGU_EXPERIMENTAL_3090';$defines+=$(switch($StockProfile){'original03100062'{'-DGU_STOCK_PROFILE_ORIGINAL_03100062'} 'observed03000062'{'-DGU_STOCK_PROFILE_OBSERVED_03000062'} 'entryLatchedSs1Preserve'{'-DGU_ENTRY_LATCHED_SS1_PRESERVE'} 'entryLatchedSs1Full'{'-DGU_ENTRY_LATCHED_SS1_FULL'}})}
 New-Item -ItemType Directory -Path $out -Force | Out-Null
 $sources=@('app','app_io','ownership','format','runtime','sha256','checks','port','compat',
  'vendor/unlock_610','vendor/prep_610','vendor/layout_610','vendor/vbios_610','vendor/gfw_eval')
 $objects=@()
 foreach($source in $sources) {
  $object="$out/"+($source -replace '/','_')+'.obj'
  & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -Wno-unused-parameter -D__KERNEL__ -DGU_UEFI_PORT "-DGU_TARGET_PCI_ID=0x$($hw.TargetPci)U" @defines $hw.Define -I compat -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 -c "$source.c" -o $object
  if($LASTEXITCODE -ne 0){throw "Compile failed: $source"};$objects+=$object
 }
 & $LldLink /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf "/map:$out/$name.map" "/out:$out/$name.EFI" @objects
 if($LASTEXITCODE -ne 0){throw 'EFI link failed'}
 & $Python verify_pe.py "$out/$name.EFI" "$out/$name.map" --output "$out/pe-verification.json"
 if($LASTEXITCODE -ne 0){throw 'PE verification failed'}
 $manifest=[ordered]@{BuiltUtc=[DateTime]::UtcNow.ToString('o');HardwareProfile=$hw.Name;TargetPci=$hw.TargetPci;Subsystem=$hw.Subsystem;
  Mode=$(if($HeadlessResearch){'headless-research-automatic-internal-exact-card-unlock'}elseif($AutomaticBoot){'automatic-internal-exact-card-unlock'}elseif($ExperimentalUnlock){'experimental-exact-card-manual-unlock'}else{'diagnostic-preflight-only'});
  CanArm=[bool]$ExperimentalUnlock;Defines=$defines;ManualConfirmation=$(if($AutomaticBoot){'none; Esc within 3 seconds cancels'}else{'U once; Esc or 30-second timeout cancels'});
  StockProfile=$(if($ExperimentalUnlock){$StockProfile}else{$null});ColdChipExpected=$hw.Chip;ColdSS0Expected=$(if($StockProfile -like 'entryLatchedSs1*'){'first fully validated cold entry; immutable'}elseif($ExperimentalUnlock){if($StockProfile -eq 'original03100062'){'0x03100062'}else{'0x03000062'}}else{'unknown'});ColdSS1Expected=$(if($StockProfile -like 'entryLatchedSs1*'){'first fully validated cold entry <= 0xf; immutable'}else{'0x6'});
  SS0Target='0x88888888';SS1Target=$(if($StockProfile -eq 'entryLatchedSs1Preserve'){'preserve entry; no SS1 writes'}elseif($StockProfile -eq 'entryLatchedSs1Full'){'0x8; one guarded write'}elseif($ExperimentalUnlock){'0x8'}else{'no writes'});
  OriginalBaselineCapture='G:/RTX3090-PREFLIGHT/RUN00/REPORT.TXT';
  StockProfileSource=$(if($StockProfile -like 'entryLatchedSs1*'){'Explicit immutable validated entry latch; no fixed stock selector assumption'}elseif($StockProfile -eq 'observed03000062'){'Explicit observed03000062 experiment selection; original baseline capture does not evidence this SS0'}elseif($ExperimentalUnlock){'OriginalBaselineCapture'}else{'preflight has no selected stock profile'});
  PromSha256=$hw.Prom;
  DeviceMemorySize=$hw.Memory;HardwareTested=$false;
  EfiSha256=(Get-FileHash -LiteralPath "$out/$name.EFI" -Algorithm SHA256).Hash;
  Headers=@(Get-ChildItem -LiteralPath . -Recurse -File -Filter '*.h' | Where-Object {$_.FullName.Substring($PSScriptRoot.Length+1) -notmatch '(^|[\\/])build([\\/]|$)'} | Sort-Object FullName | ForEach-Object {[ordered]@{Path=$_.FullName.Substring($PSScriptRoot.Length+1);Sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}});
  Sources=@($sources | ForEach-Object {[ordered]@{Path="$_.c";Sha256=(Get-FileHash -LiteralPath "$_.c" -Algorithm SHA256).Hash}})
  }
 if($HardwareProfile -eq 'local'){
  if($evidence){
   $afterEvidence=Assert-LocalProfileEvidence $repo
   if($afterEvidence.TreeSha256 -cne $evidence.TreeSha256 -or (Get-LocalProfile $repo).HeaderSha256 -cne $hw.HeaderSha256){throw 'Local inputs changed while building; rerun tests and rebuild'}
  }
  $manifest.LocalProfile=[ordered]@{HeaderSha256=$hw.HeaderSha256;Evidence=$(if($evidence){[ordered]@{TreeSha256=$evidence.TreeSha256;TestedUtc=$evidence.TestedUtc}}else{'none: diagnostic build'})}
 }
 $manifest | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath "$out/build-manifest.json"
 "Built $name.EFI; ExperimentalUnlock=$([bool]$ExperimentalUnlock); hardware untested."
} finally {Pop-Location}
