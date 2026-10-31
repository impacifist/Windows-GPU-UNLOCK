# Collect an existing failed attempt and boot-entry comparison. No firmware/GPU writes.
[CmdletBinding()]
param([string]$StateDirectory='E:\UNLOCK\EVGA-PROFILE')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-firmware-lib.ps1')
. (Join-Path $PSScriptRoot 'evga-package-lib.ps1')
function Get-EvgaFailureBytesHash([byte[]]$Bytes){
 $sha=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
}
$state=(Resolve-Path -LiteralPath $StateDirectory).Path
$journals=@(Get-ChildItem -LiteralPath $state -File -Filter 'attempt-*.json' | Sort-Object Name)
if(!$journals.Count){throw 'No existing attempt journal; nothing to inspect'}
$attempt=Get-Content -LiteralPath $journals[-1].FullName -Raw | ConvertFrom-Json
if($attempt.Schema -cne 'evga-attended-attempt-v1' -or $attempt.Run -cnotmatch '^EVP[0-9]{2}$'){throw 'Invalid attempt journal'}
$existing=Join-Path $state ($attempt.Run+'-results.zip')
if(!(Test-Path -LiteralPath $existing -PathType Leaf)){throw 'Collect first; expected existing results ZIP'}
if(-not ('SharedEspFirmware.Native' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'shared-esp-firmware-native.cs')}
$scope=[SharedEspFirmware.Native]::EnablePrivilege()
try {
 $read={param($name) $v=[SharedEspFirmware.Native]::Read($name);if($v){ConvertTo-FirmwareValue $v.Data $v.Attributes}}
 $now=Get-EvgaFirmwareSnapshot $read
 $entries=@(foreach($entry in $now.BootEntries){
  $prior=@($attempt.BootEntries | Where-Object {$_.Name -ceq $entry.Name})
  [ordered]@{name=$entry.Name;description=(Get-EvgaOptionDescription $entry.Value);attributes=$entry.Value.Attributes;sha256=(Get-EvgaFailureBytesHash ([Convert]::FromBase64String($entry.Value.DataBase64)));previously_in_order=($prior.Count -eq 1);unchanged=($prior.Count -eq 1 -and (Test-FirmwareValueEqual $prior[0].Value $entry.Value));owned_temporary=($entry.Name -ceq $attempt.OptionName)}
 })
 $out=Join-Path $state ($attempt.Run+'-inspection-'+[guid]::NewGuid().ToString('N'))
 [void][IO.Directory]::CreateDirectory($out)
 Copy-Item -LiteralPath $existing -Destination (Join-Path $out 'results.zip')
 foreach($name in @('package.json','source-manifest.json','pe-verification.json','platform-config.h','profile.json')){
  $path=Join-Path $attempt.PackageDirectory $name
  if(Test-Path -LiteralPath $path -PathType Leaf){Copy-Item -LiteralPath $path -Destination (Join-Path $out $name)}
 }
 $resultPath=Join-Path $state ('result-{0:D2}.json' -f [int]$attempt.AttemptSlot)
 if(Test-Path -LiteralPath $resultPath){Copy-Item -LiteralPath $resultPath -Destination (Join-Path $out 'result.json')}
 $next=& $read 'BootNext';$current=& $read 'BootCurrent'
 [ordered]@{schema='evga-package-failure-inspection-v1';run=$attempt.Run;previous_order=@($attempt.BootEntries | ForEach-Object {$_.Name});current_entries=$entries;boot_next_present=($null -ne $next);boot_current=$(if($current){'Boot{0:X4}' -f [BitConverter]::ToUInt16([Convert]::FromBase64String($current.DataBase64),0)}else{$null});owned_option=$attempt.OptionName;results_sha256=(Get-EvgaPackageHash $existing);firmware_written=$false;gpu_accessed=$false} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $out 'inspection.json') -Encoding utf8
 $zip=$out+'.zip';Compress-Archive -LiteralPath $out -DestinationPath $zip
 [ordered]@{zip=$zip;run=$attempt.Run;firmware_written=$false;gpu_accessed=$false;next='Send this ZIP. Do not reboot, rearm or delete existing state for this test.'} | ConvertTo-Json
}finally{if($scope){$scope.Dispose()}}

