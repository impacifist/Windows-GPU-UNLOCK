# GPL-2.0-only. Synthetic partition/USB mocks; build output stays in an ignored sandbox.
# Never stages an EFI or queries a real partition. Resulting EFI uses a dummy GUID: do not boot it.
param([Parameter(Mandatory)][string]$Clang,[Parameter(Mandatory)][string]$LldLink,[string]$Python='python')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$sandbox=Join-Path $PSScriptRoot ('build/probe-cli-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($sandbox)
# Copy source files only, never private local fixtures, captures or previous build output.
foreach($base in @('internal-boot/boot','internal-boot/unlock','deps/edk2/MdePkg/Include')) {
 foreach($file in (Get-ChildItem -LiteralPath (Join-Path $repo $base) -Recurse -File)) {
  $rel=$file.FullName.Substring($repo.Length+1).Replace('\','/')
  if($rel -match '/(build|local)/' -or $file.Name -in @('profile-local.h','platform-config.h','generated.h')){continue}
  if($file.Extension -notin @('.c','.h','.py')){continue}
  $dest=Join-Path $sandbox $rel
  [void][IO.Directory]::CreateDirectory((Split-Path $dest))
  Copy-Item -LiteralPath $file.FullName -Destination $dest
 }
}
foreach($rel in @('internal-boot/efi_runtime.c','research/profile/build-probe.ps1','research/profile/new-machine.ps1',
                 'internal-boot/unlock/local-profile.ps1','research/profile/host.ps1','research/profile/proven.json')) {
 $dest=Join-Path $sandbox $rel;[void][IO.Directory]::CreateDirectory((Split-Path $dest))
 Copy-Item -LiteralPath (Join-Path $repo $rel) -Destination $dest
}
$dummy=[guid]'11111111-2222-3333-4444-555555555555'
function Get-Partition {param($DriveLetter) [pscustomobject]@{Guid=$dummy;IsSystem=$true;DiskNumber=123}}
function Get-Disk {param($Number) [pscustomobject]@{BusType='USB';IsBoot=$false}}
function Get-Volume {param($DriveLetter) [pscustomobject]@{FileSystem='FAT32'}}
$build=Join-Path $sandbox 'research/profile/build-probe.ps1'
$checks=0
foreach($bus in @($null,1,43,255)) {
 $options=@{};if($null -ne $bus){$options.TargetBus=$bus}
 & $build -WindowsEspGuid $dummy -Clang $Clang -LldLink $LldLink -Python $Python @options
 if(!$?){throw 'Sandbox probe build failed'}
 $m=Get-Content -LiteralPath (Join-Path $sandbox 'internal-boot/boot/build/probe-generic/manifest.json') -Raw | ConvertFrom-Json
 if($m.CanUnlock -or $m.CanDetach -or $m.WritesFirmwareVariables -or $m.HardwareTested){throw 'Invalid diagnostic flags'}
 if($null -eq $bus) {
  if($m.ProbeBusMode -cne 'auto' -or $null -ne $m.TargetBus -or $m.Defines -cnotcontains '-DGU_PROBE_AUTO_BUS' -or
     @($m.Defines|Where-Object {$_ -match '^-DGU_PROBE_BUS='}).Count){throw 'Missing/ambiguous automatic bus selection'}
 } else {
  if($m.ProbeBusMode -cne 'explicit' -or $m.TargetBus -ne $bus -or $m.Defines -ccontains '-DGU_PROBE_AUTO_BUS' -or
     $m.Defines -cnotcontains ('-DGU_PROBE_BUS=0x{0:x}U' -f $bus)){throw 'Explicit bus selection was lost'}
 }
 $checks++
}
foreach($bus in @(0,256)) {
 try {& $build -WindowsEspGuid $dummy -Clang $Clang -LldLink $LldLink -TargetBus $bus;throw 'Invalid build bus accepted'}
 catch {if($_.FullyQualifiedErrorId -notmatch 'ParameterArgumentValidation'){throw}};$checks++
}
# The targeted ASRock diagnostic stays explicit on captured bus 7. It must not
# inherit automatic discovery or let another bus widen its exact admission.
& $build -WindowsEspGuid $dummy -Clang $Clang -LldLink $LldLink -Python $Python -Asrock3060 -TargetBus 7
if(!$?){throw 'Sandbox ASRock probe build failed'}
$m=Get-Content -LiteralPath (Join-Path $sandbox 'internal-boot/boot/build/probe-generic/manifest.json') -Raw | ConvertFrom-Json
if($m.CanUnlock -or $m.CanDetach -or $m.WritesFirmwareVariables -or $m.HardwareTested -or
   $m.ProbeBusMode -cne 'explicit' -or $m.TargetBus -ne 7 -or
   $m.Defines -ccontains '-DGU_PROBE_AUTO_BUS'){throw 'Invalid ASRock diagnostic contract'}
foreach($define in @('-DGU_PROBE_ASROCK_3060','-DGU_PROBE_DUMP_PROM','-DGU_USB_MEDIA','-DGU_PROBE_BUS=0x7U')) {
 if($m.Defines -cnotcontains $define){throw "Missing ASRock diagnostic define: $define"}
}
$checks++
foreach($bus in @($null,1,43,255)) {
 $options=@{};if($null -ne $bus){$options.TargetBus=$bus}
 try {& $build -WindowsEspGuid $dummy -Clang $Clang -LldLink $LldLink -Python $Python -Asrock3060 @options;throw 'Invalid ASRock bus accepted'}
 catch {if($_.Exception.Message -cne 'ASRock diagnostic requires PCI bus 7'){throw}};$checks++
}
# Stop at the very first probe-builder call: this tests real new-machine parameter
# binding/forwarding with all USB staging paths unreachable. IsSystem is false for the mock stick.
function Get-Partition {param($DriveLetter) [pscustomobject]@{Guid=$dummy;IsSystem=$false;DiskNumber=123}}
function Join-Path {
 param($Path,$ChildPath)
 if($Path -eq 'Z:'){return 'Z:\RTX3090-UNLOCK'}
 Microsoft.PowerShell.Management\Join-Path -Path $Path -ChildPath $ChildPath
}
# Get-Local* functions are dot-sourced but not called before the build. Force the
# initial directory check false, leaving all actual Copy/Move/staging calls unreachable.
function Test-Path {param($LiteralPath) return $false}
[IO.File]::WriteAllText($build,'param($WindowsEspGuid,$Clang,$LldLink,$Python,[int]$TargetBus) $global:GuTestProbeOptions=@{Explicit=$PSBoundParameters.ContainsKey("TargetBus");Bus=$TargetBus};throw "GU_TEST_STOP_BEFORE_STAGING"')
$workflow=Join-Path $sandbox 'research/profile/new-machine.ps1'
foreach($bus in @($null,1,43,255)) {
 $options=@{};if($null -ne $bus){$options.TargetBus=$bus}
 $global:GuTestProbeOptions=$null
 try {& $workflow -Drive Z -WindowsEspGuid $dummy -Assets 'unused' @options;throw 'Workflow did not stop'}
 catch {if($_.Exception.Message -cne 'GU_TEST_STOP_BEFORE_STAGING'){throw}}
 if($null -eq $global:GuTestProbeOptions -or $global:GuTestProbeOptions.Explicit -ne ($null -ne $bus) -or
    ($null -ne $bus -and $global:GuTestProbeOptions.Bus -ne $bus)){throw 'Workflow dropped TargetBus selection'}
 $checks++
}
foreach($bus in @(0,256)) {
  $global:GuTestProbeOptions=$null
  try {& $workflow -Drive Z -WindowsEspGuid $dummy -Assets 'unused' -TargetBus $bus;throw 'Invalid bus accepted'}
  catch {if($_.FullyQualifiedErrorId -notmatch 'ParameterArgumentValidation'){throw}}
  if($null -ne $global:GuTestProbeOptions){throw 'Invalid bus reached builder'};$checks++
}
# Resume existing captures: an explicit bus must be enforced before archiving,
# copying, host compilation or staging. Every copy/move below is a stop sentinel.
$resume=Join-Path $sandbox 'mock-usb-captures'
[void][IO.Directory]::CreateDirectory($resume)
function Join-Path {
 param($Path,$ChildPath)
 if($Path -eq 'Z:'){return $resume}
 Microsoft.PowerShell.Management\Join-Path -Path $Path -ChildPath $ChildPath
}
function Test-Path {param($LiteralPath) return $LiteralPath -eq $resume}
function Copy-Item {
 param([Parameter(ValueFromPipeline=$true)]$InputObject,$Destination,$LiteralPath)
 process {throw 'GU_TEST_STOP_BEFORE_CAPTURE_COPY'}
}
function Move-Item {throw 'GU_TEST_UNEXPECTED_MOVE'}
$valid='probe-target seg=0x0000 bus=0x002b dev=0x0000 fun=0x0000'
foreach($case in @(
 @{Target=$valid;Bus=$null;Accept=$true},
 @{Target=$valid;Bus=43;Accept=$true},
 @{Target=$valid;Bus=1;Accept=$false},
 @{Target=$valid;Bus=255;Accept=$false},
 @{Target=($valid+"`r`n"+$valid);Bus=43;Accept=$false},
 @{Target='probe-target seg=0x0000 bus=0x002b dev=0x0001 fun=0x0000';Bus=43;Accept=$false},
 @{Target='probe-target seg=0x0001 bus=0x002b dev=0x0000 fun=0x0000';Bus=43;Accept=$true},
 @{Target='probe-target seg=0xffff bus=0x002b dev=0x0000 fun=0x0000';Bus=43;Accept=$true},
 @{Target='probe-target seg=0x10000 bus=0x002b dev=0x0000 fun=0x0000';Bus=43;Accept=$false},
 @{Target='probe-target malformed';Bus=43;Accept=$false}
)) {
 foreach($n in @(0,1)) {
  [IO.File]::WriteAllText((Join-Path $resume ('DIAG{0:00}.TXT' -f $n)),
    "probe-pci-handles=1`r`n$($case.Target)`r`ncapture-complete=1`r`n")
 }
 foreach($stage in @($false,$true)) {
  $options=@{StageUnlock=$stage};if($null -ne $case.Bus){$options.TargetBus=$case.Bus}
  $global:GuTestProbeOptions=$null
  try {& $workflow -Drive Z -WindowsEspGuid $dummy -Assets 'unused' @options;throw 'Resume did not stop'}
  catch {
   if($case.Accept) {
    if($_.Exception.Message -cne 'GU_TEST_STOP_BEFORE_CAPTURE_COPY'){throw}
   } elseif($_.Exception.Message -cne 'Captured probe target does not match explicit -TargetBus; no profile or package was generated'){throw}
  }
  if($null -ne $global:GuTestProbeOptions){throw 'Resume unexpectedly rebuilt probe'}
  $checks++
 }
}
"PASS $checks probe CLI/build/resume cases; synthetic partition and USB mocks; dummy-GUID EFI must never be booted."
