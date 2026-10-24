# GPL-2.0-only. Local build provenance; no disk or firmware mutation.
function Get-HeadlessInputInventory([string]$Repo,[switch]$WithContract){
 $base=[IO.Path]::GetFullPath($Repo).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)
 $runtime=Join-Path $base 'internal-boot'
 $paths=@(Get-ChildItem -LiteralPath $runtime -Recurse -File | Where-Object {$_.Extension -in '.c','.h','.ps1' -and $_.FullName.Substring($runtime.Length+1) -notmatch '(^|[\\/])build([\\/]|$)'})
 $paths+=Get-ChildItem -LiteralPath (Join-Path $base 'deps/edk2/MdePkg/Include') -Recurse -File -Filter '*.h'
 $paths+=Get-Item -LiteralPath (Join-Path $base 'internal-boot/local-config.json'),(Join-Path $base 'internal-boot/unlock/assets.json'),(Join-Path $base 'research/headless/build-unlock.ps1')
 if($WithContract){$paths+=Get-Item -LiteralPath (Join-Path $base 'internal-boot/boot/build/headless/child-contract.h')}
 return @($paths | Sort-Object FullName -Unique | ForEach-Object {[ordered]@{Path=$_.FullName.Substring($base.Length+1).Replace([char]92,[char]47);Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}})
}
function Get-HeadlessGuidInitializer([guid]$Value){
 $b=$Value.ToByteArray();$tail=($b[8..15] | ForEach-Object {'0x{0:x2}' -f $_}) -join ','
 return ('{{0x{0:x8},0x{1:x4},0x{2:x4},{{{3}}}}}' -f [BitConverter]::ToUInt32($b,0),[BitConverter]::ToUInt16($b,4),[BitConverter]::ToUInt16($b,6),$tail)
}
function Assert-HeadlessLocalConfig([string]$Repo){
 $cfg=Get-Content -LiteralPath (Join-Path $Repo 'internal-boot/local-config.json') -Raw | ConvertFrom-Json
 $header=Join-Path $Repo 'internal-boot/boot/platform-config.h'
 if(!$cfg.GeneratedHeaderSha256 -or (Get-FileHash -LiteralPath $header).Hash -ne $cfg.GeneratedHeaderSha256 -or $cfg.UnlockDriveLetter -notmatch '^[D-Z]$' -or [guid]$cfg.UnlockPartitionGuid -eq [guid]$cfg.WindowsPartitionGuid -or $cfg.UnlockSize -lt 1GB){throw 'Configure the separate ESP again; header/config contract is missing or stale'}
 $text=Get-Content -LiteralPath $header
 if($text -notcontains ('#define GU_INTERNAL_PARTITION_INITIALIZER '+(Get-HeadlessGuidInitializer ([guid]$cfg.UnlockPartitionGuid))) -or $text -notcontains ('#define GU_WINDOWS_PARTITION_INITIALIZER '+(Get-HeadlessGuidInitializer ([guid]$cfg.WindowsPartitionGuid)))){throw 'Generated partition header differs from local configuration'}
 return $cfg
}
function Get-HeadlessProfileIdentity([string]$Profile){
 switch($Profile){
  'ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE'{return @{Board='TUF GAMING B650M-PLUS WIFI';Bios='3057'}}
  'ASUS-TUF-GAMING-B660M-E-D4-4003-RTX3070TI'{return @{Board='TUF GAMING B660M-E D4';Bios='4003'}}
 }
 throw ('Unknown hardware profile: '+$Profile)
}
function Assert-HeadlessPackage([string]$Repo){
 $null=Assert-HeadlessLocalConfig $Repo
 $m=Get-Content -LiteralPath (Join-Path $Repo 'internal-boot/boot/build/headless/manifest.json') -Raw | ConvertFrom-Json
 if($m.Format -ne 2 -or $m.Mode -ne 'single-rtx-headless-unlock-then-windows' -or $m.CanUnlock -ne $true -or $m.RequiresHumanKey -ne $false -or $m.HardwareProfile -notin @('ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE','ASUS-TUF-GAMING-B660M-E-D4-4003-RTX3070TI')){throw 'Unexpected headless package profile'}
 $actual=Get-HeadlessInputInventory $Repo -WithContract
 if($actual.Count -ne $m.Inputs.Count){throw 'Build source set changed; rebuild the package'}
 for($i=0;$i -lt $actual.Count;$i++){if($actual[$i].Path -cne $m.Inputs[$i].Path -or $actual[$i].Sha256 -ne $m.Inputs[$i].Sha256){throw ('Build input changed; rebuild: '+$actual[$i].Path)}}
 $wrapper=Join-Path $Repo 'internal-boot/boot/build/headless/HEADLESS.EFI'
 $child=Join-Path $Repo 'internal-boot/unlock/build/experimental/entryLatchedSs1Full/internal/automatic/headless-research/RTX3090-EXPERIMENTAL-entryLatchedSs1Full-INTERNAL-AUTO-HEADLESS-RESEARCH.EFI'
 $cmFile=Join-Path (Split-Path $child) 'build-manifest.json';$cm=Get-Content -LiteralPath $cmFile -Raw | ConvertFrom-Json
 if((Get-FileHash -LiteralPath $wrapper).Hash -ne $m.EfiSha256 -or (Get-Item -LiteralPath $wrapper).Length -ne $m.EfiBytes -or (Get-FileHash -LiteralPath $child).Hash -ne $m.ChildSha256 -or (Get-Item -LiteralPath $child).Length -ne $m.ChildBytes -or (Get-FileHash -LiteralPath $cmFile).Hash -ne $m.ChildManifestSha256 -or $cm.Mode -ne 'headless-research-automatic-internal-exact-card-unlock' -or $cm.StockProfile -ne 'entryLatchedSs1Full' -or $cm.EfiSha256 -ne $m.ChildSha256){throw 'Headless EFI/child manifest mismatch'}
 foreach($item in @($cm.Sources)+@($cm.Headers)){
  $root=[IO.Path]::GetFullPath((Join-Path $Repo 'internal-boot/unlock')).TrimEnd([char]92)+[IO.Path]::DirectorySeparatorChar
  $path=[IO.Path]::GetFullPath((Join-Path $root $item.Path))
  if(!$path.StartsWith($root,[StringComparison]::OrdinalIgnoreCase) -or (Get-FileHash -LiteralPath $path).Hash -ne $item.Sha256){throw 'Child source/header provenance mismatch'}
 }
 $contract=Get-Content -LiteralPath (Join-Path $Repo 'internal-boot/boot/build/headless/child-contract.h')
 if($contract -notcontains ('#define GU_UNLOCK_SHA256 "'+$m.ChildSha256.ToLowerInvariant()+'"') -or $contract -notcontains ('#define GU_UNLOCK_BYTES '+$m.ChildBytes)){throw 'Wrapper child contract mismatch'}
 return $m
}
function Get-HeadlessPromotionOrder([string[]]$Current,[string]$Id){
 if($Id -notmatch '^\{[0-9a-f-]{36}\}$' -or $Current -notcontains '{bootmgr}' -or $Current -notcontains $Id -or @($Current | Select-Object -Unique).Count -ne $Current.Count){throw 'Invalid headless promotion input'}
 return @($Id,'{bootmgr}')+@($Current | Where-Object {$_ -ne $Id -and $_ -ne '{bootmgr}'})
}
function Get-HeadlessAssetSpecs([string]$Repo){
 $specs=@(Get-Content -LiteralPath (Join-Path $Repo 'internal-boot/unlock/assets.json') -Raw | ConvertFrom-Json)
 if($specs.Count -eq 1 -and $specs[0].Name -is [array]){
  $flat=$specs[0];$specs=@()
  for($i=0;$i -lt $flat.Name.Count;$i++){$specs+=[pscustomobject]@{Name=$flat.Name[$i];Bytes=[int64]$flat.Bytes[$i];Sha256=[string]$flat.Sha256[$i]}}
 }
 $names=@('booter_load_ga102.bin','v67-ga102-610.43.03.bin','gsp_ga10x.bin','gsp_boot_ga102.bin','gsp_boot_desc_ga102.bin')
 if($specs.Count -ne 5 -or @($specs.Name | Select-Object -Unique).Count -ne 5){throw 'Exactly five unique firmware assets required'}
 foreach($s in $specs){if($s.Name -notin $names -or $s.Bytes -le 0 -or $s.Sha256 -notmatch '^[0-9a-f]{64}$'){throw 'Invalid firmware asset contract'}}
 return $specs
}
function Write-HeadlessJson([string]$Path,$Value){
 $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($Value | ConvertTo-Json -Depth 8))
 $temp=$Path+'.pending-'+[guid]::NewGuid().ToString('N')
 $stream=[IO.File]::Open($temp,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
 try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
 if(Test-Path -LiteralPath $Path){[IO.File]::Replace($temp,$Path,[NullString]::Value)}else{[IO.File]::Move($temp,$Path)}
}
