# Independent CPU-side fallback for one exact, originally enabled GPU.
param([Parameter(Mandatory=$true)][string]$StatePath,
      [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$principal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Elevation required.' }
$state = Get-Content -LiteralPath $StatePath -Raw | ConvertFrom-Json
if ($state.InstanceId -notlike 'PCI\VEN_10DE&DEV_2782&SUBSYS_51321462&REV_A1\*' -or $state.PreviouslyEnabled -ne $true) {
    throw 'Unexpected recovery target/state.'
}
function Publish-Json($Path,$Value) {
    $temporary=$Path+'.new'
    [IO.File]::WriteAllText($temporary,($Value | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    if ([IO.File]::Exists($Path)) { [IO.File]::Replace($temporary,$Path,[NullString]::Value) }
    else { [IO.File]::Move($temporary,$Path) }
}
Publish-Json (Join-Path $OutputDirectory 'recovery-ready.json') @{ Ready=$true; Elevated=$true; Pid=$PID }
for ($step=0; $step -lt 45; $step++) {
    if ((Test-Path -LiteralPath (Join-Path $OutputDirectory 'controller-done')) -or
        (Test-Path -LiteralPath (Join-Path $OutputDirectory 'controller-cancelled'))) { exit 0 }
    Start-Sleep -Seconds 1
}
$result = [ordered]@{ EnableAttempted=$false; ProblemCode=$null; Completed=$false; ResetStillRunning=$false }
try {
    $issuedPath=Join-Path $OutputDirectory 'restart-issued.json'
    if (-not (Test-Path -LiteralPath $issuedPath)) { throw 'No restart-issued marker; fallback not authorized.' }
    $issued=Get-Content -LiteralPath $issuedPath -Raw | ConvertFrom-Json
    $resetProcess=Get-Process -Id $issued.ResetPid -ErrorAction SilentlyContinue
    if ($resetProcess) {
        # Do not overlap an enable with an unresolved reset, even if the PID
        # has been reused or its identity can no longer be established.
        $result.ResetStillRunning=$true
        throw 'Restart process is still present; no enable attempted.'
    }
    $problem = Get-PnpDeviceProperty -InstanceId $state.InstanceId -KeyName DEVPKEY_Device_ProblemCode -ErrorAction Stop
    $result.ProblemCode = [int]$problem.Data
    if ($result.ProblemCode -eq 22) {
        $result.EnableAttempted = $true
        Enable-PnpDevice -InstanceId $state.InstanceId -Confirm:$false -ErrorAction Stop
    }
    $result.Completed = $true
} catch { $result.Error = $_.Exception.Message }
Publish-Json (Join-Path $OutputDirectory 'recovery-result.json') $result
# No automatic disable/restart/removal/install/reboot or further retry.
