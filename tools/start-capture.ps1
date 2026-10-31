# GPL-2.0-only. Interactive capture setup; never selects an unlock stage.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'capture-setup.ps1')
try {
 Write-Host "`nWindows GPU diagnostics - capture setup"
 Write-Host 'This prepares a USB for diagnostics. It does not unlock your GPU or restart this PC.'
 Write-Host 'No firmware assets or MSVC installation are needed for this capture stage.'
 Write-Host "`n1/4 Checking your build tools..."
 $tools=@{}
 foreach($name in @('python','clang','lld-link')) {
  $path=Find-CaptureTool $name
  if(!$path) {
   Write-Host "$name was not found. Install the missing dependency, then reopen this window."
   Write-Host 'Python: https://www.python.org/downloads/windows/'
   Write-Host 'LLVM (clang and lld-link): https://github.com/llvm/llvm-project/releases'
   Write-Host 'Already installed? Paste the full executable path below. Do not include quotation marks.'
   $path=Read-Host "$name path (Enter to cancel)"
   if(!$path -or !(Test-Path -LiteralPath $path -PathType Leaf)){throw 'Tool unavailable. Nothing was staged. See docs/CAPTURE-SETUP.en.md for help.'}
  }
  $tools[$name]=$path
 }
 $raw=& $tools.python (Join-Path $PSScriptRoot 'check_prerequisites.py') --phase capture --clang $tools.clang --lld-link $tools['lld-link']
 $code=$LASTEXITCODE
 if($code -ne 0){Write-Host ($raw -join "`n");throw 'Tool checks did not pass. Nothing was staged. Review the missing tool or version above.'}
 $report=($raw -join "`n") | ConvertFrom-Json
 foreach($check in $report.checks){Write-Host ("Found {0}: {1}" -f $check.tool,$check.version)}
 Write-Host "`n2/4 Finding the Windows boot partition..."
 $windows=Get-CaptureWindowsPartition
 Write-Host "Detected Windows boot partition on internal disk $($windows.DiskNumber). No GUID copying is needed."
 Write-Host "`n3/4 Choose your USB. Back up any files you care about first."
 $choices=@(Get-CaptureUsbChoices)
 if(!$choices.Count){throw 'No USB volumes with drive letters were found. Connect a FAT32 USB and try again. No disk was changed.'}
 for($i=0;$i -lt $choices.Count;$i++) {
  $c=$choices[$i];$state=if($c.Eligible){'available'}else{'unavailable: '+$c.Reason}
  Write-Host ("{0}. {1}: {2} | {3} | {4} GiB | {5} | {6}" -f ($i+1),$c.Drive,$c.Label,$c.Model,$c.SizeGiB,$c.FileSystem,$state)
 }
 $selected=Select-CaptureUsb $choices (Read-Host 'Type the USB number (Enter to cancel)')
 Write-Host "`n4/4 Ready to prepare $($selected.Drive): $($selected.Label) ($($selected.Model), $($selected.SizeGiB) GiB)."
 Write-Host 'This builds and copies diagnostic boot files. An existing boot loader is backed up by the staging script.'
 Write-Host 'It does not format the USB. The USB will contain a bootable hardware diagnostic, which can hang when booted.'
 if((Read-Host 'Type PREPARE to continue, or Enter to cancel') -cne 'PREPARE'){throw 'Cancelled. Nothing was staged.'}
 Assert-CaptureUsbUnchanged $selected
 $now=Get-CaptureWindowsPartition
 if($now.Guid -ne $windows.Guid){throw 'Windows boot partition changed. Start setup again.'}
 & (Join-Path $PSScriptRoot '../research/profile/new-machine.ps1') -Drive $selected.Drive -WindowsEspGuid $windows.Guid -CaptureOnly -Clang $tools.clang -LldLink $tools['lld-link'] -Python $tools.python
 if(!$?){throw 'Diagnostic preparation failed. Do not boot the USB until the error has been reviewed.'}
 $progress=Get-CaptureProgress $selected.Drive
 Write-Host "`nComplete diagnostic logs: $($progress.Count) of $($progress.Required). This is file completeness, not a compatibility verdict."
 if($progress.Complete) {
  Write-Host "Collection complete. No more boots are requested by this setup. Keep the original files in $($progress.Directory)."
  Write-Host 'Ask a maintainer to review your results before choosing any further test. Do not publicly upload PROM.BIN or IMG files.'
 } else {
 Write-Host 'Next: read docs/CAPTURE-SETUP.en.md before booting. Shut down fully, then select this USB in your firmware boot menu.'
 Write-Host 'The probe attempts to start Windows after collection. Repeat from a separate full shutdown as instructed.'
 Write-Host 'If it hangs, power off and remove the USB before selecting normal Windows Boot Manager. Recovery is not guaranteed.'
 Write-Host 'After returning to Windows, run Start-Capture.cmd again and choose the same USB.'
 }
 Write-Host 'When collection is complete, keep the original files. This setup never continues into unlock.'
} catch {
 Write-Host "`nSetup stopped: $($_.Exception.Message)" -ForegroundColor Yellow
 exit 1
}
