# Eighth AD104 boot: output state and display-reset predicate

English | [한국어](AD104-EIGHTH-BOOT.ko.md)

This is a read-only follow-up to the successful EDID experiment. No driver
disconnect occurs. It fills gaps identified in the
[text cleanup analysis](AD104-TEXT-LIFECYCLE.en.md): console callback locations,
text/GOP mode state and the Boolean controlling a conditional GOP SetMode.
Do not reuse the seventh mutation EFI for this question.

After exact live preflight, the probe copies ConOut/StdErr interface methods
and text-mode fields, and the discovered display child's GOP methods,
mode information, pixel masks and framebuffer address/size. It does not read
framebuffer contents or arbitrary heap/list records. Firmware-provided protocol
buffers and loaded-image allocations remain trusted contracts.

It requires a unique native PCD protocol, enumerates native tokens with
GetNextToken (maximum 4,096 calls, strictly increasing nonzero results), and
queries GetBool(`0x4ac`) only if that exact token exists and GetSize reports one
Boolean byte. The Boolean use comes from the pinned IMG14 code; size alone is
not a type proof. It calls no PCD setter, SetSku, display method or driver Stop.
Missing/invalid tokens are reported without attempting the Boolean query. No
other token values or configuration database contents are dumped.

Two queries must agree; three interface/mode snapshots surround them. Fresh
ownership and PCI configuration checks follow. The log separates:

```text
ad104-output-observation snapshot-status=...
ad104-output-pcd token=0x04ac first-status=... second-status=...
```

Snapshot success does not imply a successful token query. Values appear only
when both queries succeed; failure is not interpreted as false. A true value
would support the conditional SetMode path under these observations, not prove
that every preceding cleanup step would reach it. A false value likewise
does not establish safe text removal. Its platform-specific semantic name
remains unknown.

The probe logs text modes, GOP mode/framebuffer metadata and the containing
loaded-image base/size/RVA for selected callbacks: console/error OutputString,
console SetMode, GOP SetMode and PCD GetBool. Range containment is attribution
evidence, not proof of executable-section membership, safe invocation or ownership
of every console callback. Raw addresses remain in private USB/host captures.

Implementation `0997398`: EFI 32,768 bytes, SHA-256
`7f9cba828a34660723e389465ffd9a190ce8ead20dc08d11477c741d0d5946b1`.
Build with `-Ad104Msi -Ad104Metadata -Ad104Resources -Ad104Preflight
-Ad104OutputObservation -WindowsMbr`. Manifest: CanDetach/CanUnlock/CanReadGpuMmio/
CanDereferenceRomImage/WritesFirmwareVariables all false; CanQueryPcdBoolean true.
The mode is compile-incompatible with the EDID mutation test. Native AMD64 PE
checks passed; no imports/IAT/delay imports, NX/dynamic base. Host checks passed:
78 output observations, 27 topology, 84 EDID lifecycle, 15 handoff, 151 deep AD104,
24 metadata, 113 ROM-buffer, 66 resources, 66 MBR, four GPT, 24 rejected compile
combinations and 31 Python tests. These are not physical execution evidence.

Preserve the seventh capture and EFI/manifest on both host and USB before
staging. Save work, leave the capture USB connected, run `shutdown /s /t 0`,
wait ten seconds after power-off, then ASUS Delete → F8 → **UEFI: USB DISK 2.0**
(8GB). **No test key is required.** Metadata `capture-complete=0`/`EFI_NOT_READY`
remains expected; this diagnostic attempts Windows afterward. If Windows has
not returned within five minutes, power off, remove USB and select normal
Windows Boot Manager. Firmware query callbacks can hang; return is not
guaranteed. Boot once and preserve the capture before another test.
