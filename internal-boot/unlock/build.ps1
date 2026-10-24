# Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md.
param(
 [string]$Clang='clang',
 [string]$LldLink='lld-link',
 [string]$Python='python',
 [switch]$ExperimentalUnlock,
 [switch]$InternalBoot,
 [switch]$AutomaticBoot,
 [switch]$HeadlessResearch,
 [ValidateSet('original03100062','observed03000062','entryLatchedSs1Preserve','entryLatchedSs1Full')][string]$StockProfile,
 [ValidateSet('b650m-3090fe','b660m-3070ti')][string]$HardwareProfile='b650m-3090fe'
)
# Reviewed single-machine profiles; mirrors internal-boot/unlock/machine-profile.h.
# A profile is fixed provenance: it selects constants compiled into the EFI,
# never a user-editable switch.
$hwTable=@{
 'b650m-3090fe'=[ordered]@{Define='-DGU_PROFILE_B650M_3090FE';Name='ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE';TargetPci='220410de';Subsystem='145410de';Chip='0xb72000a1';Prom='8ea297373e9e5d3600ff41ad9fae88f2d14c98c017d100669f7333981da673b4';Memory='0x6000';HeadlessOnly=$false}
 'b660m-3070ti'=[ordered]@{Define='-DGU_PROFILE_B660M_3070TI';Name='ASUS-TUF-GAMING-B660M-E-D4-4003-RTX3070TI';TargetPci='248210de';Subsystem='50521462';Chip='0xb74000a1';Prom='f429aa1d5f0f18d11ed9a19503683a674fe3541ae77e959df8e732652c2b4df0';Memory='0x2000';HeadlessOnly=$true}
}
$hw=$hwTable[$HardwareProfile]
$ErrorActionPreference='Stop'
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
 if($AutomaticBoot -and (!$InternalBoot -or !$ExperimentalUnlock -or $StockProfile -ne 'entryLatchedSs1Full')){throw 'Automatic boot requires internal full experimental profile'}
 if($InternalBoot) {
  if(!$ExperimentalUnlock -or $StockProfile -ne 'entryLatchedSs1Full'){throw 'Internal boot requires the validated full experimental profile'}
  $out+='/internal';$name+='-INTERNAL';$defines+='-DGU_INTERNAL_BOOT'
 }
 if($AutomaticBoot){$out+='/automatic';$name+='-AUTO';$defines+='-DGU_AUTOMATIC_BOOT'}
 if($HeadlessResearch){$out+='/headless-research';$name+='-HEADLESS-RESEARCH';$defines+='-DGU_HEADLESS_RESEARCH'}
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
 [ordered]@{BuiltUtc=[DateTime]::UtcNow.ToString('o');HardwareProfile=$hw.Name;TargetPci=$hw.TargetPci;Subsystem=$hw.Subsystem;
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
  } | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath "$out/build-manifest.json"
 "Built $name.EFI; ExperimentalUnlock=$([bool]$ExperimentalUnlock); hardware untested."
} finally {Pop-Location}
