# Filesystem fixture only. Approval/storage/boot hashes mocked; no physical media touched.
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$fixture=Join-Path $repo ('research/profile/build/shared-unlock-stage-'+[guid]::NewGuid().ToString('N'))
$global:unlockEsp=Join-Path $fixture 'esp';$usb=Join-Path $fixture 'usb'
if(Test-Path X:\){throw 'X: already exists'}
[void](New-Item -ItemType Directory -Path (Join-Path $global:unlockEsp 'EFI/Microsoft/Boot'),(Join-Path $global:unlockEsp 'EFI/limine'),(Join-Path $global:unlockEsp 'RTX3090-UNLOCK/ASSETS'),(Join-Path $usb 'RTX3090-UNLOCK') -Force)
Set-Content (Join-Path $global:unlockEsp 'EFI/Microsoft/Boot/bootmgfw.efi') 'Windows fixture'
Set-Content (Join-Path $global:unlockEsp 'EFI/limine/limine_x64.efi') 'Limine fixture'
Copy-Item (Join-Path $repo 'research/profile/build/evga-assets/output/*') (Join-Path $global:unlockEsp 'RTX3090-UNLOCK/ASSETS')
foreach($n in 17..22){Copy-Item (Join-Path $repo ('research/profile/build/evga-first-live-result/EVGA{0:D2}.TXT' -f $n)) (Join-Path $usb 'RTX3090-UNLOCK')}
foreach($name in @('EVGA-UNLOCK.ARMED','EVGA-UNLOCK2.ARMED','EVGA-UNLOCK3.ARMED','EVGA-UNLOCK4.ARMED','EVGA-UNLOCK5.ARMED','EVGA-UNLOCK6.ARMED')){Set-Content (Join-Path $usb ('RTX3090-UNLOCK/'+$name)) 'prior marker fixture'}
$events=@('evga-text-stop-return','evga-virtual-text-stop-return','evga-graphics-stop-return','evga-edid-stop-return','evga-edid-sibling1-return','evga-edid-sibling2-return','evga-nvidia-children-stop-return','evga-nvidia-parent-stop-return','evga-shared-claim-return','evga-shared-close-return','evga-state-prom-hash','evga-runtime-config','evga-runtime-bar','evga-memory-disable-return','evga-text-stop-ready-windows','evga-decode-command-restored','evga-decode-command-restored')
$qual=Join-Path $global:unlockEsp 'RTX3090-UNLOCK/QUAL01.TXT'
Set-Content -LiteralPath $qual -Value (($events | ForEach-Object {$_+' status=0x0000000000000000'})+@('capture-complete=1','capture-complete=1'))
Set-Content (Join-Path $global:unlockEsp 'RTX3090-UNLOCK/QUAL01.ARMED') 'entry fixture'
Set-Content (Join-Path $global:unlockEsp 'RTX3090-UNLOCK/QUAL01-GPU.ARMED') 'gpu fixture'
$global:unlockApproval=[ordered]@{Schema='shared-esp-unlock-qualification-v1';Approved=$false;WindowsReturned=$true;UsbRestored=$true;MarkerSha256=(Get-FileHash (Join-Path $global:unlockEsp 'RTX3090-UNLOCK/QUAL01.ARMED')).Hash;GpuMarkerSha256=(Get-FileHash (Join-Path $global:unlockEsp 'RTX3090-UNLOCK/QUAL01-GPU.ARMED')).Hash;LogSha256=(Get-FileHash $qual).Hash}
function Get-Content {param([string]$LiteralPath,[switch]$Raw) if($LiteralPath -like '*shared-unlock-qualification.json'){$global:unlockApproval | ConvertTo-Json}else{Microsoft.PowerShell.Management\Get-Content -LiteralPath $LiteralPath -Raw:$Raw}}
function Get-Partition {param([string]$DriveLetter) if($DriveLetter){[pscustomobject]@{DiskNumber=12;IsSystem=$false;IsBoot=$false}}else{[pscustomobject]@{IsSystem=$true;Guid='c36c3e86-02aa-4f76-af1b-96e327f9d1c1'}}}
function Get-Volume {param([Parameter(ValueFromPipeline)]$InputObject,[string]$DriveLetter) process {if($DriveLetter){[pscustomobject]@{FileSystem='FAT32'}}else{[pscustomobject]@{FileSystem='FAT32';Path=$global:unlockEsp;SizeRemaining=200MB}}}}
function Get-Disk {param($Number) [pscustomobject]@{BusType='USB'}}
function Get-FileHash {param([string]$LiteralPath,[string]$Algorithm='SHA256')
 if($LiteralPath -like '*bootmgfw.efi'){return [pscustomobject]@{Hash='2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'}}
 if($LiteralPath -like '*limine_x64.efi'){return [pscustomobject]@{Hash='a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462'}}
 Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath -Algorithm $Algorithm
}
& subst X: $usb
if($LASTEXITCODE -ne 0){throw 'Cannot create fixture drive'}
try {
 $stage=Join-Path $PSScriptRoot 'stage-shared-esp-unlock.ps1';$target=Join-Path $global:unlockEsp 'EFI/EVGA-SHARED-UNLOCK.EFI'
 $refused=$false;try {& $stage -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -Drive X}catch{$refused=$true}
 if(!$refused -or (Test-Path 'X:/RTX3090-UNLOCK/EVGA-UNLOCK7.ARMED') -or (Test-Path $target)){throw 'Unapproved qualification was not blocked before writes'}
 $global:unlockApproval.Approved=$true
 $saved=$global:unlockApproval.LogSha256;$global:unlockApproval.LogSha256='0'*64
 $refused=$false;try {& $stage -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -Drive X -WhatIf}catch{$refused=$true}
 if(!$refused){throw 'Wrong reviewed qualification hash accepted'};$global:unlockApproval.LogSha256=$saved
 & $stage -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -Drive X -WhatIf
 if((Test-Path $target) -or (Test-Path 'X:/RTX3090-UNLOCK/EVGA-UNLOCK7.ARMED')){throw 'WhatIf wrote files'}
 & $stage -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -Drive X
 if(!(Test-Path $target) -or !(Test-Path 'X:/RTX3090-UNLOCK/EVGA-UNLOCK7.ARMED') -or !(Test-Path (Join-Path $global:unlockEsp 'RTX3090-UNLOCK/USB-HISTORY-SULK01/EVGA22.TXT'))){throw 'Missing cross-volume reservation/snapshot/EFI'}
 $refused=$false;try {& $stage -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -Drive X}catch{$refused=$true}
 if(!$refused){throw 'Repeat staging accepted'}
 $refused=$false;try {Get-SharedUnlockHistory 'X:/'}catch{$refused=$true}
 if(!$refused){throw 'USB reservation did not prevent a later unsupported USB attempt'}
 Set-Content -LiteralPath (Join-Path $global:unlockEsp 'RTX3090-UNLOCK/SULK01.TXT') -Value "evga-unlock-slot-07 status=0x0000000000000000`r`ncapture-complete=1"
 $result=& (Join-Path $PSScriptRoot 'read-shared-esp-unlock.ps1') -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -AttemptSlot 7 -OutputDirectory (Join-Path $fixture 'results-no-gpu') | ConvertFrom-Json
 if($result.gpu_marker_present -or $result.files.Count -ne 2){throw 'Collector confused USB reservation with ESP runtime marker'}
 Set-Content -LiteralPath (Join-Path $global:unlockEsp 'RTX3090-UNLOCK/EVGA-UNLOCK7.ARMED') -Value 'runtime marker fixture'
 $result=& (Join-Path $PSScriptRoot 'read-shared-esp-unlock.ps1') -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -AttemptSlot 7 -OutputDirectory (Join-Path $fixture 'results-with-gpu') | ConvertFrom-Json
 if(!$result.gpu_marker_present -or $result.files.Count -ne 3){throw 'Collector omitted numbered runtime marker'}
 'Shared unlock stage fixture passed approval/hash gates, WhatIf, two-volume reservation/snapshot and repeat refusal. Storage and approval mocked; firmware not executed.'
}finally{& subst X: /d}
