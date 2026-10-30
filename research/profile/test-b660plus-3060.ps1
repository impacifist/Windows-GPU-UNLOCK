# GPL-2.0-only. Host tests for the reviewed-style -DGU_PROFILE_B660PLUS_3060 profile
# (ASUS PRIME B660-PLUS D4 / RTX 3060 12GB, aggregate console). Mock/replay only: no GPU,
# firmware or boot access. The reviewed-profile host jobs, run against fixtures from this
# machine's read-only probe captures (PROM.BIN, probe and cold REPORT.TXT), which stay
# git-ignored under research/profile/local/fixtures. The R2 single-RTX console replay is
# omitted (iGPU topology only); test-aggregate-graph-b660plus.ps1 covers the console graph.
param([Parameter(Mandatory)][string]$Assets,[string]$Clang='clang',[string]$Python='python',[string]$VcVars)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
function Import-GuHostCompiler([string]$VcVars) {
 if(!$VcVars) {
  $where=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  if(Test-Path -LiteralPath $where){$VcVars=& $where -latest -products * -find 'VC\Auxiliary\Build\vcvars64.bat' | Select-Object -First 1}
  if(!$VcVars){throw 'vcvars64.bat not found; pass -VcVars'}
 }
 $env:Path=(Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer')+';'+$env:Path
 $lines=& cmd.exe /d /c "call `"$VcVars`" >nul && set"
 if($LASTEXITCODE -ne 0){throw 'MSVC environment failed'}
 foreach($l in $lines){if($l -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process')}}
}
$fx=Join-Path $repo 'research/profile/local/fixtures'
$prom=Join-Path $fx 'PROM.BIN';$probe=Join-Path $fx 'probe/REPORT.TXT';$cold=Join-Path $fx 'cold/REPORT.TXT'
foreach($f in @($prom,$probe,$cold)){if(!(Test-Path -LiteralPath $f)){throw "Missing fixture $f"}}
if((Get-FileHash -LiteralPath $prom).Hash.ToLowerInvariant() -ne '7cc9018e80c63e4aec79c71cd4ed6bdeb5e147aeeb22b21b96efe90887ba8806'){throw 'PROM fixture is not this card'}
foreach($a in (Get-Content -LiteralPath (Join-Path $repo 'internal-boot/unlock/assets.json') -Raw | ConvertFrom-Json)) {
 $p=Join-Path $Assets $a.Name
 if(!(Test-Path -LiteralPath $p) -or (Get-Item -LiteralPath $p).Length -ne $a.Bytes -or (Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant() -ne $a.Sha256.ToLowerInvariant()){throw "Asset verification failed: $($a.Name)"}
}
Import-GuHostCompiler $VcVars

$profile=@('-DGU_PROFILE_B660PLUS_3060')
$foreign='-DGU_PROFILE_B660M_3070TI' # same board family and topology, other card: must reject
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

$unlock=Join-Path $repo 'internal-boot/unlock'
Push-Location -LiteralPath $unlock
try {
 $out='build/tests/b660plus-3060';[void][IO.Directory]::CreateDirectory((Join-Path $unlock $out))
 $flags=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-D_CRT_SECURE_NO_WARNINGS','-Icompat','-I../../deps/edk2/MdePkg/Include','-I../../deps/edk2/MdePkg/Include/X64')
 $core=@('port.c','compat.c','vendor/unlock_610.c','vendor/prep_610.c','vendor/layout_610.c','vendor/vbios_610.c','vendor/gfw_eval.c')
 $defs=@('-D__KERNEL__','-DGU_UEFI_PORT','-DGU_TARGET_PCI_ID=0x248710deU')
 & $Python tests/test_stock_selection.py $Clang
 if($LASTEXITCODE -ne 0){throw 'Stock selection negative tests failed'};$results['stock-selection']=[ordered]@{Exit=0;Expected=0}
 & $Clang @flags -fno-builtin -Dmemcpy=GuTestMemcpy -Dmemset=GuTestMemset -Dmemchr=GuTestMemchr -Dmemcmp=GuTestMemcmp -Dstrcmp=GuTestStrcmp -Dstrlen=GuTestStrlen -c runtime.c -o "$out/runtime-host.obj"
 if($LASTEXITCODE -ne 0){throw 'Runtime host compile failed'}
 Run-Job 'checks (real config)' ($flags+$profile+@('tests/test_checks.c','checks.c','-o',"$out/checks.exe")) @("./$out/checks.exe",$probe)
 Run-Job 'checks other profile (must reject)' ($flags+@($foreign,'tests/test_checks.c','checks.c','-o',"$out/checks-other.exe")) @("./$out/checks-other.exe",$probe) 1
 Run-Job 'port' ($flags+$profile+@('-DGU_HOST_WRITE_TEST','tests/test_port.c','port.c','-o',"$out/port.exe")) @("./$out/port.exe")
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
 [void][IO.Directory]::CreateDirectory((Join-Path $boot 'build/cold/include'))
 @('/* Host-test placeholder; never used for an EFI build. */','#define GU_INTERNAL_PARTITION_INITIALIZER {0,0,0,{0,0,0,0,0,0,0,0}}','#define GU_WINDOWS_PARTITION_INITIALIZER {0,0,0,{0,0,0,0,0,0,0,0}}') | Set-Content build/cold/include/platform-config.h -Encoding ascii
 $w=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS','-I','build/cold/include','-I','../../deps/edk2/MdePkg/Include','-I','../../deps/edk2/MdePkg/Include/X64')
 Run-Job 'cold pre-check (read-only)' ($w+$profile+@('-DGU_HEADLESS_RESEARCH','-DGU_SINGLE_RTX_CONSOLE','-DGU_FULL_UNLOCK','-DGU_AUTOMATIC_BOOT','test_cold_precheck.c','console-flow.c','../unlock/checks.c','../unlock/sha256.c','-o','build/cold/test-cold-b660plus.exe')) @('./build/cold/test-cold-b660plus.exe')
 Run-Job 'full flow gates' ($w+@('test_full_flow.c','full-flow.c','-o','build/cold/test-full-flow.exe')) @('./build/cold/test-full-flow.exe')
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

"All $($results.Count) b660plus-3060 host jobs matched expectations; mock/replay only (hardware evidence: CHANGES.md)."
