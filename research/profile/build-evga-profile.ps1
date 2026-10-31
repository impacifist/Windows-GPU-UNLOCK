# Build/package only. Never stages media or writes firmware.
param([ValidateSet('SharedEsp','Usb','SeparateEsp')][string]$Layout='SharedEsp',
 [guid]$WindowsEspGuid='c36c3e86-02aa-4f76-af1b-96e327f9d1c1',[guid]$UnlockPartitionGuid=[guid]::Empty,
 [ValidateRange(8,99)][int]$AttemptSlot=10,[Parameter(Mandatory)][string]$OutputDirectory,
 [string]$Clang='clang',[string]$LldLink='lld-link',[string]$Python='python')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'evga-package-lib.ps1')
$profile=Get-EvgaPackageProfile $Layout
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $out){throw 'Output already exists; choose a fresh directory, never overwrite a prior attempt'}
$build=$out+'.build'
& (Join-Path $PSScriptRoot 'build-shared-esp-unlock.ps1') -WindowsEspGuid $WindowsEspGuid -UnlockPartitionGuid $UnlockPartitionGuid -AttemptSlot $AttemptSlot -PackageLayout $Layout -PackageOutput $build -Clang $Clang -LldLink $LldLink -Python $Python
[void][IO.Directory]::CreateDirectory($out)
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$revision=(& git -C $repo rev-parse HEAD).Trim()
if($LASTEXITCODE -ne 0 -or $revision -notmatch '^[0-9a-f]{40}$'){throw 'Cannot establish source revision'}
$dirty=@(& git -C $repo status --porcelain).Count -ne 0
$readme=Join-Path $build 'package-README.md'
$text=@"
# Attended EVGA profile payload

Layout: $Layout. Attempt slot: $AttemptSlot. Experimental exact-machine package.
Requires a fresh Shift+G; no permanent or unattended unlock is claimed.
Install/rearm controller: research/profile/manage-evga-profile.ps1 in the matching repository checkout.
The ZIP carries EFI, profiles, integrity records and GPL-2.0 source pointers; it is not a standalone Windows application.
Firmware assets are NOT bundled. Supply the five hash-pinned files and NVIDIA-LICENSE/NOTICE.md from your existing asset directory.
Do not reuse consumed receipts, overwrite attempt journals or copy the package to unqualified hardware.
GPU execution can lose display, hang, retain resources or damage boot/data; recovery is not guaranteed.
Source repository: https://github.com/DeclanFrampton/Windows-GPU-UNLOCK/tree/$revision
Local source hashes are in source-manifest.json; SourceTreeDirty records local modifications at build time.
The same configuration's attended legacy flow has evidence; this new package itself requires live validation.
"@
[IO.File]::WriteAllText($readme,$text,[Text.UTF8Encoding]::new($false))
$slug=@{SharedEsp='shared-esp';Usb='usb';SeparateEsp='separate-esp'}[$Layout]
$files=@{
 'UNLOCK.EFI'=(Join-Path $build 'BOOTX64.EFI');'profile.json'=(Join-Path $PSScriptRoot ('profiles/evga-z790-3090-'+$slug+'.json'));
 'assets.json'=(Join-Path $repo 'internal-boot/unlock/assets.json');'source-manifest.json'=(Join-Path $build 'manifest.json');
 'pe-verification.json'=(Join-Path $build 'pe-verification.json');'platform-config.h'=(Join-Path $build 'platform-config.h');'LICENSE'=(Join-Path $repo 'LICENSE');'README.md'=$readme
}
$inventory=@(foreach($name in @($files.Keys | Sort-Object)){
 $data=[IO.File]::ReadAllBytes($files[$name]);$target=Join-Path $out $name;Write-SharedSecondNewFile $target $data
 [ordered]@{Name=$name;Bytes=$data.Length;Sha256=(Get-EvgaPackageHash $target)}
})
$run='EVP{0:D2}' -f $AttemptSlot
$manifest=[ordered]@{Schema='evga-attended-package-v1';ProfileId=$profile.id;Layout=$Layout;AttemptSlot=$AttemptSlot;Run=$run;WindowsEspGuid=$WindowsEspGuid.ToString();UnlockPartitionGuid=$UnlockPartitionGuid.ToString();RequiresFreshShiftG=$true;HardwareTested=$false;EfiPath=('\EFI\WindowsGPUUnlock\'+$run+'\UNLOCK.EFI');AssetsBundled=$false;SourceCommit=$revision;SourceTreeDirty=$dirty;Files=$inventory}
Write-EvgaPackageJson (Join-Path $out 'package.json') $manifest
$null=Assert-EvgaPackage $out
Compress-Archive -LiteralPath $out -DestinationPath ($out+'.zip') -ErrorAction Stop
[ordered]@{package=$out;zip=($out+'.zip');profile=$profile.id;slot=$AttemptSlot;efi_sha256=(Get-EvgaPackageHash (Join-Path $out 'UNLOCK.EFI'));hardware_tested=$false;staged=$false;firmware_written=$false} | ConvertTo-Json
