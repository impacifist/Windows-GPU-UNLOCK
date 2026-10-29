# Exact-machine Windows vendor-driver restart test. Default is read-only preflight.
param([switch]$Execute)
$ErrorActionPreference = 'Stop'
$smi = Join-Path $env:WINDIR 'System32\nvidia-smi.exe'
$powershell = Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
$targetBus = '00000000:0B:00.0'
$devices = @(Get-PnpDevice -Class Display -ErrorAction Stop | Where-Object {
    $_.InstanceId -like 'PCI\VEN_10DE&DEV_2782&SUBSYS_51321462&REV_A1\*'
})
if ($devices.Count -ne 1) { throw 'Expected one exact RTX 4070 Ti PnP target.' }
$instance = $devices[0].InstanceId
$busNumber = (Get-PnpDeviceProperty -InstanceId $instance -KeyName DEVPKEY_Device_BusNumber).Data
$problemBefore = (Get-PnpDeviceProperty -InstanceId $instance -KeyName DEVPKEY_Device_ProblemCode).Data
$initial = @(& $smi '--query-gpu=name,pci.bus_id,pci.device_id,pci.sub_device_id,driver_version,vbios_version' '--format=csv,noheader,nounits')
if ($LASTEXITCODE -ne 0 -or $initial.Count -ne 1) { throw 'NVIDIA identity query failed.' }
$identity = @($initial[0].Split(',') | ForEach-Object { $_.Trim() })
if ($busNumber -ne 11 -or $identity.Count -ne 6 -or $identity[1] -ne $targetBus -or
    $identity[2] -ne '0x278210DE' -or $identity[3] -ne '0x51321462' -or $problemBefore -ne 0 -or $devices[0].Status -ne 'OK') {
    throw 'Target identity/resources/healthy initial state mismatch.'
}
$principal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
$elevated = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $Execute) {
    @{ TargetBus=$targetBus; PnpHealthy=$true; Elevated=$elevated; ExecutionAttempted=$false } | ConvertTo-Json
    exit 0
}
if (-not $elevated) { throw 'Execution requires an elevated helper.' }
New-Item -ItemType Directory -Path (Join-Path $PSScriptRoot 'build') -Force | Out-Null
$lockPath=Join-Path $PSScriptRoot 'build\active-driver-restart.lock'
$lock=[IO.File]::Open($lockPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
$restartIssued=$false
function Publish-Json($Path,$Value) {
    $temporary=$Path+'.new'
    [IO.File]::WriteAllText($temporary,($Value | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    if ([IO.File]::Exists($Path)) { [IO.File]::Replace($temporary,$Path,[NullString]::Value) }
    else { [IO.File]::Move($temporary,$Path) }
}
$directory = Join-Path $PSScriptRoot ('build\driver-restart-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory -Force | Out-Null
Set-Content -LiteralPath (Join-Path $PSScriptRoot 'build\latest-driver-restart.txt') -Value $directory -Encoding UTF8
$statePath = Join-Path $directory 'private-state.json'
Publish-Json $statePath @{ InstanceId=$instance; PreviouslyEnabled=$true }
$bootBefore = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime.ToUniversalTime().Ticks
$report = [ordered]@{ Schema=1; TargetBus=$targetBus; Elevated=$true; Phase='preflight';
    Before=@{ Driver=$identity[4]; Vbios=$identity[5]; PnpProblemCode=[int]$problemBefore };
    RecoveryReady=$false; RestartExitCode=$null; RestartTimedOut=$false; HealthRecovered=$false;
    RestartCommandCompleted=$false; ComputeRecovered=$null; BootSessionUnchanged=$false; Snapshots=@() }
$reportPath = Join-Path $directory 'result.json'
function Save-Report { Publish-Json $reportPath $report }
Save-Report
try {
    $recoveryScript = Join-Path $PSScriptRoot 'gpu_restart_recovery.ps1'
    $recovery = Start-Process -FilePath $powershell -WindowStyle Hidden -PassThru -ArgumentList @(
        '-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$recoveryScript+'"'),
        '-StatePath',('"'+$statePath+'"'),'-OutputDirectory',('"'+$directory+'"'))
    for ($i=0; $i -lt 10; $i++) {
        $readyPath = Join-Path $directory 'recovery-ready.json'
        if (Test-Path -LiteralPath $readyPath) {
            $ready = Get-Content -LiteralPath $readyPath -Raw | ConvertFrom-Json
            if ($ready.Ready -eq $true -and $ready.Elevated -eq $true -and $ready.Pid -eq $recovery.Id) {
                $report.RecoveryReady=$true; break
            }
        }
        Start-Sleep -Milliseconds 500
    }
    if (-not $report.RecoveryReady) { throw 'Independent elevated recovery worker not ready; no restart issued.' }
    $recovery.Refresh()
    if ($recovery.HasExited) { throw 'Recovery worker exited before restart; no restart issued.' }
    $report.Phase='restart-request'; Save-Report
    $reset = Start-Process -FilePath $smi -WindowStyle Hidden -PassThru -ArgumentList @('-r','-i',$targetBus) `
        -RedirectStandardOutput (Join-Path $directory 'restart-stdout.txt') `
        -RedirectStandardError (Join-Path $directory 'restart-stderr.txt')
    $restartIssued=$true
    # Retain the native handle before waiting; Windows PowerShell otherwise
    # may return a null ExitCode for a process that has already exited.
    $resetHandle=$reset.Handle
    Publish-Json (Join-Path $directory 'restart-issued.json') @{ ResetPid=$reset.Id }
    if ($reset.WaitForExit(20000)) {
        $report.RestartCommandCompleted=$true; $report.RestartExitCode=$reset.ExitCode
    }
    else { $report.RestartTimedOut=$true }
    $report.Phase='health-poll'; Save-Report
    $consecutive=0
    for ($i=0; $i -lt 20; $i++) {
        if (-not $report.RestartCommandCompleted) {
            $reset.Refresh()
            if ($reset.HasExited) { $report.RestartCommandCompleted=$true; $report.RestartExitCode=$reset.ExitCode }
        }
        $device = Get-PnpDevice -InstanceId $instance -ErrorAction SilentlyContinue
        $problem = if ($device) { (Get-PnpDeviceProperty -InstanceId $instance -KeyName DEVPKEY_Device_ProblemCode -ErrorAction SilentlyContinue).Data } else { $null }
        $kernel = Get-CimInstance Win32_SystemDriver -Filter "Name='nvlddmkm'" -ErrorAction SilentlyContinue
        $out = Join-Path $directory ('query-'+$i+'.txt')
        $err = Join-Path $directory ('query-'+$i+'-stderr.txt')
        $query = Start-Process -FilePath $smi -WindowStyle Hidden -PassThru -ArgumentList @(
            '-i',$targetBus,'--query-gpu=name,pci.device_id,pci.sub_device_id','--format=csv,noheader,nounits') `
            -RedirectStandardOutput $out -RedirectStandardError $err
        $nvmlOk=$false
        $queryHandle=$query.Handle
        if ($query.WaitForExit(5000)) {
            $query.Refresh()
            if ($query.ExitCode -eq 0) {
                $text = Get-Content -LiteralPath $out -Raw
                $nvmlOk = $text -match 'RTX 4070 Ti' -and $text -match '0x278210DE' -and $text -match '0x51321462'
            }
        } else { $query.Kill() } # only this test's hung health-query process
        $healthy = $device -and $device.Status -eq 'OK' -and $problem -eq 0 -and $kernel.State -eq 'Running' -and $nvmlOk
        $report.Snapshots += @{ ProblemCode=$problem; PnpStatus=if($device){[string]$device.Status}else{'absent'};
            KernelDriverState=[string]$kernel.State; NvmlIdentityOk=$nvmlOk; Healthy=[bool]$healthy }
        Save-Report
        if ($healthy -and $report.RestartCommandCompleted) { $consecutive++ } else { $consecutive=0 }
        if ($consecutive -ge 2) { $report.HealthRecovered=$true; break }
        Start-Sleep -Seconds 2
    }
    $bootAfter = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime.ToUniversalTime().Ticks
    $report.BootSessionUnchanged = $bootBefore -eq $bootAfter
    if ($report.HealthRecovered) {
        Set-Content -LiteralPath (Join-Path $directory 'controller-done') -Value 'healthy' -Encoding ASCII
    }
    $report.Phase=if($report.RestartCommandCompleted){'complete'}else{'uncertain'}; Save-Report
} catch {
    if (-not $restartIssued) { Set-Content -LiteralPath (Join-Path $directory 'controller-cancelled') -Value 'pre-trigger failure' -Encoding ASCII }
    $report.Phase='error'; $report.Error=$_.Exception.Message; Save-Report
    throw
} finally {
    $lock.Close()
    if (-not $restartIssued -or ($report.RestartCommandCompleted -and $report.HealthRecovered)) {
        Remove-Item -LiteralPath $lockPath -ErrorAction SilentlyContinue
    }
}
$report | ConvertTo-Json -Depth 8
# A fresh ordinary CUDA test is still required before declaring compute recovery.
