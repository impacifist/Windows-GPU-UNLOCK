# GPL-2.0-only. Local machine profile (-DGU_PROFILE_LOCAL) identity and its write-build evidence gate.
# Dot-sourced by build.ps1, research/headless/build-usb.ps1, stage-usb.ps1 and research/profile/test-profile.ps1.
# A generated profile-local.h is never write-eligible on its own: a write build needs
# research/profile/local/test-results.json from a passing test-profile.ps1 run against exactly
# this header, this machine's fixtures and this source tree.

function Get-GuSha256([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}

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
 return [ordered]@{
  Define='-DGU_PROFILE_LOCAL';Name=(Value 'GU_PROFILE_NAME' '"(LOCAL-[0-9a-f]{8}-[0-9a-f]{8}-[0-9a-f]{8})"')
  TargetPci=(Value 'GU_BOARD_ID' '0x([0-9a-f]{8})U');Subsystem=(Value 'GU_BOARD_SUBSYSTEM' '0x([0-9a-f]{8})U')
  Chip=('0x'+(Value 'GU_CHIP_ID' '0x([0-9a-f]{8})U'));Prom=(Value 'GU_PROM_SHA256' '"([0-9a-f]{64})"')
  Memory=('0x'+(Value 'GU_DEVICE_MEMORY_SIZE' '0x([0-9a-f]+)U'));HeadlessOnly=$true;UsbOnly=$true
  HeaderSha256=(Get-GuSha256 $header)
 }
}

# Everything the host tests compile or that decides what they check. Local build inputs written
# by the build scripts themselves (platform-config.h, generated.h, build/) are excluded.
function Get-LocalRequiredFiles {
 @('internal-boot/efi_runtime.c','internal-boot/unlock/local-profile.ps1',
   'internal-boot/unlock/build.ps1','internal-boot/boot/handoff.c',
   'research/headless/build-usb.ps1','research/headless/stage-usb.ps1',
   'research/profile/make-profile.py','research/profile/test-profile.ps1',
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
function Get-LocalExpectedJobs {
 [ordered]@{'generator refusals'=0;'stock-selection'=0;'checks (real config)'=0;
 'checks other profile (must reject)'=1;'port'=0;'write-lock'=0;'master-attributes'=0;'runtime'=0;
 'profile-lock-entryLatchedSs1Full'=0;'profile-lock-entryLatchedSs1Preserve'=0;
 'r2 console replay (real graph)'=0;'r2 replay other profile (must reject)'=1;
 'cold pre-check (read-only)'=0;'full flow gates'=0;'core transaction (efi)'=0;
 'manual entry-latched'=0;'automatic internal'=0;'headless internal'=0;'headless USB (shipped child)'=0}
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
 if($report.Format -isnot [int] -or $report.Format -ne 1 -or
    $report.ProfileName -cne $Hardware.Name -or $report.HeaderSha256 -cne $Hardware.HeaderSha256 -or
    $report.PromSha256 -cne $Hardware.Prom -or $report.HardwareTested -isnot [bool] -or $report.HardwareTested){throw 'Generated profile report identity is invalid'}
 Assert-LocalFixtures $Repo $report.Fixtures
 return $report
}

function Assert-LocalProfileEvidence([string]$Repo) {
 $hw=Get-LocalProfile $Repo
 $local=Join-Path $Repo 'research/profile/local'
 $path=Join-Path $local 'test-results.json'
 if(!(Test-Path -LiteralPath $path)){throw "Local profile $($hw.Name) has no passing host-test evidence; run research/profile/test-profile.ps1"}
 $e=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
 if($e.Format -isnot [int] -or $e.Format -ne 2 -or $e.AllPassed -isnot [bool] -or !$e.AllPassed -or
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
 $jobs=Get-LocalExpectedJobs
 Assert-LocalKeys $e.Results @($jobs.Keys) 'Host jobs'
 foreach($name in $jobs.Keys){
  $job=$e.Results.$name
  if($job.Exit -isnot [int] -or $job.Expected -isnot [int] -or $job.Exit -ne $jobs[$name] -or $job.Expected -ne $jobs[$name]){throw "Missing or invalid host result: $name"}
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
 if($Manifest.Format -isnot [int] -or $Manifest.Format -ne 2 -or
    $Manifest.Mode -cne 'single-rtx-headless-unlock-then-windows-usb' -or
    $Manifest.CanUnlock -isnot [bool] -or !$Manifest.CanUnlock -or
    $Manifest.RequiresHumanKey -isnot [bool] -or $Manifest.RequiresHumanKey -or
    $Manifest.HardwareTested -isnot [bool] -or $Manifest.HardwareTested){throw 'Invalid headless USB manifest format/mode/flags; rebuild the package'}
 foreach($key in @('EfiSha256','ChildSha256')){
  if($Manifest.$key -isnot [string] -or $Manifest.$key -cnotmatch '^[0-9a-f]{64}$'){throw "Invalid USB artifact hash: $key"}
 }
 $reviewed=@('ASUS-ROG-STRIX-Z690-I-4601-RTX3090TI','ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE','ASUS-TUF-GAMING-B660M-E-D4-4003-RTX3070TI','GIGABYTE-X870-AORUS-ELITE-WIFI7-F13d-RTX3090TI','ASUS-PRIME-B660-PLUS-D4-3010-RTX3060','GIGABYTE-H610M-H-FC-RTX3060-12GB')
 if($Manifest.HardwareProfile -isnot [string]){throw 'Missing USB hardware profile'}
 if($Manifest.HardwareProfile -cmatch '^LOCAL-[0-9a-f]{8}-[0-9a-f]{8}-[0-9a-f]{8}$') {
  Assert-LocalProfilePackage $Repo $Manifest
 } elseif($reviewed -ccontains $Manifest.HardwareProfile) {
  foreach($key in @('LocalProfileHeaderSha256','LocalProfileTreeSha256','LocalProfileEvidenceSha256')){
   if((Get-LocalObjectNames $Manifest) -ccontains $key){throw 'Local package metadata cannot select a reviewed profile'}
  }
 } else {throw 'Unknown USB hardware profile; rebuild the package'}
}
