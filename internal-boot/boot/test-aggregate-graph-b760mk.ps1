# GPL-2.0-only. Replay the DRAFT B760M-K / RTX 3060 console graph from real read-only
# captures in WebAssembly. Each capture becomes capture-graph.h (capture-replay-b760mk.py),
# then the actual release-planning code runs against it. No firmware or GPU access, and
# no x64 UEFI object: the draft profile refuses every non-WebAssembly write build.
param(
 [string[]]$Capture=@(),
 [string]$Clang='clang',
 [string]$Node='node',
 [string]$Python='python'
)
$ErrorActionPreference='Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
 if(!(Get-Command -Name $Clang -CommandType Application -ErrorAction SilentlyContinue)){throw "clang unavailable: $Clang"}
 if(!(Get-Command -Name $Node -CommandType Application -ErrorAction SilentlyContinue)){throw "Node unavailable: $Node"}
 if(!$Capture.Count){
  # Private captures are kept next to the repository, never committed.
  $scans=Join-Path $PSScriptRoot '../../../scans'
  $Capture=@(Get-ChildItem -LiteralPath $scans -Directory -ErrorAction SilentlyContinue |
   Where-Object {Test-Path (Join-Path $_.FullName 'DIAG00.TXT')} | ForEach-Object FullName)
 }
 if(!$Capture.Count){throw 'No captures found; pass -Capture <dir>'}
 $common=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin',
           '-fno-stack-protector','-fshort-wchar','-DGU_PROFILE_DRAFT_B760MK_3060','-DGU_DRAFT_REVIEW',
           '-DGU_DRAFT_HOST_TEST','-DGU_HEADLESS_RESEARCH','-DGU_SINGLE_RTX_CONSOLE','-DGU_FULL_UNLOCK',
           '-DGU_AUTOMATIC_BOOT','-I','../../deps/edk2/MdePkg/Include','-I','../../deps/edk2/MdePkg/Include/Ia32')
 $runner='build/aggregate-b760mk/run.js'
 New-Item -ItemType Directory -Path 'build/aggregate-b760mk' -Force | Out-Null
 @(
  'const fs=require("fs");',
  'const b=fs.readFileSync(process.argv[2]);',
  'WebAssembly.instantiate(b,{}).then(({instance})=>process.exit(instance.exports.mainCRTStartup())).catch(e=>{console.error(e);process.exit(99)});'
 ) | Set-Content -LiteralPath $runner -Encoding ascii
 foreach($dir in $Capture) {
  $name=Split-Path $dir -Leaf
  $build="build/aggregate-b760mk/$name"
  New-Item -ItemType Directory -Path "$build/include" -Force | Out-Null
  @(
   '#define GU_INTERNAL_PARTITION_INITIALIZER {0x11111111,0x2222,0x3333,{0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb}}',
   '#define GU_WINDOWS_PARTITION_INITIALIZER {0xaaaaaaaa,0xbbbb,0xcccc,{0xdd,0xee,0xff,0,0x11,0x22,0x33,0x44}}'
  ) | Set-Content -LiteralPath "$build/include/platform-config.h" -Encoding ascii
  & $Python -I capture-replay-b760mk.py $dir "$build/include/capture-graph.h"
  if($LASTEXITCODE -ne 0){throw "Capture refused: $name"}
  $wasm=@('--target=wasm32-unknown-unknown')+$common+@('-I',"$build/include")
  foreach($source in @('test_aggregate_graph_b760mk.c','../unlock/checks.c','../unlock/sha256.c')) {
   $obj=[IO.Path]::GetFileNameWithoutExtension($source)
   & $Clang @wasm '-c' $source '-o' "$build/$obj.wasm.o"
   if($LASTEXITCODE -ne 0){throw "Graph replay compile failed: $source ($name)"}
  }
  & $Clang '--target=wasm32-unknown-unknown' '-nostdlib' '-Wl,--no-entry' '-Wl,--export=mainCRTStartup' `
   '-Wl,--initial-memory=1310720' '-o' "$build/replay.wasm" "$build/test_aggregate_graph_b760mk.wasm.o" `
   "$build/checks.wasm.o" "$build/sha256.wasm.o"
  if($LASTEXITCODE -ne 0){throw "Graph replay link failed ($name)"}
  & $Node $runner "$build/replay.wasm"
  if($LASTEXITCODE -ne 0){throw "Graph replay failed at invariant $LASTEXITCODE ($name)"}
  Write-Output "PASS $name"
 }
 Write-Output "PASS B760M-K single-child aggregate replay on $($Capture.Count) real captures; no firmware or GPU access"
} finally {Pop-Location}
