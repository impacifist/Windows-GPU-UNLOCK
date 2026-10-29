# ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md.
# GPL-2.0-only. Build-only diagnostics for this ASRock/RTX 3060 machine.
# Does not stage USB files, change boot settings, or build an unlock application.
param(
    [string]$Clang = "$PSScriptRoot/toolchain/bin/clang.exe",
    [string]$LldLink = "$PSScriptRoot/toolchain/bin/lld-link.exe",
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$board = @(Get-CimInstance Win32_BaseBoard)
$bios = @(Get-CimInstance Win32_BIOS)
$video = @(Get-CimInstance Win32_VideoController)
if ($board.Count -ne 1 -or $board[0].Manufacturer -ne 'ASRock' -or
    $board[0].Product -ne 'B450M Pro4-F' -or $bios.Count -ne 1 -or
    $bios[0].SMBIOSBIOSVersion -ne 'P10.31') {
    throw 'This diagnostic build targets ASRock B450M Pro4-F BIOS P10.31 only.'
}
$nvidia = @($video | Where-Object PNPDeviceID -Like 'PCI\VEN_10DE&*')
if ($nvidia.Count -ne 1 -or
    $nvidia[0].PNPDeviceID -notmatch '^PCI\\VEN_10DE&DEV_2503&SUBSYS_40741458&REV_A1\\') {
    throw 'Expected exactly one NVIDIA GPU: 10de:2503, subsystem 1458:4074, revision A1.'
}
$esp = @(Get-Partition | Where-Object IsSystem)
if ($esp.Count -ne 1 -or !$esp[0].Guid) {
    throw 'Could not unambiguously identify the Windows EFI system partition.'
}
& "$PSScriptRoot/research/profile/build-probe.ps1" -WindowsEspGuid $esp[0].Guid `
    -TargetBus 7 -Asrock3060 -Clang $Clang -LldLink $LldLink -Python $Python
if (!$?) { throw 'Diagnostic build failed.' }
$build = "$PSScriptRoot/internal-boot/boot/build/probe-generic"
$manifest = Get-Content "$build/manifest.json" -Raw | ConvertFrom-Json
foreach ($capability in 'CanUnlock', 'CanDetach', 'WritesFirmwareVariables', 'HardwareTested') {
    if ($manifest.$capability -isnot [bool] -or $manifest.$capability) {
        throw "Unexpected diagnostic capability: $capability"
    }
}
if ($manifest.EfiSha256 -cne (Get-FileHash "$build/BOOTX64.EFI").Hash.ToLowerInvariant()) {
    throw 'Diagnostic binary hash mismatch.'
}
$destination = "$PSScriptRoot/research/results/asrock-b450m-3060-diagnostic"
New-Item -ItemType Directory -Path $destination -Force | Out-Null
Copy-Item "$build/BOOTX64.EFI", "$build/manifest.json", "$build/pe-verification.json" $destination
[ordered]@{
    Board = 'ASRock B450M Pro4-F'; Bios = 'P10.31'
    PciId = '10de:2503'; Subsystem = '1458:4074'; TargetBus = 7
    WindowsEspGuid = [string]$esp[0].Guid
    Mode = 'experimental-targeted-read-only-capture'; CanUnlock = $false
    DeepGpuCaptureAvailable = $true; HardwareTested = $false
    Limitation = 'Experimental bounded reads of GA106 registers and PROM; no GPU writes or driver detach. Hardware validation pending. The automatic unlock-profile generator still refuses this GPU.'
    EfiSha256 = $manifest.EfiSha256
} | ConvertTo-Json | Set-Content "$destination/machine.json" -Encoding utf8
Write-Output "Built $destination. No USB, boot configuration, or GPU state was changed."
