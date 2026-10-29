# ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md.
"""Use pinned upstream asset generation with Python 3.14's Zstandard reader."""
import importlib.util
from pathlib import Path
import sys
import tarfile

root = Path(__file__).resolve().parents[2]
source = root / 'firmware-source/firmware/generate_v67.py'
spec = importlib.util.spec_from_file_location('pinned_asset_generator', source)
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


def extract_member(package, name):
    with tarfile.open(package, mode='r:*') as archive:
        return generator.read_member(archive, name)


generator.extract_member = extract_member
sys.argv = [str(source), str(root / 'firmware-source/inputs/nvidia-utils-610.43.03-1-x86_64_v3.pkg.tar.zst'),
            str(root / 'assets')]
raise SystemExit(generator.main())
