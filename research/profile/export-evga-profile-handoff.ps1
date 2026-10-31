# Final acceptance receipt for the reviewed EVP12 sequence. No staging, GPU or firmware writes.
param([string]$StateDirectory='E:\UNLOCK\EVGA-PROFILE')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'evga-package-lib.ps1')
$config=Get-Content -LiteralPath (Join-Path $StateDirectory 'installation.json') -Raw | ConvertFrom-Json
if($config.Schema -cne 'evga-attended-install-v1' -or $config.Layout -cne 'SharedEsp'){throw 'Expected reviewed shared-ESP installation'}
if((Get-EvgaProfileNextSlot $StateDirectory $config) -ne 13){throw 'Expected completed EVP12 and no subsequent attempt'}
$attempt=Get-Content -LiteralPath (Join-Path $StateDirectory 'attempt-12.json') -Raw | ConvertFrom-Json
$result=Get-Content -LiteralPath (Join-Path $StateDirectory 'result-12.json') -Raw | ConvertFrom-Json
if(!$result.Success -or !$result.CleanupCompleted -or $result.LogSha256 -cne 'ba15d7ef5ffd98ff195bab474f00d541d3b4f492d21831c4ee7e242c078f9db1' -or $result.GateSha256 -cne '3e7d5d8f12e408f37c557abecba929dec9b8680c7c1eda4c6058a7a6891e0e40'){throw 'Expected exact reviewed EVP12 success/cleanup'}
$normal=Get-Content -LiteralPath (Join-Path $StateDirectory 'normal-boot-11-result.json') -Raw | ConvertFrom-Json
if($normal.schema -cne 'evga-profile-normal-boot-result-v1' -or $normal.slot -ne 11 -or !$normal.normal_restart_confirmed -or !$normal.temporary_option_absent -or !$normal.boot_next_clear -or !$normal.primary_entries_unchanged){throw 'EVP11 ordinary boot evidence missing'}
$status=& (Join-Path $PSScriptRoot 'manage-evga-profile.ps1') -Action Status -StateDirectory $StateDirectory | ConvertFrom-Json
if($status.slot -ne 12 -or $status.option_present -or $null -ne $status.boot_next){throw 'Temporary option or BootNext still present'}
$out=Join-Path $StateDirectory ('profile-handoff-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($out)
$summary=[ordered]@{schema='evga-profile-acceptance-handoff-v1';profile='evga-z790-3090-shared-esp';latest_slot=12;transaction_success=$true;cleanup_completed=$true;default_rearm_demonstrated=$true;log_sha256=$result.LogSha256;gate_sha256=$result.GateSha256;gpu_marker_sha256=$result.GpuMarkerSha256;ordinary_boot_confirmed_after_slot=11;temporary_option_absent=$true;boot_next_clear=$true;source_package_sha256=$attempt.PackageSha256;permanent_unlock_proven=$false;unattended_unlock_proven=$false;general_hardware_compatibility_proven=$false;firmware_written=$false;gpu_accessed=$false}
Write-EvgaPackageJson (Join-Path $out 'acceptance.json') $summary
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'profiles/evga-z790-3090-shared-esp.json') -Destination (Join-Path $out 'profile.json')
foreach($name in @('REUSABLE-PROFILE.en.md','REUSABLE-PROFILE.ko.md','EVP12-RESULT.en.md','EVP12-RESULT.ko.md','evp11-benchmark.json')){
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('../../evidence/evga-3090-capture-review/'+$name)) -Destination (Join-Path $out $name)
}
$zip=$out+'.zip';Compress-Archive -LiteralPath $out -DestinationPath $zip -ErrorAction Stop
[ordered]@{acceptance_zip=$zip;profile='evga-z790-3090-shared-esp';latest_slot=12;firmware_written=$false;gpu_accessed=$false;next='Send this ZIP and confirm full shutdown/USB absence for EVP12. No further EFI or reboot requested; maintainer review remains pending.'} | ConvertTo-Json
