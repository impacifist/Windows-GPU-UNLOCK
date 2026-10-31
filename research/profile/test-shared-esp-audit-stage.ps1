$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'shared-esp-audit-stage-lib.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$fixture=Join-Path $repo ('research/profile/build/shared-stage-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $fixture 'EFI/Microsoft/Boot'),(Join-Path $fixture 'EFI/limine') -Force | Out-Null
$windows=Join-Path $fixture 'EFI/Microsoft/Boot/bootmgfw.efi';$limine=Join-Path $fixture 'EFI/limine/limine_x64.efi';$source=Join-Path $fixture 'audit-source.efi'
Set-Content -LiteralPath $windows -Value 'Windows fixture';Set-Content -LiteralPath $limine -Value 'Limine fixture';Set-Content -LiteralPath $source -Value 'audit fixture'
$wh=(Get-FileHash -LiteralPath $windows).Hash;$lh=(Get-FileHash -LiteralPath $limine).Hash;$sh=(Get-FileHash -LiteralPath $source).Hash
$target=Copy-SharedEspAudit $fixture $source $sh $wh $lh
if((Get-FileHash -LiteralPath $target).Hash -ne $sh -or (Get-FileHash -LiteralPath $windows).Hash -ne $wh -or (Get-FileHash -LiteralPath $limine).Hash -ne $lh){throw 'Fixture verification failed'}
$refused=$false;try {Copy-SharedEspAudit $fixture $source $sh $wh $lh | Out-Null}catch{$refused=$true};if(!$refused){throw 'Existing target was overwritten'}
$other=Join-Path $repo ('research/profile/build/shared-stage-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $other 'EFI/Microsoft/Boot'),(Join-Path $other 'EFI/limine') -Force | Out-Null
Copy-Item -LiteralPath $windows -Destination (Join-Path $other 'EFI/Microsoft/Boot/bootmgfw.efi');Copy-Item -LiteralPath $limine -Destination (Join-Path $other 'EFI/limine/limine_x64.efi')
$refused=$false;try {Copy-SharedEspAudit $other $source ('0'*64) $wh $lh | Out-Null}catch{$refused=$true};if(!$refused -or (Test-Path (Join-Path $other 'EFI/EVGA-SHARED-AUDIT.EFI'))){throw 'Wrong source hash not refused before creation'}
$refused=$false;try {Copy-SharedEspAudit $other $source $sh ('0'*64) $lh | Out-Null}catch{$refused=$true};if(!$refused -or (Test-Path (Join-Path $other 'EFI/EVGA-SHARED-AUDIT.EFI'))){throw 'Changed Windows hash not refused before creation'}
'Shared ESP staging fixture passed file verification, no-overwrite and changed-hash refusal; no ESP accessed.'
