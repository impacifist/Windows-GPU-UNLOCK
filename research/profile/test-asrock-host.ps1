# ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md.
# GPL-2.0-only. Research host tests only; never creates write-build evidence.
param([Parameter(Mandatory)][string]$Clang)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out=Join-Path $repo 'research/results/asrock-host-tests'
New-Item -ItemType Directory -Path $out -Force | Out-Null
. (Join-Path $repo 'internal-boot/unlock/local-profile.ps1')
$treeBefore=Get-LocalTreeDigest $repo
$results=[ordered]@{}
$evidence=Join-Path $out 'test-results.json'
if(Test-Path $evidence){Remove-Item -LiteralPath $evidence}
function Run-AsrockJob([string]$Name,[string[]]$Flags,[string[]]$Sources,[string[]]$Arguments=@(),[int]$Expected=0) {
 $exe=Join-Path $out ($Name+'.exe')
 & $Clang @Flags @Sources -o $exe
 if($LASTEXITCODE -ne 0){throw "Compile failed: $Name"}
 $log=& $exe @Arguments 2>&1 | Out-String
 $code=$LASTEXITCODE
 $log | Set-Content (Join-Path $out ($Name+'.log'))
 if($code -ne $Expected){Write-Output $log;throw "Failed $Name, exit $code expected $Expected"}
 $script:results[$Name]=[ordered]@{Exit=$code;Expected=$Expected;LogSha256=(Get-FileHash (Join-Path $out ($Name+'.log'))).Hash}
 Write-Output "$Name passed"
}
$candidate=Get-Content (Join-Path $repo 'research/results/asrock-profile-candidate/candidate.json') -Raw | ConvertFrom-Json
$capture=$candidate.captures[-1]
$capturePath=Get-ChildItem (Join-Path $repo 'research/results') -Directory -Filter 'asrock-targeted-second-*' | Sort-Object Name -Descending | Select-Object -First 1
if(!$capturePath){throw 'Missing second capture'}
$header=@('/* Host simulation only; never an EFI profile. */',
 '#ifndef GU_ASROCK_HOST_TEST','#error This header is only for host tests','#endif',
 '#define GU_LOCAL_PROFILE_GENERATOR 1','#define GU_PROFILE_NAME "ASROCK-HOST-TEST-ONLY"')
$header+=@($capture.defines | ForEach-Object {'#define '+$_[0]+' '+$_[1]})
$fwsec=Get-Content (Join-Path $capturePath.FullName 'fwsec.json') -Raw | ConvertFrom-Json
$header+=@(('#define GU_LOCAL_TEST_COLD_LINE "'+$capture.cold_line+'"'),
 ('#define GU_LOCAL_FWSEC_IMAGE_OFF 0x{0:x}U' -f $fwsec.fwsec.image_off),
 ('#define GU_LOCAL_FWSEC_IMAGE_SIZE 0x{0:x}U' -f $fwsec.fwsec.image_size))
$header | Set-Content (Join-Path $out 'profile-local.h') -Encoding ascii
$boot=Join-Path $repo 'internal-boot/boot'
& python -I (Join-Path $boot 'make-console-fixture.py') (Join-Path $capturePath.FullName 'DIAG00.TXT') --single-rtx --rtx-id=250310de "--rtx-fb=$($capture.rtx_fb)" --no-edid-child --no-igpu
if($LASTEXITCODE -ne 0){throw 'Console fixture failed'}
$flags=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-fshort-wchar',
 '-D_CRT_SECURE_NO_WARNINGS','-DGU_PROFILE_ASROCK_B450M_3060','-DGU_ASROCK_HOST_TEST',
 '-DGU_SINGLE_RTX_CONSOLE','-DGU_DIAGNOSTIC_ONLY','-DGU_CONSOLE_PREPARE',
 '-I',$out,'-I',(Join-Path $repo 'deps/edk2/MdePkg/Include'),
 '-I',(Join-Path $repo 'deps/edk2/MdePkg/Include/X64'))
