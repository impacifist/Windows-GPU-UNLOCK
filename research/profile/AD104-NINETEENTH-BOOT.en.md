# Nineteenth attended test: delayed register stability

Executed result: [test19](AD104-NINETEENTH-RESULT.en.md). Candidate contract below.


English | [한국어](AD104-NINETEENTH-BOOT.ko.md)

Built and host-tested; not yet physically executed. Extends successful
[run 18](AD104-THIRTEENTH-EIGHTEENTH-RESULT.en.md) without new GPU registers or writes.

The existing five stops and Enable(0x300) precede batch 1:21 unique registers plus
a chip-ID bookend (22 reads). After successful capture and logging, flush a delay
intent, call Stall(1000000) once, flush the returned status, then collect fresh
full PCI/attributes/ownership/old-child-absence state before batch 2. Read-method
identity and all error/sentinel gates remain active. No logging inside a batch.
Missing/failed Stall or changed admission prevents further MMIO. The one second
is a requested firmware delay, not measured wall-clock time.

Both batches must match exactly. After fresh V and final checks, one Disable(0x300)
must restore the complete saved post-parent state, checked twice and again after
flush before Windows. No MMIO after Disable. Independent STATUS.TXT remains open
across main-log Close/StartImage with the ExitBootServices no-file-I/O guard.
No reset, reconnect, BME/VGA enable, PROM read, firmware upload, unlock or rollback.

This tests delayed stability/settling and recovery repeatability. Reference SS1=1
and VGA_BASE=1 still differ from observed 5/0x10e09. Stable differences do not grant
cold-entry/unlock eligibility. Any value drift conservatively blocks handoff;
manual power-off may be required and recovery is unproven for this new run.

Candidate: 53,760-byte AMD64 PE, no imports, SHA-256
`f63b358ef0c42e314103ab07655d40f9c5d81a385c6685520a0e228a48734600`.
Preparation includes 2,701 delayed-read and 2,074 decode-off RAM checks plus existing
capture/lifecycle/PCD/input/handoff/compile guards. Mocks are not hardware proof.
Builder requires delayed-read→decode-off→decode-read; manifest delay 1,000,000 us.

Save work, fully shut down, wait 10 seconds, select ASUS UEFI USB. Shift+G once;
wait five seconds, tap Shift+V every two seconds for up to one minute. No further
screen prompt after detach. V permits decode-off then Windows. If no Windows
within five minutes, power off, remove USB, select normal Windows Boot Manager.
Run once; preserve captures before any retry. EFI 1024×768 versus Windows 5120×1440
aspect mismatch remains unchanged; blinking cursor is not a progress indicator.
