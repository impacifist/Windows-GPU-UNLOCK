# GPL-2.0-only. Host tests for this machine's generated profile (-DGU_PROFILE_LOCAL).
# Mock/replay only: no GPU, firmware or boot access. Runs the reviewed-profile jobs against the
# fixtures make-profile.py wrote to research/profile/local/, plus the production transaction
# against this card's real PROM and the five stock assets with EFI simulated, plus the
# generator's own refusal cases. Only a complete pass writes local/test-results.json, the
# evidence build.ps1, build-usb.ps1 and stage-usb.ps1 require for a write build.
param([Parameter(Mandatory)][string]$Assets,[string]$Clang='clang',[string]$Python='python',[string]$VcVars)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $repo 'internal-boot/unlock/local-profile.ps1')
. (Join-Path $PSScriptRoot 'host.ps1')
$local=Join-Path $repo 'research/profile/local'
$evidencePath=Join-Path $local 'test-results.json'
if(Test-Path -LiteralPath $evidencePath){Remove-Item -LiteralPath $evidencePath} # never leave an earlier pass behind a failing run
$hw=Get-LocalProfile $repo
$report=Get-LocalProfileReport $repo $hw
$reportSha=Get-GuSha256 (Join-Path $local 'profile.json')
if($report.HeaderSha256 -ne $hw.HeaderSha256){throw 'profile-local.h is not the header make-profile.py wrote; regenerate it'}
$fx=Join-Path $local 'fixtures'
foreach($f in $report.Fixtures.PSObject.Properties){if((Get-GuSha256 (Join-Path $fx $f.Name)) -ne $f.Value){throw "Fixture $($f.Name) changed since make-profile.py"}}
foreach($a in (Get-Content -LiteralPath (Join-Path $repo 'internal-boot/unlock/assets.json') -Raw | ConvertFrom-Json)) {
 $p=Join-Path $Assets $a.Name
 if(!(Test-Path -LiteralPath $p) -or (Get-Item -LiteralPath $p).Length -ne $a.Bytes -or (Get-GuSha256 $p) -ne $a.Sha256.ToLowerInvariant()){throw "Asset verification failed: $($a.Name)"}
}
$inputManifest=@(Get-LocalTreeInputs $repo)
$tree=Get-LocalInputDigest $inputManifest
Import-GuHostCompiler $VcVars

# A reviewed profile for another card must reject this machine's real config and console graph.
$reviewed=@(@('-DGU_PROFILE_B650M_3090FE','220410de','145410de'),@('-DGU_PROFILE_B660M_3070TI','248210de','50521462'),@('-DGU_PROFILE_X870_3090TI','220310de','88701043'))
$foreign=($reviewed | Where-Object {$_[1] -ne $hw.TargetPci -or $_[2] -ne $hw.Subsystem} | Select-Object -First 1)[0]
$profile=@($hw.Define)
$prom=Join-Path $fx 'PROM.BIN';$probe=Join-Path $fx 'probe/REPORT.TXT';$cold=Join-Path $fx 'cold/REPORT.TXT'
$results=[ordered]@{}
function Run-Job([string]$name,[string[]]$compile,[string[]]$run,[int]$expect=0) {
 & $Clang @compile
 if($LASTEXITCODE -ne 0){throw "Compile failed: $name"}
 $ErrorActionPreference='Continue' # negative jobs print FAIL lines on stderr by design
 $out=& $run[0] @($run | Select-Object -Skip 1) 2>&1 | ForEach-Object {"$_"}
 $code=$LASTEXITCODE;$ErrorActionPreference='Stop'
 $script:results[$name]=[ordered]@{Exit=$code;Expected=$expect;Last=($out | Select-Object -Last 1)}
 "{0,-36} exit={1} expected={2}  {3}" -f $name,$code,$expect,($out | Select-Object -Last 1)
 if($code -ne $expect){throw "Unexpected result: $name"}
}

foreach($test in @('test_probe_records.py','test_local_topology.py','test_gpu_path.py')) {
 & $Python -I (Join-Path $PSScriptRoot $test)
 if($LASTEXITCODE -ne 0){throw "Capture-free generator regression failed: $test"}
}
& $Python -I (Join-Path $PSScriptRoot 'test_gpu_path_contract.py') --clang $Clang --output (Join-Path $PSScriptRoot ('build/path-contract-'+[Guid]::NewGuid().ToString('N')))
if($LASTEXITCODE -ne 0){throw 'Native/Python GPU path contract comparison failed'}
& $Python -I (Join-Path $PSScriptRoot 'test_make_profile.py') (Join-Path $local 'capture') (Build-GuFwsecLocate $repo $Clang)
if($LASTEXITCODE -ne 0){throw 'Generator refusal tests failed'};$results['generator refusals']=[ordered]@{Exit=0;Expected=0}

