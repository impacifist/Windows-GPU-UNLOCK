# GPL-2.0-only. Synthetic filesystem fixtures only: never authorizes a real write build.
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $repo 'internal-boot/unlock/local-profile.ps1')
$sandbox=Join-Path $PSScriptRoot ('build/evidence-'+[Guid]::NewGuid().ToString('N'))
function Put([string]$Relative,[string]$Text) {
 $p=Join-Path $sandbox $Relative
 [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($p))
 [IO.File]::WriteAllText($p,$Text)
}
foreach($path in (Get-LocalRequiredFiles)){Put $path 'synthetic input'}
Put 'deps/edk2/MdePkg/Include/Uefi.h' 'synthetic header'
Put 'research/profile/local/fixtures/PROM.BIN' 'synthetic PROM, not GPU data'
$prom=Get-GuSha256 (Join-Path $sandbox 'research/profile/local/fixtures/PROM.BIN')
$header=@"
#define GU_LOCAL_PROFILE_GENERATOR 1
#define GU_NO_EDID_DRIVER 0
#define GU_PROFILE_NAME "LOCAL-220410de-145410de-12345678"
#define GU_BOARD_ID 0x220410deU
#define GU_BOARD_SUBSYSTEM 0x145410deU
#define GU_CHIP_ID 0xb72000a1U
#define GU_PROM_SHA256 "$prom"
#define GU_DEVICE_MEMORY_SIZE 0x6000U
"@
Put 'internal-boot/unlock/profile-local.h' $header
$fixtures=[ordered]@{'PROM.BIN'=$prom}
foreach($path in @('probe/REPORT.TXT','cold/REPORT.TXT','console/DIAG.TXT')) {
 Put ('research/profile/local/fixtures/'+$path) ('synthetic '+$path)
 $fixtures[$path]=Get-GuSha256 (Join-Path $sandbox ('research/profile/local/fixtures/'+$path))
}
$hw=Get-LocalProfile $sandbox
$report=[ordered]@{Format=1;ProfileName=$hw.Name;HeaderSha256=$hw.HeaderSha256;PromSha256=$prom;HardwareTested=$false;Fixtures=$fixtures;Replay=[ordered]@{NoEdidDriver=$false}}
Put 'research/profile/local/profile.json' ($report|ConvertTo-Json -Depth 8)
$jobs=[ordered]@{}
$expected=Get-LocalExpectedJobs
foreach($name in $expected.Keys){$jobs[$name]=[ordered]@{Exit=[int]$expected[$name];Expected=[int]$expected[$name]}}
$inputs=@(Get-LocalTreeInputs $sandbox)
$baseline=[ordered]@{Format=2;AllPassed=$true;HardwareAccess=$false;ProfileName=$hw.Name;
 HeaderSha256=$hw.HeaderSha256;PromSha256=$prom;ProfileReportSha256=(Get-GuSha256 (Join-Path $sandbox 'research/profile/local/profile.json'));
 Fixtures=$fixtures;Inputs=$inputs;TreeSha256=(Get-LocalInputDigest $inputs);Results=$jobs}|ConvertTo-Json -Depth 10
