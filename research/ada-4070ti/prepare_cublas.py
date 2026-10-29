"""Fetch/verify isolated NVIDIA user libraries. No installer or driver changes."""

import hashlib
import json
from pathlib import Path
import urllib.request
import zipfile

HERE = Path(__file__).resolve().parent


def digest(stream):
    value = hashlib.sha256()
    for block in iter(lambda: stream.read(1024*1024), b''):
        value.update(block)
    return value.hexdigest()


def main():
    record_path = HERE / 'cublas-package-provenance.json'
    record = json.loads(record_path.read_text())
    base = HERE / 'build/cuda-12.6'
    base.mkdir(parents=True, exist_ok=True)
    dlls = {}
    for item in record['components']:
        if item['component'] not in ('cuda_cudart', 'libcublas') or not item['url'].startswith(
                'https://developer.download.nvidia.com/compute/cuda/redist/'):
            raise ValueError('Unexpected package source')
        archive = base / item['url'].rsplit('/', 1)[1]
        if not archive.exists():
            with urllib.request.urlopen(item['url'], timeout=60) as source, archive.open('xb') as output:
                for block in iter(lambda: source.read(8*1024*1024), b''):
                    output.write(block)
        with archive.open('rb') as stream:
            if archive.stat().st_size != item['bytes'] or digest(stream) != item['sha256']:
                raise ValueError('Archive hash/size mismatch')
        destination = (base / item['component']).resolve()
        with zipfile.ZipFile(archive) as source:
            for name in source.namelist():
                if not (destination / name).resolve().is_relative_to(destination):
                    raise ValueError('Package path escapes isolated folder')
            if not destination.exists():
                source.extractall(destination)
            for name in source.namelist():
                if Path(name).name not in ('cudart64_12.dll', 'cublas64_12.dll', 'cublasLt64_12.dll'):
                    continue
                with source.open(name) as stream:
                    expected = digest(stream)
                path = destination / name
                with path.open('rb') as stream:
                    if digest(stream) != expected:
                        raise ValueError('Extracted DLL differs from verified archive')
                dlls[path.relative_to(base).as_posix()] = expected
        print('Verified', item['component'])
    if len(dlls) != 3:
        raise ValueError('Expected three selected NVIDIA libraries')
    record['dll_sha256'] = dlls
    record_path.write_bytes((json.dumps(record, indent=2)+'\n').encode())


if __name__ == '__main__':
    main()
