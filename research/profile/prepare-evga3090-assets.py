"""Prepare pinned GA102 assets locally; no GPU access, installation or EFI staging."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile

PIN = "0ebdb1c6308b6a2072be5bf04513472942541956"
ROOT = Path(__file__).resolve().parents[2]
DEFAULT = ROOT / "research/profile/build/evga-assets"

def run(*args, cwd=None):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def verify(output):
    specs = json.loads((ROOT / "internal-boot/unlock/assets.json").read_text())
    for item in specs:
        path = output / item["Name"]
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"Missing regular asset: {path.name}")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if path.stat().st_size != item["Bytes"] or digest != item["Sha256"]:
            raise ValueError(f"Asset mismatch: {path.name}")
    return specs

def prepare(source, output):
    if sys.version_info < (3, 14):
        raise ValueError("Use Python3.14+ for native zstd archive support")
    if output.exists() and any(output.iterdir()):
        raise ValueError("Output is nonempty; preserve it and use --verify-only or a new output directory")
    if not source.exists():
        source.parent.mkdir(parents=True, exist_ok=True)
        run("git", "clone", "--filter=blob:none", "--no-checkout", "https://github.com/Zagitta/gpu-unlock.git", str(source))
        run("git", "checkout", "--detach", PIN, cwd=source)
    if run("git", "rev-parse", "HEAD", cwd=source) != PIN:
        raise ValueError("Source checkout is not the pinned revision")
    run("git", "diff", "--exit-code", "HEAD", "--", "firmware", cwd=source)
    firmware = source / "firmware"
    fetch = load("evga_asset_fetch", firmware / "fetch_artifacts.py")
    manifest = json.loads(fetch.MANIFEST.read_text())
    source_archive, package = [fetch.fetch(item, source / "inputs") for item in fetch.inputs(manifest)]
    # New extraction directory each time; no recursive deletion or replacement.
    import tempfile
    extraction = Path(tempfile.mkdtemp(prefix="source-", dir=source))
    with tarfile.open(source_archive) as archive:
        archive.extractall(extraction, filter="data")
    generate = load("evga_asset_generate", firmware / "generate_v67.py")
    def native_member(package_path, name):
        with tarfile.open(package_path, mode="r:*") as archive:
            return generate.read_member(archive, name)
    # Keep pinned parsing, package identity, payload construction and hash checks.
    # Replace only the external decompressor with Python's native zstd support.
    generate.extract_member = native_member
    old_argv = sys.argv
    try:
        sys.argv = [str(firmware / "generate_v67.py"), str(package), str(output)]
        result = generate.main()
        if result:
            raise ValueError(f"Payload generation failed: {result}")
    finally:
        sys.argv = old_argv
    extract = load("evga_boot_extract", firmware / "extract_boot_assets.py")
    extract.extract_assets(extraction / source_archive.name.removesuffix(".tar.xz"), output)
    specs = verify(output)
    shutil.copyfile(firmware / "NOTICE.md", output / "NOTICE.md")
    (output / "verified-assets.json").write_text(json.dumps({"source_revision": PIN, "assets": specs}, indent=2) + "\n", encoding="utf8")
    return specs

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT / "source")
    parser.add_argument("--output", type=Path, default=DEFAULT / "output")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    try:
        specs = verify(args.output) if args.verify_only else prepare(args.source.resolve(), args.output.resolve())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Asset preparation refused: {error}\n")
    print(json.dumps({"output": str(args.output.resolve()), "verified_assets": specs,
                      "gpu_accessed": False, "efi_staged": False}, indent=2))
    print("Keep NVIDIA-LICENSE and NOTICE.md with the assets; review them before use. Do not upload firmware.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
