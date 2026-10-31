# GPL-2.0-only. Shared package contracts; no operations at import.
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
. (Join-Path $PSScriptRoot 'shared-esp-second-arm-lib.ps1')
function Get-EvgaPackageProfile([string]$Layout){
 $slug=@{SharedEsp='shared-esp';Usb='usb';SeparateEsp='separate-esp'}[$Layout]
 if(!$slug){throw 'Unknown layout'}
 $path=Join-Path $PSScriptRoot ('profiles/evga-z790-3090-'+$slug+'.json')
 $p=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
 if($p.schema -cne 'evga-attended-profile-v1' -or $p.layout -cne $Layout -or $p.machine_profile -cne 'GU_PROFILE_EVGA_Z790_3090' -or $p.pci_id -cne '220410de' -or $p.subsystem_id -cne '33873842' -or $p.requires_fresh_shift_g -ne $true -or $p.automatic_unlock -ne $false -or $p.permanent_unlock -ne $false -or $p.general_hardware_compatibility -ne $false){throw 'Unsupported profile contract'}
 return $p
}
function Get-EvgaPackageHash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Write-EvgaPackageJson([string]$Path,$Value){Write-SharedSecondNewFile $Path ([Text.UTF8Encoding]::new($false).GetBytes(($Value | ConvertTo-Json -Depth 12)))}
function Assert-EvgaPackageResult([string]$Root,[int]$Slot,[string]$Run){
 $log=Get-AuditCheckedPath $Root @('RTX3090-UNLOCK',($Run+'.TXT')) $false
 $gate=Get-AuditCheckedPath $Root @('RTX3090-UNLOCK',($Run+'.ARMED')) $false
 $gpu=Get-AuditCheckedPath $Root @('RTX3090-UNLOCK',("EVGA-UNLOCK$Slot.ARMED")) $false
 if((Get-Item -LiteralPath $log).Length -gt 9MB -or (Get-Item -LiteralPath $gate).Length -ne 512 -or (Get-Item -LiteralPath $gpu).Length -gt 1024){throw 'Unexpected prior evidence size'}
 $prefix=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($gate))
 if(!$prefix.StartsWith("STARTED $Run - DO NOT REPEAT`r`n",[StringComparison]::Ordinal)){throw 'Prior gate did not record execution'}
 $text=[IO.File]::ReadAllText($log)
 foreach($event in @(('evga-unlock-slot-{0:D2}' -f $Slot),'evga-unlock-transaction-return','evga-unlock-assets-released','evga-unlock-close-return','evga-text-stop-ready-windows')){if(!$text.Contains($event+' status=0x0000000000000000')){throw "Prior attempt incomplete: $event"}}
 if(([regex]::Matches($text,'(?m)^capture-complete=1\r?$')).Count -lt 2){throw 'Prior attempt lacks two captures'}
 return [pscustomobject]@{Log=$log;LogSha256=(Get-EvgaPackageHash $log);GateSha256=(Get-EvgaPackageHash $gate);GpuMarkerSha256=(Get-EvgaPackageHash $gpu)}
}
function Assert-EvgaPackage([string]$Directory){
 $attrs=[IO.File]::GetAttributes($Directory)
 if(($attrs -band [IO.FileAttributes]::ReparsePoint) -or !($attrs -band [IO.FileAttributes]::Directory)){throw 'Invalid package root'}
 $m=Get-Content -LiteralPath (Join-Path $Directory 'package.json') -Raw | ConvertFrom-Json
 $p=Get-EvgaPackageProfile $m.Layout
 if(($m.AttemptSlot -isnot [int] -and $m.AttemptSlot -isnot [long]) -or $m.RequiresFreshShiftG -isnot [bool] -or $m.HardwareTested -isnot [bool]){throw 'Invalid package field types'}
 if($m.Schema -cne 'evga-attended-package-v1' -or $m.ProfileId -cne $p.id -or $m.AttemptSlot -lt 8 -or $m.AttemptSlot -gt 99 -or $m.Run -cne ('EVP{0:D2}' -f $m.AttemptSlot) -or $m.EfiPath -cne ('\EFI\WindowsGPUUnlock\'+$m.Run+'\UNLOCK.EFI') -or $m.RequiresFreshShiftG -ne $true -or $m.HardwareTested -ne $false){throw 'Invalid package identity'}
 foreach($file in $m.Files){
  if($file.Name -cnotin @('UNLOCK.EFI','profile.json','assets.json','source-manifest.json','pe-verification.json','platform-config.h','LICENSE','README.md')){throw 'Unknown package file'}
  $path=Get-AuditCheckedPath $Directory @($file.Name) $false
  if((Get-Item -LiteralPath $path).Length -ne $file.Bytes -or (Get-EvgaPackageHash $path) -cne $file.Sha256){throw 'Package file changed'}
 }
 if($m.Files.Count -ne 8 -or @($m.Files.Name | Select-Object -Unique).Count -ne 8){throw 'Incomplete package inventory'}
 $pe=Get-Content -LiteralPath (Join-Path $Directory 'pe-verification.json') -Raw | ConvertFrom-Json
 $exe=@($m.Files | Where-Object Name -CEQ 'UNLOCK.EFI')[0]
 if($pe.sha256 -cne $exe.Sha256 -or $pe.machine -cne 'AMD64' -or $pe.subsystem -ne 10 -or $pe.imports -ne 0 -or $pe.iat -ne 0 -or $pe.delay_imports -ne 0 -or !$pe.nx -or !$pe.dynamic_base){throw 'PE verification differs'}
 $source=Get-Content -LiteralPath (Join-Path $Directory 'source-manifest.json') -Raw | ConvertFrom-Json
 if($source.Mode -cne 'evga-attended-reusable-package' -or $source.AttemptSlot -ne $m.AttemptSlot -or $source.Layout -cne $m.Layout -or $source.WindowsEspGuid -cne $m.WindowsEspGuid -or $source.UnlockPartitionGuid -cne $m.UnlockPartitionGuid -or $source.EfiSha256 -cne $exe.Sha256 -or $source.CanUnlock -ne $true){throw 'Package and compiled source manifest differ'}
 return $m
}
function Get-EvgaFirmwareSnapshot([scriptblock]$Read){
 $order=& $Read 'BootOrder'
 if(!$order -or $order.Attributes -ne 7){throw 'Expected BootOrder attributes 7'}
 [byte[]]$bytes=[Convert]::FromBase64String($order.DataBase64)
 if(!$bytes.Length -or $bytes.Length%2){throw 'Malformed BootOrder'}
 $entries=@();$names=@()
 for($i=0;$i -lt $bytes.Length;$i+=2){
  $name='Boot{0:X4}' -f [BitConverter]::ToUInt16($bytes,$i)
  if($name -in $names){throw 'Duplicate BootOrder entry'};$names+=,$name
  $value=& $Read $name;if(!$value){throw 'BootOrder references a missing entry'}
  $entries+=,[pscustomobject]@{Name=$name;Value=$value}
 }
 [pscustomobject]@{BootOrder=$order;BootEntries=$entries}
}
function Get-EvgaOptionDescription($Value){
 [byte[]]$data=[Convert]::FromBase64String($Value.DataBase64)
 if($data.Length -lt 8){return ''};$end=6
 while($end+1 -lt $data.Length -and ($data[$end] -ne 0 -or $data[$end+1] -ne 0)){$end+=2}
 if($end+1 -ge $data.Length){return ''}
 [Text.Encoding]::Unicode.GetString($data,6,$end-6)
}
function Test-EvgaReviewedUsbOrderRemoval([string]$Name,[uint32]$Attributes,[string]$Digest,[bool]$Present,[bool]$Equal){
 return $Name -ceq 'Boot000D' -and $Attributes -eq 7 -and $Digest -ceq 'ac1eb792d44382686770c715aacae3681d572ab8f61cc7e6dc9d71fa0fab94ee' -and (!$Present -or $Equal)
}
function Get-EvgaPreservedCleanupSnapshot($Before,[scriptblock]$Read){
 $now=Get-EvgaFirmwareSnapshot $Read
 $names=@($now.BootEntries | ForEach-Object {$_.Name});$kept=@()
 $fallback=@('UEFI:CD/DVD Drive','UEFI:Removable Device','UEFI:Network Device')
 foreach($entry in $Before.BootEntries){
  $value=& $Read $entry.Name
  if($entry.Name -notin $names){
   if($null -eq $value -and (Get-EvgaOptionDescription $entry.Value) -cin $fallback){continue}
   # Exact reviewed removable entry absent after the USB-free EVP11 boot.
   # This does not authorize deleting/replacing a present or different USB entry.
   $sha=[Security.Cryptography.SHA256]::Create()
   try{$digest=([BitConverter]::ToString($sha.ComputeHash([Convert]::FromBase64String($entry.Value.DataBase64)))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
   if(Test-EvgaReviewedUsbOrderRemoval $entry.Name $entry.Value.Attributes $digest ($null -ne $value) (Test-FirmwareValueEqual $value $entry.Value)){continue}
  }
  if(!(Test-FirmwareValueEqual $value $entry.Value)){throw 'An existing boot entry changed; cleanup refused'}
  $kept+=,$entry.Name
 }
 if($names.Count -lt $kept.Count){throw 'BootOrder lost an existing entry'}
 for($i=0;$i -lt $kept.Count;$i++){if($names[$i] -cne $kept[$i]){throw 'Existing BootOrder sequence changed'}}
 foreach($entry in @($now.BootEntries | Select-Object -Skip $kept.Count)){
  if((Get-EvgaOptionDescription $entry.Value) -cnotin $fallback){throw 'Unreviewed additional boot entry; cleanup refused'}
 }
 return $now
}
function Remove-EvgaPackageOption($Attempt,[scriptblock]$Read,[scriptblock]$Write,[scriptblock]$Save){
 if($Attempt.OptionName -cnotmatch '^Boot7E[0-9A-F]{2}$' -or $Attempt.Option.Attributes -ne 7){throw 'Invalid owned option'}
 $snapshot=Get-EvgaPreservedCleanupSnapshot $Attempt $Read
 if($snapshot.BootEntries.Name -ccontains $Attempt.OptionName -or $Attempt.BootEntries.Name -ccontains $Attempt.OptionName){throw 'Temporary option is referenced by BootOrder; no cleanup attempted'}
 $next=& $Read 'BootNext';$owned=ConvertTo-FirmwareValue ([BitConverter]::GetBytes([uint16]$Attempt.OptionNumber))
 if($null -ne $next -and !(Test-FirmwareValueEqual $next $owned)){throw 'Another pending BootNext exists; no cleanup attempted'}
 $current=& $Read $Attempt.OptionName
 if($null -ne $current -and !(Test-FirmwareValueEqual $current $Attempt.Option)){throw 'Owned option changed'}
 & $Save $snapshot
 Assert-SharedEspFirmwareSnapshot $snapshot $Read
 if($null -ne $next){
  if(!(Test-FirmwareValueEqual (& $Read 'BootNext') $owned)){throw 'BootNext raced; stop'}
  & $Write 'BootNext' $null
 }
 if($null -ne (& $Read 'BootNext')){throw 'BootNext not clear after cancellation'}
 if($null -ne $current){
  Assert-SharedEspFirmwareSnapshot $snapshot $Read
  if($null -ne (& $Read 'BootNext')){throw 'BootNext appeared before option deletion; stop'}
  if(!(Test-FirmwareValueEqual (& $Read $Attempt.OptionName) $Attempt.Option)){throw 'Owned option raced; stop'}
  & $Write $Attempt.OptionName $null
 }
 Assert-SharedEspFirmwareSnapshot $snapshot $Read
 if($null -ne (& $Read $Attempt.OptionName) -or $null -ne (& $Read 'BootNext')){throw 'Cleanup readback failed'}
}
function Get-EvgaCheckedDirectory([string]$Root,[string[]]$Components){
 $attrs=[IO.File]::GetAttributes($Root)
 if(($attrs -band [IO.FileAttributes]::ReparsePoint) -or !($attrs -band [IO.FileAttributes]::Directory)){throw 'Invalid root directory'}
 $path=$Root
 foreach($name in $Components){
  if($name -notmatch '^[A-Za-z0-9._-]+$' -or $name -in @('.','..')){throw 'Invalid directory component'}
  $candidate=Join-Path $path $name
  if(!(Test-Path -LiteralPath $candidate)){[void][IO.Directory]::CreateDirectory($candidate)}
  $path=Get-AuditCheckedPath $path @($name) $true
 }
 return $path
}
function Get-EvgaAssetSpecifications([string]$SpecFile){
 $specs=@(Get-Content -LiteralPath $SpecFile -Raw | ConvertFrom-Json)
 if($specs.Count -eq 1 -and $specs[0].Name -is [array]){
  $flat=$specs[0];$specs=@()
  for($i=0;$i -lt $flat.Name.Count;$i++){$specs+=,[pscustomobject]@{Name=$flat.Name[$i];Bytes=$flat.Bytes[$i];Sha256=$flat.Sha256[$i]}}
 }
 if($specs.Count -ne 5 -or @($specs.Name | Select-Object -Unique).Count -ne 5){throw 'Expected five unique asset specifications'}
 return $specs
}
function Assert-EvgaAssetDirectory([string]$Directory,[string]$SpecFile){
 $specs=@(Get-EvgaAssetSpecifications $SpecFile)
 foreach($spec in $specs){
  if($spec.Name -cnotin @('booter_load_ga102.bin','v67-ga102-610.43.03.bin','gsp_ga10x.bin','gsp_boot_ga102.bin','gsp_boot_desc_ga102.bin')){throw 'Invalid asset name'}
  $file=Get-AuditCheckedPath $Directory @($spec.Name) $false
  if((Get-Item -LiteralPath $file).Length -ne $spec.Bytes -or (Get-EvgaPackageHash $file) -cne $spec.Sha256){throw "Firmware asset differs: $($spec.Name)"}
 }
 foreach($name in @('NVIDIA-LICENSE','NOTICE.md')){[void](Get-AuditCheckedPath $Directory @($name) $false)}
 return $specs
}
function Copy-EvgaPackageFile([string]$Source,[string]$Target,[string]$Expected){
 if((Get-EvgaPackageHash $Source) -cne $Expected){throw 'Copy source changed'}
 if(Test-Path -LiteralPath $Target){if((Get-EvgaPackageHash $Target) -cne $Expected){throw 'Existing destination differs; never overwrite'};return}
 $inputStream=$null;$outputStream=$null
 try {$inputStream=[IO.File]::OpenRead($Source);$outputStream=[IO.File]::Open($Target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$inputStream.CopyTo($outputStream);$outputStream.Flush($true)}finally{if($outputStream){$outputStream.Dispose()};if($inputStream){$inputStream.Dispose()}}
 if((Get-EvgaPackageHash $Target) -cne $Expected){throw 'Copy readback differs; preserve partial state'}
}
function Assert-EvgaReviewedEvp10Retry([string]$StateDirectory,$Attempt,$Result){
 if($Attempt.AttemptSlot -ne 10 -or $Attempt.Run -cne 'EVP10' -or $Attempt.Layout -cne 'SharedEsp' -or $Result.AttemptSlot -ne 10 -or $Result.Success -ne $false -or $Result.Cancelled -ne $false){throw 'Not the reviewed EVP10 refusal'}
 $cleanup=Get-Content -LiteralPath (Join-Path $StateDirectory 'reviewed-evp10-cleanup-result.json') -Raw | ConvertFrom-Json
 if($cleanup.schema -cne 'reviewed-evp10-cleanup-v1' -or $cleanup.cleanup_completed -ne $true -or $cleanup.failed_attempt_preserved -ne $true -or $cleanup.usb_entry_preserved -ne $true -or $cleanup.boot_order_written -ne $false -or $cleanup.gpu_accessed -ne $false -or $cleanup.rearm_authorized -ne $false){throw 'Reviewed cleanup confirmation missing'}
 $hashes=@{
  'EVP10.TXT'='bbd5c27a727a9835f7ff886551bebf298509759e211bc3dda66c81158b96eaa5'
  'EVP10.ARMED'='7414f7145d718a42262f68840503473f597771cda90082a09d853616daa53264'
  'EVGA-UNLOCK10.ARMED'='c81ed25e0ed465df4a3813ac40781f6d0fb46f06fe899d4ed4fb6b4666c70aca'
 }
 foreach($name in $hashes.Keys){if((Get-EvgaPackageHash (Join-Path $Attempt.TargetRoot ('RTX3090-UNLOCK/'+$name))) -cne $hashes[$name]){throw 'Reviewed failed evidence changed'}}
}
function Get-EvgaProfileNextSlot([string]$StateDirectory,$Installation,[switch]$ReviewedEvp10Retry){
 $files=@(Get-ChildItem -LiteralPath $StateDirectory -File -Filter 'attempt-*.json')
 $slots=@()
 foreach($file in $files){if($file.Name -cnotmatch '^attempt-([0-9]{2})\.json$'){throw 'Unexpected attempt journal'};$slots+=,[int]$Matches[1]}
 $last=[int]$Installation.SeedSlot
 foreach($slot in @($slots | Sort-Object)){if($slot -ne $last+1){throw 'Attempt journal gap or duplicate'};$last=$slot}
 if($last -gt [int]$Installation.SeedSlot){
  $attempt=Get-Content -LiteralPath (Join-Path $StateDirectory ('attempt-{0:D2}.json' -f $last)) -Raw | ConvertFrom-Json
  $resultPath=Join-Path $StateDirectory ('result-{0:D2}.json' -f $last)
  if(!(Test-Path -LiteralPath $resultPath)){throw 'Collect the previous attempt before rearming. If it failed/hung, report it; never reset its markers.'}
  $result=Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
  if($ReviewedEvp10Retry){
   if($last -ne 10){throw 'Reviewed retry applies only after EVP10'}
   Assert-EvgaReviewedEvp10Retry $StateDirectory $attempt $result
  }else{
   if($result.Success -ne $true -or $result.AttemptSlot -ne $last){throw 'Prior attempt failed, was cancelled or remains incomplete; rearm refused'}
   $proof=Assert-EvgaPackageResult $attempt.TargetRoot $last $attempt.Run
   if($proof.LogSha256 -cne $result.LogSha256 -or $proof.GateSha256 -cne $result.GateSha256 -or $proof.GpuMarkerSha256 -cne $result.GpuMarkerSha256){throw 'Prior evidence changed after collection'}
   if($result.CleanupCompleted -ne $true){
    $cleanup=Get-Content -LiteralPath (Join-Path $StateDirectory ('cleanup-{0:D2}-result.json' -f $last)) -Raw | ConvertFrom-Json
    if($cleanup.Schema -cne 'evga-attended-cleanup-v1' -or $cleanup.AttemptSlot -ne $last -or $cleanup.CleanupCompleted -ne $true -or $cleanup.LogSha256 -cne $proof.LogSha256 -or $cleanup.GateSha256 -cne $proof.GateSha256 -or $cleanup.GpuMarkerSha256 -cne $proof.GpuMarkerSha256 -or $cleanup.OptionName -cne $attempt.OptionName){throw 'Missing matching separate cleanup receipt'}
   }
  }
 }
 if($last -ge 99){throw 'Attempt slots exhausted'}
 if($ReviewedEvp10Retry -and $last -ne 10){throw 'No reviewed failed slot 10'}
 return $last+1
}
