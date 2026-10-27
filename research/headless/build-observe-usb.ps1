# GPL-2.0-only. Build the read-only observation EFI for a FAT32 USB stick (-DGU_USB_MEDIA).
# Same GU_DIAGNOSTIC_ONLY image as build-observe.ps1: graph capture and the read-only
# probe, then Windows Boot Manager from the pinned Windows ESP. No internal ESP is
# needed, and nothing is installed, staged or registered here.
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[string]$Clang='clang',[string]$LldLink='lld-link',[string]$Python='python',
 [ValidateSet('b650m-3090fe','b660m-3070ti','x870-3090ti','b660plus-3060','h610m-3060','probe-h610m-3060','probe-b550-3060')][string]$HardwareProfile='probe-h610m-3060',[switch]$DumpProm)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$boot=Join-Path $repo 'internal-boot/boot'
$HardwareProfile=$HardwareProfile.ToLowerInvariant()
. (Join-Path $PSScriptRoot 'observe-manifest.ps1')
function GuidC([guid]$Value){
 $b=$Value.ToByteArray();$tail=($b[8..15] | ForEach-Object {'0x{0:x2}' -f $_}) -join ','
 return ('{{0x{0:x8},0x{1:x4},0x{2:x4},{{{3}}}}}' -f [BitConverter]::ToUInt32($b,0),[BitConverter]::ToUInt16($b,4),[BitConverter]::ToUInt16($b,6),$tail)
}
$windows=@(Get-Partition | Where-Object {$_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($windows.Count -ne 1 -or !$windows[0].IsSystem){throw 'WindowsEspGuid must identify the current system EFI partition'}
# Shares build-usb.ps1's marker: both are USB builds and never pin an internal partition.
$config=Join-Path $boot 'platform-config.h';$marker='/* Local USB configuration (build-usb.ps1). Internal partition unused under GU_USB_MEDIA. */'
if((Test-Path -LiteralPath $config) -and (Get-Content -LiteralPath $config -TotalCount 1) -ne $marker){throw 'platform-config.h belongs to an internal Configure.ps1 setup; move it aside before a USB build'}
@($marker,('#define GU_INTERNAL_PARTITION_INITIALIZER '+(GuidC ([guid]::Empty))),('#define GU_WINDOWS_PARTITION_INITIALIZER '+(GuidC $WindowsEspGuid))) | Set-Content -LiteralPath $config -Encoding ascii
# GU_DIAGNOSTIC_ONLY excludes generated.h and all child contracts entirely.
& (Join-Path $boot 'build-diagnostic.ps1') -Clang $Clang -LldLink $LldLink -Python $Python -HardwareProfile $HardwareProfile -UsbMedia -DumpProm:$DumpProm
if(!$?){throw 'Observation build failed'}
$built=Join-Path $boot 'build/diagnostic'
$m=Get-Content -LiteralPath (Join-Path $built 'manifest.json') -Raw | ConvertFrom-Json
Assert-ObservationManifest $repo $m 'graph-capture-then-normal-windows' (Join-Path $built 'DIAGNOSTIC.EFI')
if($m.HardwareProfile -cne $HardwareProfile -or $m.DumpsProm -ne [bool]$DumpProm){throw 'Unexpected observation build selection'}
$output=Join-Path $PSScriptRoot 'build/usb-observe'
[void][IO.Directory]::CreateDirectory($output)
$target=Join-Path $output 'OBSERVE-USB.EFI'
Copy-Item -LiteralPath (Join-Path $built 'DIAGNOSTIC.EFI') -Destination $target -Force
if((Get-FileHash -LiteralPath $target).Hash -ne $m.EfiSha256){throw 'Observation copy hash mismatch'}
$m.Mode='usb-observation-only'
Assert-ObservationManifest $repo $m 'usb-observation-only' $target
$m | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $output 'manifest.json') -Encoding utf8
Write-Output "Built $target ($($m.EfiSha256)). No staging, driver release, unlock or boot-order change performed."
