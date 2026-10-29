# ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md.
"""Run the production VBIOS parser on a local 1 MiB PROM through a host DLL."""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path


class Firmware(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint32) for name in (
        'image_off', 'image_size', 'imem_pa', 'imem_size', 'imem_va',
        'dmem_pa', 'data_off', 'dmem_size', 'pkc_off', 'interface_off',
        'ucode_id', 'engine_mask', 'sig_count', 'sig_versions', 'sig_off', 'sig_bytes')]


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('parser', type=Path)
    args.add_argument('prom', type=Path)
    args = args.parse_args()
    data = args.prom.read_bytes()
    if len(data) != 1048576:
        raise ValueError('Expected exactly 1 MiB of PROM data')
    library = ctypes.CDLL(str(args.parser.resolve()))
    parse = library.gpu_unlock_vbios_fwsec
    parse.argtypes = [ctypes.POINTER(ctypes.c_ubyte), ctypes.c_size_t, ctypes.POINTER(Firmware)]
    parse.restype = ctypes.c_int
    image = (ctypes.c_ubyte * len(data)).from_buffer_copy(data)
    firmware = Firmware()
    result = parse(image, len(data), ctypes.byref(firmware))
    if result:
        raise ValueError(f'Production VBIOS parser refused PROM: {result}')
    print(json.dumps({
        'prom_sha256': hashlib.sha256(data).hexdigest(),
        'parser_sha256': hashlib.sha256(args.parser.read_bytes()).hexdigest(),
        'fwsec': {name: getattr(firmware, name) for name, _ in Firmware._fields_},
        'can_unlock': False,
    }, indent=2))


if __name__ == '__main__':
    main()