$unlock=Join-Path $repo 'internal-boot/unlock'
Push-Location -LiteralPath $unlock
try {
 $out='build/tests/local';[void][IO.Directory]::CreateDirectory((Join-Path $unlock $out))
 $flags=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-D_CRT_SECURE_NO_WARNINGS','-Icompat','-I../../deps/edk2/MdePkg/Include','-I../../deps/edk2/MdePkg/Include/X64')
 $core=@('port.c','compat.c','vendor/unlock_610.c','vendor/prep_610.c','vendor/layout_610.c','vendor/vbios_610.c','vendor/gfw_eval.c')
 $defs=@('-D__KERNEL__','-DGU_UEFI_PORT',"-DGU_TARGET_PCI_ID=0x$($hw.TargetPci)U")
 & $Python tests/test_stock_selection.py $Clang
 if($LASTEXITCODE -ne 0){throw 'Stock selection negative tests failed'};$results['stock-selection']=[ordered]@{Exit=0;Expected=0}
 & $Clang @flags -fno-builtin -Dmemcpy=GuTestMemcpy -Dmemset=GuTestMemset -Dmemchr=GuTestMemchr -Dmemcmp=GuTestMemcmp -Dstrcmp=GuTestStrcmp -Dstrlen=GuTestStrlen -c runtime.c -o "$out/runtime-host.obj"
 if($LASTEXITCODE -ne 0){throw 'Runtime host compile failed'}
 Run-Job 'checks (real config)' ($flags+$profile+@('tests/test_checks.c','checks.c','-o',"$out/checks.exe")) @("./$out/checks.exe",$probe)
 Run-Job 'checks other profile (must reject)' ($flags+@($foreign,'tests/test_checks.c','checks.c','-o',"$out/checks-other.exe")) @("./$out/checks-other.exe",$probe) 1
 Run-Job 'port' ($flags+$profile+@('-DGU_HOST_WRITE_TEST','tests/test_port.c','port.c','-o',"$out/port.exe")) @("./$out/port.exe")
 Run-Job 'fresh PCI identity' ($flags+$profile+@('-DGU_HOST_WRITE_TEST','tests/test_refresh_identity.c','ownership.c','port.c','-o',"$out/fresh-identity.exe")) @("./$out/fresh-identity.exe")
 Run-Job 'write-lock' ($flags+$profile+@('tests/test_preflight_lock.c','port.c','-o',"$out/write-lock.exe")) @("./$out/write-lock.exe")
 Run-Job 'master-attributes' ($flags+$profile+$defs+@('-DGU_HOST_WRITE_TEST','tests/test_master_attributes.c')+$core+@('-o',"$out/master.exe")) @("./$out/master.exe")
 Run-Job 'runtime' ($flags+@('-fno-builtin','tests/test_runtime.c','format.c',"$out/runtime-host.obj",'-o',"$out/runtime.exe")) @("./$out/runtime.exe")
 foreach($p in @(@('entryLatchedSs1Full','-DGU_ENTRY_LATCHED_SS1_FULL'),@('entryLatchedSs1Preserve','-DGU_ENTRY_LATCHED_SS1_PRESERVE'))) {
  Run-Job "profile-lock-$($p[0])" ($flags+$profile+@('-DGU_EXPERIMENTAL_3090',$p[1],'tests/test_profile_lock.c','port.c','-o',"$out/lock-$($p[0]).exe")) @("./$out/lock-$($p[0]).exe")
 }
} finally {Pop-Location}

