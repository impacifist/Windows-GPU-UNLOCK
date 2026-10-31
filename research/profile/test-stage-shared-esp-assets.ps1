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
 if($LiteralPath -like '*bootmgfw.efi'){return [pscustomobject]@{Hash='2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec'}}
 if($LiteralPath -like '*limine_x64.efi'){return [pscustomobject]@{Hash='a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462'}}
 Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath -Algorithm $Algorithm
}
Set-Content -LiteralPath (Join-Path $testRoot 'EFI/EVGA-SHARED-AUDIT.EFI') -Value 'prior audit preserved'
$prior=(Get-FileHash -LiteralPath (Join-Path $testRoot 'EFI/EVGA-SHARED-AUDIT.EFI')).Hash
$stage=Join-Path $PSScriptRoot 'stage-shared-esp-assets.ps1';$target=Join-Path $testRoot 'EFI/EVGA-SHARED-ASSETS.EFI'
$global:assetTestFreeSpace=1MB
$refused=$false
try {& $stage -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1' -Assets (Join-Path $repo 'research/profile/build/evga-assets/output')}catch{$refused=$true}
if(!$refused -or (Test-Path (Join-Path $testRoot 'RTX3090-UNLOCK'))){throw 'Low space not refused before writes'}
$global:assetTestFreeSpace=200MB
& $stage -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1' -Assets (Join-Path $repo 'research/profile/build/evga-assets/output') -WhatIf
if((Test-Path -LiteralPath $target) -or (Test-Path (Join-Path $testRoot 'RTX3090-UNLOCK'))){throw 'WhatIf created files'}
& $stage -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1' -Assets (Join-Path $repo 'research/profile/build/evga-assets/output')
if(!(Test-Path -LiteralPath $target)){throw 'Audit target missing'}
$refused=$false;try {& $stage -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1' -Assets (Join-Path $repo 'research/profile/build/evga-assets/output')}catch{$refused=$true}
if(!$refused){throw 'Existing target not refused'}
if((Get-FileHash -LiteralPath (Join-Path $testRoot 'EFI/EVGA-SHARED-AUDIT.EFI')).Hash -ne $prior){throw 'Prior audit changed'}
'Full assets staging script passed WhatIf, fixture copy and no-overwrite refusal; inventory and boot-file hashes mocked.'
