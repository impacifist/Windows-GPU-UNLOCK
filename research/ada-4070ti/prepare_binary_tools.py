"""Fetch two pinned portable NVIDIA tools into ignored build storage."""
import hashlib
import json
from pathlib import Path
import urllib.request
import zipfile

HERE = Path(__file__).resolve().parent


def main():
    base = HERE / 'build/binary-tools-12.6'
    base.mkdir(parents=True, exist_ok=True)
    record = json.loads((HERE / 'binary-tools-provenance.json').read_text())
    for item in record['components']:
        if item['tool'] not in ('ptxas.exe', 'nvdisasm.exe') or not item['url'].startswith(
                'https://developer.download.nvidia.com/compute/cuda/redist/'):
            raise ValueError('Unexpected tool/source')
        archive = base / item['url'].rsplit('/', 1)[1]
        if not archive.exists():
            with urllib.request.urlopen(item['url'], timeout=60) as source, archive.open('xb') as output:
                while block := source.read(1024*1024):
                    output.write(block)
        if archive.stat().st_size != item['archive_bytes'] or hashlib.sha256(
                archive.read_bytes()).hexdigest() != item['archive_sha256']:
            raise ValueError('Archive verification failed')
        with zipfile.ZipFile(archive) as package:
            names = [name for name in package.namelist() if Path(name).name == item['tool']]
            if len(names) != 1:
                raise ValueError('Ambiguous executable')
            data = package.read(names[0])
        if hashlib.sha256(data).hexdigest() != item['tool_sha256']:
            raise ValueError('Tool verification failed')
        (base / item['tool']).write_bytes(data)
        print('Verified', item['tool'])


if __name__ == '__main__':
    main()
