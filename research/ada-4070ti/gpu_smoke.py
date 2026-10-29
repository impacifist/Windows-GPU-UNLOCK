"""Run a bounded stock CUDA correctness test on this RTX 4070 Ti; no settings changed."""

import ctypes as c
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import time

from cuda_driver import Cuda

PTX = r'''.version 7.5
.target sm_80
.address_size 64
.visible .entry smoke(.param .u64 output, .param .u32 count, .param .u32 bias) {
 .reg .pred p;
 .reg .b32 r<6>;
 .reg .b64 rd<3>;
 ld.param.u64 rd0, [output];
 ld.param.u32 r0, [count];
 ld.param.u32 r5, [bias];
 mov.u32 r1, %tid.x;
 mov.u32 r2, %ctaid.x;
 mov.u32 r3, %ntid.x;
 mad.lo.u32 r1, r2, r3, r1;
 setp.ge.u32 p, r1, r0;
 @p bra DONE;
 mad.lo.u32 r4, r1, 3, r5;
 mul.wide.u32 rd1, r1, 4;
 add.u64 rd2, rd0, rd1;
 st.global.u32 [rd2], r4;
DONE:
 ret;
}
'''


def telemetry():
    fields = 'name,pci.device_id,pci.sub_device_id,driver_version,vbios_version,memory.total,memory.used,temperature.gpu,power.draw,power.limit,utilization.gpu'
    result = subprocess.run(['nvidia-smi', '--query-gpu=' + fields, '--format=csv,noheader,nounits'],
                            capture_output=True, text=True, timeout=15, check=True)
    rows = [r.strip() for r in result.stdout.splitlines() if r.strip()]
    if len(rows) != 1:
        raise RuntimeError('Expected one NVIDIA GPU telemetry row')
    values = [v.strip() for v in rows[0].split(',')]
    if len(values) != len(fields.split(',')) or values[1:3] != ['0x278210DE', '0x51321462']:
        raise RuntimeError('GPU identity differs from reviewed inventory')
    return dict(zip(fields.split(','), values))


def main():
    before = telemetry()
    cuda = Cuda()
    result = {'scope': 'stock CUDA correctness smoke; not a tensor or unlock benchmark',
              'utc': datetime.now(timezone.utc).isoformat(),
              'before': before, 'elements': 65536, 'launches': 10,
              'device_buffer_bytes': 65536 * 4, 'ptx_sha256': hashlib.sha256(PTX.encode()).hexdigest()}
    start = time.perf_counter()
    try:
        result['cuda'] = cuda.open()
        output = cuda.allocate(result['device_buffer_bytes'])
        kernel = cuda.load(PTX, 'smoke')
        count = c.c_uint32(result['elements'])
        host = (c.c_uint32 * count.value)()
        for launch in range(result['launches']):
            bias = c.c_uint32(7 + launch)
            cuda.launch(kernel, 256, 256, [output, count, bias])
            cuda.call('cuCtxSynchronize')
            cuda.call('cuMemcpyDtoH_v2', host, output, result['device_buffer_bytes'])
            if any(value != index * 3 + bias.value for index, value in enumerate(host)):
                raise RuntimeError('GPU output mismatch on launch ' + str(launch))
        result['full_output_verified_every_launch'] = True
        result['launch_dependent_bias'] = [7, 16]
        result['verified_elements_total'] = count.value * result['launches']
        result['output_sha256'] = hashlib.sha256(bytes(host)).hexdigest()
    finally:
        cuda.close()
    result['cleanup_completed'] = True
    result['elapsed_host_seconds_including_jit_and_validation'] = time.perf_counter() - start
    result['after'] = telemetry()
    result['research_source_sha256'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                       for p in (Path(__file__), Path(__file__).with_name('cuda_driver.py'))}
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
