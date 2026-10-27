# GPL-2.0-only. Local stale-input and capability guards for observation packages.
# These hashes are not signatures and do not authenticate an untrusted host.
. (Join-Path $PSScriptRoot '../../internal-boot/unlock/local-profile.ps1')

function Get-ObservationState([string]$Repo) {
 $inputs=@(Get-LocalTreeInputs $Repo)
 $config=Join-Path $Repo 'internal-boot/boot/platform-config.h'
 if(!(Test-Path -LiteralPath $config -PathType Leaf) -or
    ((Get-Item -LiteralPath $config -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Missing or indirect observation platform configuration'}
 [pscustomobject]@{Inputs=$inputs;TreeSha256=(Get-LocalInputDigest $inputs);PlatformConfigSha256=(Get-GuSha256 $config)}
}

function Assert-ObservationManifest([string]$Repo,$Manifest,[string]$Mode,[string]$Efi) {
 if(($Manifest.Format -isnot [int] -and $Manifest.Format -isnot [long]) -or $Manifest.Format -ne 1 -or
    $Mode -cnotin @('graph-capture-then-normal-windows','usb-observation-only') -or $Manifest.Mode -cne $Mode){throw 'Invalid observation manifest format or mode'}
 foreach($name in @('CanUnlock','CanDetach','WritesFirmwareVariables','HardwareTested')) {
  if($Manifest.$name -isnot [bool] -or $Manifest.$name){throw "Invalid observation capability: $name"}
 }
 if($Manifest.UsbMedia -isnot [bool] -or !$Manifest.UsbMedia -or $Manifest.DumpsProm -isnot [bool]){throw 'Invalid observation USB/PROM flags'}
 $profiles=@{'b650m-3090fe'=@();'b660m-3070ti'=@('-DGU_PROFILE_B660M_3070TI');'x870-3090ti'=@('-DGU_PROFILE_X870_3090TI');
  'b660plus-3060'=@('-DGU_PROFILE_B660PLUS_3060');'h610m-3060'=@('-DGU_PROFILE_H610M_3060');'probe-h610m-3060'=@('-DGU_PROFILE_PROBE_H610M_3060');'probe-b550-3060'=@('-DGU_PROFILE_PROBE_B550_3060')}
 if($Manifest.HardwareProfile -isnot [string] -or @($profiles.Keys) -cnotcontains $Manifest.HardwareProfile){throw 'Unknown observation hardware profile'}
 $defines=@($profiles[$Manifest.HardwareProfile])+@('-DGU_USB_MEDIA')
 if($Manifest.DumpsProm){$defines+='-DGU_PROBE_DUMP_PROM'}
 if(@($Manifest.Defines).Count -ne $defines.Count){throw 'Invalid observation build defines'}
 for($i=0;$i -lt $defines.Count;$i++){if($Manifest.Defines[$i] -cne $defines[$i]){throw 'Observation build defines changed'}}
 foreach($name in @('EfiSha256','TreeSha256','PlatformConfigSha256')) {
  if($Manifest.$name -isnot [string] -or $Manifest.$name -cnotmatch '^[0-9a-f]{64}$'){throw "Invalid observation hash: $name"}
 }
 if(!(Test-Path -LiteralPath $Efi -PathType Leaf) -or (Get-GuSha256 $Efi) -cne $Manifest.EfiSha256){throw 'Observation EFI does not match its manifest'}
 $state=Get-ObservationState $Repo
 if($Manifest.TreeSha256 -cne $state.TreeSha256 -or $Manifest.PlatformConfigSha256 -cne $state.PlatformConfigSha256 -or
    @($Manifest.Inputs).Count -ne $state.Inputs.Count){throw 'Observation build inputs changed; rebuild the package'}
 for($i=0;$i -lt $state.Inputs.Count;$i++) {
  if($Manifest.Inputs[$i].Path -cne $state.Inputs[$i].Path -or $Manifest.Inputs[$i].Sha256 -cne $state.Inputs[$i].Sha256){throw 'Observation input manifest changed'}
 }
}

function Assert-ObservationCaptureEmpty([string]$Directory) {
 if(Test-Path -LiteralPath $Directory) {
  if((Get-Item -LiteralPath $Directory -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Indirect observation capture directory'}
  $captures=@(Get-ChildItem -LiteralPath $Directory -File | Where-Object {$_.Name -match '^(DIAG\d\d\.TXT|IMG\d\d\.BIN|PROM\.BIN)$'})
  if($captures.Count){throw 'Existing observation captures found. Archive the complete capture set before staging or booting again; nothing was removed'}
 }
}
