# Exact-machine attended installer/rearm. No formatting, BootOrder writes or automatic reboot.
[CmdletBinding(SupportsShouldProcess)]
param([ValidateSet('Install','Rearm','Collect','Cleanup','Cancel','Status')][string]$Action='Install',
 [ValidateSet('SharedEsp','Usb','SeparateEsp')][string]$Layout='SharedEsp',
 [string]$StateDirectory='E:\UNLOCK\EVGA-PROFILE',
 [guid]$WindowsEspGuid='c36c3e86-02aa-4f76-af1b-96e327f9d1c1',
 [ValidatePattern('^[A-Za-z]$')][string]$Drive='D',[ValidatePattern('^[A-Za-z]$')][string]$TargetDrive='U',
 [string]$Assets,[string]$EvidenceDirectory,[string]$PackageDirectory,[switch]$ReviewedEvp10Retry,
 [string]$Clang='C:/Program Files/LLVM/bin/clang.exe',[string]$LldLink='C:/Program Files/LLVM/bin/lld-link.exe',[string]$Python='python')
$ErrorActionPreference='Stop'
if($ReviewedEvp10Retry -and $Action -ne 'Rearm'){throw 'Reviewed EVP10 retry requires explicit Rearm'}
. (Join-Path $PSScriptRoot 'shared-esp-firmware-lib.ps1')
. (Join-Path $PSScriptRoot 'evga-package-lib.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$stateDir=[IO.Path]::GetFullPath($StateDirectory)
if($stateDir -notmatch '^[A-Za-z]:\\'){throw 'State must be a local directory outside the ESP'}
for($p=$stateDir;$p;){if((Test-Path -LiteralPath $p) -and ([IO.File]::GetAttributes($p) -band [IO.FileAttributes]::ReparsePoint)){throw 'State path contains a reparse point'};$parent=[IO.Path]::GetDirectoryName($p.TrimEnd([char[]]'\/'));if($parent -eq $p){break};$p=$parent}
$configPath=Join-Path $stateDir 'installation.json'
$config=$null
if($Action -ne 'Install'){
 $config=Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
 if($config.Schema -cne 'evga-attended-install-v1'){throw 'Invalid installation state'}
 $Layout=$config.Layout;$WindowsEspGuid=[guid]$config.WindowsEspGuid
 if(!$PSBoundParameters.ContainsKey('Drive')){$Drive=$config.CoordinatorDrive}
 if(!$PSBoundParameters.ContainsKey('TargetDrive')){$TargetDrive=$config.TargetDrive}
 $null=Get-EvgaPackageProfile $Layout
}
elseif(Test-Path -LiteralPath $stateDir){throw 'State directory already exists; use Rearm/Status, never overwrite state'}
if(-not ('SharedEspFirmware.Native' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'shared-esp-firmware-native.cs')}
$scope=[SharedEspFirmware.Native]::EnablePrivilege()
try {
 $read={param($name) $v=[SharedEspFirmware.Native]::Read($name);if($v){ConvertTo-FirmwareValue $v.Data $v.Attributes}}
 $write={param($name,$value)
  if($name -cne 'BootNext' -and $name -cnotmatch '^Boot7E[0-9A-F]{2}$'){throw 'Unexpected firmware mutation'}
  if($null -eq $value){[SharedEspFirmware.Native]::Write($name,$null,7)}else{[SharedEspFirmware.Native]::Write($name,[Convert]::FromBase64String($value.DataBase64),[uint32]$value.Attributes)}
 }
 if($Action -in @('Status','Collect','Cleanup','Cancel')){
  $journals=@(Get-ChildItem -LiteralPath $stateDir -File -Filter 'attempt-*.json' | Sort-Object Name)
  if(!$journals.Count){throw 'No attempt journal; preserve incomplete install and report'}
  $attempt=Get-Content -LiteralPath $journals[-1].FullName -Raw | ConvertFrom-Json
  if($attempt.Schema -cne 'evga-attended-attempt-v1' -or $attempt.AttemptSlot -lt 8 -or $attempt.AttemptSlot -gt 99 -or $attempt.Layout -cne $Layout){throw 'Invalid attempt journal'}
  $m=Assert-EvgaPackage $attempt.PackageDirectory
  if((Get-EvgaPackageHash (Join-Path $attempt.PackageDirectory 'package.json')) -cne $attempt.PackageSha256 -or $m.AttemptSlot -ne $attempt.AttemptSlot){throw 'Installed package changed'}
  if($m.Layout -cne $Layout -or $attempt.Run -cne $m.Run){throw 'Attempt/package layout or run differs'}
  if($Layout -ne 'Usb'){
   if($attempt.OptionName -cnotmatch '^Boot7E[0-9A-F]{2}$' -or $attempt.OptionName -cne ('Boot{0:X4}' -f [int]$attempt.OptionNumber) -or $attempt.Option.Attributes -ne 7){throw 'Invalid owned firmware option'}
   $decoded=Read-SharedEspLoadOption $attempt.Option
   $expectedGuid=if($Layout -eq 'SharedEsp'){[guid]$config.WindowsEspGuid}else{[guid]$config.UnlockPartitionGuid}
   if(!$decoded.Active -or $decoded.Path -cne $m.EfiPath -or $decoded.PartitionGuid -ne $expectedGuid){throw 'Owned option does not match package target'}
  }
  $pending=$null;$observed=& $read 'BootNext';if($null -ne $observed){$pending=$observed}
  if($Action -eq 'Status'){
   [ordered]@{profile=$m.ProfileId;slot=$attempt.AttemptSlot;run=$attempt.Run;boot_next=$pending;option_present=$(if($Layout -eq 'Usb'){$false}else{$null -ne (& $read $attempt.OptionName)});log_present=(Test-Path -LiteralPath (Join-Path $attempt.TargetRoot ('RTX3090-UNLOCK/'+$attempt.Run+'.TXT')));rearm_requires_successful_collection=$true;firmware_written=$false} | ConvertTo-Json -Depth 5
   return
  }
  $resultPath=Join-Path $stateDir ('result-{0:D2}.json' -f $attempt.AttemptSlot)
  if($Action -eq 'Cleanup'){
   if($Layout -eq 'Usb'){throw 'Separate cleanup action is for internal-media firmware options only'}
   $previous=Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
   if($previous.Success -ne $true -or $previous.AttemptSlot -ne $attempt.AttemptSlot -or $previous.CleanupCompleted -eq $true){throw 'Expected successful collection with pending cleanup'}
   $proof=Assert-EvgaPackageResult $attempt.TargetRoot $attempt.AttemptSlot $attempt.Run
   if($proof.LogSha256 -cne $previous.LogSha256 -or $proof.GateSha256 -cne $previous.GateSha256 -or $proof.GpuMarkerSha256 -cne $previous.GpuMarkerSha256){throw 'Collected evidence changed'}
   $receipt=Join-Path $stateDir ('cleanup-{0:D2}-result.json' -f $attempt.AttemptSlot)
   if(Test-Path -LiteralPath $receipt){throw 'Cleanup already recorded; preserve receipt'}
   if(!$PSCmdlet.ShouldProcess($attempt.Run,'Remove exact owned temporary option; preserve collection result')){return}
   $save={param($snapshot)Write-EvgaPackageJson (Join-Path $stateDir ('cleanup-{0:D2}-separate-before.json' -f $attempt.AttemptSlot)) $snapshot}
   Remove-EvgaPackageOption $attempt $read $write $save
   $done=[ordered]@{Schema='evga-attended-cleanup-v1';AttemptSlot=$attempt.AttemptSlot;CleanupCompleted=$true;OptionName=$attempt.OptionName;LogSha256=$proof.LogSha256;GateSha256=$proof.GateSha256;GpuMarkerSha256=$proof.GpuMarkerSha256;OriginalResultPreserved=$true;BootOrderWritten=$false;GpuAccessed=$false}
   Write-EvgaPackageJson $receipt $done
   $done | ConvertTo-Json
   return
  }
  if(Test-Path -LiteralPath $resultPath){throw 'Attempt already finalized; do not repeat collection/cancellation'}
  if(!$PSCmdlet.ShouldProcess($attempt.Run,($Action+' current attempt; retain every receipt, reservation and backup'))){return}
  $proof=$null;$evidenceError=$null;$zip=$null
  if($Action -eq 'Collect'){
   $out=Join-Path $stateDir ($attempt.Run+'-results')
   if(Test-Path -LiteralPath $out){throw 'Evidence output already exists; preserve it'}
   [void][IO.Directory]::CreateDirectory($out)
   foreach($name in @(($attempt.Run+'.ARMED'),($attempt.Run+'.TXT'),('EVGA-UNLOCK'+$attempt.AttemptSlot+'.ARMED'))){
    $path=Join-Path $attempt.TargetRoot ('RTX3090-UNLOCK/'+$name)
    if(Test-Path -LiteralPath $path){$source=Get-AuditCheckedPath $attempt.TargetRoot @('RTX3090-UNLOCK',$name) $false;Copy-EvgaPackageFile $source (Join-Path $out $name) (Get-EvgaPackageHash $source)}
   }
   try{$proof=Assert-EvgaPackageResult $attempt.TargetRoot $attempt.AttemptSlot $attempt.Run}catch{$evidenceError=$_.Exception.Message}
   Copy-EvgaPackageFile (Join-Path $attempt.PackageDirectory 'package.json') (Join-Path $out 'package.json') $attempt.PackageSha256
   $zip=$out+'.zip';Compress-Archive -LiteralPath $out -DestinationPath $zip -ErrorAction Stop
  }
  $cleanup=$false;$cleanupError=$null
  try {
   if($Layout -eq 'Usb'){
    $loader=Get-AuditCheckedPath $attempt.TargetRoot @('EFI','BOOT','BOOTX64.EFI') $false
    if((Get-EvgaPackageHash $loader) -cne $attempt.EfiSha256){throw 'USB loader changed; refuse restoration'}
    if($attempt.UsbBackup){
     if((Get-EvgaPackageHash $attempt.UsbBackup) -cne $attempt.UsbBeforeSha256){throw 'USB backup changed'}
     $temp=$loader+'.restore-'+$attempt.Run
     Copy-EvgaPackageFile $attempt.UsbBackup $temp $attempt.UsbBeforeSha256
     [IO.File]::Replace($temp,$loader,[NullString]::Value)
     if((Get-EvgaPackageHash $loader) -cne $attempt.UsbBeforeSha256){throw 'Restored USB loader readback failed'}
    }else{[IO.File]::Delete($loader)}
    $cleanup=$true
   }else{
    $save={param($snapshot) Write-EvgaPackageJson (Join-Path $stateDir ('cleanup-{0:D2}.json' -f $attempt.AttemptSlot)) $snapshot}
    Remove-EvgaPackageOption $attempt $read $write $save
    $cleanup=$true
   }
  }catch{$cleanupError=$_.Exception.Message}
  $result=[ordered]@{Schema='evga-attended-result-v1';AttemptSlot=$attempt.AttemptSlot;Success=($Action -eq 'Collect' -and $null -ne $proof);LogSha256=$(if($proof){$proof.LogSha256}else{$null});GateSha256=$(if($proof){$proof.GateSha256}else{$null});GpuMarkerSha256=$(if($proof){$proof.GpuMarkerSha256}else{$null});CleanupCompleted=$cleanup;EvidenceError=$evidenceError;CleanupError=$cleanupError;Zip=$zip;Cancelled=($Action -eq 'Cancel');WindowsReturnRequiresOperatorReport=$true}
  Write-EvgaPackageJson $resultPath $result
  $result['Next']='Send the JSON and evidence ZIP. Rearm is allowed only after a successful collected run and cleanup. Failed/cancelled attempts require review; never delete state to retry.'
  $result | ConvertTo-Json -Depth 5
  return
 }
 # Install/rearm are preparation only. Live EFI still requires fresh Shift+G.
 $gpu=@(Get-CimInstance Win32_VideoController | Where-Object {$_.PNPDeviceID -match 'VEN_10DE&DEV_2204&SUBSYS_33873842'})
 if($gpu.Count -ne 1){throw 'Expected the exact reviewed EVGA 2204:3387 GPU; no install attempted'}
 $windows=@(Get-Partition | Where-Object {$_.IsSystem -and $_.Guid -and [guid]$_.Guid -eq $WindowsEspGuid})
 if($windows.Count -ne 1){throw 'Expected unique pinned Windows system ESP'}
 $wv=@($windows[0] | Get-Volume);if($wv.Count -ne 1 -or $wv[0].FileSystem -ne 'FAT32'){throw 'Expected FAT32 Windows ESP'}
 $windowsRoot=$wv[0].Path
 $normal=Get-AuditCheckedPath $windowsRoot @('EFI','Microsoft','Boot','bootmgfw.efi') $false
 if((Get-EvgaPackageHash $normal) -cne '2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'){throw 'Reviewed Windows boot file changed; no install attempted'}
 $coordinator=@(Get-Partition -DriveLetter $Drive);$uv=Get-Volume -DriveLetter $Drive
 if($coordinator.Count -ne 1 -or $coordinator[0].IsSystem -or $coordinator[0].IsBoot -or (Get-Disk -Number $coordinator[0].DiskNumber).BusType -ne 'USB' -or $uv.FileSystem -ne 'FAT32' -or !$uv.UniqueId){throw 'Connect the original FAT32 capture USB for the permanent reservation'}
 $usbCapture=Get-AuditCheckedPath $uv.Path @('RTX3090-UNLOCK') $true
 $unlockGuid=[guid]::Empty
 if($Layout -eq 'SharedEsp'){$targetPart=$windows[0];$targetVolume=$wv[0]}
 elseif($Layout -eq 'Usb'){$targetPart=$coordinator[0];$targetVolume=$uv}
 else{
  $tp=@(Get-Partition -DriveLetter $TargetDrive);$tv=Get-Volume -DriveLetter $TargetDrive
  if($tp.Count -ne 1 -or $tp[0].IsSystem -or $tp[0].IsBoot -or !$tp[0].Guid -or [guid]$tp[0].Guid -eq $WindowsEspGuid -or [guid]$tp[0].GptType -ne [guid]'c12a7328-f81f-11d2-ba4b-00a0c93ec93b' -or (Get-Disk -Number $tp[0].DiskNumber).BusType -eq 'USB' -or $tv.FileSystem -ne 'FAT32'){throw 'SeparateEsp needs an existing distinct internal FAT32 unlock ESP; no formatting is performed'}
  $targetPart=$tp[0];$targetVolume=$tv;$unlockGuid=[guid]$tp[0].Guid
 }
 $targetRoot=$targetVolume.Path
 if(!$targetRoot -or !$targetVolume.UniqueId){throw 'Target volume identity unavailable'}
 $backupPart=@(Get-Partition -DriveLetter $stateDir.Substring(0,1))
 if($backupPart.Count -ne 1 -or ($backupPart[0].DiskNumber -eq $targetPart.DiskNumber -and $backupPart[0].PartitionNumber -eq $targetPart.PartitionNumber)){throw 'State directory must be on a separate accessible partition from the target'}
 if(!$Assets){$candidate=Join-Path $windowsRoot 'RTX3090-UNLOCK/ASSETS';$Assets=if(Test-Path -LiteralPath $candidate){$candidate}else{Join-Path $usbCapture 'ASSETS'}}
 $specFile=Join-Path $repo 'internal-boot/unlock/assets.json'
 $specs=Assert-EvgaAssetDirectory $Assets $specFile
 if($targetVolume.SizeRemaining -lt (32MB+($specs | Measure-Object Bytes -Sum).Sum)){throw 'Not enough target free space; no resizing/deleting is performed'}
 $secure=& $read 'SecureBoot'
 if(!$secure -or [Convert]::ToBase64String([Convert]::FromBase64String($secure.DataBase64)) -cne 'AA=='){throw 'Secure Boot must already be disabled; no setting is changed'}
 if($null -ne (& $read 'BootNext')){throw 'A pending BootNext exists; do not replace it'}
 $snapshot=Get-EvgaFirmwareSnapshot $read
 $windowsFound=$false
 foreach($entry in $snapshot.BootEntries){try{$decoded=Read-SharedEspLoadOption $entry.Value;if($decoded.Active -and $decoded.PartitionGuid -eq $WindowsEspGuid -and $decoded.Path -ieq '\EFI\Microsoft\Boot\bootmgfw.efi'){$windowsFound=$true}}catch{}}
 if(!$windowsFound){throw 'No active pinned Windows boot entry in current BootOrder'}
 if($Action -eq 'Install'){
  . (Join-Path $PSScriptRoot 'select-evga-attempt.ps1')
  $markers=@(Get-ChildItem -LiteralPath $usbCapture -File -Filter 'EVGA-UNLOCK*.ARMED' | ForEach-Object {$_.Name})
  $seed=(Get-EvgaNextAttemptSlot $markers)-1
  if(!$EvidenceDirectory){$EvidenceDirectory=if($seed -eq 9){Join-Path $windowsRoot 'RTX3090-UNLOCK'}else{$usbCapture}}
  $seedLog=if($seed -eq 9){Get-AuditCheckedPath $EvidenceDirectory @('SULK02.TXT') $false}elseif($seed -eq 7){Get-AuditCheckedPath $EvidenceDirectory @('EVGA23.TXT') $false}else{throw 'Only reviewed slot-7 USB or slot-9 shared migration is supported'}
  $expected=if($seed -eq 9){'38963300821c434cbffd4a076286edf07799b738d0e042378d00492246b66db4'}else{'7c2c6972f2e8c971f36127028aecbcc5df06cfaedc3dae249a6d846569114bc4'}
  if((Get-EvgaPackageHash $seedLog) -cne $expected){throw 'Migration evidence differs from reviewed successful run'}
  $config=[ordered]@{Schema='evga-attended-install-v1';Layout=$Layout;WindowsEspGuid=$WindowsEspGuid.ToString();UnlockPartitionGuid=$unlockGuid.ToString();CoordinatorUniqueId=$uv.UniqueId;CoordinatorDrive=$Drive;TargetDrive=$TargetDrive;TargetUniqueId=$targetVolume.UniqueId;SeedSlot=$seed;SeedLogSha256=$expected}
  $slot=$seed+1
 }else{
  if($config.CoordinatorUniqueId -cne $uv.UniqueId -or $config.TargetUniqueId -cne $targetVolume.UniqueId -or [guid]$config.UnlockPartitionGuid -ne $unlockGuid){throw 'Installation volume identity changed'}
  $slot=Get-EvgaProfileNextSlot $stateDir $config -ReviewedEvp10Retry:$ReviewedEvp10Retry
  if($ReviewedEvp10Retry){
   $prior=Get-Content -LiteralPath (Join-Path $stateDir 'attempt-10.json') -Raw | ConvertFrom-Json
   if($Layout -cne 'SharedEsp' -or $null -ne (& $read $prior.OptionName)){throw 'Prior reviewed option remains or layout differs'}
  }
 }
 $run='EVP{0:D2}' -f $slot
 $reservation=Join-Path $usbCapture ('EVGA-UNLOCK-RESERVED'+$slot+'.ARMED')
 foreach($path in @($reservation,(Join-Path $targetRoot ('RTX3090-UNLOCK/'+$run+'.ARMED')),(Join-Path $targetRoot ('RTX3090-UNLOCK/'+$run+'.TXT')),(Join-Path $targetRoot ('RTX3090-UNLOCK/EVGA-UNLOCK'+$slot+'.ARMED')))){if(Test-Path -LiteralPath $path){throw 'Fresh attempt target already exists; preserve state and stop'}}
 if(!$PSCmdlet.ShouldProcess($run,('Build/install '+$Layout+' attended profile; reserve fresh slot and select one boot where supported; GPU execution requires Shift+G'))){return}
 if(!$PackageDirectory){
  $PackageDirectory=Join-Path $PSScriptRoot ('build/packages/'+$Layout+'-'+$run+'-'+[guid]::NewGuid().ToString('N'))
  & (Join-Path $PSScriptRoot 'build-evga-profile.ps1') -Layout $Layout -WindowsEspGuid $WindowsEspGuid -UnlockPartitionGuid $unlockGuid -AttemptSlot $slot -OutputDirectory $PackageDirectory -Clang $Clang -LldLink $LldLink -Python $Python | Out-Host
 }
 $m=Assert-EvgaPackage $PackageDirectory
 if($m.Layout -cne $Layout -or $m.AttemptSlot -ne $slot -or [guid]$m.WindowsEspGuid -ne $WindowsEspGuid -or [guid]$m.UnlockPartitionGuid -ne $unlockGuid -or (Get-EvgaPackageHash (Join-Path $PackageDirectory 'assets.json')) -cne (Get-EvgaPackageHash $specFile)){throw 'Package does not match current installation'}
 if($Action -eq 'Install'){[void][IO.Directory]::CreateDirectory($stateDir);Write-EvgaPackageJson $configPath $config}
 $exeHash=Get-EvgaPackageHash (Join-Path $PackageDirectory 'UNLOCK.EFI')
 $attempt=[ordered]@{Schema='evga-attended-attempt-v1';AttemptSlot=$slot;Run=$run;Layout=$Layout;PackageDirectory=[IO.Path]::GetFullPath($PackageDirectory);PackageSha256=(Get-EvgaPackageHash (Join-Path $PackageDirectory 'package.json'));EfiSha256=$exeHash;TargetRoot=$targetRoot;BootOrder=$snapshot.BootOrder;BootEntries=$snapshot.BootEntries;SecureBoot=$secure;OptionName=$null;OptionNumber=$null;Option=$null;UsbBackup=$null;UsbBeforeSha256=$null}
 if($Layout -ne 'Usb'){
  $disk=Get-Disk -Number $targetPart.DiskNumber;$sector=[uint64]$disk.LogicalSectorSize
  if($disk.PartitionStyle -ne 'GPT' -or $disk.BusType -eq 'USB' -or $sector -notin @(512,4096) -or $targetPart.Offset%$sector -or $targetPart.Size%$sector){throw 'Unsupported internal target geometry'}
  $number=$null
  for($n=0x7e00;$n -le 0x7eff;$n++){if($null -eq (& $read ('Boot{0:X4}' -f $n)) -and $snapshot.BootEntries.Name -cnotcontains ('Boot{0:X4}' -f $n)){$number=$n;break}}
  if($null -eq $number){throw 'No free temporary firmware option'}
  $attempt.OptionNumber=$number;$attempt.OptionName='Boot{0:X4}' -f $number
  $targetGuid=if($Layout -eq 'SharedEsp'){$WindowsEspGuid}else{$unlockGuid}
  $attempt.Option=ConvertTo-FirmwareValue (New-SharedEspLoadOption $targetGuid ([uint32]$targetPart.PartitionNumber) ([uint64]([decimal]$targetPart.Offset/$sector)) ([uint64]([decimal]$targetPart.Size/$sector)) ('EVGA ATTENDED '+$run) $m.EfiPath)
 }else{
  $loader=Join-Path $targetRoot 'EFI/BOOT/BOOTX64.EFI'
  if(Test-Path -LiteralPath $loader){$attempt.UsbBackup=Join-Path $stateDir ('usb-before-'+$run+'.efi');$attempt.UsbBeforeSha256=Get-EvgaPackageHash $loader}
 }
 $journal=Join-Path $stateDir ('attempt-{0:D2}.json' -f $slot)
 Write-EvgaPackageJson $journal $attempt
 $journalHash=Get-EvgaPackageHash $journal
 Write-SharedSecondNewFile $reservation ([Text.Encoding]::ASCII.GetBytes("RESERVED $run $Layout. NOT EXECUTION PROOF. DO NOT DELETE OR RETRY.`r`n"))
 $logRoot=Get-EvgaCheckedDirectory $targetRoot @('RTX3090-UNLOCK')
 $assetTarget=Get-EvgaCheckedDirectory $logRoot @('ASSETS')
 foreach($spec in $specs){Copy-EvgaPackageFile (Join-Path $Assets $spec.Name) (Join-Path $assetTarget $spec.Name) $spec.Sha256}
 foreach($name in @('NVIDIA-LICENSE','NOTICE.md')){Copy-EvgaPackageFile (Join-Path $Assets $name) (Join-Path $assetTarget $name) (Get-EvgaPackageHash (Join-Path $Assets $name))}
 $efiDir=Get-EvgaCheckedDirectory $targetRoot @('EFI','WindowsGPUUnlock',$run)
 $installed=Join-Path $efiDir 'UNLOCK.EFI';Copy-EvgaPackageFile (Join-Path $PackageDirectory 'UNLOCK.EFI') $installed $exeHash
 $pending=New-Object byte[] 512;$prefix=[Text.Encoding]::ASCII.GetBytes("PENDING $run`r`n");[Array]::Copy($prefix,$pending,$prefix.Length)
 $receipt=Join-Path $logRoot ($run+'.ARMED');Write-SharedSecondNewFile $receipt $pending
 $verify={
  if((Get-EvgaPackageHash $journal) -cne $journalHash -or (Get-EvgaPackageHash $installed) -cne $exeHash -or (Get-EvgaPackageHash $normal) -cne '2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'){throw 'Stage, journal or normal boot file changed'}
  if([Convert]::ToBase64String([IO.File]::ReadAllBytes($receipt)) -cne [Convert]::ToBase64String($pending)){throw 'Pending receipt changed'}
  $null=Assert-EvgaAssetDirectory $assetTarget (Join-Path $PackageDirectory 'assets.json')
 }
 & $verify
 if($Layout -eq 'Usb'){
  $bootDir=Get-EvgaCheckedDirectory $targetRoot @('EFI','BOOT');$loader=Join-Path $bootDir 'BOOTX64.EFI'
  if($attempt.UsbBackup){
   Copy-EvgaPackageFile $loader $attempt.UsbBackup $attempt.UsbBeforeSha256
   $temp=$loader+'.'+$run+'.new';Copy-EvgaPackageFile $installed $temp $exeHash
   if((Get-EvgaPackageHash $loader) -cne $attempt.UsbBeforeSha256){throw 'USB loader raced before replacement'}
   [IO.File]::Replace($temp,$loader,[NullString]::Value)
  }else{Copy-EvgaPackageFile $installed $loader $exeHash}
  if((Get-EvgaPackageHash $loader) -cne $exeHash){throw 'USB loader readback failed'}
 }else{
  $save={param($unused) if((Get-EvgaPackageHash $journal) -cne $journalHash){throw 'Immutable journal changed'};& $verify}
  Register-SharedEspFirmwareOption $attempt $read $write $save
  Select-RegisteredSharedSecond $attempt $read $write {} $verify
 }
 [ordered]@{prepared=$true;profile=$m.ProfileId;slot=$slot;run=$run;package_zip=($PackageDirectory+'.zip');requires_fresh_shift_g=$true;full_shutdown_required=$true;boot_next_selected=($Layout -ne 'Usb');boot_order_written=$false;automatic_reboot=$false;next=$(if($Layout -eq 'Usb'){'Save work, full shutdown with Shift+Shut down, power on and boot this USB once; fresh Shift+G only after guards. After Windows run this command with -Action Collect.'}else{'Remove storage USBs, save work, full shutdown with Shift+Shut down, then power on; fresh Shift+G only after guards. After Windows run this command with -Action Collect.'})} | ConvertTo-Json
}finally{$scope.Dispose()}