$checks=0
function Check([string]$Label,[scriptblock]$Mutate,[bool]$Accept=$false) {
 Put 'research/profile/local/test-results.json' $baseline
 $e=$baseline|ConvertFrom-Json
 & $Mutate $e
 Put 'research/profile/local/test-results.json' ($e|ConvertTo-Json -Depth 10)
 $passed=$true
 try{$null=Assert-LocalProfileEvidence $sandbox}catch{$passed=$false}
 if($passed -ne $Accept){throw "Evidence guard failed: $Label (accepted=$passed)"}
 $script:checks++
}
Check 'complete synthetic evidence' {} $true
Check 'old format' {param($e) $e.Format=1}
Check 'string pass flag' {param($e) $e.AllPassed='true'}
Check 'hardware flag' {param($e) $e.HardwareAccess=$true}
Check 'missing fixture' {param($e) $e.Fixtures.PSObject.Properties.Remove('cold/REPORT.TXT')}
Check 'extra fixture' {param($e) $e.Fixtures|Add-Member NoteProperty extra ('0'*64)}
Check 'fixture hash' {param($e) $e.Fixtures.'PROM.BIN'='0'*64}
Check 'report hash' {param($e) $e.ProfileReportSha256='0'*64}
Check 'missing job' {param($e) $e.Results.PSObject.Properties.Remove('runtime')}
Check 'extra job' {param($e) $e.Results|Add-Member NoteProperty extra @{Exit=0;Expected=0}}
Check 'string exit' {param($e) $e.Results.runtime.Exit='0'}
Check 'unexpected failure' {param($e) $e.Results.runtime.Exit=1}
Check 'negative job must reject' {param($e) $e.Results.'checks other profile (must reject)'.Exit=0}
Check 'weakened expected' {param($e) $e.Results.runtime.Expected=1;$e.Results.runtime.Exit=1}
Check 'missing manifest row' {param($e) $e.Inputs=@($e.Inputs|Select-Object -Skip 1)}
Check 'manifest hash' {param($e) $e.Inputs[0].Sha256='0'*64}
Check 'manifest path' {param($e) $e.Inputs[0].Path='other.c'}
Check 'digest' {param($e) $e.TreeSha256='0'*64}
# A firmware without an EDID driver releases the GPU unlike every reviewed profile, so its evidence
# must also hold the driver-release replay on its own graph, and that replay must have passed.
$noEdidHeader=$header.Replace('#define GU_NO_EDID_DRIVER 0','#define GU_NO_EDID_DRIVER 1')
Put 'internal-boot/unlock/profile-local.h' $noEdidHeader
$noEdidHw=Get-LocalProfile $sandbox
$reportNoEdid=[ordered]@{Format=1;ProfileName=$hw.Name;HeaderSha256=$noEdidHw.HeaderSha256;PromSha256=$prom;HardwareTested=$false;Fixtures=$fixtures;Replay=[ordered]@{NoEdidDriver=$true}}
Put 'research/profile/local/profile.json' ($reportNoEdid|ConvertTo-Json -Depth 8)
$jobsNoEdid=[ordered]@{};$expectedNoEdid=Get-LocalExpectedJobs $true
foreach($name in $expectedNoEdid.Keys){$jobsNoEdid[$name]=[ordered]@{Exit=[int]$expectedNoEdid[$name];Expected=[int]$expectedNoEdid[$name]}}
$normalBaseline=$baseline
$baseline=[ordered]@{Format=2;AllPassed=$true;HardwareAccess=$false;ProfileName=$hw.Name;
 HeaderSha256=$noEdidHw.HeaderSha256;PromSha256=$prom;ProfileReportSha256=(Get-GuSha256 (Join-Path $sandbox 'research/profile/local/profile.json'));
 Fixtures=$fixtures;Inputs=$inputs;TreeSha256=(Get-LocalInputDigest $inputs);Results=$jobsNoEdid}|ConvertTo-Json -Depth 10
Check 'no-EDID-driver evidence with the release replay' {} $true
Check 'no-EDID-driver evidence without the release replay' {param($e) $e.Results.PSObject.Properties.Remove('release replay, no EDID driver (real graph)')}
Check 'no-EDID-driver release replay failed' {param($e) $e.Results.'release replay, no EDID driver (real graph)'.Exit=1}
Check 'no-EDID-driver release replay expectation weakened' {param($e) $e.Results.'release replay, no EDID driver (real graph)'.Expected=1;$e.Results.'release replay, no EDID driver (real graph)'.Exit=1}
foreach($value in @($false,$null,'true','false',1)) {
 $reportNoEdid.Replay.NoEdidDriver=$value
 Put 'research/profile/local/profile.json' ($reportNoEdid|ConvertTo-Json -Depth 8)
 Check ('header1/report NoEdidDriver mismatch '+[string]$value) {
  param($e)
  $e.ProfileReportSha256=Get-GuSha256 (Join-Path $sandbox 'research/profile/local/profile.json')
  if(-not [bool]$value){$e.Results.PSObject.Properties.Remove('release replay, no EDID driver (real graph)')}
 }
}
$reportNoEdid.Replay.NoEdidDriver=$true
$baseline=$normalBaseline
Put 'internal-boot/unlock/profile-local.h' $header
Put 'research/profile/local/profile.json' ($report|ConvertTo-Json -Depth 8)
foreach($value in @($true,$null,'false',0)) {
 $report.Replay.NoEdidDriver=$value
 Put 'research/profile/local/profile.json' ($report|ConvertTo-Json -Depth 8)
 Check ('header0/report NoEdidDriver mismatch '+[string]$value) {
  param($e)
  $e.ProfileReportSha256=Get-GuSha256 (Join-Path $sandbox 'research/profile/local/profile.json')
  if([bool]$value){$e.Results|Add-Member NoteProperty 'release replay, no EDID driver (real graph)' @{Exit=0;Expected=0}}
 }
}
$report.Replay.NoEdidDriver=$false
Put 'research/profile/local/profile.json' ($report|ConvertTo-Json -Depth 8)
foreach($invalid in @(
 $header.Replace('#define GU_NO_EDID_DRIVER 0',''),
 $header.Replace('#define GU_NO_EDID_DRIVER 0','#define GU_NO_EDID_DRIVER 2'),
 ($header+"`n#define GU_NO_EDID_DRIVER 1"),
 ($header+"`n#undef GU_NO_EDID_DRIVER"),
 $header.Replace('#define GU_NO_EDID_DRIVER 0','#define GU_NO_EDID_DRIVER (0)'))) {
 Put 'internal-boot/unlock/profile-local.h' $invalid
 $accepted=$true;try{$null=Get-LocalProfile $sandbox}catch{$accepted=$false}
 if($accepted){throw 'Ambiguous/malformed local no-EDID define accepted'};$checks++
}
Put 'internal-boot/unlock/profile-local.h' $header
Check 'release replay job on a profile that has an EDID driver' {param($e) $e.Results|Add-Member NoteProperty 'release replay, no EDID driver (real graph)' @{Exit=0;Expected=0}}
Put 'research/profile/local/test-results.json' $baseline
$package=[ordered]@{HardwareProfile=$hw.Name;LocalProfileHeaderSha256=$hw.HeaderSha256;
 LocalProfileTreeSha256=(Get-LocalInputDigest $inputs);
 LocalProfileEvidenceSha256=(Get-GuSha256 (Join-Path $sandbox 'research/profile/local/test-results.json'))}
