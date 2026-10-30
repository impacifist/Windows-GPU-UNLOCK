# GPL-2.0-only. Capture-free EDID/no-EDID release matrix; no GPU/firmware access.
param([string]$Clang='clang',[string]$Python='python',[string]$VcVars)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'host.ps1')
Import-GuHostCompiler $VcVars
$output=Join-Path $PSScriptRoot ('build/local-release-'+[Guid]::NewGuid().ToString('N'))
& $Python -I (Join-Path $PSScriptRoot 'test_local_release.py') --clang $Clang --output $output
if($LASTEXITCODE -ne 0){throw 'Synthetic local release matrix failed'}
