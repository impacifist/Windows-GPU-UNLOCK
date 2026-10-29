"""Compile/disassemble or execute exact hashed stock-compute cubins."""
import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
import re
import subprocess

import gpu_accumulation_probe as probe
from cuda_driver import Cuda

HERE = Path(__file__).resolve().parent
BUILD = HERE / 'build/binary-probe'
TOOLS = HERE / 'build/binary-tools-12.6'
MANIFEST = HERE / 'gpu-binary-manifest.json'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_tools():
    record = json.loads((HERE / 'binary-tools-provenance.json').read_text())
    for item in record['components']:
        if item['tool'] not in ('ptxas.exe', 'nvdisasm.exe') or sha(TOOLS / item['tool']) != item['tool_sha256']:
            raise ValueError('Tool provenance mismatch')


def prepare():
    verify_tools()
    BUILD.mkdir(parents=True, exist_ok=True)
    report = {'target': 'sm_89', 'limit': 'Static disassembly of offline-compiled kernels; not prior driver-JIT code.',
              'kernels': {}, 'source_sha256': sha(Path(__file__))}
    for mode, source in [('f16', probe.ptx_f16()), ('f32', probe.ptx_f32())]:
        ptx, cubin = BUILD / (mode + '.ptx'), BUILD / (mode + '.cubin')
        ptx.write_text(source, encoding='ascii', newline='\n')
        compiled = subprocess.run([str(TOOLS / 'ptxas.exe'), '-arch=sm_89', '-O3', '-v', str(ptx), '-o', str(cubin)],
                                  capture_output=True, text=True, check=True, timeout=30)
        disasm = subprocess.run([str(TOOLS / 'nvdisasm.exe'), str(cubin)], capture_output=True,
                                text=True, check=True, timeout=30).stdout
        sass = HERE / ('gpu-binary-' + mode + '.sass')
        sass.write_text(disasm, encoding='utf-8', newline='\n')
        instructions = [line.strip() for line in disasm.splitlines() if re.search(r'/\*[0-9a-fA-F]+\*/', line)]
        mma = [line for line in instructions if 'HMMA.' in line]
        if not mma or any('IMMA.' in line for line in instructions):
            raise ValueError('Unexpected matrix instruction family')
        report['kernels'][mode] = {'ptx_sha256': sha(ptx), 'cubin_sha256': sha(cubin),
            'cubin_bytes': cubin.stat().st_size, 'sass_file': sass.name, 'sass_sha256': sha(sass),
            'compiler_log': compiled.stdout + compiled.stderr,
            'static_hmma_instructions': mma,
            'register_count': int(re.search(r'SHI_REGISTERS=(\d+)', disasm).group(1)),
            'static_instruction_count': len(instructions)}
    MANIFEST.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps(report, indent=2))


class BinaryCuda(Cuda):
    loaded_metadata = {}

    def load(self, ptx, name):
        manifest = json.loads(MANIFEST.read_text())
        digest = hashlib.sha256(ptx.encode('ascii')).hexdigest()
        matches = [(mode, item) for mode, item in manifest['kernels'].items() if item['ptx_sha256'] == digest]
        if len(matches) != 1 or name != 'tensor':
            raise ValueError('Unexpected requested kernel')
        mode, item = matches[0]
        binary = (BUILD / (mode + '.cubin')).read_bytes()
        if hashlib.sha256(binary).hexdigest() != item['cubin_sha256']:
            raise ValueError('Cubin changed before module loading')
        module, function = c.c_void_p(), c.c_void_p()
        image = c.create_string_buffer(binary)
        self.call('cuModuleLoadData', c.byref(module), image)
        self.modules.append(module)
        self.call('cuModuleGetFunction', c.byref(function), module, name.encode('ascii'))
        self.lib.cuFuncGetAttribute.argtypes = [c.POINTER(c.c_int), c.c_int, c.c_void_p]
        self.lib.cuFuncGetAttribute.restype = c.c_int
        self.lib.cuOccupancyMaxActiveBlocksPerMultiprocessor.argtypes = [
            c.POINTER(c.c_int), c.c_void_p, c.c_int, c.c_size_t]
        self.lib.cuOccupancyMaxActiveBlocksPerMultiprocessor.restype = c.c_int
        metadata = {}
        for label, attribute in [('registers', 4), ('local_bytes_per_thread', 3),
                                 ('static_shared_bytes', 1), ('binary_version', 6)]:
            value = c.c_int()
            self.call('cuFuncGetAttribute', c.byref(value), attribute, function)
            metadata[label] = value.value
        blocks = c.c_int()
        self.call('cuOccupancyMaxActiveBlocksPerMultiprocessor', c.byref(blocks), function, 256, 0)
        metadata['predicted_max_active_blocks_per_sm_at_256_threads'] = blocks.value
        metadata['limit'] = 'Resource-based occupancy prediction; not measured achieved occupancy.'
        self.loaded_metadata[mode] = metadata
        return function


def run():
    # Reuse the original bounded grid, iterations, event timing and full-output
    # validation. Only module loading changes; capture the existing JSON report.
    import contextlib
    import io
    probe.Cuda = BinaryCuda
    BinaryCuda.loaded_metadata = {}
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        probe.main()
    report = json.loads(output.getvalue())
    report['scope'] = 'exact offline-compiled cubin accumulation comparison; no selector inference'
    report['binary_manifest_sha256'] = sha(MANIFEST)
    report['loaded_function_metadata'] = BinaryCuda.loaded_metadata
    report['executed_cubin_sha256'] = {mode: item['cubin_sha256'] for mode, item in
                                      json.loads(MANIFEST.read_text())['kernels'].items()}
    report['research_source_sha256'][Path(__file__).name] = sha(Path(__file__))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=['prepare', 'run'])
    args = parser.parse_args()
    prepare() if args.action == 'prepare' else run()
