param([string]$Clang='clang',[string]$VcVars='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat')
$ErrorActionPreference='Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
 $lines=& cmd.exe /d /c "call `"$VcVars`" >nul && set"
 if($LASTEXITCODE -ne 0){throw 'MSVC environment failed'}
 foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process')}}
 New-Item -ItemType Directory -Path build/diagnostic -Force | Out-Null
 & $Clang -std=c11 -O1 -Wall -Wextra -Werror -D_CRT_SECURE_NO_WARNINGS -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 test_diagnostic.c -o build/diagnostic/test-diagnostic.exe
 if($LASTEXITCODE -ne 0){throw 'Diagnostic tests compile failed'}
 & ./build/diagnostic/test-diagnostic.exe
 if($LASTEXITCODE -ne 0){throw 'Diagnostic tests failed'}
 # Probe regression tests: the real probe.c behind simulated EFI services for
 # both reviewed profiles. No GPU, firmware or driver access is available.
 foreach($profile in @('GU_PROFILE_B650M_3090FE','GU_PROFILE_B660M_3070TI','GU_PROFILE_B660M_3090TI','GU_PROFILE_X870_3090TI','GU_PROFILE_B660PLUS_3060')) {
  & $Clang -std=c11 -O1 -Wall -Wextra -Werror -D_CRT_SECURE_NO_WARNINGS -DGU_DIAGNOSTIC_PROBE "-D$profile" -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 test_probe.c probe.c ../unlock/sha256.c -o "build/diagnostic/test-probe-$profile.exe"
  if($LASTEXITCODE -ne 0){throw "Probe tests compile failed: $profile"}
  & "./build/diagnostic/test-probe-$profile.exe"
  if($LASTEXITCODE -ne 0){throw "Probe tests failed: $profile"}
 }
 # Both new identities use the guarded MMIO fixture, with and without PROM output.
 foreach($profile in @('GU_PROFILE_H610M_3060','GU_PROFILE_PROBE_H610M_3060')) {
  foreach($prom in @($false,$true)) {
   $extra=@();if($prom){$extra+='-DGU_PROBE_DUMP_PROM'}
   $dest="build/diagnostic/test-checked-$profile-$prom.exe"
   & $Clang -std=c11 -O1 -Wall -Wextra -Werror -D_CRT_SECURE_NO_WARNINGS -DGU_DIAGNOSTIC_ONLY -DGU_DIAGNOSTIC_PROBE -DGU_USB_MEDIA "-D$profile" @extra -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 test_probe_generic.c probe.c ../unlock/sha256.c -o $dest
   if($LASTEXITCODE -ne 0){throw "Checked probe compile failed: $profile dump=$prom"}
   & "./$dest"
   if($LASTEXITCODE -ne 0){throw "Checked probe tests failed: $profile dump=$prom"}
  }
 }
 # The probe-only profile must never compile into a write-side unit. These compiles
 # are expected to fail; PowerShell 5.1 would turn their stderr into a terminating error.
 $ErrorActionPreference='Continue'
 foreach($bad in @('-DGU_FULL_UNLOCK','-DGU_UEFI_PORT','-DGU_HEADLESS_RESEARCH','-DGU_EXPERIMENTAL_3090','-DGU_INTERNAL_BOOT','-DGU_CONSOLE_PREPARE','-DGU_AUTOMATIC_BOOT')) {
  $out=& $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fsyntax-only -DGU_DIAGNOSTIC_ONLY -DGU_DIAGNOSTIC_PROBE -DGU_PROFILE_PROBE_H610M_3060 $bad -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 probe.c 2>&1 | Out-String
  if($LASTEXITCODE -eq 0 -or $out -notmatch 'must not be combined with release or unlock builds'){throw "Probe-only profile not rejected by its guard with $bad"}
 }
 $out=& $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fsyntax-only -DGU_DIAGNOSTIC_PROBE -DGU_PROFILE_PROBE_H610M_3060 -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 probe.c 2>&1 | Out-String
 if($LASTEXITCODE -eq 0 -or $out -notmatch 'is for the read-only diagnostic probe only'){throw 'Probe-only profile not rejected outside a diagnostic-only build'}
 $out=& $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fsyntax-only -DGU_DIAGNOSTIC_PROBE -DGU_PROBE_DUMP_PROM -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 probe.c 2>&1 | Out-String
 if($LASTEXITCODE -eq 0 -or $out -notmatch 'only for the observation-only diagnostic image'){throw 'PROM dump not rejected outside a diagnostic-only build'}
 $ErrorActionPreference='Stop'
 & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fshort-wchar -fsyntax-only -Wall -Wextra -Werror -DGU_DIAGNOSTIC_ONLY -DGU_DIAGNOSTIC_PROBE -DGU_USB_MEDIA -DGU_PROBE_DUMP_PROM -DGU_PROFILE_PROBE_H610M_3060 -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 probe.c
 if($LASTEXITCODE -ne 0){throw 'PROM dump probe does not compile'}
 'PASS probe-only profile rejected by write-side and non-diagnostic builds'
 & $Clang -std=c11 -O1 -Wall -Wextra -Werror -D_CRT_SECURE_NO_WARNINGS -DGU_DIAGNOSTIC_ONLY -DGU_DIAGNOSTIC_PROBE -DGU_PROBE_GENERIC -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 test_probe_generic.c probe.c ../unlock/sha256.c -o build/diagnostic/test-probe-generic.exe
 if($LASTEXITCODE -ne 0){throw 'Generic probe tests compile failed'}
 & ./build/diagnostic/test-probe-generic.exe
 if($LASTEXITCODE -ne 0){throw 'Generic probe tests failed'}
# Non-default deep-read bus (e.g. AM4 boards with the CPU x16 slot on 0x2b): same suite, other bus.
 & $Clang -std=c11 -O1 -Wall -Wextra -Werror -D_CRT_SECURE_NO_WARNINGS -DGU_DIAGNOSTIC_ONLY -DGU_DIAGNOSTIC_PROBE -DGU_PROBE_GENERIC -DGU_PROBE_BUS=0x2bU -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 test_probe_generic.c probe.c ../unlock/sha256.c -o build/diagnostic/test-probe-generic-bus2b.exe
 if($LASTEXITCODE -ne 0){throw 'Generic probe (bus 0x2b) tests compile failed'}
 & ./build/diagnostic/test-probe-generic-bus2b.exe
  if($LASTEXITCODE -ne 0){throw 'Generic probe (bus 0x2b) tests failed'}
  foreach($bus in @(1,43,255)) {
   $dest="build/diagnostic/test-probe-auto-$bus.exe"
   & $Clang -std=c11 -O1 -Wall -Wextra -Werror -D_CRT_SECURE_NO_WARNINGS -DGU_DIAGNOSTIC_ONLY -DGU_DIAGNOSTIC_PROBE -DGU_PROBE_GENERIC -DGU_PROBE_AUTO_BUS "-DGU_TEST_PROBE_BUS=$bus" -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 test_probe_generic.c probe.c ../unlock/sha256.c -o $dest
   if($LASTEXITCODE -ne 0){throw "Auto-bus probe compile failed: $bus"}
   & "./$dest"
   if($LASTEXITCODE -ne 0){throw "Auto-bus probe failed: $bus"}
  }
 # A non-default bus must never compile outside the read-only diagnostic probe.
 $ErrorActionPreference='Continue'
 $out=& $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fsyntax-only -DGU_DIAGNOSTIC_PROBE -DGU_PROBE_GENERIC -DGU_PROBE_BUS=0x2bU -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 probe.c 2>&1 | Out-String
  if($LASTEXITCODE -eq 0 -or $out -notmatch 'only for the read-only diagnostic probe'){throw 'Non-default GU_PROBE_BUS not rejected outside a diagnostic-only build'}
  foreach($bad in @('-DGU_PROBE_BUS=1U','-DGU_FULL_UNLOCK','-DGU_EXPERIMENTAL_3090','-UGU_DIAGNOSTIC_ONLY','-UGU_PROBE_GENERIC','-UGU_DIAGNOSTIC_PROBE')) {
   $out=& $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fsyntax-only -DGU_DIAGNOSTIC_ONLY -DGU_DIAGNOSTIC_PROBE -DGU_PROBE_GENERIC -DGU_PROBE_AUTO_BUS $bad -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 probe.c 2>&1 | Out-String
   if($LASTEXITCODE -eq 0 -or $out -notmatch 'GU_PROBE_AUTO_BUS requires'){throw "Auto-bus compile guard failed: $bad"}
  }
  foreach($bus in @(0,256)) {
   $out=& $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fsyntax-only -DGU_DIAGNOSTIC_ONLY -DGU_DIAGNOSTIC_PROBE -DGU_PROBE_GENERIC "-DGU_PROBE_BUS=$bus" -I ../../deps/edk2/MdePkg/Include -I ../../deps/edk2/MdePkg/Include/X64 probe.c 2>&1 | Out-String
   if($LASTEXITCODE -eq 0 -or $out -notmatch 'GU_PROBE_BUS must be between'){throw "Out-of-range bus compiled: $bus"}
  }
 $ErrorActionPreference='Stop'
 'PASS non-default probe bus rejected outside diagnostic-only builds'
} finally {Pop-Location}
