# GPL-2.0-only. Explicit dedicated ESP; no partition operations or automatic reboot.
param([string]$Assets,[switch]$Register,[switch]$EnableAutomatic,[switch]$ConfirmedSuccess,[switch]$RestoreWindowsFirst)
$ErrorActionPreference='Stop'
if(@(@($Register,$EnableAutomatic,$RestoreWindowsFirst) | Where-Object {$_}).Count -gt 1){throw 'Choose one action'}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'install/automatic-policy.ps1')
. (Join-Path $PSScriptRoot 'install/headless-package.ps1')
$stateFile=Join-Path $PSScriptRoot 'install/headless-state.json'
$power='HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Power'
$entryName='GPU Headless UNLOCK - EXPERIMENTAL';$entryPath='\EFI\WindowsGPUUnlock\HEADLESS.EFI'
function Bcd([string[]]$a){$v=@(& bcdedit @a);if($LASTEXITCODE -ne 0){throw ('BCDEdit failed: '+($a -join ' '))};return ($v -join "`r`n")}
function Order([string]$text){
 $m=[regex]::Match($text,'(?ms)^displayorder\s+(.*?)(?=\r?\n\S|\z)');if(!$m.Success){throw 'No firmware order'}
 $ids=@([regex]::Matches($m.Groups[1].Value,'\{(?:bootmgr|[0-9a-f-]{36})\}') | ForEach-Object {$_.Value})
 if($ids -notcontains '{bootmgr}' -or @($ids | Select-Object -Unique).Count -ne $ids.Count){throw 'Invalid firmware order'};return $ids
}
function SetOrder([string[]]$ids){[void](Bcd (@('/set','{fwbootmgr}','displayorder')+$ids))}
function Ids([string]$text){return @([regex]::Matches($text,'(?mi)^identifier\s+(\{[0-9a-f-]{36}\})') | ForEach-Object {$_.Groups[1].Value})}
function ExistsBcd([string]$id){$null=& bcdedit /enum $id;return $LASTEXITCODE -eq 0}
function ReadHiber {
 $k=Get-Item -LiteralPath $power;$exists=$k.GetValueNames() -contains 'HiberbootEnabled'
 if($exists -and $k.GetValueKind('HiberbootEnabled') -ne 'DWord'){throw 'Unexpected Fast Startup registry value type'}
 return @{Exists=$exists;Value=$(if($exists){[int]$k.GetValue('HiberbootEnabled')}else{$null})}
}
function SetHiber($plan){
 if($plan.Exists){[void](New-ItemProperty -LiteralPath $power -Name HiberbootEnabled -PropertyType DWord -Value ([int]$plan.Value) -Force)}
 elseif((Get-Item -LiteralPath $power).GetValueNames() -contains 'HiberbootEnabled'){Remove-ItemProperty -LiteralPath $power -Name HiberbootEnabled -ErrorAction Stop}
 $r=ReadHiber;if($r.Exists -ne [bool]$plan.Exists -or ($r.Exists -and $r.Value -ne [int]$plan.Value)){throw 'Fast Startup readback failed'}
}
function SaveState($s){Write-HeadlessJson $stateFile $s}
if($RestoreWindowsFirst){
 # Recovery is independent of EFI, configuration, build outputs and ESP presence.
 $s=$null
 if(Test-Path -LiteralPath $stateFile){try{$s=Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json}catch{Write-Warning 'Saved state unreadable; Windows-first recovery still proceeds, but previous Fast Startup setting is unavailable'}}
 $order=Order (Bcd @('/enum','{fwbootmgr}'))
 Invoke-RecoveryActions -Actions @(
  {SetOrder (@('{bootmgr}')+@($order | Where-Object {$_ -ne '{bootmgr}'}))},
  {if($s -and $s.HiberbootBefore){SetHiber $s.HiberbootBefore}},
  {if($s){$s.Mode='windows-first';SaveState $s}}
 )
 if((Order (Bcd @('/enum','{fwbootmgr}')))[0] -ne '{bootmgr}'){throw 'Windows-first recovery verification failed'}
 Write-Output 'Windows first restored. Existing entries and files retained; no reboot.';return
}
if($EnableAutomatic -and !$ConfirmedSuccess){throw 'First verify headless unlock, Windows display recovery and Tensor performance; then pass -ConfirmedSuccess'}
$config=Assert-HeadlessLocalConfig $repo
$package=Assert-HeadlessPackage $repo
function Target {
 $p=Get-Partition -DriveLetter $config.UnlockDriveLetter;$disk=Get-Disk -Number $p.DiskNumber;$v=$p | Get-Volume
 $w=@(Get-Partition | Where-Object {$_.Guid -eq $config.WindowsPartitionGuid})
 if($w.Count -ne 1 -or !$w[0].IsSystem -or $p.IsSystem -or $p.Guid -ne $config.UnlockPartitionGuid -or $p.Guid -eq $config.WindowsPartitionGuid -or $p.Offset -ne $config.UnlockOffset -or $p.Size -ne $config.UnlockSize -or $p.Size -lt 1GB -or $disk.UniqueId -ne $config.UnlockDiskUniqueId -or $p.GptType -ne '{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}' -or $v.FileSystem -ne 'FAT32' -or !$v.Path){throw 'Configured dedicated ESP identity changed'}
 $wv=$w[0] | Get-Volume
 if(!$wv.Path -or !(Test-Path -LiteralPath (Join-Path $wv.Path 'EFI\Microsoft\Boot\bootmgfw.efi'))){throw 'Windows Boot Manager missing'}
 $board=Get-CimInstance Win32_BaseBoard;$bios=Get-CimInstance Win32_BIOS
 $identity=Get-HeadlessProfileIdentity $package.HardwareProfile
 if($board.Product -ne $identity.Board -or $bios.SMBIOSBIOSVersion -ne $identity.Bios){throw 'Unsupported firmware profile'}
 return $v.Path
}
$root=Target
if(Confirm-SecureBootUEFI){throw 'This unsigned experimental EFI requires Secure Boot disabled'}
$enc=Get-BitLockerVolume -MountPoint C:
if([int]$enc.VolumeStatus -ne 0 -or [int]$enc.ProtectionStatus -ne 0){throw 'C: must be fully decrypted and protection off; this tool never changes encryption settings'}
$child='unlock/build/experimental/entryLatchedSs1Full/internal/automatic/headless-research/RTX3090-EXPERIMENTAL-entryLatchedSs1Full-INTERNAL-AUTO-HEADLESS-RESEARCH.EFI'
$artifacts=@(
 @('boot/build/headless/HEADLESS.EFI','EFI\WindowsGPUUnlock\HEADLESS.EFI',$package.EfiSha256),
 @($child,'EFI\WindowsGPUUnlock\UNLOCK-HEADLESS.EFI',$package.ChildSha256)
)
$assetSpecs=@(Get-HeadlessAssetSpecs $repo)
function StageFile([string]$source,[string]$relative,[string]$hash){
 if((Get-FileHash -LiteralPath $source).Hash -ne $hash){throw 'Source file hash mismatch'}
 $dest=Join-Path $root $relative
 if(Test-Path -LiteralPath $dest){if((Get-FileHash -LiteralPath $dest).Hash -ne $hash){throw ('Different installed file; preserve it before upgrading: '+$relative)}}
 else{[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($dest));[IO.File]::Copy($source,$dest,$false)}
 if((Get-FileHash -LiteralPath $dest).Hash -ne $hash){throw 'Installed file readback mismatch'}
}
function CheckStaged {
 if((Target) -ne $root){throw 'Target changed'}
 $null=Assert-HeadlessPackage $repo
 foreach($a in $artifacts){if((Get-FileHash -LiteralPath (Join-Path $root $a[1])).Hash -ne $a[2]){throw 'Installed EFI changed'}}
 foreach($a in $assetSpecs){$p=Join-Path $root ('RTX3090-UNLOCK\ASSETS\'+$a.Name);if((Get-Item -LiteralPath $p).Length -ne $a.Bytes -or (Get-FileHash -LiteralPath $p).Hash -ne $a.Sha256){throw 'Installed asset changed'}}
}
if(!$EnableAutomatic){
 if(!$Assets){throw 'Specify the directory containing the five required local firmware assets'}
 # Validate every input before staging any file.
 foreach($a in $assetSpecs){$p=Join-Path $Assets $a.Name;if((Get-Item -LiteralPath $p).Length -ne $a.Bytes -or (Get-FileHash -LiteralPath $p).Hash -ne $a.Sha256){throw ('Asset mismatch: '+$a.Name)}}
 foreach($a in $artifacts){StageFile (Join-Path $PSScriptRoot $a[0]) $a[1] $a[2]}
 foreach($a in $assetSpecs){StageFile (Join-Path $Assets $a.Name) ('RTX3090-UNLOCK\ASSETS\'+$a.Name) $a.Sha256}
}
CheckStaged
if(!$Register -and !$EnableAutomatic){Write-Output 'Headless files staged and verified only. Boot settings unchanged.';return}
$fw=Bcd @('/enum','{fwbootmgr}');$order=Order $fw;$firmware=Bcd @('/enum','firmware')
if($fw -match '(?mi)^bootsequence\s+'){throw 'Pending BootNext; refuse to change boot configuration'}
$existing=@{};foreach($id in $order){$existing[$id]=Bcd @('/enum',$id)}
$backup=Join-Path $env:ProgramData ('WindowsGPUUnlock\HeadlessBackups\'+(Get-Date -Format 'yyyyMMdd-HHmmssfff'))
[void][IO.Directory]::CreateDirectory($backup);[void](Bcd @('/export',(Join-Path $backup 'BCD-export')))
[IO.File]::WriteAllText((Join-Path $backup 'firmware-before.txt'),$firmware)
function CheckUnchanged([string]$except){foreach($id in $existing.Keys){if($id -ne $except -and (Bcd @('/enum',$id)) -cne $existing[$id]){throw 'Unrelated boot entry changed'}}}
function CheckEntry([string]$text){
 if($text -notmatch ('(?mi)^device\s+partition='+[regex]::Escape($config.UnlockDriveLetter)+':\s*$') -or $text -notmatch ('(?mi)^path\s+'+[regex]::Escape($entryPath)+'\s*$') -or $text -notmatch ('(?m)^description\s+'+[regex]::Escape($entryName)+'\s*$')){throw 'Headless entry identity mismatch'}
}
if($Register){
 if(Test-Path -LiteralPath $stateFile){throw 'Headless registration state already exists'}
 if($firmware -match ('(?m)^description\s+'+[regex]::Escape($entryName)+'\s*$')){throw 'An existing headless entry needs its original registration state; refusing duplicate/adoption'}
 $manager=Bcd @('/enum','{bootmgr}');$before=Ids (Bcd @('/enum','all'));$reg=@{Id=$null}
 $s=[ordered]@{EntryId=$null;Name=$entryName;Path=$entryPath;Mode='registering';BootOrderBefore=$order;BackupRoot=$backup;EfiSha256=$package.EfiSha256;ChildSha256=$package.ChildSha256;HiberbootBefore=$null;HardwareTested=$false}
 CheckStaged
 if((Bcd @('/enum','{fwbootmgr}')) -cne $fw -or (Bcd @('/enum','firmware')) -cne $firmware){throw 'Firmware changed before registration'}
 Invoke-AutomaticTransition -Steps @(
  {
   try{[void](Bcd @('/copy','{bootmgr}','/d',$entryName))}
   finally{$entries=@{};foreach($id in (Ids (Bcd @('/enum','all')))){if($before -notcontains $id){$entries[$id]=Bcd @('/enum',$id)}};$reg.Id=Resolve-NewBootCopy -BeforeIds $before -AfterEntries $entries -Source $manager -Name $entryName}
   $s.EntryId=$reg.Id;SaveState $s
  },
  {[void](Bcd @('/set',$reg.Id,'device',('partition='+$config.UnlockDriveLetter+':')))},
  {[void](Bcd @('/set',$reg.Id,'path',$entryPath))},
  {SetOrder (@($order)+@($reg.Id))}
 ) -Verify {
  if(((Order (Bcd @('/enum','{fwbootmgr}'))) -join ',') -cne ((@($order)+@($reg.Id)) -join ',')){throw 'Registration order mismatch'}
  if((Bcd @('/enum','{fwbootmgr}')) -match '(?mi)^bootsequence\s+'){throw 'BootNext changed'}
  CheckEntry (Bcd @('/enum',$reg.Id));CheckUnchanged '';CheckStaged
  $s.Mode='registered';SaveState $s
  $saved=Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
  if($saved.EntryId -ne $reg.Id -or $saved.Mode -ne 'registered'){throw 'Registration state readback failed'}
  [IO.File]::WriteAllText((Join-Path $backup 'firmware-after.txt'),(Bcd @('/enum','firmware')))
 } -Rollback {
  Invoke-RecoveryActions -Actions @({SetOrder $order},{if($reg.Id -and (ExistsBcd $reg.Id)){[void](Bcd @('/delete',$reg.Id))}})
  if(!$reg.Id -or (ExistsBcd $reg.Id) -or (Bcd @('/enum','{fwbootmgr}')) -cne $fw){throw "Registration rollback incomplete; inspect $backup"};CheckUnchanged ''
  if(Test-Path -LiteralPath $stateFile){$saved=Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json;if($saved.EntryId -ne $reg.Id){throw 'State ownership differs'};Remove-Item -LiteralPath $stateFile -ErrorAction Stop}
 }
 Write-Output "Registered $entryName at end of firmware menu; default order unchanged. Test via F8 first. Backup: $backup";return
}
$s=Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
if($s.EntryId -notmatch '^\{[0-9a-f-]{36}\}$' -or $s.Mode -notin 'registered','windows-first' -or $order -notcontains $s.EntryId -or $s.EfiSha256 -ne $package.EfiSha256 -or $s.ChildSha256 -ne $package.ChildSha256){throw 'Expected registered headless package state'}
$entry=Bcd @('/enum',$s.EntryId);CheckEntry $entry
$hiber=ReadHiber;$plan=Get-HeadlessPromotionOrder $order $s.EntryId
[IO.File]::Copy($stateFile,(Join-Path $backup 'state-before.json'),$false)
$recoveryPlan=[ordered]@{EntryId=$s.EntryId;BootOrderBefore=$order;HiberbootBefore=$hiber}
Write-HeadlessJson (Join-Path $backup 'recovery-plan.json') $recoveryPlan
$savedPlan=Get-Content -LiteralPath (Join-Path $backup 'recovery-plan.json') -Raw | ConvertFrom-Json
if($savedPlan.EntryId -ne $s.EntryId -or ($savedPlan.BootOrderBefore -join ',') -cne ($order -join ',') -or $savedPlan.HiberbootBefore.Exists -ne $hiber.Exists -or $savedPlan.HiberbootBefore.Value -ne $hiber.Value){throw 'Promotion recovery plan readback failed'}
CheckStaged
if((Bcd @('/enum','{fwbootmgr}')) -cne $fw -or (Bcd @('/enum','firmware')) -cne $firmware -or (ReadHiber | ConvertTo-Json -Compress) -cne ($hiber | ConvertTo-Json -Compress)){throw 'Boot/power state changed before promotion'}
Invoke-AutomaticTransition -Steps @(
 {
  $s.HiberbootBefore=$hiber;$s.Mode='promotion-preparing';SaveState $s
  $persisted=Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
  if($persisted.EntryId -ne $s.EntryId -or $persisted.Mode -ne 'promotion-preparing' -or $persisted.HiberbootBefore.Exists -ne $hiber.Exists -or $persisted.HiberbootBefore.Value -ne $hiber.Value){throw 'Previous Fast Startup setting was not persisted before mutation'}
 },
 {SetHiber @{Exists=$true;Value=0}},
 {SetOrder $plan}
) -Verify {
 $after=Bcd @('/enum','{fwbootmgr}');if(((Order $after) -join ',') -cne ($plan -join ',') -or $after -match '(?mi)^bootsequence\s+'){throw 'Automatic order mismatch'}
 CheckUnchanged '';CheckEntry (Bcd @('/enum',$s.EntryId));CheckStaged
 $r=ReadHiber;if(!$r.Exists -or $r.Value -ne 0){throw 'Fast Startup not disabled'}
 if(!$s.HiberbootBefore){$s.HiberbootBefore=$hiber};$s.Mode='automatic';SaveState $s
 $saved=Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
 if($saved.Mode -ne 'automatic' -or $saved.EntryId -ne $s.EntryId -or !$saved.HiberbootBefore){throw 'Automatic state readback failed'}
 [IO.File]::WriteAllText((Join-Path $backup 'firmware-after.txt'),(Bcd @('/enum','firmware')))
} -Rollback {
 Invoke-RecoveryActions -Actions @({SetOrder $order},{SetHiber $hiber})
 if((Bcd @('/enum','{fwbootmgr}')) -cne $fw){throw "Promotion rollback order mismatch; inspect $backup"};CheckUnchanged ''
 [IO.File]::Copy((Join-Path $backup 'state-before.json'),$stateFile,$true)
 if((Get-FileHash -LiteralPath $stateFile).Hash -ne (Get-FileHash -LiteralPath (Join-Path $backup 'state-before.json')).Hash){throw 'Promotion state rollback readback failed'}
}
Write-Output "Automatic order: Headless then Windows, remaining entries preserved. Fast Startup off. No reboot. Backup: $backup"
