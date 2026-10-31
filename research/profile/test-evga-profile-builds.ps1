param([Parameter(Mandatory)][string[]]$PackageDirectories,[string]$Clang='clang',[string]$LldLink='lld-link')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'evga-package-lib.ps1')
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Push-Location $repo
try {
 foreach($directory in $PackageDirectories){
  $m=Assert-EvgaPackage $directory
  $out=$directory+'.host-tests';[void][IO.Directory]::CreateDirectory($out)
  $defines=@('-DGU_EVGA_PACKAGE','-DGU_SHARED_ESP_UNLOCK')
  if($m.Layout -eq 'SharedEsp'){$defines+='-DGU_EVGA_SHARED_ESP_ADMISSION'}
  if($m.Layout -eq 'Usb'){$defines+='-DGU_USB_MEDIA'}
  $base=@('-target','x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fno-builtin','-fno-stack-protector','-fshort-wchar','-ffunction-sections','-O1','-Wall','-Wextra','-Werror','-I','deps/edk2/MdePkg/Include','-I','deps/edk2/MdePkg/Include/X64','-include',(Join-Path $directory 'platform-config.h'))
  foreach($test in @('test_evga_package_media','test_shared_esp_unlock_gate')){
   & $Clang @base @defines -c ("internal-boot/boot/$test.c") -o "$out/test.obj"
   if($LASTEXITCODE -ne 0){throw "Compile failed: $test $($m.Layout)"}
   & $Clang @base -c internal-boot/efi_runtime.c -o "$out/runtime.obj"
   if($LASTEXITCODE -ne 0){throw 'Runtime compile failed'}
   & $LldLink /nologo /nodefaultlib /subsystem:console /entry:main /machine:x64 /opt:ref "/out:$out/test.exe" "$out/test.obj" "$out/runtime.obj"
   if($LASTEXITCODE -ne 0){throw 'Host link failed'}
   & "$out/test.exe"
   if($LASTEXITCODE -ne 0){throw "Host test failed: $test $($m.Layout) exit $LASTEXITCODE"}
  }
  Write-Host "PASS $($m.Layout): package/PE inventory, media rejection matrix and six one-shot receipt cases. No hardware accessed."
 }
}finally{Pop-Location}
