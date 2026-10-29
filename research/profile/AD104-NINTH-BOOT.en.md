# Ninth AD104 boot: PCD capture followed by one text disconnect

English | [한국어](AD104-NINTH-BOOT.ko.md)

The operator requested combining the missing PCD fingerprint capture with the
text-detach experiment to save a boot. This mode captures first and permits one
text attempt only after that capture and fresh checks succeed. It does not
perform EDID or PCI-owner release, attribute writes, GPU MMIO or unlock.

## Capture and admission

The baseline output observation runs first. A unique loaded producer must match
firmware file GUID `80cf7257-87ab-47f9-a3fe-d50b76d89541`, size `0x2e40`,
boot-services code type and GetBool RVA `0x4f4`. GetBool, GetNextToken and GetSize
must fall within executable PE sections of this image. It copies the loaded
image to RAM, rechecks its bytes and protocol identity around file operations,
then writes a new private `PCDPROD.BIN` with full write/flush/close checks and a
SHA-256 record. It refuses an existing file, ambiguous producer, malformed PE,
changed bytes or root/file alias. No SPI flash or original firmware file is read.
The captured image is never loaded or executed as a new image. Normal live PCD
queries continue through the installed protocol.

The same-run producer hash, interface and three methods must remain unchanged
throughout the text checks. This establishes consistency with the saved bytes,
**not prior independent review or attestation of the producer code**. Offline
review of the new dump happens after this combined run. Capture first does not
turn a newly computed hash into a previously trusted fingerprint.

Before the prompt and again before the call, require the exact GPU/code/binding
pins, eight relevant edges, IMG08 shared state `1/1/1`, full PCI configuration,
BAR descriptor and attributes `718`/support `1b77b`. Fresh PCD token enumeration,
size and GetBool checks must return false; GOP mode/interface/info/framebuffer
metadata must match and remain stable across the query. No SetMode is called by
the experiment. The native Stop may have display effects of its own.
Expected output adds `PCDPROD.BIN` to the usual log and 32 driver dumps: 34 files.

IMG14's loaded list count must be one, with matching non-null first/last pointers
and the expected cached console handle. Its cached PCD interface must be absent
or the current interface. These are bounded fields inside the pinned image;
the heap record is not dereferenced. Intent must be written and flushed before
the one-shot attempt.

## One transition and quiet completion

The only lifecycle call is
`DisconnectController(discovered_gop_child, exact_img14_binding, NULL)`.
Console output is disabled in the application before entering that call and
stays disabled, including error/hold/Windows paths. No console output follows the attempt; guarded Say calls return without
calling OutputString. This restriction applies to this
application; it cannot constrain calls internal to firmware or Windows.

Regardless of callback success/error, collect two independent post-states.
Expected: only the two text edges disappear (eight become six), EDID and both
pointer roles remain, IMG08 shared values stay `1/1/1`, and PCI/BAR/attribute/GOP
state is unchanged. The text list becomes empty and its cached PCD interface
matches the current producer. Helper/allocation failure can cause rejection
despite a successful Stop return. The protocol hash and false predicate must
remain valid. Matching post-state cannot prove that no transient SetMode or
other display operation occurred.

Successful postchecks lead to a quiet actual-input challenge. An early queued
key is discarded; a later fresh uppercase `V` is required. Another fresh
collection, durable logging, result recording and file closing must succeed
before Windows handoff. Failure holds without retry, reconnect, restoration,
reset or firmware BootOrder fallback—even if no detach occurred.

## Operator instructions

1. Save work. Keep USB connected; run `shutdown /s /t 0`, wait ten seconds after
   power-off, then ASUS Delete → F8 → **UEFI: USB DISK 2.0** (8GB).
2. Wait for the capture and first prompt. Press **Shift+T** once to permit the
   single text attempt. Any other key or timeout stops it.
3. After `T`, wait five seconds, then **tap Shift+V every two seconds** until
   Windows starts, for up to one minute. Do not hold keys. There is no second
   prompt, and the display may go blank or retain the old prompt.
4. If it stops/hangs, or Windows has not returned within five minutes, power
   off, remove USB and select normal Windows Boot Manager. Boot this test once;
   preserve its files before another EFI run.

Display/input loss, firmware hang and failure to return to Windows are possible.
The PC has no integrated GPU and recovery is unproven for this target. The key
wait has a 60-second polling budget; firmware callbacks can themselves hang.
The five-second delay is an operator instruction, not a guaranteed callback
duration. Repeated taps avoid losing the only response while postchecks finish.

Implementation `908be19`; native EFI 41,984 bytes, SHA-256
`2e2cb3069992b076db3e84a0ff9ca266230ab4d107f0cd96866547907f18de7d`.
Build: `-Ad104Msi -Ad104Metadata -Ad104Resources -Ad104Preflight
-Ad104OutputObservation -Ad104PcdCapture -Ad104TextTest -WindowsMbr`.
Manifest CanDetach/CanCapturePcdProducer true; CanUnlock/CanReadGpuMmio/
CanDereferenceRomImage/WritesFirmwareVariables false; HardwareTested false.
Native AMD64 PE checks passed (no imports/IAT/delay imports, NX/dynamic base).
RAM checks passed: 52 producer, 29 display, 139 text lifecycle, 21 quiet handoff,
plus the existing suites and 31 Python tests; 26 invalid build combinations
were rejected. These checks establish software behavior in mocks, not physical
text-detach success. Preserve the eighth capture/EFI on host and USB before staging.
