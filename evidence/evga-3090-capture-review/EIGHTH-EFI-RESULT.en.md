# EVGA07: three consumer Stops completed

[English](EIGHTH-EFI-RESULT.en.md) | [한국어](EIGHTH-EFI-RESULT.ko.md)

EVGA07 records one successful IMG13 Stop, one IMG16 Stop and one IMG17 child Stop, passing postchecks, two complete captures and ready-windows. Movian reports “Behavior: Same as previous”, referring to automatic Windows return. Contributor EFI: 23040 bytes, SHA256 db0044a358f52aef115f466a1195f43631f5e15fcb91f70056612a5f50aa7fbc. [Sanitized result](eighth-efi-run-result.json).

Independent complete recorded-graph comparison finds the same 549 handles and both GOPs retained. The physical GOP loses IMG17’s two active opens; the virtual GOP loses IMG16’s one active GOP open and the three text-related protocol GUIDs already seen in EVGA06. The removed text protocol contained IMG13’s two active opens. No other recorded active opens change and no protocols are added. Passive open counts can change as capture queries run.

Three EDID-discovered handles remain owned by IMG07, including the physical GOP and two non-GOP siblings. EDID-discovered is GUID 1c0c34f6-d380-41fa-a049-8ad06c1a66aa; c7a7030c-c3d8-45ee-bed9-5d9e76762953 is a separate system-console protocol and must not be used as the EDID selector. NVIDIA ownership also remains. This is progress in console release, not complete GPU release, profile admission, reliable recovery or unlock proof.
