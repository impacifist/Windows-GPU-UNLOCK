# GPL-2.0-only. Build/test and optionally stage; never reboots or accesses the GPU.
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[Parameter(Mandatory)][ValidatePattern('^[A-Za-z]$')][string]$Drive,[string]$Clang='clang',[string]$LldLink='lld-link',[string]$Python='python',[switch]$Stage,[ValidateRange(5,99)][int]$BuildOnlySlot=0)
$ErrorActionPreference='Stop'
if($BuildOnlySlot -and $Stage){throw 'BuildOnlySlot cannot be used with Stage'}
$slot=if($BuildOnlySlot){$BuildOnlySlot}else{& (Join-Path $PSScriptRoot 'get-shared-esp-unlock-slot.ps1') -Drive $Drive}
if($slot -isnot [int] -or $slot -lt 5 -or $slot -gt 99){throw 'Slot discovery did not return one valid attempt number'}
if($BuildOnlySlot){Write-Host "Development build slot: $slot. NOT qualified for staging or hardware execution."}else{Write-Host "Next attempt slot: $slot. Record it outside the USB."}
& (Join-Path $PSScriptRoot 'build-shared-esp-unlock-launcher.ps1') -WindowsEspGuid $WindowsEspGuid -AttemptSlot $slot -Clang $Clang -LldLink $LldLink -Python $Python
foreach($name in @('test-shared-esp-unlock-gate.ps1','test-shared-esp-unlock-prepare.ps1','test-shared-esp-unlock-launcher.ps1')){
 Write-Host "Running $name"
 & (Join-Path $PSScriptRoot $name) -Clang $Clang -LldLink $LldLink
}
if($Stage){
 & (Join-Path $PSScriptRoot 'prepare-shared-esp-unlock-stage.ps1') -WindowsEspGuid $WindowsEspGuid -Drive $Drive
}else{
 if($BuildOnlySlot){Write-Host 'Development build and host tests completed. Do not stage or boot this artifact; reviewed history admission is still required.'}else{Write-Host 'Build and host tests completed. No ESP or USB staging performed; add -Stage to prepare the one-shot test.'}
}
