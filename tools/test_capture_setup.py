"""Host-only tests for the guided capture input and storage-selection boundaries."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import sys
import json

PWSH = shutil.which("pwsh") or shutil.which("powershell")
HELPER = Path(__file__).with_name("capture-setup.ps1")


@unittest.skipUnless(PWSH, "PowerShell required")
class GuidedSetupTests(unittest.TestCase):
    def run_wizard(self, confirmation, expect_stage, shell=PWSH):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tools = root / 'tools'
            profile = root / 'research/profile'
            tools.mkdir()
            profile.mkdir(parents=True)
            shutil.copyfile(HELPER.with_name('start-capture.ps1'), tools / 'start-capture.ps1')
            marker = root / 'staged.txt'
            q = lambda x: "'" + str(x).replace("'", "''") + "'"
            helper = HELPER.read_text() + f"""
function Find-CaptureTool {{param($Name) return {q(sys.executable)}}}
function Get-CaptureWindowsPartition {{[pscustomobject]@{{Guid=[guid]'11111111-1111-1111-1111-111111111111';DiskNumber=1}}}}
function Get-CaptureUsbChoices {{[pscustomobject]@{{Drive='D';Label='TEST';Model='Test USB';SizeGiB=64;Eligible=$true;FileSystem='FAT32'}}}}
function Assert-CaptureUsbUnchanged {{param($Selected)}}
function Get-CaptureProgress {{param($Drive) [pscustomobject]@{{Count=2;Required=2;Directory='test-only';Complete=$true}}}}
$script:answers=@('1',{q(confirmation)})
$script:answerIndex=0
function Read-Host {{param($Prompt) $answer=$script:answers[$script:answerIndex]; $script:answerIndex++; return $answer}}
"""
            (tools / 'capture-setup.ps1').write_text(helper)
            report = {'checks': [{'tool': 'fixture', 'version': 'test-only'}]}
            (tools / 'check_prerequisites.py').write_text('print(' + repr(json.dumps(report)) + ')')
            (profile / 'new-machine.ps1').write_text(f"""
param($Drive,$WindowsEspGuid,[switch]$CaptureOnly,$Clang,$LldLink,$Python,$Assets,[switch]$StageUnlock)
if(!$CaptureOnly -or $Assets -or $StageUnlock){{throw 'Unsafe dispatch'}}
'mock only' | Set-Content -LiteralPath {q(marker)}
""")
            run = subprocess.run([shell, '-NoProfile', '-File', str(tools / 'start-capture.ps1')], capture_output=True, text=True, timeout=20)
            self.assertEqual(marker.exists(), expect_stage, run.stdout + run.stderr)
            self.assertEqual(run.returncode, 0 if expect_stage else 1, run.stdout + run.stderr)
            if expect_stage:
                self.assertIn('never continues into unlock', run.stdout)
                self.assertIn('No more boots are requested', run.stdout)

    def test_wizard_cancellation_never_dispatches_staging(self):
        self.run_wizard('', False)

    def test_wizard_dispatches_capture_only_after_confirmation(self):
        self.run_wizard('PREPARE', True)

    @unittest.skipUnless(shutil.which('powershell.exe'), 'Windows PowerShell not installed')
    def test_launcher_shell_can_run_guided_flow(self):
        self.run_wizard('PREPARE', True, shutil.which('powershell.exe'))

    def run_ps(self, body):
        source = "'" + str(HELPER).replace("'", "''") + "'"
        script = f"""
