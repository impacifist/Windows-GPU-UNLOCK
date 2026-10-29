# Second AD104 boot result: ROM metadata available

English | [한국어](AD104-SECOND-RESULT.ko.md)

The user returned from the second test on 2026-10-26. The USB EFI SHA-256
matched the staged metadata-only artifact:
`92e038ddf621a227ac37c7afba2d589272d0a39ad431ad769c5f1f8730f6792d`.
All 33 new capture files were copied into a separate private host archive and
each copy matched its USB SHA-256. The USB originals were left unchanged.

The exact device remained `10de:2782`, subsystem `1462:5132`, class/revision
`030000a1`, at `0000:0b:00.0`. The key observation is:

```text
ad104-metadata-only=1 rom-pointer-present=1 rom-bytes=0x0000000000024a00 command=0x0007 memory-enabled=1 bus-master-enabled=1
```

Firmware reports a non-null PCI I/O ROM pointer and 150,016 bytes. The probe
did not dereference the pointer or copy any ROM contents. This establishes
availability of reported metadata only: readability, valid option-ROM format,
complete image coverage and equivalence to physical PROM remain unknown.
Bus mastering is still enabled; the original deep-capture guard is unchanged.

`ad104-metadata-only-no-mmio`, `capture-complete=0` and `EFI_NOT_READY` are the
expected outcome of this mode. They do not indicate that an attempted ROM read
failed. The log is 393,832 bytes. There are 32 driver image snapshots with 25
unique hashes. The offline analyzer again joins the PCI owner to IMG29, the
GOP child, and the text-output console layer to IMG14. These joins remain
observations, not permission or proof for a disconnect sequence.

No ROM bytes, GPU registers, unlock attempt or driver release were added by
this test. The log stops before the Windows start call, so it still does not
prove automatic return through that exact call.

The next useful experiment is a separately reviewed, bounded copy of the
firmware-provided ROM buffer, with format, length, identity and consistency
checks. It must keep BME unchanged and avoid BAR MMIO. No third-test EFI has
been prepared or staged as part of checking this result.
