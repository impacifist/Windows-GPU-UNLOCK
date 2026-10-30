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
Check 'EDID-driver evidence without the release replay' {param($e) $e.Results.PSObject.Properties.Remove('release replay, EDID driver (real graph)')}
Check 'EDID-driver release replay failed' {param($e) $e.Results.'release replay, EDID driver (real graph)'.Exit=1}
Check 'EDID-driver release replay expectation weakened' {param($e) $e.Results.'release replay, EDID driver (real graph)'.Expected=1;$e.Results.'release replay, EDID driver (real graph)'.Exit=1}
# Test the actual JSON number tokens, not just pre-serialization .NET types.
foreach($json in @('0','1','2','-1','2147483648','9223372036854775807')) {
 $value=ConvertFrom-Json ('{"value":'+$json+'}')
 foreach($want in @(0,1,2)) {
  $accepted=Test-GuExactInteger $value.value $want
  if($accepted -ne ($json -ceq [string]$want)){throw "JSON integer comparison failed: $json expected $want"};$checks++
 }
}
foreach($json in @('null','true','false','"0"','"1"','"2"','0.0','1.0','2e0','[]','{}')) {
 $value=ConvertFrom-Json ('{"value":'+$json+'}')
 foreach($want in @(0,1,2)) {
  if(Test-GuExactInteger $value.value $want){throw "Coerced JSON token accepted: $json expected $want"};$checks++
 }
}
Check 'string evidence format' {param($e) $e.Format='2'}
Check 'boolean evidence format' {param($e) $e.Format=$true}
Check 'oversized evidence format' {param($e) $e.Format=[long]2147483648}
Check 'boolean exit' {param($e) $e.Results.runtime.Exit=$false}
Check 'null expected' {param($e) $e.Results.runtime.Expected=$null}
foreach($json in @('"1"','1.0','true','null','2147483648')) {
 $validReport=$report|ConvertTo-Json -Depth 8
 Put 'research/profile/local/profile.json' ([regex]::Replace($validReport,'"Format":\s*1\b',('"Format": '+$json)))
 Check ('invalid report Format token '+$json) {param($e) $e.ProfileReportSha256=Get-GuSha256 (Join-Path $sandbox 'research/profile/local/profile.json')}
}
Put 'research/profile/local/profile.json' ($report|ConvertTo-Json -Depth 8)
Check 'old format' {param($e) $e.Format=1}
Check 'string pass flag' {param($e) $e.AllPassed='true'}
Check 'hardware flag' {param($e) $e.HardwareAccess=$true}
Check 'missing fixture' {param($e) $e.Fixtures.PSObject.Properties.Remove('cold/REPORT.TXT')}
Check 'extra fixture' {param($e) $e.Fixtures|Add-Member NoteProperty extra ('0'*64)}
Check 'fixture hash' {param($e) $e.Fixtures.'PROM.BIN'='0'*64}
Check 'report hash' {param($e) $e.ProfileReportSha256='0'*64}
Check 'missing job' {param($e) $e.Results.PSObject.Properties.Remove('runtime')}
Check 'missing actual input policy job' {param($e) $e.Results.PSObject.Properties.Remove('research input policy')}
Check 'previous job set without fresh PCI identity' {param($e) $e.Results.PSObject.Properties.Remove('fresh PCI identity')}
Check 'fresh PCI identity failed' {param($e) $e.Results.'fresh PCI identity'.Exit=1}
Check 'fresh PCI identity expectation weakened' {param($e) $e.Results.'fresh PCI identity'.Exit=1;$e.Results.'fresh PCI identity'.Expected=1}
Check 'input policy expectation weakened' {param($e) $e.Results.'research input policy'.Exit=1;$e.Results.'research input policy'.Expected=1}
Check 'extra job' {param($e) $e.Results|Add-Member NoteProperty extra @{Exit=0;Expected=0}}
Check 'string exit' {param($e) $e.Results.runtime.Exit='0'}
Check 'unexpected failure' {param($e) $e.Results.runtime.Exit=1}
Check 'negative job must reject' {param($e) $e.Results.'checks other profile (must reject)'.Exit=0}
Check 'weakened expected' {param($e) $e.Results.runtime.Expected=1;$e.Results.runtime.Exit=1}
Check 'missing manifest row' {param($e) $e.Inputs=@($e.Inputs|Select-Object -Skip 1)}
Check 'manifest hash' {param($e) $e.Inputs[0].Sha256='0'*64}
Check 'manifest path' {param($e) $e.Inputs[0].Path='other.c'}
Check 'digest' {param($e) $e.TreeSha256='0'*64}
Put 'research/pci-decode/snapshot.c' 'changed readonly input'
Check 'external readonly source changed' {param($e)}
Put 'research/pci-decode/snapshot.c' 'synthetic input'
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
foreach($aggregate in @(0,1)) {
 foreach($children in @(1,2)) {
  $topologyHeader=$header+"`n#define GU_LOCAL_CONSOLE_AGGREGATE $aggregate`n#define GU_LOCAL_CHILD_COUNT $children`n"
  Put 'internal-boot/unlock/profile-local.h' $topologyHeader
  $topologyHw=Get-LocalProfile $sandbox
  if($topologyHw.LegacyTopology -or $topologyHw.ConsoleAggregate -ne [bool]$aggregate -or $topologyHw.ChildCount -ne $children){throw 'Local topology metadata changed during parsing'};$checks++
  $topologyReport=[ordered]@{Format=1;ProfileName=$hw.Name;HeaderSha256=$topologyHw.HeaderSha256;PromSha256=$prom;HardwareTested=$false;Fixtures=$fixtures;Replay=[ordered]@{NoEdidDriver=$false;Aggregate=[bool]$aggregate;ChildCount=$children}}
  Put 'research/profile/local/profile.json' ($topologyReport|ConvertTo-Json -Depth 8)
  $null=Get-LocalProfileReport $sandbox $topologyHw;$checks++
  $topologyExpected=Get-LocalExpectedJobs $false ([bool]$aggregate)
  $topologyJobs=[ordered]@{}
  foreach($name in $topologyExpected.Keys){$topologyJobs[$name]=[ordered]@{Exit=[int]$topologyExpected[$name];Expected=[int]$topologyExpected[$name]}}
  $baseline=[ordered]@{Format=2;AllPassed=$true;HardwareAccess=$false;ProfileName=$hw.Name;
   HeaderSha256=$topologyHw.HeaderSha256;PromSha256=$prom;ProfileReportSha256=(Get-GuSha256 (Join-Path $sandbox 'research/profile/local/profile.json'));
   Fixtures=$fixtures;Inputs=$inputs;TreeSha256=(Get-LocalInputDigest $inputs);Results=$topologyJobs}|ConvertTo-Json -Depth 10
  Check 'matching topology evidence' {} $true
  if($aggregate) {
   Check 'aggregate release replay missing' {param($e) $e.Results.PSObject.Properties.Remove('release replay, aggregate console (real graph)')}
   Check 'aggregate replay replaced with nonaggregate replay' {param($e)
    $e.Results.PSObject.Properties.Remove('release replay, aggregate console (real graph)')
    $e.Results|Add-Member NoteProperty 'release replay, EDID driver (real graph)' @{Exit=0;Expected=0}
   }
   Check 'aggregate replay failure' {param($e) $e.Results.'release replay, aggregate console (real graph)'.Exit=1}
   Check 'aggregate replay expectation weakened' {param($e) $e.Results.'release replay, aggregate console (real graph)'.Exit=1;$e.Results.'release replay, aggregate console (real graph)'.Expected=1}
  }
  foreach($token in @('null','true','false','"1"','"2"','1.0','2e0','0','3','2147483648')) {
   $text=$topologyReport|ConvertTo-Json -Depth 8
   Put 'research/profile/local/profile.json' ([regex]::Replace($text,'"ChildCount":\s*[12]\b',('"ChildCount": '+$token)))
   $accepted=$true;try{$null=Get-LocalProfileReport $sandbox $topologyHw}catch{$accepted=$false}
   if($accepted){throw "Invalid topology child count accepted: $token"};$checks++
  }
  foreach($value in @($null,'false',0,(-not [bool]$aggregate))) {
   $topologyReport.Replay.Aggregate=$value
   Put 'research/profile/local/profile.json' ($topologyReport|ConvertTo-Json -Depth 8)
   $accepted=$true;try{$null=Get-LocalProfileReport $sandbox $topologyHw}catch{$accepted=$false}
   if($accepted){throw 'Invalid topology aggregate flag accepted'};$checks++
  }
  $topologyReport.Replay.Remove('Aggregate');$topologyReport.Replay.Remove('ChildCount')
  Put 'research/profile/local/profile.json' ($topologyReport|ConvertTo-Json -Depth 8)
  $accepted=$true;try{$null=Get-LocalProfileReport $sandbox $topologyHw}catch{$accepted=$false}
  if($accepted){throw 'New topology header accepted a legacy report'};$checks++
 }
}
foreach($suffix in @(
 '#define GU_LOCAL_CONSOLE_AGGREGATE 1',
 '#define GU_LOCAL_CHILD_COUNT 2',
 "#define GU_LOCAL_CONSOLE_AGGREGATE 2`n#define GU_LOCAL_CHILD_COUNT 1",
 "#define GU_LOCAL_CONSOLE_AGGREGATE 1`n#define GU_LOCAL_CHILD_COUNT 0",
 "#define GU_LOCAL_CONSOLE_AGGREGATE 1`n#define GU_LOCAL_CHILD_COUNT 3",
 "#define GU_LOCAL_CONSOLE_AGGREGATE (1)`n#define GU_LOCAL_CHILD_COUNT 1",
 "#define GU_LOCAL_CONSOLE_AGGREGATE 1`n#define GU_LOCAL_CHILD_COUNT 1`n#define GU_LOCAL_CHILD_COUNT 2",
 '#undef GU_LOCAL_CONSOLE_AGGREGATE',
 '#define GU_CONSOLE_AGGREGATE 1',
 '#undef GU_CONSOLE_CHILD_COUNT')) {
 Put 'internal-boot/unlock/profile-local.h' ($header+"`n"+$suffix+"`n")
 $accepted=$true;try{$null=Get-LocalProfile $sandbox}catch{$accepted=$false}
 if($accepted){throw 'Malformed topology header accepted'};$checks++
}
Put 'internal-boot/unlock/profile-local.h' ($noEdidHeader+"`n#define GU_LOCAL_CONSOLE_AGGREGATE 1`n#define GU_LOCAL_CHILD_COUNT 1`n")
$accepted=$true;try{$null=Get-LocalProfile $sandbox}catch{$accepted=$false}
if($accepted){throw 'Aggregate without EDID driver accepted'};$checks++
$baseline=$normalBaseline
Put 'internal-boot/unlock/profile-local.h' $header
Put 'research/profile/local/profile.json' ($report|ConvertTo-Json -Depth 8)
Check 'release replay job on a profile that has an EDID driver' {param($e) $e.Results|Add-Member NoteProperty 'release replay, no EDID driver (real graph)' @{Exit=0;Expected=0}}
foreach($children in @(3,4)) {
 $manyHeader=$noEdidHeader+"`n#define GU_LOCAL_CONSOLE_AGGREGATE 0`n#define GU_LOCAL_CHILD_COUNT $children`n"
 Put 'internal-boot/unlock/profile-local.h' $manyHeader
 $manyHw=Get-LocalProfile $sandbox
 if(!$manyHw.NoEdidDriver -or $manyHw.ConsoleAggregate -or $manyHw.ChildCount -ne $children){throw 'Many-child topology parsed incorrectly'};$checks++
 Check 'previous evidence cannot authorize a many-child header' {}
 $manyReport=[ordered]@{Format=1;ProfileName=$hw.Name;HeaderSha256=$manyHw.HeaderSha256;PromSha256=$prom;HardwareTested=$false;Fixtures=$fixtures;Replay=[ordered]@{NoEdidDriver=$true;Aggregate=$false;ChildCount=$children}}
 Put 'research/profile/local/profile.json' ($manyReport|ConvertTo-Json -Depth 8)
 $e=$normalBaseline|ConvertFrom-Json
 $e.HeaderSha256=$manyHw.HeaderSha256;$e.ProfileReportSha256=Get-GuSha256 (Join-Path $sandbox 'research/profile/local/profile.json')
 $e.Results=$jobsNoEdid;$baseline=$e|ConvertTo-Json -Depth 10
 Check 'matching many-child evidence and release replay' {} $true
 foreach($token in @('null','true','"3"','"4"','3.0','4e0','0','5',(''+($children -bxor 1)))) {
  $text=$manyReport|ConvertTo-Json -Depth 8
  Put 'research/profile/local/profile.json' ([regex]::Replace($text,'"ChildCount":\s*[34]\b',('"ChildCount": '+$token)))
  Check ('many-child report mismatch '+$token) {param($e) $e.ProfileReportSha256=Get-GuSha256 (Join-Path $sandbox 'research/profile/local/profile.json')}
 }
 foreach($bad in @($manyHeader.Replace('GU_NO_EDID_DRIVER 1','GU_NO_EDID_DRIVER 0'),
                    $manyHeader.Replace('GU_LOCAL_CONSOLE_AGGREGATE 0','GU_LOCAL_CONSOLE_AGGREGATE 1'))) {
  Put 'internal-boot/unlock/profile-local.h' $bad
  $accepted=$true;try{$null=Get-LocalProfile $sandbox}catch{$accepted=$false}
  if($accepted){throw 'Many-child profile escaped no-EDID/nonaggregate restriction'};$checks++
 }
 $baseline=$normalBaseline
}
foreach($children in @(0,5)) {
 Put 'internal-boot/unlock/profile-local.h' ($noEdidHeader+"`n#define GU_LOCAL_CONSOLE_AGGREGATE 0`n#define GU_LOCAL_CHILD_COUNT $children`n")
 $accepted=$true;try{$null=Get-LocalProfile $sandbox}catch{$accepted=$false}
 if($accepted){throw 'Invalid no-EDID child count accepted'};$checks++
}
Put 'internal-boot/unlock/profile-local.h' $header
Put 'research/profile/local/profile.json' ($report|ConvertTo-Json -Depth 8)
# A matching header/report is required even if evidence hashes were recomputed.
foreach($segment in @(0,1,65535)) {
 $segmentHeader=$header+("`n#define GU_TARGET_SEGMENT 0x{0:x}U`n" -f $segment)
 Put 'internal-boot/unlock/profile-local.h' $segmentHeader
 $segmentHw=Get-LocalProfile $sandbox
 if($segmentHw.LegacySegment -or $segmentHw.Segment -ne $segment){throw 'Segment changed during header parsing'};$checks++
 $segmentReport=[ordered]@{Format=1;ProfileName=$hw.Name;HeaderSha256=$segmentHw.HeaderSha256;PromSha256=$prom;HardwareTested=$false;Fixtures=$fixtures;Replay=[ordered]@{NoEdidDriver=$false;Segment=$segment}}
 Put 'research/profile/local/profile.json' ($segmentReport|ConvertTo-Json -Depth 8)
 $e=$normalBaseline|ConvertFrom-Json
 $e.HeaderSha256=$segmentHw.HeaderSha256
 $e.ProfileReportSha256=Get-GuSha256 (Join-Path $sandbox 'research/profile/local/profile.json')
 $baseline=$e|ConvertTo-Json -Depth 10
 Check 'matching segment evidence' {} $true
 foreach($token in @('null','true','false','"0"','"1"','0.0','1.0','65535.0','1e0','-1','65536','2147483648',(''+($segment -bxor 1)))) {
  $text=$segmentReport|ConvertTo-Json -Depth 8
  Put 'research/profile/local/profile.json' ([regex]::Replace($text,'"Segment":\s*[0-9]+\b',('"Segment": '+$token)))
  Check ('invalid segment token '+$token) {param($e) $e.ProfileReportSha256=Get-GuSha256 (Join-Path $sandbox 'research/profile/local/profile.json')}
 }
 $segmentReport.Replay.Remove('Segment')
 Put 'research/profile/local/profile.json' ($segmentReport|ConvertTo-Json -Depth 8)
 Check 'new segment header requires report segment' {param($e) $e.ProfileReportSha256=Get-GuSha256 (Join-Path $sandbox 'research/profile/local/profile.json')}
}
foreach($suffix in @(
 '#define GU_TARGET_SEGMENT 0x10000U',
 '#define GU_TARGET_SEGMENT -1',
 '#define GU_TARGET_SEGMENT 1',
 '#define GU_TARGET_SEGMENT (0x1U)',
 '#define GU_TARGET_SEGMENT 0xFFFFU',
 "#define GU_TARGET_SEGMENT 0x1U`n#define GU_TARGET_SEGMENT 0x1U",
 "#define GU_TARGET_SEGMENT 0x1U`n#undef GU_TARGET_SEGMENT",
 '#undef GU_TARGET_SEGMENT')) {
 Put 'internal-boot/unlock/profile-local.h' ($header+"`n"+$suffix+"`n")
 $accepted=$true;try{$null=Get-LocalProfile $sandbox}catch{$accepted=$false}
 if($accepted){throw 'Malformed or overridden segment header accepted'};$checks++
}
Put 'internal-boot/unlock/profile-local.h' $header
$legacyHw=Get-LocalProfile $sandbox
if(!$legacyHw.LegacySegment -or $legacyHw.Segment -ne 0){throw 'Legacy profile must default to segment zero'};$checks++
$report.Replay.Segment=0
Put 'research/profile/local/profile.json' ($report|ConvertTo-Json -Depth 8)
$null=Get-LocalProfileReport $sandbox $legacyHw;$checks++
$report.Replay.Segment=1
Put 'research/profile/local/profile.json' ($report|ConvertTo-Json -Depth 8)
$accepted=$true;try{$null=Get-LocalProfileReport $sandbox $legacyHw}catch{$accepted=$false}
if($accepted){throw 'Legacy segment-zero header accepted a nonzero report'};$checks++
$report.Replay.Remove('Segment')
$baseline=$normalBaseline
Put 'research/profile/local/profile.json' ($report|ConvertTo-Json -Depth 8)
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
foreach($json in @('"2"','2.0','2e0','true','null','2147483648')) {
 $text=$manifest|ConvertTo-Json -Depth 8
 $badManifest=([regex]::Replace($text,'"Format":\s*2\b',('"Format": '+$json)))|ConvertFrom-Json
 $accepted=$true;try{Assert-GuUsbManifest $sandbox $badManifest}catch{$accepted=$false}
 if($accepted){throw "Invalid USB Format token accepted: $json"};$checks++
}
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
foreach($name in @('ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE','ASUS-TUF-GAMING-B660M-E-D4-4003-RTX3070TI','GIGABYTE-X870-AORUS-ELITE-WIFI7-F13d-RTX3090TI','ASUS-PRIME-B660-PLUS-D4-3010-RTX3060','GIGABYTE-H610M-H-FC-RTX3060-12GB')) {
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