Push-Location $boot
try {
 & $Clang @flags test_r2.c research-input.c console-flow.c -o (Join-Path $out 'console-replay.exe')
 if($LASTEXITCODE -ne 0){throw 'Console replay compile failed'}
 $result=& (Join-Path $out 'console-replay.exe') 2>&1 | Out-String
 $exitCode=$LASTEXITCODE
 $result | Set-Content (Join-Path $out 'console-replay.log')
 Write-Output $result
 if($exitCode -ne 0){throw "Console replay failed: $exitCode"}
 $results['console-replay']=[ordered]@{Exit=0;Expected=0;LogSha256=(Get-FileHash (Join-Path $out 'console-replay.log')).Hash}
} finally {Pop-Location}
$cold=Join-Path $out 'cold.txt'
$coldLines=@()
for($i=0;$i -lt 64;$i+=4){$coldLines+=('config[{0:x}]={1}' -f ($i*4),($capture.config[$i..($i+3)] -join ' '))}
$coldLines | Set-Content $cold -Encoding ascii
@($capture.cold_line,$capture.observed_line) | Add-Content $cold -Encoding ascii
$configBytes=New-Object byte[] 256
for($i=0;$i -lt 64;$i++){[BitConverter]::GetBytes([Convert]::ToUInt32($capture.config[$i],16)).CopyTo($configBytes,$i*4)}
[IO.File]::WriteAllBytes((Join-Path $out 'config.bin'),$configBytes)
$probe=Join-Path $out 'probe.txt'
$probeLines=@();for($i=0;$i -lt 64;$i++){$probeLines+=('config_offset=0x{0:x}' -f ($i*4));$probeLines+=('value=0x'+$capture.config[$i])}
$probeLines | Set-Content $probe -Encoding ascii
$assets=Join-Path $repo 'assets'
foreach($asset in (Get-Content (Join-Path $repo 'internal-boot/unlock/assets.json') -Raw | ConvertFrom-Json)) {
 $file=Join-Path $assets $asset.Name
 if((Get-Item $file).Length -ne $asset.Bytes -or (Get-FileHash $file).Hash -ne $asset.Sha256){throw 'Asset verification failed'}
}
Push-Location (Join-Path $repo 'internal-boot/unlock')
try {
 $core=@('port.c','compat.c','vendor/unlock_610.c','vendor/prep_610.c','vendor/layout_610.c','vendor/vbios_610.c','vendor/gfw_eval.c')
 $txFlags=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function',
 '-D_CRT_SECURE_NO_WARNINGS','-DGU_ASROCK_HOST_TEST','-DGU_PROFILE_ASROCK_B450M_3060','-D__KERNEL__','-DGU_UEFI_PORT',
 '-DGU_TARGET_PCI_ID=0x250310deU','-DGU_INTERNAL_BOOT','-DGU_EXPERIMENTAL_3090','-DGU_ENTRY_LATCHED_SS1_FULL',
 '-Icompat','-I',$out,'-I../../deps/edk2/MdePkg/Include','-I../../deps/edk2/MdePkg/Include/X64')
 & $Clang @txFlags tests/test_efi_transaction.c checks.c @core -o (Join-Path $out 'transaction.exe')
 if($LASTEXITCODE -ne 0){throw 'Transaction compile failed'}
 $result=& (Join-Path $out 'transaction.exe') (Join-Path $capturePath.FullName 'PROM.BIN') $assets $cold 2>&1 | Out-String
 $exitCode=$LASTEXITCODE
 $result | Set-Content (Join-Path $out 'transaction.log')
 Write-Output ($result -split "`n" | Select-Object -Last 4)
 if($exitCode -ne 0){throw "Transaction failed: $exitCode"}
 $results['transaction']=[ordered]@{Exit=0;Expected=0;LogSha256=(Get-FileHash (Join-Path $out 'transaction.log')).Hash}
 $app=@('app_io.c','ownership.c','format.c','sha256.c','checks.c')+$core
 foreach($variant in @(
  @{Name='manual-app';Source='tests/test_entry_latched.c';Extra=@()},
  @{Name='automatic-app';Source='tests/test_automatic.c';Extra=@('-DGU_AUTOMATIC_BOOT')},
  @{Name='headless-usb-app';Source='tests/test_headless.c';Extra=@('-DGU_AUTOMATIC_BOOT','-DGU_HEADLESS_RESEARCH','-DGU_USB_MEDIA')})) {
  Run-AsrockJob $variant.Name ($txFlags+$variant.Extra) (@($variant.Source)+$app) @((Join-Path $capturePath.FullName 'PROM.BIN'),$cold,$assets)
 }
 $base=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-D_CRT_SECURE_NO_WARNINGS',
 '-DGU_ASROCK_HOST_TEST','-DGU_PROFILE_ASROCK_B450M_3060','-Icompat','-I../../deps/edk2/MdePkg/Include','-I../../deps/edk2/MdePkg/Include/X64')
 Run-AsrockJob 'checks' $base @('tests/test_checks.c','checks.c') @($probe)
 Run-AsrockJob 'port' ($base+@('-DGU_HOST_WRITE_TEST')) @('tests/test_port.c','port.c')
 Run-AsrockJob 'write-lock' $base @('tests/test_preflight_lock.c','port.c')
 Run-AsrockJob 'master-attributes' ($base+@('-D__KERNEL__','-DGU_UEFI_PORT','-DGU_TARGET_PCI_ID=0x250310deU','-DGU_HOST_WRITE_TEST')) (@('tests/test_master_attributes.c')+$core)
 foreach($selector in @('GU_ENTRY_LATCHED_SS1_FULL','GU_ENTRY_LATCHED_SS1_PRESERVE')) {
  Run-AsrockJob $selector ($base+@('-DGU_EXPERIMENTAL_3090',"-D$selector")) @('tests/test_profile_lock.c','port.c')
 }
} finally {Pop-Location}
Push-Location $boot
try {
 $releaseFlags=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-fshort-wchar','-D_CRT_SECURE_NO_WARNINGS',
 '-DGU_ASROCK_HOST_TEST','-DGU_PROFILE_ASROCK_B450M_3060','-DGU_HEADLESS_RESEARCH','-DGU_SINGLE_RTX_CONSOLE','-DGU_FULL_UNLOCK','-DGU_AUTOMATIC_BOOT','-DGU_USB_MEDIA',
 '-I../../deps/edk2/MdePkg/Include','-I../../deps/edk2/MdePkg/Include/X64')
 Run-AsrockJob 'release' $releaseFlags @('test_asrock_release.c','console-flow.c','../unlock/checks.c','../unlock/sha256.c') @(
 (Join-Path $capturePath.FullName $capture.code.edid.dump),(Join-Path $capturePath.FullName $capture.code.nvgop.dump),
 (Join-Path $capturePath.FullName 'PROM.BIN'),(Join-Path $out 'config.bin'))
 Run-AsrockJob 'cold-precheck' $releaseFlags @('test_cold_precheck.c','console-flow.c','../unlock/checks.c','../unlock/sha256.c')
 Run-AsrockJob 'full-flow' $releaseFlags @('test_full_flow.c','full-flow.c')
} finally {Pop-Location}
if((Get-LocalTreeDigest $repo) -cne $treeBefore){throw 'Source inputs changed during tests'}
$fixtures=[ordered]@{}
foreach($fixture in @('DIAG00.TXT','PROM.BIN',$capture.code.edid.dump,$capture.code.nvgop.dump)){
 $fixtures[$fixture]=(Get-FileHash (Join-Path $capturePath.FullName $fixture)).Hash
}
[ordered]@{Format=1;AllPassed=$true;HardwareTested=$false;TreeSha256=$treeBefore;Results=$results;CapturePath=$capturePath.FullName;
 Fixtures=$fixtures;
 ProfileSha256=(Get-FileHash (Join-Path $repo 'internal-boot/unlock/asrock-b450m-3060.h')).Hash;
 PromSha256=(Get-FileHash (Join-Path $capturePath.FullName 'PROM.BIN')).Hash} | ConvertTo-Json -Depth 5 | Set-Content $evidence
