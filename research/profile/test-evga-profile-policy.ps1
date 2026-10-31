# Additional malformed-input/race review coverage. All native/storage behavior is fake.
param([Parameter(Mandatory)][string]$PackageRoot)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test-evga-profile-manager.ps1') -PackageRoot $PackageRoot
. (Join-Path $PSScriptRoot 'shared-esp-firmware-lib.ps1')
$checks=0
function Check($ok,$why){$script:checks++;if(!$ok){throw $why}}
$proofState=Join-Path $fixture 'proof-tamper-state';[void][IO.Directory]::CreateDirectory($proofState)
foreach($name in @('attempt-08.json','result-08.json')){Copy-Item -LiteralPath (Join-Path $f.State $name) -Destination (Join-Path $proofState $name)}
$prior=Get-Content -LiteralPath (Join-Path $proofState 'attempt-08.json') -Raw | ConvertFrom-Json
foreach($name in @('EVP08.ARMED','EVP08.TXT','EVGA-UNLOCK8.ARMED')){
 $path=Join-Path $prior.TargetRoot ('RTX3090-UNLOCK/'+$name);$bytes=[IO.File]::ReadAllBytes($path);$changed=[byte[]]$bytes.Clone();$changed[$changed.Length-1]=$changed[$changed.Length-1] -bxor 1
 try{
  [IO.File]::WriteAllBytes($path,$changed);$blocked=$false
  try{Get-EvgaProfileNextSlot $proofState ([pscustomobject]@{SeedSlot=7}) | Out-Null}catch{$blocked=$true}
  Check $blocked ('Modified evidence allowed rearm: '+$name)
 }finally{[IO.File]::WriteAllBytes($path,$bytes)}
}
$f=Setup-ManagerFixture 'policy'
foreach($n in 8..9){Set-Content -LiteralPath (Join-Path $global:managerRoots.usb ("RTX3090-UNLOCK/EVGA-UNLOCK$n.ARMED")) -Value 'old reservation'}
$package=Join-Path $PackageRoot 'SharedEsp-10'
$global:managerGpuOk=$false;$blocked=$false
try{& $manager -Action Install -StateDirectory $f.State -Assets $f.Assets -PackageDirectory $package}catch{$blocked=$true}
Check ($blocked -and !(Test-Path $f.State) -and ![SharedEspFirmware.Native]::Writes.Count) 'Wrong GPU reached mutation'
$global:managerGpuOk=$true
Set-FixtureValue 'BootNext' ([byte[]](8,0));$blocked=$false
try{& $manager -Action Install -StateDirectory $f.State -Assets $f.Assets -PackageDirectory $package}catch{$blocked=$true}
Check ($blocked -and !(Test-Path $f.State) -and ![SharedEspFirmware.Native]::Writes.Count) 'Foreign BootNext reached mutation'
[void][SharedEspFirmware.Native]::Values.Remove('BootNext')
$bad=Join-Path $fixture 'bad-package';[void][IO.Directory]::CreateDirectory($bad)
foreach($file in Get-ChildItem -LiteralPath $package -File){Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $bad $file.Name)}
$original=Get-Content -LiteralPath (Join-Path $bad 'package.json') -Raw
foreach($mode in @('slot-string','automatic','path','inventory','binary')){
 Set-Content -LiteralPath (Join-Path $bad 'package.json') -Value $original -Encoding utf8
 $m=$original | ConvertFrom-Json
 switch($mode){
  'slot-string' {$m.AttemptSlot='10'}
  'automatic' {$m.RequiresFreshShiftG=$false}
  'path' {$m.EfiPath='\EFI\Microsoft\Boot\bootmgfw.efi'}
  'inventory' {$m.Files=@($m.Files | Select-Object -Skip 1)}
  'binary' {Set-Content -LiteralPath (Join-Path $bad 'UNLOCK.EFI') -Value 'not the image'}
 }
 $m | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $bad 'package.json') -Encoding utf8
 $blocked=$false;try{Assert-EvgaPackage $bad | Out-Null}catch{$blocked=$true}
 Check $blocked ('Malformed package accepted: '+$mode)
}
# The observed fallback contraction is allowed; reordered/changed real entries are not.
$g=[guid]'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'
foreach($pair in @(@('Boot000E','UEFI:CD/DVD Drive'),@('Boot000F','UEFI:Removable Device'),@('Boot0010','UEFI:Network Device'))){
 $v=New-Object SharedEspFirmware.Value;$v.Data=New-SharedEspLoadOption $g 1 2048 2097152 $pair[1];$v.Attributes=7;[SharedEspFirmware.Native]::Values[$pair[0]]=$v
}
Set-FixtureValue 'BootOrder' ([byte[]](0,0,8,0,14,0,15,0,16,0))
$read={param($name) $v=[SharedEspFirmware.Native]::Read($name);if($v){ConvertTo-FirmwareValue $v.Data $v.Attributes}}
$before=Get-EvgaFirmwareSnapshot $read
foreach($name in @('Boot000E','Boot000F','Boot0010')){[void][SharedEspFirmware.Native]::Values.Remove($name)}
Set-FixtureValue 'BootOrder' ([byte[]](0,0,8,0))
$kept=Get-EvgaPreservedCleanupSnapshot $before $read
Check ($kept.BootEntries.Count -eq 2) 'Reviewed missing generic fallback policy failed'
Set-FixtureValue 'BootOrder' ([byte[]](8,0,0,0));$blocked=$false
try{Get-EvgaPreservedCleanupSnapshot $before $read | Out-Null}catch{$blocked=$true}
Check $blocked 'Reordered primary entries accepted'
Set-FixtureValue 'BootOrder' ([byte[]](0,0,8,0))
$savedWindows=[SharedEspFirmware.Native]::Values['Boot0008'];[void][SharedEspFirmware.Native]::Values.Remove('Boot0008');$blocked=$false
try{Get-EvgaPreservedCleanupSnapshot $before $read | Out-Null}catch{$blocked=$true}
Check $blocked 'Missing real Windows entry accepted'
[SharedEspFirmware.Native]::Values['Boot0008']=$savedWindows
$option=ConvertTo-FirmwareValue (New-SharedEspLoadOption $g 1 2048 2097152 'owned' '\EFI\WindowsGPUUnlock\EVP10\UNLOCK.EFI')
$attempt=[pscustomobject]@{OptionName='Boot7E00';OptionNumber=32256;Option=$option;BootOrder=$before.BootOrder;BootEntries=$before.BootEntries}
Set-FixtureValue 'Boot7E00' ([Convert]::FromBase64String($option.DataBase64))
$global:policyWrites=0
$write={param($n,$v) $global:policyWrites++;throw 'No writes expected in refusal case'}
foreach($mode in @('save-fail','option-race','next-race')){
 Set-FixtureValue 'Boot7E00' ([Convert]::FromBase64String($option.DataBase64));[void][SharedEspFirmware.Native]::Values.Remove('BootNext')
 $save={param($s) if($mode -eq 'save-fail'){throw 'Injected backup failure'};if($mode -eq 'option-race'){[void][SharedEspFirmware.Native]::Values.Remove('Boot7E00')};if($mode -eq 'next-race'){Set-FixtureValue 'BootNext' ([byte[]](8,0))}}
 $blocked=$false;try{Remove-EvgaPackageOption $attempt $read $write $save}catch{$blocked=$true}
 Check ($blocked -and !$global:policyWrites) ('Cleanup mutated after '+$mode)
}
"PASS $checks malformed package, identity, pending selection and cleanup policy/race checks. No hardware accessed."
