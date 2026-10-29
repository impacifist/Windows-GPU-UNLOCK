# ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md.
# GPL-2.0-only. Experimental ASRock build gate; mock results are not hardware proof.
. (Join-Path $PSScriptRoot 'local-profile.ps1')
function Assert-AsrockEvidence([string]$Repo) {
 $out=Join-Path $Repo 'research/results/asrock-host-tests'
 $e=Get-Content (Join-Path $out 'test-results.json') -Raw | ConvertFrom-Json
 if($e.Format -ne 1 -or $e.AllPassed -isnot [bool] -or !$e.AllPassed -or $e.HardwareTested -isnot [bool] -or $e.HardwareTested){throw 'Invalid ASRock host evidence'}
 $jobs=@('console-replay','transaction','manual-app','automatic-app','headless-usb-app','checks','port','write-lock','master-attributes',
 'GU_ENTRY_LATCHED_SS1_FULL','GU_ENTRY_LATCHED_SS1_PRESERVE','release','cold-precheck','full-flow')
 Assert-LocalKeys $e.Results $jobs 'ASRock test jobs'
 foreach($job in $jobs) {
  if($e.Results.$job.Exit -ne 0 -or $e.Results.$job.Expected -ne 0 -or
     (Get-GuSha256 (Join-Path $out ($job+'.log'))) -cne $e.Results.$job.LogSha256.ToLowerInvariant()){throw "Missing or changed ASRock test: $job"}
 }
 Assert-LocalKeys $e.Fixtures @('DIAG00.TXT','PROM.BIN','IMG07.BIN','IMG47.BIN') 'ASRock fixtures'
 foreach($fixture in @('DIAG00.TXT','PROM.BIN','IMG07.BIN','IMG47.BIN')) {
  if((Get-GuSha256 (Join-Path $e.CapturePath $fixture)) -cne $e.Fixtures.$fixture.ToLowerInvariant()){throw 'ASRock fixture changed'}
 }
 if((Get-LocalTreeDigest $Repo) -cne $e.TreeSha256 -or
    (Get-GuSha256 (Join-Path $Repo 'internal-boot/unlock/asrock-b450m-3060.h')) -cne $e.ProfileSha256.ToLowerInvariant() -or
    (Get-GuSha256 (Join-Path $e.CapturePath 'PROM.BIN')) -cne $e.PromSha256.ToLowerInvariant()){throw 'ASRock sources or fixtures changed; rerun host tests'}
 return $e
}
