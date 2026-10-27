"""Run the pinned upstream firmware/generate_v67.py on Windows without WSL.

Upstream pipes the .tar.zst package through an external `zstd` binary. Python 3.14's tarfile
reads zstd natively, so this wrapper swaps only that reader; every upstream package, firmware,
payload and license hash check still runs unchanged.

    py -3.14 -I tools\\generate_v67_windows.py <gpu-unlock checkout> <nvidia-utils .pkg.tar.zst> <assets dir>
"""
import importlib.util
import sys
import tarfile
from pathlib import Path

if sys.version_info < (3, 14):
    raise SystemExit('Python 3.14 or newer is required (tarfile zstd support)')
if len(sys.argv) != 4:
    raise SystemExit(__doc__)
checkout, package, assets = (Path(a) for a in sys.argv[1:])
script = checkout / 'firmware' / 'generate_v67.py'
spec = importlib.util.spec_from_file_location('generate_v67', script)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def extract_member(path, name):
    with tarfile.open(path, mode='r:zst') as archive:
        return module.read_member(archive, name)


module.extract_member = extract_member
sys.argv = [str(script), str(package), str(assets)]
raise SystemExit(module.main())
