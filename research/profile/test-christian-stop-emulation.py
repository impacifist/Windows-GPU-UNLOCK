"""Execute three exact MSI text-Stop argument paths in isolated CPU emulation.
Requires developer-only unicorn; never executes firmware natively or accesses hardware.
"""
import argparse
import importlib.util
import json
from pathlib import Path
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RSP, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RAX

spec = importlib.util.spec_from_file_location('pins', Path(__file__).with_name('inspect-evga3090-console-code.py'))
code = importlib.util.module_from_spec(spec)
spec.loader.exec_module(code)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('captures', type=Path)
args = parser.parse_args()
rows = []
for log in sorted(args.captures.glob('DIAG??.TXT')):
    role = code.fingerprint(args.captures, log.read_text(encoding='utf-8-sig'))['text_console']
    if (role['code_sha256'], role['stop_rva'], role['version']) != ('3a14de3d645d704fdd327e9e814c7b8928d98ee734d777823d3abe389d628aaa', 4284, 16):
        raise ValueError('Exact MSI text driver differs')
    image = (args.captures / role['dump']).read_bytes()
    for children, expected in [(0, 0), (2, 0x8000000000000002), (1, 0x8000000000000002)]:
        uc = Uc(UC_ARCH_X86, UC_MODE_64)
        base, stack, stop = 0x10000000, 0x20000000, 0x30000000
        uc.mem_map(base, (len(image)+4095)//4096*4096)
        uc.mem_write(base, image)
        uc.mem_map(stack, 0x10000)
        uc.mem_map(stop, 0x1000)
        sp = stack + 0x8008
        uc.mem_write(sp, stop.to_bytes(8, 'little'))
        for reg, value in [(UC_X86_REG_RSP, sp), (UC_X86_REG_RCX, 0), (UC_X86_REG_RDX, 0), (UC_X86_REG_R8, children), (UC_X86_REG_R9, 0)]:
            uc.reg_write(reg, value)
        seen = []
        def hook(engine, address, size, data):
            if not base + role['stop_rva'] <= address < base + 0x1300:
                raise AssertionError('Unexpected control transfer outside reviewed Stop range')
            seen.append(address-base)
        uc.hook_add(UC_HOOK_CODE, hook)
        uc.emu_start(base + role['stop_rva'], stop, count=1000)
        actual = uc.reg_read(UC_X86_REG_RAX)
        if uc.reg_read(UC_X86_REG_RSP) != sp+8 or actual != expected:
            raise AssertionError('Stop did not return expected status')
        rows.append({'capture': log.name, 'children': children, 'null_child_buffer': True, 'status': hex(actual), 'instructions': len(seen)})
if len(rows) != 6:
    raise ValueError('Expected two captures and six emulated cases')
print(json.dumps({'test': 'msi-text-stop-argument-emulation-v1', 'results': rows, 'native_firmware_executed': False, 'hardware_accessed': False, 'one_child_release_qualified': False, 'unlock_proven': False}, indent=2))
