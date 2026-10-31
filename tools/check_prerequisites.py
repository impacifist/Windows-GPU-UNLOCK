"""GPL-2.0-only. Host tool discovery only; no build, staging or hardware access."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def resolve_tool(value):
    found = shutil.which(value)
    return str(Path(found).resolve()) if found else None


def check_tool(name, value):
    path = resolve_tool(value)
    if not path:
        return {"tool": name, "ok": False, "requested": value,
                "error": "Not found. Reopen your terminal after installation or pass the full executable path."}
    try:
        run = subprocess.run([path, "--version"], capture_output=True, text=True,
                             errors="replace", timeout=10, check=False)
        output = (run.stdout + run.stderr).strip()
        expected = r"clang version" if name == "clang" else r"\bLLD\s+\d"
        ok = run.returncode == 0 and bool(re.search(expected, output, re.I))
        return {"tool": name, "ok": ok, "path": path,
                "version": output.splitlines()[0] if output else "",
                **({} if ok else {"error": "Expected LLVM tool version output was not returned."})}
    except (OSError, subprocess.TimeoutExpired) as exc:
        return {"tool": name, "ok": False, "path": path, "error": str(exc)}


def find_vcvars(explicit=None):
    if explicit:
        path = Path(explicit)
        return str(path.resolve()) if path.is_file() else None
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    if not vswhere.is_file():
        return None
    try:
        run = subprocess.run([str(vswhere), "-latest", "-products", "*", "-find",
                              r"VC\Auxiliary\Build\vcvars64.bat"], capture_output=True,
                             text=True, timeout=10, check=False)
        if run.returncode == 0:
            return next((line for line in run.stdout.splitlines() if Path(line).is_file()), None)
    except (OSError, subprocess.TimeoutExpired):
        pass
    return None


def report(args):
    checks = [{"tool": "python", "ok": sys.version_info >= (3, 10),
               "path": sys.executable, "version": sys.version.split()[0]},
              check_tool("clang", args.clang), check_tool("lld-link", args.lld_link)]
    if args.phase == "profile":
        path = find_vcvars(args.vcvars)
        checks.append({"tool": "MSVC environment", "ok": bool(path), "path": path,
                       "note": "Discovery only; vcvars was not run and host linking has not been tested."})
    return {"schema_version": 1, "phase": args.phase, "ok": all(c["ok"] for c in checks),
            "checks": checks, "scope": "Host discovery/version checks only. No compiler feature, build, USB, boot, asset or hardware qualification."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phase", choices=("capture", "profile"), default="capture")
    parser.add_argument("--clang", default="clang")
    parser.add_argument("--lld-link", default="lld-link")
    parser.add_argument("--vcvars")
    args = parser.parse_args()
    result = report(args)
    print(json.dumps(result, indent=2))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
