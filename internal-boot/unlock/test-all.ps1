# Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md.
param(
 [switch]$WithLocalFixtures,
 [string]$Capture='G:/RTX3090-PROBE/RUN00/REPORT.TXT',
 [string]$Prom='G:/RTX3090-PROBE/RUN00/PROM.BIN',
 [string]$ColdCapture='G:/RTX3090-PREFLIGHT/RUN00/REPORT.TXT',
 [string]$Assets=(Join-Path $PSScriptRoot '../analysis/pinned-assets'),
 [string]$Clang='clang',
 [string]$VcVars='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat',
 [switch]$ExperimentalUnlock,
 [switch]$InternalBoot,
 [ValidateSet('original03100062','observed03000062','entryLatchedSs1Preserve','entryLatchedSs1Full')][string]$StockProfile,
 # Machine profile for every host job; mirrors build.ps1. Default keeps the original behaviour.
 [ValidateSet('b650m-3090fe','b660m-3070ti','x870-3090ti','b660plus-3060','h610m-3060')][string]$HardwareProfile='b650m-3090fe',
 # Headless-only profiles have no manual/internal EFI; pass the built child (without extension) to PE-check instead.
 [string]$PeBase
)
$ErrorActionPreference='Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
 if(!(Test-Path -LiteralPath $PSScriptRoot)){throw 'Missing output parent'}
 if($ExperimentalUnlock -and !$PSBoundParameters.ContainsKey('StockProfile')){throw '-StockProfile is required with -ExperimentalUnlock'}
 if(!$ExperimentalUnlock -and $PSBoundParameters.ContainsKey('StockProfile')){throw '-StockProfile is only valid with -ExperimentalUnlock'}
 $selection=if($ExperimentalUnlock){$StockProfile}else{'default'}
 $out="build/tests/$selection"
 New-Item -ItemType Directory -Path $out -Force | Out-Null
 $profiles=if($ExperimentalUnlock){@($StockProfile)}else{@('original03100062','observed03000062','entryLatchedSs1Preserve','entryLatchedSs1Full')}
 function ProfileDefine([string]$profile) {
  switch($profile){'original03100062'{'-DGU_STOCK_PROFILE_ORIGINAL_03100062'} 'observed03000062'{'-DGU_STOCK_PROFILE_OBSERVED_03000062'} 'entryLatchedSs1Preserve'{'-DGU_ENTRY_LATCHED_SS1_PRESERVE'} 'entryLatchedSs1Full'{'-DGU_ENTRY_LATCHED_SS1_FULL'}}
 }
 # Import VS2022's host CRT/SDK environment for the official LLVM compiler.
 $vc=$VcVars
 $environment=& cmd.exe /d /c "call `"$vc`" >nul && set"
 if($LASTEXITCODE -ne 0){throw 'VS2022 environment failed'}
 foreach($line in $environment){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process')}}
 $flags=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-D_CRT_SECURE_NO_WARNINGS','-Icompat','-I../../deps/edk2/MdePkg/Include','-I../../deps/edk2/MdePkg/Include/X64')
 $core=@('port.c','compat.c','vendor/unlock_610.c','vendor/prep_610.c','vendor/layout_610.c','vendor/vbios_610.c','vendor/gfw_eval.c')
 $hwTargets=@{'b650m-3090fe'=@('','220410de');'b660m-3070ti'=@('-DGU_PROFILE_B660M_3070TI','248210de');'x870-3090ti'=@('-DGU_PROFILE_X870_3090TI','220310de');'b660plus-3060'=@('-DGU_PROFILE_B660PLUS_3060','248710de');'h610m-3060'=@('-DGU_PROFILE_H610M_3060','250410de')}
 $hwDefine=$hwTargets[$HardwareProfile][0]
 if($hwDefine){$flags+=$hwDefine}
 $defs=@('-D__KERNEL__','-DGU_UEFI_PORT',"-DGU_TARGET_PCI_ID=0x$($hwTargets[$HardwareProfile][1])U")
 if($InternalBoot) {
  if(!$ExperimentalUnlock -or $StockProfile -ne 'entryLatchedSs1Full'){throw 'Internal tests require the full profile'}
  $defs+='-DGU_INTERNAL_BOOT'
 }
 & $Clang @flags -fno-builtin -Dmemcpy=GuTestMemcpy -Dmemset=GuTestMemset -Dmemchr=GuTestMemchr -Dmemcmp=GuTestMemcmp -Dstrcmp=GuTestStrcmp -Dstrlen=GuTestStrlen -c runtime.c -o "$out/runtime-host.obj"
 if($LASTEXITCODE -ne 0){throw 'Runtime host compile failed'}
 $jobs=@(
  @{Name='checks';Sources=@('tests/test_checks.c','checks.c');Defines=@();Args=@($Capture)},
  @{Name='port';Sources=@('tests/test_port.c','port.c');Defines=@('-DGU_HOST_WRITE_TEST');Args=@()},
  @{Name='preflight';Sources=@('tests/test_3090_preflight.c','app.c','app_io.c','ownership.c','format.c','sha256.c','checks.c')+$core;Defines=$defs;Args=@($Prom,$Capture)},
  @{Name='write-lock';Sources=@('tests/test_preflight_lock.c','port.c');Defines=@();Args=@()},
  @{Name='master-attributes';Sources=@('tests/test_master_attributes.c')+$core;Defines=$defs+@('-DGU_HOST_WRITE_TEST');Args=@()},
  @{Name='runtime';Sources=@('tests/test_runtime.c','format.c',"$out/runtime-host.obj");Defines=@('-fno-builtin');Args=@()}
 )
 foreach($profile in $profiles) {
  $profileDefs=@('-DGU_EXPERIMENTAL_3090',(ProfileDefine $profile))
  $jobs+=@{Name="profile-lock-$profile";Sources=@('tests/test_profile_lock.c','port.c');Defines=$profileDefs;Args=@()}
 }
  if($WithLocalFixtures) {
   $jobs+=@{Name='sha';Sources=@('tests/test_sha256.c','sha256.c');Defines=@();Args=@($Prom)}
   $experimentalArgs=@($Prom,$ColdCapture)
   $haveAssets=Test-Path -LiteralPath (Join-Path $Assets 'gsp_boot_desc_ga102.bin')
   if($haveAssets) {
    foreach($asset in (Get-Content -Raw -LiteralPath assets.json | ConvertFrom-Json)) {
     $path=Join-Path $Assets $asset.Name
     if(!(Test-Path -LiteralPath $path) -or (Get-Item -LiteralPath $path).Length -ne $asset.Bytes -or
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $asset.Sha256){throw "Asset verification failed: $($asset.Name)"}
    }
    $experimentalArgs+=$Assets
   }
   foreach($profile in $profiles) {
    $profileDefs=$defs+@('-DGU_EXPERIMENTAL_3090',(ProfileDefine $profile))
    if($haveAssets){$jobs+=@{Name="core24GB-$profile";Sources=@('tests/test_efi_transaction.c')+$core;Defines=$profileDefs;Args=@($Prom,$Assets,$ColdCapture)}}
    $appTest=if($profile -like 'entryLatchedSs1*'){'tests/test_entry_latched.c'}else{'tests/test_3090_experimental.c'}
    $jobs+=@{Name="experimental-$profile";Sources=@($appTest,'app_io.c','ownership.c','format.c','sha256.c','checks.c')+$core;Defines=$profileDefs;Args=$experimentalArgs}
   }
 }
  $results=@()
  & python tests/test_stock_selection.py $Clang
  if($LASTEXITCODE -ne 0){throw 'Stock selection negative tests failed'}
  & python tests/test_hardware_profiles.py $Clang
  if($LASTEXITCODE -ne 0){throw 'Hardware profile tests failed'}
 foreach($job in $jobs) {
  $exe="$out/$($job.Name)-tests.exe"
  & $Clang @flags @($job.Defines) @($job.Sources) -o $exe
  if($LASTEXITCODE -ne 0){throw "Compile failed: $($job.Name)"}
  & "./$exe" @($job.Args)
  $code=$LASTEXITCODE;$results+=[ordered]@{Name=$job.Name;ExitCode=$code}
  if($code -ne 0){throw "Test failed: $($job.Name)"}
 }
 $peBase=if($PeBase){$PeBase}elseif($InternalBoot){"build/experimental/$StockProfile/internal/RTX3090-EXPERIMENTAL-$StockProfile-INTERNAL"}elseif($ExperimentalUnlock){"build/experimental/$StockProfile/RTX3090-EXPERIMENTAL-$StockProfile"}else{'build/RTX3090-PREFLIGHT'}
 & python tests/test_pe.py "$peBase.EFI" "$peBase.map"
 if($LASTEXITCODE -ne 0){throw 'PE tests failed'}
  $stockComplete=([bool]$WithLocalFixtures -and [bool]$haveAssets)
  $resultPath=if($WithLocalFixtures){"$out/test-results-local.json"}else{"$out/test-results.json"}
  [ordered]@{AllPassed=(!$WithLocalFixtures -or $stockComplete);ExecutedTestsPassed=$true;ExperimentalComplete=$stockComplete;HardwareAccess=$false;Mode='preflight-and-experimental-host-mocks';Results=$results;
   StockProfile=$(if($ExperimentalUnlock){$StockProfile}else{$null});TestedProfiles=$profiles;HardwareProfile=$HardwareProfile;StockTransactions=$(if($haveAssets){'Actual PROM + verified stock assets, EFI mocked; selected SS0 simulated'}else{'BLOCKED: missing stock asset fixtures'});WithLocalFixtures=[bool]$WithLocalFixtures
  } | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath $resultPath
  "All $($jobs.Count) host mock groups and PE tests passed; hardware untested."
  if($WithLocalFixtures -and !$stockComplete) {
   Write-Warning "PARTIAL: stock transaction fixtures are missing. Experimental build is not fully verified; see $resultPath."
   exit 2
  }
} finally {Pop-Location}
