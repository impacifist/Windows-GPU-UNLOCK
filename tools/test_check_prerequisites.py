"""Host-only discovery regression tests; no compiler or hardware execution."""
import argparse
import subprocess
import unittest
from unittest.mock import patch
import check_prerequisites as doctor


class DoctorTests(unittest.TestCase):
    def test_missing_tool_is_actionable(self):
        with patch.object(doctor.shutil, "which", return_value=None):
            result = doctor.check_tool("clang", "missing-clang")
        self.assertFalse(result["ok"])
        self.assertIn("full executable path", result["error"])

    def test_explicit_path_with_spaces_is_one_argument(self):
        path = str(doctor.Path("compiler directory/clang.exe").resolve())
        with patch.object(doctor, "resolve_tool", return_value=path), patch.object(
                doctor.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "clang version 20.1\n", "")) as run:
            self.assertTrue(doctor.check_tool("clang", path)["ok"])
        self.assertEqual(run.call_args.args[0], [path, "--version"])

    def test_wrong_tool_and_nonzero_exit_are_rejected(self):
        for code, output in [(0, "some other tool"), (1, "LLD 20.1")]:
            with self.subTest(code=code), patch.object(doctor, "resolve_tool", return_value="lld-link"), patch.object(
                    doctor.subprocess, "run", return_value=subprocess.CompletedProcess([], code, output, "")):
                self.assertFalse(doctor.check_tool("lld-link", "lld-link")["ok"])

    def test_timeout_is_a_failed_check(self):
        with patch.object(doctor, "resolve_tool", return_value="clang"), patch.object(
                doctor.subprocess, "run", side_effect=subprocess.TimeoutExpired("clang", 10)):
            self.assertFalse(doctor.check_tool("clang", "clang")["ok"])

    def test_capture_does_not_require_msvc(self):
        args = argparse.Namespace(phase="capture", clang="clang", lld_link="lld-link", vcvars=None)
        with patch.object(doctor, "check_tool", return_value={"ok": True}), patch.object(doctor, "find_vcvars") as find:
            self.assertTrue(doctor.report(args)["ok"])
            find.assert_not_called()

    def test_profile_requires_msvc_discovery(self):
        args = argparse.Namespace(phase="profile", clang="clang", lld_link="lld-link", vcvars="missing.bat")
        with patch.object(doctor, "check_tool", return_value={"ok": True}), patch.object(doctor, "find_vcvars", return_value=None):
            self.assertFalse(doctor.report(args)["ok"])


if __name__ == "__main__":
    unittest.main()
