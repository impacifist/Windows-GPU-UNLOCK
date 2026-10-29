"""Minimal CUDA user-space API for bounded stock compute; no hardware controls."""

import ctypes as c
import os
from pathlib import Path


class Cuda:
    def __init__(self):
        os.environ['CUDA_CACHE_DISABLE'] = '1'  # this process only
        self.lib = c.WinDLL(str(Path(os.environ.get('SystemRoot', r'C:\Windows')) / 'System32/nvcuda.dll'))
        p, u, i, z, q = c.c_void_p, c.c_uint, c.c_int, c.c_size_t, c.c_uint64
        signatures = {
            'cuInit': [u], 'cuDriverGetVersion': [c.POINTER(i)],
            'cuDeviceGetCount': [c.POINTER(i)], 'cuDeviceGet': [c.POINTER(i), i],
            'cuDeviceGetName': [p, i, i], 'cuDeviceGetPCIBusId': [p, i, i],
            'cuDeviceComputeCapability': [c.POINTER(i), c.POINTER(i), i],
            'cuDeviceGetAttribute': [c.POINTER(i), i, i],
            'cuCtxCreate_v2': [c.POINTER(p), u, i], 'cuCtxDestroy_v2': [p],
            'cuMemGetInfo_v2': [c.POINTER(z), c.POINTER(z)],
            'cuMemAlloc_v2': [c.POINTER(q), z], 'cuMemFree_v2': [q],
            'cuModuleLoadData': [c.POINTER(p), p], 'cuModuleUnload': [p],
            'cuModuleGetFunction': [c.POINTER(p), p, c.c_char_p],
            'cuLaunchKernel': [p, u, u, u, u, u, u, u, p, c.POINTER(p), c.POINTER(p)],
            'cuCtxSynchronize': [], 'cuMemcpyDtoH_v2': [p, q, z],
            'cuGetErrorString': [i, c.POINTER(c.c_char_p)],
            'cuEventCreate': [c.POINTER(p), u], 'cuEventRecord': [p, p],
            'cuEventSynchronize': [p], 'cuEventElapsedTime': [c.POINTER(c.c_float), p, p],
            'cuEventDestroy_v2': [p],
        }
        for name, arguments in signatures.items():
            function = getattr(self.lib, name)
            function.argtypes, function.restype = arguments, i
        self.context = p()
        self.allocations, self.modules, self.events = [], [], []

    def call(self, name, *arguments):
        status = getattr(self.lib, name)(*arguments)
        if status:
            message = c.c_char_p()
            described = self.lib.cuGetErrorString(status, c.byref(message))
            text = message.value.decode() if described == 0 and message.value else 'unknown CUDA error'
            raise RuntimeError(name + ': ' + str(status) + ' ' + text)

    def open(self):
        self.call('cuInit', 0)
        count, device, version = c.c_int(), c.c_int(), c.c_int()
        self.call('cuDeviceGetCount', c.byref(count))
        if count.value != 1:
            raise RuntimeError('Expected exactly one CUDA GPU; explicit selection review required')
        self.call('cuDeviceGet', c.byref(device), 0)
        name, bus = c.create_string_buffer(256), c.create_string_buffer(64)
        self.call('cuDeviceGetName', name, len(name), device)
        self.call('cuDeviceGetPCIBusId', bus, len(bus), device)
        major, minor = c.c_int(), c.c_int()
        self.call('cuDeviceComputeCapability', c.byref(major), c.byref(minor), device)
        if name.value != b'NVIDIA GeForce RTX 4070 Ti' or (major.value, minor.value) != (8, 9):
            raise RuntimeError('This research requires the RTX 4070 Ti / compute capability 8.9')
        self.call('cuDriverGetVersion', c.byref(version))
        self.call('cuCtxCreate_v2', c.byref(self.context), 4, device)  # blocking host synchronization
        free, total = c.c_size_t(), c.c_size_t()
        self.call('cuMemGetInfo_v2', c.byref(free), c.byref(total))
        return {'gpu': name.value.decode(), 'pci_bus_id': bus.value.decode(),
                'compute_capability': [major.value, minor.value], 'cuda_driver_api': version.value,
                'free_bytes_after_context': free.value, 'total_bytes': total.value}

    def allocate(self, size):
        if not 0 < size <= 16 * 1024 * 1024:
            raise ValueError('Research allocation must be at most 16 MiB')
        pointer = c.c_uint64()
        self.call('cuMemAlloc_v2', c.byref(pointer), size)
        self.allocations.append(pointer)
        return pointer

    def load(self, ptx, name):
        module, function = c.c_void_p(), c.c_void_p()
        source = c.create_string_buffer(ptx.encode('ascii'))
        self.call('cuModuleLoadData', c.byref(module), source)
        self.modules.append(module)
        self.call('cuModuleGetFunction', c.byref(function), module, name.encode('ascii'))
        return function

    def launch(self, function, grid, block, arguments):
        if not 1 <= grid <= 4096 or block not in (32, 128, 256):
            raise ValueError('Unexpected research launch dimensions')
        params = (c.c_void_p * len(arguments))(*(c.cast(c.byref(a), c.c_void_p) for a in arguments))
        self.call('cuLaunchKernel', function, grid, 1, 1, block, 1, 1, 0, None, params, None)

    def close(self):
        failures = []
        for name, values in (('cuEventDestroy_v2', self.events), ('cuMemFree_v2', self.allocations),
                             ('cuModuleUnload', self.modules), ('cuCtxDestroy_v2', [self.context] if self.context.value else [])):
            for value in reversed(values):
                status = getattr(self.lib, name)(value)
                if status:
                    failures.append(name + ':' + str(status))
        if failures:
            raise RuntimeError('CUDA cleanup failed: ' + ', '.join(failures))