$boot=Join-Path $repo 'internal-boot/boot'
Push-Location -LiteralPath $boot
try {
 $r=$report.Replay
 $fixtureArgs=@('--single-rtx',"--rtx-id=$($r.RtxId)","--rtx-fb=$($r.RtxFramebuffer)",'--no-edid-child')
 # A machine without an integrated GPU has no second display adapter to replay.
 if($r.IgpuId){$fixtureArgs+="--amd-id=$($r.IgpuId)"}else{$fixtureArgs+='--no-igpu'}
 # A firmware that binds no EDID driver (GU_NO_EDID_DRIVER) has no EDID consumer to replay.
 if($hw.NoEdidDriver){$fixtureArgs+='--no-edid-driver'}
 if($hw.ConsoleAggregate){$fixtureArgs+='--aggregate'}
 & $Python -I make-console-fixture.py (Join-Path $fx 'console/DIAG.TXT') @fixtureArgs
 if($LASTEXITCODE -ne 0){throw 'Replay fixture failed'}
 $d=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS','-DGU_SINGLE_RTX_CONSOLE','-DGU_DIAGNOSTIC_ONLY','-DGU_CONSOLE_PREPARE','-I','../../deps/edk2/MdePkg/Include','-I','../../deps/edk2/MdePkg/Include/X64')
 if(!$hw.ConsoleAggregate) {
  Run-Job 'r2 console replay (real graph)' ($d+$profile+@('test_r2.c','research-input.c','console-flow.c','-o','build/r2/test-r2-local.exe')) @('./build/r2/test-r2-local.exe')
 }
 Run-Job 'r2 replay other profile (must reject)' ($d+@($foreign,'test_r2.c','research-input.c','console-flow.c','-o','build/r2/test-r2-other.exe')) @('./build/r2/test-r2-other.exe') 1
 [void][IO.Directory]::CreateDirectory((Join-Path $boot 'build/cold/include'))
 @('/* Host-test placeholder; never used for an EFI build. */','#define GU_INTERNAL_PARTITION_INITIALIZER {0,0,0,{0,0,0,0,0,0,0,0}}','#define GU_WINDOWS_PARTITION_INITIALIZER {0,0,0,{0,0,0,0,0,0,0,0}}') | Set-Content build/cold/include/platform-config.h -Encoding ascii
 $w=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS','-I','build/cold/include','-I','../../deps/edk2/MdePkg/Include','-I','../../deps/edk2/MdePkg/Include/X64')
 if($hw.ConsoleAggregate) {
  [void][IO.Directory]::CreateDirectory((Join-Path $boot 'build/aggregate'))
  Copy-Item -LiteralPath 'build/r2/replay-fixture.h' -Destination 'build/aggregate/replay-fixture.h'
  Run-Job 'r2 console replay (real graph)' ($w+$profile+@('-DGU_HEADLESS_RESEARCH','-DGU_SINGLE_RTX_CONSOLE','-DGU_FULL_UNLOCK','-DGU_AUTOMATIC_BOOT','-DGU_AGGREGATE_CAPTURE_FIXTURE','test_aggregate_prepare.c','console-flow.c','../unlock/checks.c','../unlock/sha256.c','-o','build/aggregate/test-prepare-local.exe')) @('./build/aggregate/test-prepare-local.exe')
 }
 Run-Job 'research input policy' ($w+@('test_research_input.c','research-input.c','console-flow.c','-o','build/cold/test-input-local.exe')) @('./build/cold/test-input-local.exe')
 Run-Job 'cold pre-check (read-only)' ($w+$profile+@('-DGU_HEADLESS_RESEARCH','-DGU_SINGLE_RTX_CONSOLE','-DGU_FULL_UNLOCK','-DGU_AUTOMATIC_BOOT','test_cold_precheck.c','console-flow.c','../unlock/checks.c','../unlock/sha256.c','-o','build/cold/test-cold-local.exe')) @('./build/cold/test-cold-local.exe')
 Run-Job 'full flow gates' ($w+@('test_full_flow.c','full-flow.c','-o','build/cold/test-full-flow.exe')) @('./build/cold/test-full-flow.exe')
  # Every generated profile replays the complete driver release with its own captures.
  $words=[regex]::Matches([IO.File]::ReadAllText($probe),'value=0x([0-9a-f]{8})') | ForEach-Object {[Convert]::ToUInt32($_.Groups[1].Value,16)}
  if(@($words).Count -ne 64){throw 'The probe fixture does not hold 64 config dwords'}
  $configImage=New-Object byte[] 256
  for($k=0;$k -lt 64;$k++){[BitConverter]::GetBytes([uint32]@($words)[$k]).CopyTo($configImage,$k*4)}
  [void][IO.Directory]::CreateDirectory((Join-Path $boot 'build/release'))
  $configPath=Join-Path $boot 'build/release/config.bin';[IO.File]::WriteAllBytes($configPath,$configImage)
  $driverImage=Join-Path $local ('capture/'+$report.Code.nvgop.dump)
  if(!(Test-Path -LiteralPath $driverImage -PathType Leaf)){throw 'The captured nvgop image is missing from research/profile/local/capture'}
  $releaseName='release replay, no EDID driver (real graph)'
  $releaseArgs=@('./build/release/test-release-local.exe',$driverImage,$prom,$configPath)
  if(!$hw.NoEdidDriver) {
   $edidImage=Join-Path $local ('capture/'+$report.Code.edid.dump)
   if(!(Test-Path -LiteralPath $edidImage -PathType Leaf)){throw 'The captured EDID image is missing from research/profile/local/capture'}
   $releaseName='release replay, EDID driver (real graph)'
   $releaseArgs=@('./build/release/test-release-local.exe',$edidImage,$driverImage,$prom,$configPath)
  }
  $releaseSource='test_headless_full.c';$releaseExtra=@()
  if($hw.ConsoleAggregate) {
   $releaseName='release replay, aggregate console (real graph)'
   $releaseSource='test_aggregate_full.c';$releaseExtra=@('-DGU_AGGREGATE_CAPTURE_FIXTURE')
   $releaseArgs=@('./build/release/test-release-local.exe',$edidImage,$driverImage,$prom)
  }
  Run-Job $releaseName ($w+$profile+$releaseExtra+@('-DGU_HEADLESS_RESEARCH','-DGU_SINGLE_RTX_CONSOLE','-DGU_FULL_UNLOCK','-DGU_AUTOMATIC_BOOT',$releaseSource,'console-flow.c','../unlock/checks.c','../unlock/sha256.c','-o','build/release/test-release-local.exe')) $releaseArgs
} finally {Pop-Location}

