# Tests the actual stage script with a filesystem fixture and mocked storage/boot hashes.
$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$testRoot=Join-Path $repo ('research/profile/build/shared-stage-full-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $testRoot 'EFI/Microsoft/Boot'),(Join-Path $testRoot 'EFI/limine') -Force | Out-Null
Set-Content -LiteralPath (Join-Path $testRoot 'EFI/Microsoft/Boot/bootmgfw.efi') -Value 'Windows fixture'
Set-Content -LiteralPath (Join-Path $testRoot 'EFI/limine/limine_x64.efi') -Value 'Limine fixture'
function Get-Partition {[pscustomobject]@{IsSystem=$true;Guid='c36c3e86-02aa-4f76-af1b-96e327f9d1c1'}}
$global:assetTestFreeSpace=200MB
function Get-Volume {param([Parameter(ValueFromPipeline)]$InputObject) process {[pscustomobject]@{FileSystem='FAT32';Path=$testRoot;SizeRemaining=$global:assetTestFreeSpace}}}
function Get-FileHash {
 param([string]$LiteralPath,[string]$Algorithm='SHA256')
 if($LiteralPath -like '*INTG01.ARMED'){return [pscustomobject]@{Hash='e2fe1103ef03fe341df9b9b935ac13fd70cb6c0c792d4ce7c4228b2c248955c2'}}
 if($LiteralPath -like '*INTG01.TXT'){return [pscustomobject]@{Hash=if($global:qualPriorMismatch){'0'*64}else{'f73b4001af51857f036460c3a6ee7139c3ffcea133f59b0b74d3e1eba7664d6c'}}}
 if($LiteralPath -like '*bootmgfw.efi'){return [pscustomobject]@{Hash='2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'}}
 if($LiteralPath -like '*limine_x64.efi'){return [pscustomobject]@{Hash='a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462'}}
 Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath -Algorithm $Algorithm
}
Set-Content -LiteralPath (Join-Path $testRoot 'EFI/EVGA-SHARED-AUDIT.EFI') -Value 'prior audit preserved'
$prior=(Get-FileHash -LiteralPath (Join-Path $testRoot 'EFI/EVGA-SHARED-AUDIT.EFI')).Hash
[void](New-Item -ItemType Directory -Path (Join-Path $testRoot 'RTX3090-UNLOCK/ASSETS'))
Copy-Item (Join-Path $repo 'research/profile/build/evga-assets/output/*') (Join-Path $testRoot 'RTX3090-UNLOCK/ASSETS')
Set-Content -LiteralPath (Join-Path $testRoot 'RTX3090-UNLOCK/INTG01.ARMED') -Value 'mock prior marker'
Set-Content -LiteralPath (Join-Path $testRoot 'RTX3090-UNLOCK/INTG01.TXT') -Value 'mock prior log'
$stage=Join-Path $PSScriptRoot 'stage-shared-esp-qualification.ps1';$target=Join-Path $testRoot 'EFI/EVGA-SHARED-QUALIFICATION.EFI'
$global:qualPriorMismatch=$true
$refused=$false
try {& $stage -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1' -WhatIf}catch{$refused=$true}
if(!$refused -or (Test-Path (Join-Path $testRoot 'RTX3090-UNLOCK/QUAL01.ARMED'))){throw 'Wrong prior log hash not refused'}
$global:qualPriorMismatch=$false
$global:assetTestFreeSpace=1MB
$refused=$false
try {& $stage -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'}catch{$refused=$true}
if(!$refused -or (Test-Path (Join-Path $testRoot 'RTX3090-UNLOCK/QUAL01.ARMED'))){throw 'Low space not refused before writes'}
$global:assetTestFreeSpace=200MB
& $stage -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1' -WhatIf
if((Test-Path -LiteralPath $target) -or (Test-Path (Join-Path $testRoot 'RTX3090-UNLOCK/QUAL01.ARMED'))){throw 'WhatIf created files'}
& $stage -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'
if(!(Test-Path -LiteralPath $target)){throw 'Audit target missing'}
$refused=$false;try {& $stage -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'}catch{$refused=$true}
if(!$refused){throw 'Existing target not refused'}
if((Get-FileHash -LiteralPath (Join-Path $testRoot 'EFI/EVGA-SHARED-AUDIT.EFI')).Hash -ne $prior){throw 'Prior audit changed'}
'Full retention staging script passed WhatIf, fixture copy and no-overwrite refusal; inventory and boot-file hashes mocked.'

$receiptBytes=[IO.File]::ReadAllBytes((Join-Path $testRoot 'RTX3090-UNLOCK/QUAL01.ARMED'))
if($receiptBytes.Length -ne 512 -or [Text.Encoding]::ASCII.GetString($receiptBytes,0,16) -ne "PENDING QUAL01`r`n"){throw 'Pending receipt differs'}



Set-Content -LiteralPath (Join-Path $testRoot 'RTX3090-UNLOCK/QUAL01.TXT') -Value "evga-text-stop-ready-windows status=0x0000000000000000`r`ncapture-complete=1"
$result=& (Join-Path $PSScriptRoot 'read-shared-esp-qualification.ps1') -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -OutputDirectory ($testRoot+'-results') | ConvertFrom-Json
if(!$result.esp_read_only -or $result.files.Count -ne 2 -or $result.events.Count -ne 2 -or $result.gpu_marker_present){throw 'Qualification evidence reader failed'}
