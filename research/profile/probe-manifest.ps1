# GPL-2.0-only. Source binding for the generic read-only diagnostic; no staging.
function Get-GuReadonlyCaptureFiles {
 @('research/pci-decode/efi-capture.c','research/pci-decode/efi-capture.h',
   'research/pci-decode/efi-binding.h','research/pci-decode/efi-path.h',
   'research/pci-decode/snapshot.c','research/pci-decode/snapshot.h')
}
function Get-GuGenericProbeFiles {
 @('internal-boot/boot/handoff.c','internal-boot/boot/diagnostic.c','internal-boot/efi_runtime.c',
   'internal-boot/boot/probe.c','internal-boot/boot/probe-parent.c','internal-boot/unlock/sha256.c',
   'internal-boot/boot/diagnostic.h','internal-boot/boot/media.h','internal-boot/boot/probe.h',
   'internal-boot/boot/probe-parent.h','internal-boot/boot/probe-pin.h',
   'internal-boot/unlock/machine-profile.h','internal-boot/unlock/gpu-family.h',
   'internal-boot/unlock/firmware-profile.h','internal-boot/unlock/gpu-path.h',
   'research/profile/build-probe.ps1','research/profile/probe-manifest.ps1',
   'research/profile/new-machine.ps1','internal-boot/unlock/local-profile.ps1')+@(Get-GuReadonlyCaptureFiles)
}
function Assert-GuGenericProbeManifest([string]$Repo,$Manifest,[string]$Efi) {
 foreach($name in @('CanUnlock','CanDetach','WritesFirmwareVariables','HardwareTested')) {
  if($Manifest.$name -isnot [bool] -or $Manifest.$name){throw 'Generic probe manifest has invalid diagnostic flags'}
 }
 if($Manifest.Mode -isnot [string] -or $Manifest.Mode -cne 'generic-read-only-probe-then-windows-usb'){throw 'Generic probe manifest mode is stale'}
 $defines=@('-DGU_DIAGNOSTIC_ONLY','-DGU_DIAGNOSTIC_PROBE','-DGU_PROBE_GENERIC','-DGU_USB_PROBE','-DGU_PROBE_PARENT_BASELINE')
 if($Manifest.ProbeBusMode -is [string] -and $Manifest.ProbeBusMode -ceq 'auto' -and $null -eq $Manifest.TargetBus){$defines+='-DGU_PROBE_AUTO_BUS'}
 elseif($Manifest.ProbeBusMode -is [string] -and $Manifest.ProbeBusMode -ceq 'explicit' -and
        ($Manifest.TargetBus -is [int] -or $Manifest.TargetBus -is [long]) -and $Manifest.TargetBus -ge 1 -and $Manifest.TargetBus -le 255){
  $defines+=('-DGU_PROBE_BUS=0x{0:x}U' -f [int]$Manifest.TargetBus)
 }else{throw 'Generic probe bus metadata is invalid'}
 if(@($Manifest.Defines).Count -ne $defines.Count){throw 'Generic probe define set is incomplete'}
 foreach($define in $defines){if(@($Manifest.Defines | Where-Object {$_ -is [string] -and $_ -ceq $define}).Count -ne 1){throw 'Generic probe define set is invalid'}}
 $expected=@(Get-GuGenericProbeFiles);$rows=@($Manifest.Sources)
 if($rows.Count -ne $expected.Count){throw 'Generic probe source manifest is incomplete'}
 foreach($relative in $expected) {
  $row=@($rows | Where-Object { $_.Path -is [string] -and $_.Path -ceq $relative })
  if($row.Count -ne 1 -or $row[0].Sha256 -isnot [string] -or $row[0].Sha256 -cnotmatch '^[0-9a-f]{64}$' -or
     (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $Repo $relative)).Hash.ToLowerInvariant() -cne $row[0].Sha256){throw 'Generic probe source changed or manifest is invalid'}
 }
 if($Manifest.EfiSha256 -isnot [string] -or $Manifest.EfiSha256 -cnotmatch '^[0-9a-f]{64}$' -or
    (Get-FileHash -Algorithm SHA256 -LiteralPath $Efi).Hash.ToLowerInvariant() -cne $Manifest.EfiSha256){throw 'Generic probe EFI changed'}
}
