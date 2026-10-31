# GPL-2.0-only. Stages one attended GPU release/resources unlock image; never modifies firmware entries or reboots.
[CmdletBinding(SupportsShouldProcess)]
param([Parameter(Mandatory)][guid]$WindowsEspGuid,[Parameter(Mandatory)][ValidatePattern('^[A-Za-z]$')][string]$Drive)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-unlock-history.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $repo 'internal-boot/boot/build/shared-esp-unlock'
$manifest=Get-Content -LiteralPath (Join-Path $build 'manifest.json') -Raw | ConvertFrom-Json
if($WindowsEspGuid -eq [guid]::Empty -or [guid]$manifest.WindowsEspGuid -ne $WindowsEspGuid -or $manifest.CanUnlock -ne $true -or $manifest.Mode -ne 'shared-esp-actual-unlock' -or $manifest.Visible -ne $true -or $manifest.AuditFileName -ne 'EVGA-SHARED-UNLOCK.EFI'){throw 'Audit build or ESP pin mismatch'}
$boot=Join-Path $repo 'internal-boot/boot'
foreach($pin in $manifest.Sources){
 $path=[IO.Path]::GetFullPath((Join-Path $boot $pin.Path))
 if(!$path.StartsWith($repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Get-FileHash -LiteralPath $path).Hash -ine $pin.Sha256){throw 'Qualification source changed since build'}
}
$partition=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
if($partition.Count -ne 1){throw 'Expected exactly one pinned Windows system ESP'}
$volumes=@($partition[0] | Get-Volume -ErrorAction Stop)
if($volumes.Count -ne 1 -or $volumes[0].FileSystem -ne 'FAT32' -or !$volumes[0].Path){throw 'Expected accessible FAT32 system volume'}
$source=Join-Path $build 'BOOTX64.EFI'
if((Get-FileHash -LiteralPath $source).Hash -ine $manifest.EfiSha256){throw 'Preflight build hash mismatch; no write attempted'}
$root=$volumes[0].Path
$assetTarget=Get-AuditCheckedPath $root @('RTX3090-UNLOCK') $true
$assets=Get-AuditCheckedPath $root @('RTX3090-UNLOCK','ASSETS') $true
foreach($spec in (Get-Content -Raw -LiteralPath (Join-Path $repo 'internal-boot/unlock/assets.json') | ConvertFrom-Json)){
 $src=Get-AuditCheckedPath $assets @($spec.Name) $false
 if((Get-Item -LiteralPath $src).Length -ne $spec.Bytes -or (Get-FileHash -LiteralPath $src).Hash -ine $spec.Sha256){throw 'Existing ESP asset mismatch; preserve files'}
}
$approval=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'shared-unlock-qualification.json') -Raw | ConvertFrom-Json
if($approval.Schema -ne 'shared-esp-unlock-qualification-v1' -or $approval.Approved -ne $true -or $approval.WindowsReturned -ne $true -or $approval.UsbRestored -ne $true){throw 'Actual unlock staging blocked: QUAL01 raw evidence and recovery review not approved'}
foreach($pair in @(@('QUAL01.ARMED',$approval.MarkerSha256),@('QUAL01-GPU.ARMED',$approval.GpuMarkerSha256),@('QUAL01.TXT',$approval.LogSha256))){
 if($pair[1] -notmatch '^[0-9a-fA-F]{64}$'){throw 'Reviewed QUAL01 hash is missing'}
 $file=Get-AuditCheckedPath $root @('RTX3090-UNLOCK',$pair[0]) $false
 if((Get-FileHash -LiteralPath $file).Hash -ine $pair[1]){throw 'QUAL01 evidence differs from reviewed hashes; preserve files'}
}
$qualLog=Get-Content -LiteralPath (Join-Path $assetTarget 'QUAL01.TXT') -Raw
foreach($event in @('evga-text-stop-return','evga-virtual-text-stop-return','evga-graphics-stop-return','evga-edid-stop-return','evga-edid-sibling1-return','evga-edid-sibling2-return','evga-nvidia-children-stop-return','evga-nvidia-parent-stop-return','evga-shared-claim-return','evga-shared-close-return','evga-state-prom-hash','evga-runtime-config','evga-runtime-bar','evga-memory-disable-return','evga-text-stop-ready-windows')){
 if(!$qualLog.Contains($event+' status=0x0000000000000000')){throw "QUAL01 lacks successful $event"}
}
if(([regex]::Matches($qualLog,'capture-complete=1').Count) -lt 2 -or ([regex]::Matches($qualLog,'evga-decode-command-restored status=0x0000000000000000').Count) -lt 2){throw 'QUAL01 capture/decode restoration is incomplete'}
$letter=$Drive.ToUpperInvariant();$usbParts=@(Get-Partition -DriveLetter $letter);$usbVolume=Get-Volume -DriveLetter $letter
if($usbParts.Count -ne 1 -or $usbParts[0].IsSystem -or $usbParts[0].IsBoot -or $usbVolume.FileSystem -ne 'FAT32' -or (Get-Disk -Number $usbParts[0].DiskNumber).BusType -ne 'USB'){throw 'Expected original non-system FAT32 capture USB'}
$history=Get-SharedUnlockHistory ($letter+':\')
if([int]$manifest.AttemptSlot -ne $history.NextSlot){throw 'Build attempt slot differs from original USB history; preserve all markers'}
if(@(Get-ChildItem -LiteralPath $assetTarget -File -Filter 'EVGA-UNLOCK*.ARMED').Count){throw 'ESP already has a transaction marker; first shared-ESP test only'}
$historyTarget=Join-Path $assetTarget 'USB-HISTORY-SULK01'
if(Test-Path -LiteralPath $historyTarget){throw 'Shared transaction history snapshot already exists; preserve it'}
$reservation=Join-Path $history.Capture ("EVGA-UNLOCK$($history.NextSlot).ARMED")
$receipt=Join-Path $assetTarget 'SULK01.ARMED'
if((Test-Path -LiteralPath (Join-Path $assetTarget 'SULK01.TXT')) -or (Test-Path -LiteralPath $reservation)){throw 'SULK01 log already exists; preserve and report'}
if((Test-Path -LiteralPath $receipt) -or (Test-Path -LiteralPath (Join-Path $root 'EFI/EVGA-SHARED-UNLOCK.EFI'))){throw 'SULK01 already staged or used; preserve and report'}
if($volumes[0].SizeRemaining -lt (32MB+$history.Bytes)){throw 'Need 32 MiB plus history snapshot free space; do not delete or resize'}
$windowsHash='2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'
$limineHash='a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462'
if($PSCmdlet.ShouldProcess('pinned Windows ESP and original capture USB','Reserve next USB attempt and stage first shared-ESP ACTUAL UNLOCK without replacing files')){
 foreach($bootPin in @(@(@('EFI','Microsoft','Boot','bootmgfw.efi'),$windowsHash),@(@('EFI','limine','limine_x64.efi'),$limineHash))){
  $file=Get-AuditCheckedPath $root $bootPin[0] $false
  if((Get-FileHash -LiteralPath $file).Hash -ine $bootPin[1]){throw 'Boot file changed; no write attempted'}
 }
 # Reserve the numbered attempt on the ORIGINAL USB before making a bootable ESP candidate.
 # This is a reservation, not evidence a GPU transaction executed.
 $reservationBytes=[Text.Encoding]::ASCII.GetBytes("RESERVED FOR SHARED ESP SULK01 SLOT $($history.NextSlot). NOT EXECUTION PROOF. DO NOT DELETE OR RETRY.`r`n")
 $stream=$null
 try {$stream=[IO.File]::Open($reservation,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$stream.Write($reservationBytes,0,$reservationBytes.Length);$stream.Flush($true)}finally{if($stream){$stream.Dispose()}}
 if([Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($reservation)) -cne [Text.Encoding]::ASCII.GetString($reservationBytes)){throw 'USB reservation readback failed; preserve everything'}
 [void][IO.Directory]::CreateDirectory($historyTarget)
 foreach($item in $history.Files){
  if((Get-FileHash -LiteralPath $item.Path).Hash -ine $item.Hash){throw 'USB history changed during staging'}
  $dst=Join-Path $historyTarget $item.Name;$i=$null;$o=$null
  try {$i=[IO.File]::OpenRead($item.Path);$o=[IO.File]::Open($dst,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$i.CopyTo($o);$o.Flush($true)}finally{if($o){$o.Dispose()};if($i){$i.Dispose()}}
  if((Get-FileHash -LiteralPath $dst).Hash -ine $item.Hash){throw 'ESP history snapshot differs; preserve all files'}
 }
 $bytes=New-Object byte[] 512
 [Array]::Copy([Text.Encoding]::ASCII.GetBytes("PENDING SULK01`r`n"),$bytes,16)
 $stream=$null
 try {$stream=[IO.File]::Open($receipt,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{if($stream){$stream.Dispose()}}
 if((Get-Item -LiteralPath $receipt).Length -ne 512 -or (Get-FileHash -LiteralPath $receipt).Hash -ine ([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash($bytes)).Replace('-',''))){throw 'Pending receipt verification failed; preserve it'}
 $target=Copy-SharedEspAudit -Root $volumes[0].Path -Source $source -ExpectedEfiHash $manifest.EfiSha256 -WindowsHash $windowsHash -LimineHash $limineHash -AuditFileName $manifest.AuditFileName
 Write-Output "Staged ACTUAL UNLOCK EFI at $target; existing boot file hashes verified. USB attempt slot $($history.NextSlot) is RESERVED, not execution proof; preserve it. No boot entry changed or reboot performed."
}