# Transaction stage: production app + vendor unlock core against this card's real PROM and
# the five stock assets, every EFI and MMIO effect simulated.
Push-Location -LiteralPath $unlock
try {
 $app=@('app_io.c','ownership.c','format.c','sha256.c','checks.c')+$core
 $latched=$flags+$profile+$defs+@('-DGU_EXPERIMENTAL_3090','-DGU_ENTRY_LATCHED_SS1_FULL')
 $variants=@(
  @{Name='core transaction (efi)';Defs=@('-DGU_INTERNAL_BOOT');Src=@('tests/test_efi_transaction.c','checks.c')+$core;Args=@($prom,$Assets,$cold)},
  @{Name='manual entry-latched';Defs=@('-DGU_INTERNAL_BOOT');Src=@('tests/test_entry_latched.c')+$app;Args=@($prom,$cold,$Assets)},
  @{Name='automatic internal';Defs=@('-DGU_INTERNAL_BOOT','-DGU_AUTOMATIC_BOOT');Src=@('tests/test_automatic.c')+$app;Args=@($prom,$cold,$Assets)},
  @{Name='headless internal';Defs=@('-DGU_INTERNAL_BOOT','-DGU_AUTOMATIC_BOOT','-DGU_HEADLESS_RESEARCH');Src=@('tests/test_headless.c')+$app;Args=@($prom,$cold,$Assets)},
  @{Name='headless USB (shipped child)';Defs=@('-DGU_INTERNAL_BOOT','-DGU_AUTOMATIC_BOOT','-DGU_HEADLESS_RESEARCH','-DGU_USB_MEDIA');Src=@('tests/test_headless.c')+$app;Args=@($prom,$cold,$Assets)}
 )
 $i=0
 foreach($v in $variants){$exe="$out/tx-$i.exe";$i++;Run-Job $v.Name ($latched+$v.Defs+$v.Src+@('-o',$exe)) (@("./$exe")+$v.Args)}
} finally {Pop-Location}

if((Get-LocalTreeDigest $repo) -ne $tree -or (Get-LocalProfile $repo).HeaderSha256 -ne $hw.HeaderSha256){throw 'Sources or profile changed while the tests ran; rerun'}
if((Get-GuSha256 (Join-Path $local 'profile.json')) -cne $reportSha){throw 'Profile report changed during tests'}
Assert-LocalFixtures $repo $report.Fixtures
$expectedJobs=Get-LocalExpectedJobs $hw.NoEdidDriver $hw.ConsoleAggregate
Assert-LocalKeys $results @($expectedJobs.Keys) 'Completed host jobs'
foreach($jobName in $expectedJobs.Keys){if($results[$jobName].Exit -ne $expectedJobs[$jobName] -or $results[$jobName].Expected -ne $expectedJobs[$jobName]){throw 'Host result mismatch'}}
[ordered]@{Format=2;AllPassed=$true;HardwareAccess=$false;ProfileName=$hw.Name;HeaderSha256=$hw.HeaderSha256;PromSha256=$hw.Prom;
 Fixtures=$report.Fixtures;ProfileReportSha256=$reportSha;Inputs=$inputManifest;TreeSha256=$tree;TestedUtc=[DateTime]::UtcNow.ToString('o');Results=$results
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $evidencePath -Encoding utf8
"All $($results.Count) local-profile host jobs matched expectations; mock/replay only, hardware untested."
"Evidence: $evidencePath"
