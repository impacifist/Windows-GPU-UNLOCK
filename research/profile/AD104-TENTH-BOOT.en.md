# Tenth AD104 boot: text, then graphics-console consumer

English | [한국어](AD104-TENTH-BOOT.ko.md)

The ninth run demonstrated text removal and captured the PCD producer. This
test repeats the guarded text stage, then permits one new IMG17 GOP-consumer
disconnect. **The PCI owner and its GOP interface stay attached.** EDID and
pointer consumers also remain. No unlock, attribute write or GPU MMIO is included.

## Admission and sequence

The baseline again captures private `PCDPROD.BIN`. It retains all ninth-test
capture, code/binding, list/cache, predicate and display-state checks. The PCD
producer must now also match the ninth snapshot's executable-text hash:
RVA `0x2a0`, length `0x236d`, SHA-256
`cb64fe58020cf7512de99c86d8284baca00dd679109680ea1af371081e3b6367`.
There are no relocation targets in that text range. This is a code fingerprint
of the inspected accessor module, not a complete review of its database helper.
The same-run whole-image consistency checks remain in place.

One uppercase `G` authorizes both stages. Text removal must first pass the
demonstrated eight-to-six edge, empty-list, PCD-cache and display/configuration
postconditions. After text success, the new stage freshly requires the child
SimpleTextOut and companion protocol
`e1e4a857-c970-4075-a4da-e9c41b69adfc`. Their interfaces must have the exact
`text + 0x101` relationship observed in IMG17's uninstall call, and their
identities must remain stable across two prechecks. The ninth graph confirms
both protocols existed on the child; it did not log their interface values.

Flush a separate second-stage intent, revalidate the six-edge state and protocol
identities, then invoke exactly
`DisconnectController(discovered_gop_child, exact_img17_binding, NULL)`.
This targets the graphics-console consumer, not the PCI owner or every driver.
The application stays quiet after the first text attempt. It never dereferences
the saved child text interface or its mode after the IMG17 call, because those
allocations may have been freed.

Whether that call succeeds or fails, collect two independent post-states and
query protocol absence. Require:

- Five relevant edges: only the IMG17 GOP open is additionally removed.
- Both SimpleTextOut and the companion protocol absent on the child.
- Unchanged owner GOP interface/methods/mode/info/framebuffer metadata, full
  PCI configuration, BAR descriptor and attributes `718`/support `1b77b`.
- EDID/pointer relationships and IMG08 shared values `1/1/1` retained; text list
  remains empty and the cached PCD interface remains correct.
- Pinned code, producer consistency and a freshly false PCD predicate retained.

An actual quiet `V` input and another five-edge/protocol-absence check follow.
Only then may successful logging and file closing lead to Windows. A callback
success alone does not qualify the transition: IMG17 ignores several cleanup
results before returning success. Its inspected Stop has no direct SetMode/Blt
call; this does not exclude firmware-mediated or transient display effects.

Any failure stops without retry, reconnect, restoration, reset or firmware
BootOrder fallback. Each stage can run once. Capture-only success or text-stage
success cannot authorize skipping a failed GOP stage. Expected output remains
34 private files: log, 32 driver dumps and PCD producer.

## Operator instructions

1. Save work. Keep USB connected; `shutdown /s /t 0`, wait ten seconds after
   power-off, then ASUS Delete → F8 → **UEFI: USB DISK 2.0** (8GB).
2. At the first prompt, press **Shift+G** once to permit text then GOP-consumer
   removal. Any other key or timeout stops the test.
3. Wait five seconds, then **tap Shift+V every two seconds**, up to one minute
   until Windows starts. Do not hold keys. **There is no second prompt.**
4. If it stops/hangs or Windows is not back within five minutes, power off,
   remove USB and select normal Windows Boot Manager. Boot once and preserve
   the result before another EFI run.

Screen/input loss, firmware hang and Windows-return failure remain possible.
This PC has no integrated GPU. Ninth-run recovery does not establish recovery
for the new second stop. The 60-second key polling budget cannot interrupt a
hung firmware callback; five seconds is an operator wait, not a callback deadline.

Implementation `5e3af5f`, EFI 43,520 bytes, SHA-256
`3cb4550e4760c722b3f5bb03846f337e2d61a423ecd813694729de8a2dd02e22`.
Build with ninth options plus `-Ad104GopTest`. Manifest CanDetach and
CanReleaseGopConsumer true, CanReleasePciOwner/CanUnlock/CanReadGpuMmio false.
Native AMD64 PE checks passed (no imports/IAT/delay imports, NX/dynamic base).
RAM checks passed: 112 GOP lifecycle, 139 text lifecycle, 53 producer, 29 display,
21 quiet handoff, all prior suites; 27 invalid compile combinations and 31
Python tests passed. This is software validation, not physical GOP-stop proof.
Preserve the ninth capture and EFI on host and USB before staging.
