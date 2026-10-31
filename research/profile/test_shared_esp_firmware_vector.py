"""Independent packed UEFI decoder for the PowerShell-generated fixed test vector."""
import pathlib
import struct
import sys
import uuid

data = pathlib.Path(sys.argv[1]).read_bytes()
attributes, path_length = struct.unpack_from("<IH", data)
assert attributes == 1, "EFI_LOAD_OPTION must be active (distinct from NVRAM attributes 7)"
description_end = next(i for i in range(6, len(data) - 1, 2) if data[i:i + 2] == b"\0\0")
assert data[6:description_end].decode("utf-16le") == "EVGA audit test vector"
start = description_end + 2
kind, subtype, length, number, lba, sectors, signature, mbr_type, signature_type = struct.unpack_from("<BBHIQQ16sBB", data, start)
assert (kind, subtype, length, number, lba, sectors, mbr_type, signature_type) == (4, 1, 42, 1, 2048, 2097152, 2, 2)
assert uuid.UUID(bytes_le=signature) == uuid.UUID("c36c3e86-02aa-4f76-af1b-96e327f9d1c1")
kind, subtype, file_length = struct.unpack_from("<BBH", data, start + 42)
assert (kind, subtype) == (4, 4)
assert data[start + 46:start + 42 + file_length].decode("utf-16le") == "\\EFI\\EVGA-SHARED-AUDIT-VISIBLE.EFI\0"
assert data[start + 42 + file_length:] == b"\x7f\xff\x04\0"
assert path_length == 42 + file_length + 4
assert len(data) == start + path_length
print("PASS independent UEFI load-option byte layout, GPT signature/geometry, path and end-node checks")
