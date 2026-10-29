# Sixth AD104 boot: live preflight matched

English | [한국어](AD104-SIXTH-RESULT.ko.md)

The 2026-10-26 physical run passed the capture-specific live topology check:

```text
topology-status=0x0000000000000000
roles=0xf null-binding-present=1 matched-edges=0x08
first-rejection-step=0x0 second-rejection-step=0x0
shared-byte=0x01 shared-count1=1 shared-count2=1
```

Both fresh collections agreed on the four pinned loaded-code identities,
binding callbacks, null-image binding and eight relevant active relationships.
The wrapper also accepted its input/console pointer and PCI configuration
consistency checks. This establishes that the implemented discovery works on
this boot; it is not an atomic snapshot or proof that keyboard input works.

BAR0 again reported the 32-bit `fb000000..fbffffff` memory envelope, length
`01000000`, zero translation and zero BAR-specific support flags. Device-wide
queries succeeded with current attributes `718`, supported `1b77b`, and PCI
command `0007`. BME remains enabled. No full MMIO admission was attempted.

The log explicitly retains `release-authority=0`, `cold-state-known=0`,
`input-execution-tested=0` and `resource-admission-separate=1`. No Stop,
DisconnectController, attribute write, GPU MMIO, ROM copy or unlock ran.
`capture-complete=0` and final `EFI_NOT_READY` are expected for this mode.
The user's return to Windows does not establish an unattended recovery path.

All 33 files were copied to a new private host archive and each USB/copy
SHA-256 matched. `DIAG00.TXT` is 396,633 bytes, SHA-256
`8499c5df90fa10a9f79dee2d4ade30f8c10e03d2c6194989894f3cac414e2d73`.
USB EFI SHA-256 matches the prepared 28,672-byte artifact:
`df8245799c9c4ca010e56e71d7a21c486ac6933862eed6fd271b68bba0d7a169`.
Raw files remain private and USB originals were preserved.

Another unchanged preflight boot would not answer the next question. The next
candidate is a separately guarded, one-shot EDID-consumer lifecycle experiment,
described in [the experiment contract](AD104-EDID-EXPERIMENT.en.md). Its
predicted transition is not an observed result or an unlock qualification.
