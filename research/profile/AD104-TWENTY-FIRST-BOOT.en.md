# Twenty-first attended candidate: combined full PROM capture and verification

Executed result: [test21](AD104-TWENTY-FIRST-RESULT.en.md). Original candidate contract follows.


English | [한국어](AD104-TWENTY-FIRST-BOOT.ko.md)

Built/host-tested, not physically executed. Requires separate expanded-MMIO
approval before staging. [Test20](AD104-TWENTIETH-RESULT.en.md) established only a
512-byte physical PROM prefix and direct Windows recovery.

## Combined sequence

1. Existing five stops, Enable(0x300), delayed register batches and512-byte header.
2. Verify PROM.BIN absent, allocate2MiB RAM (capture/readback halves), flush intent,
   then freshly admit exact PCI/attributes/BAR/ownership/old-child state.
3. Read the fixed1MiB BAR0 window0x300000–0x3ffffc as262,144 DWORDs, once. No file,
   console or allocation callbacks inside this sequence. PCI Mem.Read callbacks
   themselves may hang or have side effects. PayloadFFFFFFFF/BADF words are valid
   data; transport errors/method drift stop. First DWORD must match the admitted
   same-boot prefix. Full captured first512bytes must match that prefix afterward.
4. Re-read signature and chip; full fresh state checks. Hash the complete RAM
   capture before file I/O. Recheck absence; create new PROM.BIN only on the
   borrowed launch root. Reject root/log/witness aliases. Exact1MiB write, flush,
   seek0, exact1MiB readback, EOF, immutable capture-hash equality and successful
   Close are required. No existing file is overwritten.
5. Free the buffer and recheck state. Log transport completion separately from
   final state admission; output callbacks are followed by fresh checks. A partial
   file after failure remains incomplete evidence and is not retried/overwritten.
6. Fresh V and final checks, then the approved one-shot Disable(0x300), repeated
   exact post-parent state checks, independent STATUS.TXT and Windows handoff.

Successful path:262,274 PROM DWORD accesses (129header +262,144full +1signature),
plus2 chip bookends and44 prior register reads:262,320 Mem.Read calls total.
This expands the range and time exposed to MMIO failure. No second full capture,
firmware execution/upload, GPU reset, reconnect, BME/VGA enable or unlock. Failure
holds without automatic rollback; display/input/Windows may fail and manual
power-off/USB removal may be required. Recovery is unproven for this candidate.

A transport-complete1MiB window is not proof every byte is valid ROM, a complete
image chain, a valid checksum or accepted FWSEC. Offline parsing/comparison follows
return. Existing SS1/VGA_BASE differences remain unresolved.

## Verification and operator timing

Host mocks exercise continuous bounds, prefix/signature/chip/method faults,
selected first/middle/last read failures, literal sentinel payload, allocation,
file aliases, short write/read, EOF, source/readback corruption, seek/flush/close/
free errors and callback-induced PCI drift. Millions of repeated assertions are
not millions of independent test cases. Existing header/lifecycle/handoff tests
remain. Host checks do not establish physical safety.

Native58,368-byte AMD64 PE, no imports, SHA-256:
`60b258feb75a2b967ef93b5231e89a5e6786e97304b2b61c9687798e860b1ae0`.
Build switch Ad104PromFullTest requires the header/delayed/decode-off/read chain;
manifest pins1,048,576capturebytes and512headerbytes.

After approval/staging: save work, fully shut down, wait10seconds, boot UEFI USB.
Shift+G accepts all stages. Wait10seconds, tap Shift+V every2seconds for up to
3minutes. This tapping period is operator guidance: firmware accepts fresh V only
within60seconds after its checks reach the key wait, not a3minute internal timer.
No screen updates after detach and no reliable live progress indication. If no
Windows within10minutes, power off, remove USB and select normal Windows Boot
Manager. Firmware callbacks have no guaranteed completion deadline. Run once;
return captures including PROM.BIN and STATUS.TXT before any retry.
