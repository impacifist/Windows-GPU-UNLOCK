# GPL-2.0-only. Actual build/staging scripts on a local fixture; no hardware or real USB.
param([string]$Clang='clang',[string]$LldLink='lld-link')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$fixture=Join-Path $repo ('research/profile/build/prepare-integration-'+[guid]::NewGuid().ToString('N'))
if(Get-PSDrive Q -ErrorAction SilentlyContinue){throw 'Q already used'}
New-Item -ItemType Directory -Path (Join-Path $fixture 'RTX3090-UNLOCK'),(Join-Path $fixture 'EFI/BOOT') -Force | Out-Null
New-PSDrive -Name Q -PSProvider FileSystem -Root $fixture | Out-Null
try {
 function Get-Partition {param($DriveLetter) if($DriveLetter){[pscustomobject]@{DiskNumber=999;IsSystem=$false;IsBoot=$false}}else{[pscustomobject]@{Guid='c36c3e86-02aa-4f76-af1b-96e327f9d1c1';IsSystem=$true}}}
 function Get-Volume {param($DriveLetter) [pscustomobject]@{FileSystem='FAT32'}}
 function Get-Disk {param($Number) [pscustomobject]@{BusType='USB'}}
 $captures=Join-Path $fixture 'RTX3090-UNLOCK'
 foreach($i in 5..22){Copy-Item -LiteralPath (Join-Path $repo ('research/profile/build/evga-first-live-result/EVGA{0:D2}.TXT' -f $i)) -Destination $captures}
 foreach($name in @('EVGA-UNLOCK.ARMED','EVGA-UNLOCK2.ARMED','EVGA-UNLOCK3.ARMED','EVGA-UNLOCK4.ARMED','EVGA-UNLOCK5.ARMED','EVGA-UNLOCK6.ARMED','DIAG00.TXT','DIAG01.TXT')){Set-Content -LiteralPath (Join-Path $captures $name) -Value 'fixture'}
 Set-Content -LiteralPath (Join-Path $fixture 'EFI/BOOT/BOOTX64.EFI') -Value 'original fixture EFI'
 $before=(Get-FileHash -LiteralPath (Join-Path $fixture 'EFI/BOOT/BOOTX64.EFI')).Hash
 & (Join-Path $PSScriptRoot 'prepare-evga-attended.ps1') -Drive Q -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1' -Clang $Clang -LldLink $LldLink
 if((Get-FileHash -LiteralPath (Join-Path $fixture 'EFI/BOOT/BOOTX64.before-evga-text-stop.efi')).Hash -ne $before){throw 'Fixture EFI backup mismatch'}
 $manifest=Get-Content -LiteralPath (Join-Path $repo 'internal-boot/boot/build/evga-text-stop/manifest.json') -Raw | ConvertFrom-Json
 if($manifest.AttemptSlot -ne 7){throw 'Incorrect build slot'}
 if((Get-FileHash -LiteralPath (Join-Path $fixture 'EFI/BOOT/BOOTX64.EFI')).Hash.ToLowerInvariant() -ne $manifest.EfiSha256){throw 'Staged EFI hash mismatch'}
 if(Test-Path -LiteralPath (Join-Path $captures 'EVGA-UNLOCK7.ARMED')){throw 'Staging must not create an attempt marker'}
 foreach($i in 4..6){if(!(Test-Path -LiteralPath (Join-Path $captures "EVGA-UNLOCK$i.ARMED"))){throw 'Prior marker missing'}}
 'Full wrapper build and staging passed with captured logs and mocked storage inventory; no GPU or physical USB accessed.'
} finally {Remove-PSDrive Q}
