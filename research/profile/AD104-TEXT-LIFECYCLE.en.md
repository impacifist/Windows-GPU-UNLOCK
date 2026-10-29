# AD104 text lifecycle: cleanup reaches GOP

English | [한국어](AD104-TEXT-LIFECYCLE.ko.md)

This is static analysis of the seventh boot's private IMG14 loaded-memory
snapshot. No callback, extracted firmware or new EFI test was executed.
The pinned Stop entry remains RVA `0x11e0`. Its zero-child branch returns success
without performing the child cleanup; that return cannot establish release.
The one-child branch requires the recorded console child, closes Simple Text
Output relationships, removes a list record and frees memory.

## More than two edge removals

Stop calls helper RVA `0x335c`, which obtains GOP through HandleProtocol. The
GUID at RVA `0x6000` is `9042a9de-23dc-4a38-96fb-7aded080516a`, independently
decoded from the saved image. The helper allocates/copies buffers and can
call GOP's SetMode through interface offset `0x8` at RVA `0x3453`, passing the
current `Mode->Mode`. Even requesting the same mode is a SetMode call; it must
not be classified as a read-only query or presumed to leave display state alone.

The controlling branch is now identified at the API level. Helper RVA `0x4aac`
locates/caches protocol GUID `11b34006-d85b-4d0a-a290-d5a571310ef7`, matching
`PCD_PROTOCOL_GUID` in the repository's EDK2 `Protocol/Pcd.h`. The indirect
call at RVA `0x343d` uses offset `0x30` with token `0x4ac`. The local protocol layout
identifies that slot as `GetBool`; its Boolean return controls the SetMode path.
The platform-specific token's semantic name and actual value are not established.
Do not infer a token name from a different firmware build or write it to suppress
this branch. The cached interface pointer is zero in the saved snapshot; that
does not imply that LocateProtocol or the later getter would fail or return false.

Both CloseProtocol return values and the helper outcome are ignored in Stop.
The later return can reflect a FreePool result rather than successful completion
of every preceding operation. Independent postconditions remain mandatory.

Snapshot-only data supports a single list record at RVA `0x7160` (count `1`),
with matching first/last pointers. The cached handle at RVA `0x6f60` matches the
logged console handle. The heap record itself is outside this image snapshot;
we have not dereferenced it or established all fields from saved bytes.
Absolute addresses are deliberately omitted here.

## Next evidence required

Before a live text-detach proposal, establish ConOut/StdErr interface and
callback ownership, the child GOP SetMode callback and mode/framebuffer state,
and the platform predicate's behavior. Prefer existing evidence and bounded
observations; do not execute the saved image or Stop to answer a read-only
question. Protocol layout identifies an API, not the behavior of its OEM
implementation. No metadata query alone qualifies a detach or unlock.

Any eventual test must record intent before invocation and work without further
OutputString calls after the attempt. It needs a separate six-edge expectation
for an independent fresh boot retaining EDID, checks for mode/resource/config
effects, actual input evidence and a reviewed hold/Windows handoff path. A
successful EDID test does not establish this target's recoverability. No text
detach or owner-release EFI has been prepared by this analysis.

Subsequent implementation: the operator requested capturing the missing PCD
producer and testing text detach in the same boot. The
[ninth boot contract](AD104-NINTH-BOOT.en.md) now implements that sequence with
same-run producer consistency checks and a quiet post-test input protocol.
This does not provide independent prior review of the newly captured producer.
