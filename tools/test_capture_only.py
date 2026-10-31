"""Run production orchestration in an isolated fake storage/tool environment."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PWSH = shutil.which("pwsh") or shutil.which("powershell")


def quote(value):
    return "'" + str(value).replace("'", "''") + "'"


@unittest.skipUnless(PWSH, "PowerShell is required for orchestration tests")
class CaptureOnlyTests(unittest.TestCase):
    def run_case(self, arguments, expected, storage=True, complete=True):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            profile = root / "research/profile"
            profile.mkdir(parents=True)
            unlock = root / "internal-boot/unlock"
            unlock.mkdir(parents=True)
            (unlock / "local-profile.ps1").write_text("# No hardware or tools in this fixture\n")
            (profile / "host.ps1").write_text("throw 'POST_CAPTURE_HOST_SCRIPT_REACHED'\n" if not storage else "# fixture\n")
            (profile / "proven.json").write_text('{"min_captures":2}')
            (profile / "build-probe.ps1").write_text("throw 'PROBE_BUILD_REACHED_WITHOUT_ASSETS'\n")
            script = profile / "new-machine.ps1"
            shutil.copyfile(ROOT / "research/profile/new-machine.ps1", script)
            logs = []
            for n in range(2):
                log = root / f"DIAG{n:02}.TXT"
                log.write_bytes(b"probe-pci-handles=1\r\ncapture-complete=1\r\n")
                logs.append(log)
            mock = f"""
$ErrorActionPreference='Stop'
function Get-Partition {{ [pscustomobject]@{{DiskNumber=7;IsSystem=$false}} }}
function Get-Disk {{ [pscustomobject]@{{BusType='USB';IsBoot=$false}} }}
function Get-Volume {{ [pscustomobject]@{{FileSystem='FAT32'}} }}
function Test-Path {{ param($LiteralPath) if($LiteralPath -eq 'D:/RTX3090-UNLOCK' -or $LiteralPath -eq 'D:\\RTX3090-UNLOCK'){{return {'$true' if complete else '$false'}}}; throw "UNEXPECTED_PATH: $LiteralPath" }}
function Get-ChildItem {{ param($LiteralPath,$Filter,[switch]$File,$ErrorAction)
 if($Filter -ne 'DIAG??.TXT'){{throw 'UNEXPECTED_ENUMERATION'}}
 Get-Item -LiteralPath {quote(logs[0])},{quote(logs[1])}
}}
try {{ & {quote(script)} -Drive D -WindowsEspGuid '11111111-1111-1111-1111-111111111111' {arguments}; exit 0 }}
catch {{ Write-Output $_.Exception.Message; exit 23 }}
"""
            harness = root / "run.ps1"
            harness.write_text(mock)
            run = subprocess.run([PWSH, "-NoProfile", "-File", str(harness)], capture_output=True, text=True, timeout=20)
            self.assertIn(expected, run.stdout + run.stderr)
            return run.returncode

    def test_complete_capture_only_stops_before_profile_and_assets(self):
        self.assertEqual(self.run_case("-CaptureOnly", "No profile generation"), 0)

    def test_initial_capture_without_assets_reaches_only_probe_builder(self):
        self.assertEqual(self.run_case("-CaptureOnly", "PROBE_BUILD_REACHED_WITHOUT_ASSETS", complete=False), 23)

    def test_assets_required_before_automatic_profile_continuation(self):
        self.assertEqual(self.run_case("", "Captures are ready"), 23)

    def test_capture_and_unlock_cannot_be_combined(self):
        self.assertEqual(self.run_case("-CaptureOnly -StageUnlock", "cannot be combined", False), 23)

    def test_capture_does_not_accept_assets(self):
        self.assertEqual(self.run_case("-CaptureOnly -Assets missing", "cannot be combined", False), 23)

    def test_unlock_staging_without_assets_fails_before_storage(self):
        self.assertEqual(self.run_case("-StageUnlock", "StageUnlock requires", False), 23)


if __name__ == "__main__":
    unittest.main()