Assert-LocalProfilePackage $sandbox $package;$checks++
foreach($key in @('HardwareProfile','LocalProfileHeaderSha256','LocalProfileTreeSha256','LocalProfileEvidenceSha256')) {
 $saved=$package[$key];$package[$key]='stale'
 $accepted=$true;try{Assert-LocalProfilePackage $sandbox $package}catch{$accepted=$false}
 if($accepted){throw "Stale package accepted: $key"};$checks++;$package[$key]=$saved
}
# A new successful test run cannot make an older package eligible to stage.
Put 'research/profile/local/test-results.json' ($baseline+"`n")
$accepted=$true;try{Assert-LocalProfilePackage $sandbox $package}catch{$accepted=$false}
if($accepted){throw 'Package from an older test run was accepted'};$checks++
Put 'research/profile/local/test-results.json' $baseline
$manifest=[ordered]@{Format=2;Mode='single-rtx-headless-unlock-then-windows-usb';CanUnlock=$true;RequiresHumanKey=$false;HardwareTested=$false;EfiSha256=('0'*64);ChildSha256=('1'*64)}
foreach($key in $package.Keys){$manifest[$key]=$package[$key]}
Assert-GuUsbManifest $sandbox $manifest;$checks++
foreach($value in @($null,'','unknown','LOCAL-bad','local-220410de-145410de-12345678')) {
 $manifest.HardwareProfile=$value
 $accepted=$true;try{Assert-GuUsbManifest $sandbox $manifest}catch{$accepted=$false}
 if($accepted){throw 'Malformed USB profile bypassed evidence gate'};$checks++
}
$manifest.HardwareProfile=$hw.Name
foreach($key in @('Format','Mode','CanUnlock','RequiresHumanKey','HardwareTested','EfiSha256','ChildSha256')) {
 $saved=$manifest[$key];$manifest[$key]=$null
 $accepted=$true;try{Assert-GuUsbManifest $sandbox $manifest}catch{$accepted=$false}
 if($accepted){throw "Malformed USB manifest accepted: $key"};$checks++;$manifest[$key]=$saved
}
$manifest.HardwareProfile='ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE'
$accepted=$true;try{Assert-GuUsbManifest $sandbox $manifest}catch{$accepted=$false}
if($accepted){throw 'Local metadata was accepted under a reviewed identity'};$checks++
foreach($key in @($package.Keys|Where-Object {$_ -ne 'HardwareProfile'})){$manifest.Remove($key)}
foreach($name in @('ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE','ASUS-TUF-GAMING-B660M-E-D4-4003-RTX3070TI','GIGABYTE-X870-AORUS-ELITE-WIFI7-F13d-RTX3090TI','ASUS-PRIME-B660-PLUS-D4-3010-RTX3060','GIGABYTE-H610M-H-FC-RTX3060-12GB','GIGABYTE-B550-AORUS-ELITE-V2-F13-RTX3060-12GB')) {
 $manifest.HardwareProfile=$name;Assert-GuUsbManifest $sandbox $manifest;$checks++
}
foreach($path in @('internal-boot/efi_runtime.c','deps/edk2/MdePkg/Include/Uefi.h','research/headless/build-usb.ps1','research/profile/test-profile.ps1')) {
 $original=[IO.File]::ReadAllText((Join-Path $sandbox $path))
 Put $path 'changed after pass'
 Check ('source edit '+$path) {}
 Put $path $original
}
Put 'internal-boot/boot/added.h' 'new dependency'
Check 'added input' {}
# Retain the ignored synthetic tree for inspection. Its evidence cannot match the real repository.
"PASS $checks evidence checks; synthetic files only; no hardware access."
