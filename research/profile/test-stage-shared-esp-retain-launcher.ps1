$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$fixture=Join-Path $env:TEMP ('esp-launcher-test-'+[guid]::NewGuid())
$usb=Join-Path $fixture 'usb';$global:fixtureEsp=Join-Path $fixture 'esp';[void](New-Item -ItemType Directory -Path (Join-Path $global:fixtureEsp 'EFI'),$usb -Force)
Set-Content -LiteralPath (Join-Path $global:fixtureEsp 'EFI/EVGA-SHARED-RETAIN.EFI') -Value 'fixture'
if(Test-Path X:\){throw 'X: already exists; no test started'}
& subst X: $usb
if($LASTEXITCODE -ne 0){throw 'Cannot create test drive'}
try {
 function Get-Partition {param([string]$DriveLetter) if($DriveLetter){[pscustomobject]@{DiskNumber=12;IsSystem=$false;IsBoot=$false}}else{[pscustomobject]@{IsSystem=$true;Guid='c36c3e86-02aa-4f76-af1b-96e327f9d1c1'}}}
 function Get-Volume {param([Parameter(ValueFromPipeline=$true)]$InputObject,[string]$DriveLetter) process {if($DriveLetter){[pscustomobject]@{FileSystem='FAT32'}}else{[pscustomobject]@{FileSystem='FAT32';Path=$global:fixtureEsp}}}}
 function Get-Disk {param($Number) [pscustomobject]@{BusType='USB'}}
 $global:wrongAuditHash=$false
 function Get-FileHash {param([string]$LiteralPath) if($LiteralPath -eq (Join-Path $global:fixtureEsp 'EFI/EVGA-SHARED-RETAIN.EFI')){[pscustomobject]@{Hash=if($global:wrongAuditHash){'0'*64}else{(Get-Content (Join-Path $repo 'internal-boot/boot/build/shared-esp-retain/launcher-manifest.json') -Raw | ConvertFrom-Json).AuditEfiSha256}}}else{Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath}}
 $global:wrongAuditHash=$true
 $refused=$false
 try {& (Join-Path $PSScriptRoot 'stage-shared-esp-retain-launcher.ps1') -Drive X -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -Confirm:$false}catch{$refused=$true}
 if(!$refused -or (Test-Path X:\EFI)){throw 'Wrong audit hash was not refused before USB mutation'}
 $global:wrongAuditHash=$false
 & (Join-Path $PSScriptRoot 'stage-shared-esp-retain-launcher.ps1') -Drive X -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -WhatIf
 if(Test-Path X:\EFI){throw 'WhatIf created files'}
 & (Join-Path $PSScriptRoot 'stage-shared-esp-retain-launcher.ps1') -Drive X -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -Confirm:$false
 if(!(Test-Path X:\EFI\BOOT\BOOTX64.EFI)){throw 'Launcher missing'}
 $refused=$false
 try {& (Join-Path $PSScriptRoot 'stage-shared-esp-retain-launcher.ps1') -Drive X -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -Confirm:$false}catch{$refused=$true}
 if(!$refused){throw 'Existing EFI directory was not refused'}
 [IO.File]::WriteAllText('X:\EFI\BOOT\BOOTX64.EFI','original boot fixture')
 $original=(Get-FileHash -LiteralPath 'X:\EFI\BOOT\BOOTX64.EFI').Hash
 & (Join-Path $PSScriptRoot 'stage-shared-esp-retain-launcher.ps1') -Drive X -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -ReuseExisting -Confirm:$false
 if((Get-FileHash -LiteralPath 'X:\EFI\BOOT\BOOTX64.before-shared-retain.efi').Hash -ne $original){throw 'Backup mismatch'}
 & (Join-Path $PSScriptRoot 'restore-shared-esp-retain-launcher.ps1') -Drive X -OriginalSha256 $original -Confirm:$false
 if((Get-FileHash -LiteralPath 'X:\EFI\BOOT\BOOTX64.EFI').Hash -ne $original){throw 'Restore mismatch'}
 & (Join-Path $PSScriptRoot 'stage-shared-esp-retain-launcher.ps1') -Drive X -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -ReuseExisting -Confirm:$false
 & (Join-Path $PSScriptRoot 'restore-shared-esp-retain-launcher.ps1') -Drive X -OriginalSha256 $original -Confirm:$false
 [IO.File]::WriteAllText('X:\EFI\BOOT\BOOTX64.before-shared-retain.efi','different backup')
 $refused=$false
 try {& (Join-Path $PSScriptRoot 'stage-shared-esp-retain-launcher.ps1') -Drive X -WindowsEspGuid c36c3e86-02aa-4f76-af1b-96e327f9d1c1 -ReuseExisting -Confirm:$false}catch{$refused=$true}
 if(!$refused -or (Get-FileHash -LiteralPath 'X:\EFI\BOOT\BOOTX64.EFI').Hash -ne $original){throw 'Changed backup should be refused without changing boot file'}
 'USB staging mock passed: WhatIf, copy, and refusal; no physical ESP or USB accessed.'
} finally {& subst X: /D;Remove-Item -LiteralPath $fixture -Recurse -Force}
