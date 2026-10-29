# ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md.
# GPL-2.0-only. Local machine profile (-DGU_PROFILE_LOCAL) identity and its write-build evidence gate.
# Dot-sourced by build.ps1, research/headless/build-usb.ps1, stage-usb.ps1 and research/profile/test-profile.ps1.
# A generated profile-local.h is never write-eligible on its own: a write build needs
# research/profile/local/test-results.json from a passing test-profile.ps1 run against exactly
# this header, this machine's fixtures and this source tree.

function Get-GuSha256([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}

# ConvertFrom-Json emits Int32 in Windows PowerShell and Int64 in PowerShell 7.
# Check the original type before comparing: no string/float/bool coercion at a gate.
function Test-GuExactInteger($Value,[int]$Expected) {
 return ($Value -is [int] -or $Value -is [long]) -and $Value -eq $Expected
}

# A resumed workflow must honor the same explicit bus constraint as its probe.
# Check before any local capture archive, profile generation or package/staging.
function Assert-GuProbeTargetBus([string[]]$Paths,[ValidateRange(1,255)][int]$TargetBus) {
 foreach($path in $Paths) {
  $item=Get-Item -LiteralPath $path
  if($item.PSIsContainer -or $item.Length -gt 8MB){throw 'Probe capture exceeds the diagnostic file limit'}
  $text=[IO.File]::ReadAllText($item.FullName)
  $rows=[regex]::Matches($text,'(?m)^probe-target[^\r\n]*\r?$')
  $target=if($rows.Count -eq 1){[regex]::Match($rows[0].Value.TrimEnd("`r"),'^probe-target seg=0x([0-9a-f]{1,4}) bus=0x([0-9a-f]{1,4}) dev=0x([0-9a-f]{1,4}) fun=0x([0-9a-f]{1,4})$')}else{$null}
  if(!$target -or !$target.Success -or
     [Convert]::ToUInt32($target.Groups[2].Value,16) -ne $TargetBus -or
     [Convert]::ToUInt32($target.Groups[3].Value,16) -ne 0 -or
     [Convert]::ToUInt32($target.Groups[4].Value,16) -ne 0){throw 'Captured probe target does not match explicit -TargetBus; no profile or package was generated'}
 }
}

function Get-LocalProfile([string]$Repo) {
 $header=Join-Path $Repo 'internal-boot/unlock/profile-local.h'
 if(!(Test-Path -LiteralPath $header)){throw 'No local profile: run research/profile/make-profile.py on this machine''s probe captures first'}
 $text=[IO.File]::ReadAllText($header)
 if($text -notmatch '(?m)^#define GU_LOCAL_PROFILE_GENERATOR [0-9]+$'){throw 'profile-local.h was not produced by research/profile/make-profile.py'}
 function Value([string]$Name,[string]$Pattern){
  $m=[regex]::Match($text,"(?m)^#define $Name $Pattern$")
  if(!$m.Success){throw "profile-local.h has no valid $Name"}
  return $m.Groups[1].Value
 }
 $edidRows=@([regex]::Matches($text,'(?m)^[ \t]*#[ \t]*define[ \t]+GU_NO_EDID_DRIVER\b[^\r\n]*'))
 if($edidRows.Count -ne 1 -or $edidRows[0].Value -cnotmatch '^#define GU_NO_EDID_DRIVER [01]$' -or
    $text -match '(?m)^[ \t]*#[ \t]*undef[ \t]+GU_NO_EDID_DRIVER\b'){
  throw 'profile-local.h requires one exact GU_NO_EDID_DRIVER 0/1; regenerate the local profile'
 }
 $noEdidDriver=$edidRows[0].Value.EndsWith(' 1')
 $segmentRows=@([regex]::Matches($text,'(?m)^[ \t]*#[ \t]*define[ \t]+GU_TARGET_SEGMENT\b[^\r\n]*'))
 $legacySegment=($segmentRows.Count -eq 0)
 $segment=0
 if($text -match '(?m)^[ \t]*#[ \t]*undef[ \t]+GU_TARGET_SEGMENT\b' -or
    (!$legacySegment -and ($segmentRows.Count -ne 1 -or $segmentRows[0].Value -cnotmatch '^#define GU_TARGET_SEGMENT 0x[0-9a-f]{1,4}U$'))){
  throw 'Local target segment requires one exact generated 16-bit hexadecimal value without overrides'
 }
 if(!$legacySegment){$segment=[Convert]::ToInt32(($segmentRows[0].Value -replace '^#define GU_TARGET_SEGMENT 0x','' -replace 'U$',''),16)}
 $aggregateRows=@([regex]::Matches($text,'(?m)^[ \t]*#[ \t]*define[ \t]+GU_LOCAL_CONSOLE_AGGREGATE\b[^\r\n]*'))
 $childRows=@([regex]::Matches($text,'(?m)^[ \t]*#[ \t]*define[ \t]+GU_LOCAL_CHILD_COUNT\b[^\r\n]*'))
 if($text -match '(?m)^[ \t]*#[ \t]*undef[ \t]+GU_LOCAL_(CONSOLE_AGGREGATE|CHILD_COUNT)\b' -or
    $text -match '(?m)^[ \t]*#[ \t]*(define|undef)[ \t]+GU_CONSOLE_(AGGREGATE|MULTI_CHILD|CHILD_COUNT)\b'){
  throw 'Local console topology must use generated local metadata without overrides'
 }
 $legacyTopology=($aggregateRows.Count -eq 0 -and $childRows.Count -eq 0)
 $consoleAggregate=$false;$childCount=0
 if(!$legacyTopology) {
  if($aggregateRows.Count -ne 1 -or $aggregateRows[0].Value -cnotmatch '^#define GU_LOCAL_CONSOLE_AGGREGATE [01]$' -or
     $childRows.Count -ne 1 -or $childRows[0].Value -cnotmatch '^#define GU_LOCAL_CHILD_COUNT [12]$'){
   throw 'Local topology requires one exact aggregate 0/1 and child count 1/2'
  }
  $consoleAggregate=$aggregateRows[0].Value.EndsWith(' 1');$childCount=[int]$childRows[0].Value.Substring($childRows[0].Value.Length-1)
 }
 if($consoleAggregate -and $noEdidDriver){throw 'Aggregate local profiles require an EDID driver'}
 return [ordered]@{
  Define='-DGU_PROFILE_LOCAL';Name=(Value 'GU_PROFILE_NAME' '"(LOCAL-[0-9a-f]{8}-[0-9a-f]{8}-[0-9a-f]{8})"')
  TargetPci=(Value 'GU_BOARD_ID' '0x([0-9a-f]{8})U');Subsystem=(Value 'GU_BOARD_SUBSYSTEM' '0x([0-9a-f]{8})U')
  Chip=('0x'+(Value 'GU_CHIP_ID' '0x([0-9a-f]{8})U'));Prom=(Value 'GU_PROM_SHA256' '"([0-9a-f]{64})"')
  Memory=('0x'+(Value 'GU_DEVICE_MEMORY_SIZE' '0x([0-9a-f]+)U'));HeadlessOnly=$true;UsbOnly=$true
  HeaderSha256=(Get-GuSha256 $header)
  NoEdidDriver=$noEdidDriver
  Segment=$segment;LegacySegment=$legacySegment
  ConsoleAggregate=$consoleAggregate;ChildCount=$childCount;LegacyTopology=$legacyTopology
 }
}

# Everything the host tests compile or that decides what they check. Local build inputs written
# by the build scripts themselves (platform-config.h, generated.h, build/) are excluded.
function Get-LocalRequiredFiles {
 @('internal-boot/efi_runtime.c','internal-boot/unlock/local-profile.ps1',
   'internal-boot/unlock/gpu-family.h','internal-boot/unlock/firmware-profile.h',
   'internal-boot/unlock/gpu-path.h','research/profile/gpu_path.py',
   'internal-boot/unlock/tests/test_gpu_paths.c','internal-boot/unlock/tests/test_gpu_path_vectors.c',
   'research/profile/test_gpu_path.py','research/profile/test_gpu_path_contract.py',
   'internal-boot/unlock/build.ps1','internal-boot/boot/handoff.c',
   'research/headless/build-usb.ps1','research/headless/stage-usb.ps1',
   'research/profile/make-profile.py','research/profile/capture_graph.py','research/profile/test-profile.ps1',
   'internal-boot/boot/make-console-fixture.py','internal-boot/boot/test_aggregate_full.c',
   'internal-boot/boot/test_aggregate_prepare.c','internal-boot/boot/test_research_input.c',
   'research/profile/host.ps1','research/profile/proven.json')
}
function Get-LocalTreeInputs([string]$Repo) {
 $Repo=[IO.Path]::GetFullPath($Repo).TrimEnd('\','/')
 foreach($required in (Get-LocalRequiredFiles)) {
  if(!(Test-Path -LiteralPath (Join-Path $Repo $required) -PathType Leaf)){throw "Missing required input: $required"}
 }
 $files=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
 $roots=@('internal-boot/unlock','internal-boot/boot','research/profile','research/headless','deps/edk2/MdePkg/Include')
 foreach($entry in $roots) {
  $base=Join-Path $Repo $entry
  if(!(Test-Path -LiteralPath $base -PathType Container)){throw "Missing input root: $entry"}
  $found=0;$pending=[Collections.Generic.Stack[string]]::new();$pending.Push($base)
  while($pending.Count) {
   foreach($f in (Get-ChildItem -LiteralPath ($pending.Pop()) -Force)) {
    $rel=$f.FullName.Substring($Repo.Length+1).Replace('\','/')
    if($f.PSIsContainer -and ($f.Name -eq 'build' -or $rel -eq 'research/profile/local')){continue}
    if($f.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Untracked indirect input: $rel"}
    if($f.PSIsContainer){$pending.Push($f.FullName);continue}
    if($f.Extension -notmatch '^\.(c|h|py|json|ps1)$'){continue}
    if($rel -in @('internal-boot/unlock/profile-local.h','internal-boot/boot/platform-config.h','internal-boot/boot/generated.h')){continue}
    $files[$rel]=Get-GuSha256 $f.FullName;$found++
   }
  }
  if(!$found){throw "Empty input root: $entry"}
 }
 $files['internal-boot/efi_runtime.c']=Get-GuSha256 (Join-Path $Repo 'internal-boot/efi_runtime.c')
 [string[]]$paths=@($files.Keys);[Array]::Sort($paths,[StringComparer]::Ordinal)
 foreach($path in $paths){[pscustomobject][ordered]@{Path=$path;Sha256=$files[$path]}}
}
function Get-LocalInputDigest($Inputs) {
 $rows=@($Inputs | ForEach-Object {"$($_.Path) $($_.Sha256)"})
 $bytes=[Text.Encoding]::UTF8.GetBytes(($rows -join "`n")+"`n")
 $sha=[Security.Cryptography.SHA256]::Create()
 try{return (($sha.ComputeHash($bytes) | ForEach-Object {$_.ToString('x2')}) -join '')}finally{$sha.Dispose()}
}
function Get-LocalTreeDigest([string]$Repo) {Get-LocalInputDigest @(Get-LocalTreeInputs $Repo)}
function Get-LocalExpectedJobs([bool]$NoEdidDriver=$false,[bool]$ConsoleAggregate=$false) {
 if($NoEdidDriver -and $ConsoleAggregate){throw 'Aggregate replay requires an EDID driver'}
 $jobs=[ordered]@{'generator refusals'=0;'stock-selection'=0;'checks (real config)'=0;
 'checks other profile (must reject)'=1;'port'=0;'write-lock'=0;'master-attributes'=0;'runtime'=0;
 'profile-lock-entryLatchedSs1Full'=0;'profile-lock-entryLatchedSs1Preserve'=0;
 'r2 console replay (real graph)'=0;'r2 replay other profile (must reject)'=1;
 'cold pre-check (read-only)'=0;'full flow gates'=0;'research input policy'=0;'core transaction (efi)'=0;
 'manual entry-latched'=0;'automatic internal'=0;'headless internal'=0;'headless USB (shipped child)'=0}
 # Every generated profile must replay the complete release on its own graph/images.
 if($ConsoleAggregate){$jobs['release replay, aggregate console (real graph)']=0}
 elseif($NoEdidDriver){$jobs['release replay, no EDID driver (real graph)']=0}
 else {$jobs['release replay, EDID driver (real graph)']=0}
 return $jobs
}
function Get-LocalObjectNames($Value) {
 if($null -eq $Value){return @()}
 if($Value -is [Collections.IDictionary]){return @($Value.Keys)}
 return @($Value.PSObject.Properties.Name)
}
function Assert-LocalKeys($Value,[string[]]$Expected,[string]$Label) {
 $actual=@(Get-LocalObjectNames $Value)
 if($actual.Count -ne $Expected.Count){throw "$Label has an incomplete or extra key set"}
 foreach($name in $Expected){if(!($actual -ccontains $name)){throw "$Label missing exact key: $name"}}
}
function Assert-LocalFixtures([string]$Repo,$Fixtures) {
 $names=@('PROM.BIN','probe/REPORT.TXT','cold/REPORT.TXT','console/DIAG.TXT')
 Assert-LocalKeys $Fixtures $names 'Fixtures'
 foreach($name in $names) {
  $digest=$Fixtures.$name
  $path=Join-Path $Repo ('research/profile/local/fixtures/'+$name)
  if($digest -isnot [string] -or $digest -cnotmatch '^[0-9a-f]{64}$' -or !(Test-Path -LiteralPath $path -PathType Leaf) -or (Get-GuSha256 $path) -cne $digest){throw "Fixture changed or invalid: $name"}
 }
}
function Get-LocalProfileReport([string]$Repo,$Hardware) {
 $report=Get-Content -LiteralPath (Join-Path $Repo 'research/profile/local/profile.json') -Raw | ConvertFrom-Json
 if(!(Test-GuExactInteger $report.Format 1) -or
    $report.ProfileName -cne $Hardware.Name -or $report.HeaderSha256 -cne $Hardware.HeaderSha256 -or
    $report.PromSha256 -cne $Hardware.Prom -or $report.HardwareTested -isnot [bool] -or $report.HardwareTested){throw 'Generated profile report identity is invalid'}
 Assert-LocalFixtures $Repo $report.Fixtures
 if($null -eq $report.Replay -or $report.Replay.NoEdidDriver -isnot [bool] -or $report.Replay.NoEdidDriver -ne $Hardware.NoEdidDriver){
  throw 'Generated profile report NoEdidDriver must be boolean and match the header'
 }
 $topologyKeys=@($report.Replay.PSObject.Properties.Name)
 if(!$Hardware.LegacySegment -or $topologyKeys -contains 'Segment') {
  if(!(Test-GuExactInteger $report.Replay.Segment $Hardware.Segment)){
   throw 'Generated profile report segment must be an integer and match the header exactly'
  }
 }
 if(!$Hardware.LegacyTopology -or $topologyKeys -contains 'Aggregate' -or $topologyKeys -contains 'ChildCount') {
  if($report.Replay.Aggregate -isnot [bool] -or $report.Replay.Aggregate -ne $Hardware.ConsoleAggregate -or
     !(Test-GuExactInteger $report.Replay.ChildCount $Hardware.ChildCount)){
   throw 'Generated profile report topology must match the header exactly'
  }
 }
 return $report
}

function Assert-LocalProfileEvidence([string]$Repo) {
 $hw=Get-LocalProfile $Repo
 $local=Join-Path $Repo 'research/profile/local'
 $path=Join-Path $local 'test-results.json'
 if(!(Test-Path -LiteralPath $path)){throw "Local profile $($hw.Name) has no passing host-test evidence; run research/profile/test-profile.ps1"}
 $e=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
 if(!(Test-GuExactInteger $e.Format 2) -or $e.AllPassed -isnot [bool] -or !$e.AllPassed -or
    $e.HardwareAccess -isnot [bool] -or $e.HardwareAccess -or $e.ProfileName -cne $hw.Name){throw 'Local profile evidence format/type mismatch; rerun host tests'}
 if($e.HeaderSha256 -ne $hw.HeaderSha256){throw 'profile-local.h changed after its host tests passed; rerun research/profile/test-profile.ps1'}
 if($e.PromSha256 -ne $hw.Prom){throw 'Local test evidence is for another PROM'}
 $report=Get-LocalProfileReport $Repo $hw
 if($e.ProfileReportSha256 -cne (Get-GuSha256 (Join-Path $local 'profile.json'))){throw 'Generated profile report changed after tests'}
 Assert-LocalFixtures $Repo $e.Fixtures
 foreach($name in @('PROM.BIN','probe/REPORT.TXT','cold/REPORT.TXT','console/DIAG.TXT')){if($e.Fixtures.$name -cne $report.Fixtures.$name){throw 'Fixture report/evidence disagreement'}}
 if((Get-GuSha256 (Join-Path $local 'fixtures/PROM.BIN')) -ne $hw.Prom){throw 'Local PROM fixture does not match the profile PROM hash'}
 $inputs=@(Get-LocalTreeInputs $Repo)
 if(@($e.Inputs).Count -ne $inputs.Count){throw 'Build input list changed after tests'}
 for($i=0;$i -lt $inputs.Count;$i++){
  if($e.Inputs[$i].Path -cne $inputs[$i].Path -or $e.Inputs[$i].Sha256 -cne $inputs[$i].Sha256){throw 'Build input path/hash changed after tests'}
 }
 if($e.TreeSha256 -cne (Get-LocalInputDigest $inputs)){throw 'Build input digest changed after tests'}
 $jobs=Get-LocalExpectedJobs $hw.NoEdidDriver $hw.ConsoleAggregate
 Assert-LocalKeys $e.Results @($jobs.Keys) 'Host jobs'
 foreach($name in $jobs.Keys){
  $job=$e.Results.$name
  if(!(Test-GuExactInteger $job.Exit $jobs[$name]) -or !(Test-GuExactInteger $job.Expected $jobs[$name])){throw "Missing or invalid host result: $name"}
 }
 return $e
}

function Assert-LocalProfilePackage([string]$Repo,$Manifest) {
 $hw=Get-LocalProfile $Repo;$e=Assert-LocalProfileEvidence $Repo
 if($Manifest.HardwareProfile -cne $hw.Name -or $Manifest.LocalProfileHeaderSha256 -cne $hw.HeaderSha256 -or
    $Manifest.LocalProfileTreeSha256 -cne $e.TreeSha256 -or
    $Manifest.LocalProfileEvidenceSha256 -cne (Get-GuSha256 (Join-Path $Repo 'research/profile/local/test-results.json'))){throw 'USB package was built from another local profile, source tree or test run; rebuild it'}
}

function Assert-GuUsbManifest([string]$Repo,$Manifest) {
 if(!(Test-GuExactInteger $Manifest.Format 2) -or
    $Manifest.Mode -cne 'single-rtx-headless-unlock-then-windows-usb' -or
    $Manifest.CanUnlock -isnot [bool] -or !$Manifest.CanUnlock -or
    $Manifest.RequiresHumanKey -isnot [bool] -or $Manifest.RequiresHumanKey -or
    $Manifest.HardwareTested -isnot [bool] -or $Manifest.HardwareTested){throw 'Invalid headless USB manifest format/mode/flags; rebuild the package'}
 foreach($key in @('EfiSha256','ChildSha256')){
  if($Manifest.$key -isnot [string] -or $Manifest.$key -cnotmatch '^[0-9a-f]{64}$'){throw "Invalid USB artifact hash: $key"}
 }
 $reviewed=@('ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE','ASUS-TUF-GAMING-B660M-E-D4-4003-RTX3070TI','GIGABYTE-X870-AORUS-ELITE-WIFI7-F13d-RTX3090TI','ASUS-PRIME-B660-PLUS-D4-3010-RTX3060','GIGABYTE-H610M-H-FC-RTX3060-12GB')
 if($Manifest.HardwareProfile -isnot [string]){throw 'Missing USB hardware profile'}
 if($Manifest.HardwareProfile -cmatch '^LOCAL-[0-9a-f]{8}-[0-9a-f]{8}-[0-9a-f]{8}$') {
  Assert-LocalProfilePackage $Repo $Manifest
 } elseif($Manifest.HardwareProfile -ceq 'ASROCK-B450M-PRO4-F-P10.31-RTX3060-EXPERIMENTAL') {
  . (Join-Path $Repo 'internal-boot/unlock/asrock-evidence.ps1')
  $asrock=Assert-AsrockEvidence $Repo
  if($Manifest.AsrockTreeSha256 -cne $asrock.TreeSha256 -or
     $Manifest.AsrockEvidenceSha256 -cne (Get-GuSha256 (Join-Path $Repo 'research/results/asrock-host-tests/test-results.json'))){throw 'ASRock package is stale; rebuild it'}
 } elseif($reviewed -ccontains $Manifest.HardwareProfile) {
  foreach($key in @('LocalProfileHeaderSha256','LocalProfileTreeSha256','LocalProfileEvidenceSha256')){
   if((Get-LocalObjectNames $Manifest) -ccontains $key){throw 'Local package metadata cannot select a reviewed profile'}
  }
 } else {throw 'Unknown USB hardware profile; rebuild the package'}
}
