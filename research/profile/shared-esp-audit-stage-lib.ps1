# GPL-2.0-only. No-overwrite staging primitives for the no-GPU audit.
function Get-AuditCheckedPath([string]$Root,[string[]]$Components,[bool]$Directory){
 $path=$Root
 foreach($component in $Components){
  $path=[IO.Path]::Combine($path,$component)
  $attrs=[IO.File]::GetAttributes($path)
  if(($attrs -band [IO.FileAttributes]::ReparsePoint) -eq [IO.FileAttributes]::ReparsePoint){throw "Reparse point in $component"}
 }
 $actualDirectory=(($attrs -band [IO.FileAttributes]::Directory) -eq [IO.FileAttributes]::Directory)
 if($actualDirectory -ne $Directory){throw 'Unexpected EFI path type'}
 return $path
}
function Copy-SharedEspAudit([string]$Root,[string]$Source,[string]$ExpectedEfiHash,[string]$WindowsHash,[string]$LimineHash,[string]$AuditFileName="EVGA-SHARED-AUDIT.EFI"){
 if($AuditFileName -notin @("EVGA-SHARED-AUDIT.EFI","EVGA-SHARED-AUDIT-VISIBLE.EFI","EVGA-SHARED-ASSETS.EFI","EVGA-SHARED-RETAIN.EFI","EVGA-SHARED-INTEGRATION.EFI","EVGA-SHARED-QUALIFICATION.EFI","EVGA-SHARED-UNLOCK.EFI")){throw "Unexpected audit filename"}
 $rootAttrs=[IO.File]::GetAttributes($Root)
 if(($rootAttrs -band [IO.FileAttributes]::ReparsePoint) -eq [IO.FileAttributes]::ReparsePoint -or ($rootAttrs -band [IO.FileAttributes]::Directory) -ne [IO.FileAttributes]::Directory){throw 'Invalid system volume root'}
 $efi=Get-AuditCheckedPath $Root @('EFI') $true
 $windows=Get-AuditCheckedPath $Root @('EFI','Microsoft','Boot','bootmgfw.efi') $false
 $limine=Get-AuditCheckedPath $Root @('EFI','limine','limine_x64.efi') $false
 if((Get-FileHash -LiteralPath $windows -Algorithm SHA256).Hash -ine $WindowsHash -or (Get-FileHash -LiteralPath $limine -Algorithm SHA256).Hash -ine $LimineHash){throw 'Existing Windows or Limine EFI changed; no copy attempted'}
 if((Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash -ine $ExpectedEfiHash){throw 'Audit build hash changed; no copy attempted'}
 $target=[IO.Path]::Combine($efi,$AuditFileName)
 $inputStream=$null;$outputStream=$null
 try {
  $inputStream=[IO.File]::Open($Source,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
  $outputStream=[IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
  $inputStream.CopyTo($outputStream);$outputStream.Flush($true)
 } finally {if($outputStream){$outputStream.Dispose()};if($inputStream){$inputStream.Dispose()}}
 if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ine $ExpectedEfiHash){throw 'Audit copy verification failed; preserve the file and report'}
 if((Get-FileHash -LiteralPath $windows -Algorithm SHA256).Hash -ine $WindowsHash -or (Get-FileHash -LiteralPath $limine -Algorithm SHA256).Hash -ine $LimineHash){throw 'Existing boot file changed during staging; preserve evidence and report'}
 return $target
}
