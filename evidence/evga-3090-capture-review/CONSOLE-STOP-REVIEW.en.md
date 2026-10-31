# EVGA3090 console Stop review

[English](CONSOLE-STOP-REVIEW.en.md) | [한국어](CONSOLE-STOP-REVIEW.ko.md)

Scope: static inspection of the uploaded loaded AMD64 images using Capstone. No firmware was executed, no driver method was called, and no GPU or USB state was changed. Movian reproduced the offline fingerprint test in Discord at 21:28 on 27 October 2026 and reported no branch-pull issues at 21:29. His displayed capture and code hashes match the [saved results](console-code-test-results.json).

## Evidence and interpretation

These are image-relative virtual addresses, not file offsets from an on-disk PE. The input dumps are loaded images; instruction decoding uses their RVA layout. The identity and executable-section hashes remain those in the saved results. No new profile or hardware authority is produced.

| Image | Observed instructions | Consequence |
| --- | --- | --- |
| IMG13, Stop 0x104c | 0x1078 tests NumberOfChildren; zero reaches xor eax,eax at 0x107d and return at 0x11f5 | A successful zero-child call would not establish removal of the recorded console membership. |
| IMG13 | 0x1084 requires count 1; 0x108e requires a child buffer; 0x1097–0x10a1 compares its first handle with a stored global child; invalid inputs reach EFI_INVALID_PARAMETER at 0x11eb | The recorded aggregate child must be identified exactly; this is not an arbitrary-child stop. |
| IMG13 | 0x10db and 0x10fd call service-table slot 0x120 with the SimpleTextOut GUID at RVA 0x5920; no immediate return-status branch follows either call | This matches a CloseProtocol-shaped pair under the standard EFI_BOOT_SERVICES layout, but success of Stop alone cannot prove those closes succeeded. Table provenance and helper effects still need qualification. |
| IMG13 | One-child path calls local helpers 0x4db0 and 0x4200, a slot-0x48 call, and a slot-0x150 call using GUID d3b36f2c-d551-11d4-9a46-0090273fc14d | Membership removal includes additional state and protocol work. The entire side-effect set is not yet qualified. |
| IMG17, Stop 0x88c | 0x8a6 selects zero versus nonzero children. Zero checks global count and buffer, can call slot 0x48 and clear globals before returning success | The zero-child path is not a universal no-op, but does not traverse the observed per-controller removal path. Do not infer detachment from success. |
| IMG17 | 0x8f4 requires count 1; 0x8fa requires child buffer; 0x8ff–0x909 compares its first handle with a stored global child; a controller-list search follows | A one-child call also depends on exact aggregate-child and controller membership. Invalid cases reach EFI_INVALID_PARAMETER at 0x922. |
| IMG17 | 0x95d and 0x97f call slot 0x120 with GOP GUID at RVA 0x1c60, without immediate status checks; list removal, a counter decrement and slot-0x48 call follow | A returned status cannot substitute for a fresh ownership graph and protocol-lifetime checks. |
| IMG17 | If members remain, helper 0xbc0 and slot-0x88 calls occur; if none remain, helper 0x156c and mode/global writes occur | Removing the final member follows a different path from removing one of several. Generic consumer-stop assumptions do not cover this captured firmware. |

Slot interpretations use the vendored `deps/edk2/MdePkg/Include/Uefi/UefiSpec.h`: 0x48 FreePool, 0x88 ReinstallProtocolInterface, 0x120 CloseProtocol, 0x150 UninstallMultipleProtocolInterfaces on the standard x64 layout. They are conditional interpretations, not proof that every indirect target resolves to that table. The embedded protocol GUID bytes were checked directly. The graphics helper 0x156c contains an indirect call through the passed interface at offset 0x10; its dynamic target has not been qualified. No claim of safe display retention or recovery follows from this review.

## Next gate

Do not stage an EFI lifecycle test yet. First qualify the service-table provenance, helper call targets, aggregate producer and remaining consumers, and the last-member branches. Then build an offline model that requires the observed links, rejects unexpected consumers, and verifies each expected edge change and object lifetime. A synthetic model would validate the harness assumptions only; a new attended hardware test would require its own explicit risk boundary.

The existing automatic topology refusal stays in place. No unlock, reset, firmware upload/execution or GPU register access was attempted. No additional action or reboot is required from the contributor while this review continues.

## Additional virtual-GOP consumer

The recorded virtual GOP also has a count-1 BY_DRIVER owner in IMG16, identity 43e7abdd-e352-4cfb-a230-4cdc1d350e5c, Stop RVA 0xb2c. The earlier nine-check chain test and two-driver fingerprint test did not qualify this third driver. Their successful results remain valid within that limited scope. They are not a complete release plan.

IMG16's entry calls table slot 0x118 at 0xb6b using SimpleTextOut GUID at RVA 0x20b0 and attribute 2 (GET_PROTOCOL). At 0xb9d it calls slot 0x120, then at 0xbd5 slot 0x150 with SimpleTextOut and e1e4a857-c970-4075-a4da-e9c41b69adfc (protocol name not assigned here). It branches on that uninstall-shaped result at 0xbdb–0xbde, then calls slot 0x120 at 0xbf8 using the GOP GUID at RVA 0x20e0 and makes three slot-0x48 calls. Standard service-layout interpretation suggests a GOP-to-text consumer that uninstalls its text interfaces and frees its state. This remains conditional until service-table provenance and all objects are qualified.

The observed dependency chain therefore includes an intermediate GOP consumer, not merely the two splitter-role bindings previously fingerprinted. Qualification must cover IMG16 and require its text consumers to be removed before releasing its GOP dependency. This is a proposed dependency order for offline analysis, not an executable or approved hardware sequence.

IMG16 executable-section fingerprint: 7680 bytes, SHA256 e5e68f0856c6e932da9bf3c680bac955dbf180eb083da78b64007388d89e24a7. This is a saved-image observation, not authenticated driver provenance.
