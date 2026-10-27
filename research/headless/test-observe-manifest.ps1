# GPL-2.0-only. Synthetic files and manifest objects only; no USB/GPU/EFI execution.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'observe-manifest.ps1')
$sandbox=Join-Path $PSScriptRoot ('build/observe-tests-'+[Guid]::NewGuid().ToString('N'))
function Put([string]$Relative,[string]$Text) {
 $path=Join-Path $sandbox $Relative
 [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))
 [IO.File]::WriteAllText($path,$Text)
}
foreach($path in (Get-LocalRequiredFiles)){Put $path 'synthetic source'}
Put 'deps/edk2/MdePkg/Include/Uefi.h' 'synthetic header'
Put 'internal-boot/boot/platform-config.h' 'synthetic config, never boot'
Put 'internal-boot/boot/build/observe.EFI' 'synthetic bytes, not an EFI program'
$efi=Join-Path $sandbox 'internal-boot/boot/build/observe.EFI'
$state=Get-ObservationState $sandbox
$baseline=[ordered]@{Format=1;Mode='usb-observation-only';CanUnlock=$false;CanDetach=$false;
 WritesFirmwareVariables=$false;HardwareTested=$false;UsbMedia=$true;DumpsProm=$true;
 HardwareProfile='probe-h610m-3060';Defines=@('-DGU_PROFILE_PROBE_H610M_3060','-DGU_USB_MEDIA','-DGU_PROBE_DUMP_PROM');
 EfiSha256=(Get-GuSha256 $efi);Inputs=$state.Inputs;TreeSha256=$state.TreeSha256;PlatformConfigSha256=$state.PlatformConfigSha256}|ConvertTo-Json -Depth 8
$checks=0
function Check([string]$Label,[scriptblock]$Mutate,[bool]$Accept=$false) {
 $m=$baseline|ConvertFrom-Json
 & $Mutate $m
 $passed=$true;try{Assert-ObservationManifest $sandbox $m 'usb-observation-only' $efi}catch{$passed=$false}
 if($passed -ne $Accept){throw "Observation guard failed: $Label (accepted=$passed)"}
 $script:checks++
}
Check 'complete synthetic package' {} $true
foreach($key in @('CanUnlock','CanDetach','WritesFirmwareVariables','HardwareTested')) {
 foreach($value in @($null,'False',$true,0)) {
  Check ('capability '+$key) {param($m) $m.$key=$value}
 }
}
foreach($key in @('UsbMedia','DumpsProm')) {
 foreach($value in @($null,'False',0)){Check ('flag '+$key) {param($m) $m.$key=$value}}
}
Check 'USB flag must be true' {param($m) $m.UsbMedia=$false}
Check 'missing format' {param($m) $m.Format=$null}
Check 'string format' {param($m) $m.Format='1'}
Check 'wrong mode' {param($m) $m.Mode='unlock'}
foreach($value in @($null,'','unknown','PROBE-H610M-3060')){Check 'profile identity' {param($m) $m.HardwareProfile=$value}}
Check 'extra unsafe define' {param($m) $m.Defines+=@('-DGU_FULL_UNLOCK')}
Check 'missing defines' {param($m) $m.Defines=@()}
Check 'wrong define' {param($m) $m.Defines[0]='-DGU_PROFILE_B650M_3090FE'}
Check 'PROM flag disagrees with defines' {param($m) $m.DumpsProm=$false}
foreach($key in @('EfiSha256','TreeSha256','PlatformConfigSha256')) {
 Check ('missing hash '+$key) {param($m) $m.$key=$null}
 Check ('different hash '+$key) {param($m) $m.$key='0'*64}
}
Check 'missing input row' {param($m) $m.Inputs=@($m.Inputs|Select-Object -Skip 1)}
Check 'wrong input hash' {param($m) $m.Inputs[0].Sha256='0'*64}
foreach($path in @('internal-boot/efi_runtime.c','deps/edk2/MdePkg/Include/Uefi.h','research/headless/stage-usb.ps1','internal-boot/boot/platform-config.h')) {
 $original=[IO.File]::ReadAllText((Join-Path $sandbox $path))
 Put $path 'changed after compilation'
 Check ('stale source/config '+$path) {}
 Put $path $original
}
$profiles=@{'b650m-3090fe'=@();'b660m-3070ti'=@('-DGU_PROFILE_B660M_3070TI');'x870-3090ti'=@('-DGU_PROFILE_X870_3090TI');
 'b660plus-3060'=@('-DGU_PROFILE_B660PLUS_3060');'h610m-3060'=@('-DGU_PROFILE_H610M_3060');'probe-h610m-3060'=@('-DGU_PROFILE_PROBE_H610M_3060');'probe-b550-3060'=@('-DGU_PROFILE_PROBE_B550_3060')}
foreach($name in $profiles.Keys) {
 Check ('valid '+$name) {param($m) $m.HardwareProfile=$name;$m.Defines=@($profiles[$name])+@('-DGU_USB_MEDIA','-DGU_PROBE_DUMP_PROM')} $true
}
$empty=Join-Path $sandbox 'empty-capture'
Assert-ObservationCaptureEmpty $empty;$checks++
foreach($name in @('DIAG00.TXT','IMG00.BIN','PROM.BIN')) {
 $relative='capture-'+$name+'/RTX3090-UNLOCK/'+$name
 Put $relative 'original capture'
 $directory=[IO.Path]::GetDirectoryName((Join-Path $sandbox $relative))
 $refused=$false;try{Assert-ObservationCaptureEmpty $directory}catch{$refused=$true}
 if(!$refused -or [IO.File]::ReadAllText((Join-Path $sandbox $relative)) -cne 'original capture'){throw 'Existing capture was not preserved'}
 $checks++
}
"PASS $checks observation manifest/capture checks; synthetic files only"