$ErrorActionPreference='Stop'
. {source}
$script:disk=[pscustomobject]@{{Number=7;BusType='USB';IsBoot=$false;IsSystem=$false;IsOffline=$false;IsReadOnly=$false;FriendlyName='Test USB';UniqueId='test-id';PartitionStyle='GPT'}}
$script:part=[pscustomobject]@{{DiskNumber=7;PartitionNumber=1;DriveLetter='D';IsSystem=$false;IsBoot=$false;Offset=1048576;Size=64000000000;Guid='11111111-1111-1111-1111-111111111111'}}
$script:vol=[pscustomobject]@{{FileSystem='FAT32';FileSystemLabel='TEST';Size=64000000000}}
function Get-Disk {{param($Number) $script:disk}}
function Get-Partition {{param($DiskNumber) $script:part}}
function Get-Volume {{param($DriveLetter) $script:vol}}
function Assert-True($Value) {{if(!$Value){{throw 'Assertion failed'}}}}
function Assert-Refused([scriptblock]$Action,[string]$Expected) {{
 try {{& $Action}} catch {{if($_.Exception.Message -like "*$Expected*"){{return}};throw}}
 throw 'Expected refusal did not happen'
}}
{body}
"""
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "test.ps1"
            path.write_text(script)
            run = subprocess.run([PWSH, "-NoProfile", "-File", str(path)], capture_output=True, text=True, timeout=20)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_only_eligible_usb_is_selected(self):
        self.run_ps("$a=@(Get-CaptureUsbChoices); Assert-True ($a.Count -eq 1); Assert-True $a[0].Eligible; $s=Select-CaptureUsb $a '1'; Assert-True ($s.Drive -eq 'D')")

    def test_empty_and_out_of_range_input_cancel(self):
        self.run_ps("$a=@(Get-CaptureUsbChoices); foreach($v in @('','0','2','abc','99999999999999999999')){Assert-Refused {Select-CaptureUsb $a $v} 'cancelled'}")

    def test_unsafe_disks_are_unselectable(self):
        for state in ("$script:disk.IsBoot=$true", "$script:part.IsSystem=$true", "$script:disk.IsReadOnly=$true", "$script:disk.IsOffline=$true", "$script:disk.UniqueId=''", "$script:part.DriveLetter='C'", "$script:vol.FileSystem='NTFS'"):
            with self.subTest(state=state):
                self.run_ps(state + "; $a=@(Get-CaptureUsbChoices); Assert-True (!$a[0].Eligible); Assert-Refused {Select-CaptureUsb $a '1'} 'cannot be used'")

    def test_drive_replacement_is_refused_before_staging(self):
        self.run_ps("$s=(Get-CaptureUsbChoices)[0]; $script:disk.UniqueId='another-device'; Assert-Refused {Assert-CaptureUsbUnchanged $s} 'selection changed'")

    def test_unchanged_selection_passes(self):
        self.run_ps("$s=(Get-CaptureUsbChoices)[0]; Assert-CaptureUsbUnchanged $s")

    def test_no_usb_and_no_drive_letter(self):
        self.run_ps("$script:disk.BusType='SATA'; Assert-True (@(Get-CaptureUsbChoices).Count -eq 0); $script:disk.BusType='USB'; $script:part.DriveLetter=$null; Assert-True (@(Get-CaptureUsbChoices).Count -eq 0)")

    def test_windows_gpt_partition_detected(self):
        self.run_ps("$script:part.IsSystem=$true; $script:disk.BusType='NVMe'; $w=Get-CaptureWindowsPartition; Assert-True ($w.Guid -eq [guid]$script:part.Guid)")

    def test_ambiguous_or_missing_system_partition_refused(self):
        self.run_ps("Assert-Refused {Get-CaptureWindowsPartition} 'ambiguous'; $script:part=@($script:part.PSObject.Copy(),$script:part.PSObject.Copy()); $script:part | ForEach-Object {$_.IsSystem=$true}; Assert-Refused {Get-CaptureWindowsPartition} 'ambiguous'")

    def test_unsupported_windows_boot_layouts_refused(self):
        for state in ("$script:disk.PartitionStyle='MBR'", "$script:part.Guid='invalid'", "$script:part.Guid=[guid]::Empty", "$script:disk.BusType='USB'"):
            with self.subTest(state=state):
                self.run_ps("$script:part.IsSystem=$true; $script:disk.BusType='NVMe'; " + state + "; Assert-Refused {Get-CaptureWindowsPartition} 'needs a GPT'")


if __name__ == "__main__":
    unittest.main()
