# Proposed AD104 EDID lifecycle experiment

English | [한국어](AD104-EDID-EXPERIMENT.ko.md)

Status: implemented in `b45ddd6`, host-tested and native-built; physical behavior
remains untested. See [the seventh boot instructions](AD104-SEVENTH-BOOT.en.md).
The operator explicitly accepted the unproven recovery boundary before staging.

## Question and single operation

Can the pinned IMG08 binding release only its EDID Discovered relationship on
the discovered GOP child while preserving its shared pointer roles and the
GPU's existing PCI state? The candidate operation is exactly one
`DisconnectController(gop_child, img08_binding_handle, NULL)`. Addresses must
come from fresh discovery, never saved logs. This must not target the PCI
controller, console, all drivers, IMG29 or its null-image binding. Do not call
Stop directly: firmware driver-model orchestration and its outcome are part of
the experiment.

Static analysis predicts that the EDID branch clears the IMG08 byte at RVA
`546a` and closes the EDID relationship. That byte's full operational meaning
is unknown. Firmware may do more than the visible branch suggests; successful
return or a matching graph cannot exclude delayed effects.

## Required admission and evidence

The implementation must retain these gates together:

1. Validate the exact GPU identity/BDF, four code fingerprints and bindings,
   null binding, eight relevant active edges, shared `1/1/1` observations,
   full PCI configuration, BAR descriptor, attributes `718` and support
   `1b77b`. Require two agreeing collections immediately before invocation.
   A topology match alone must not authorize the operation.
2. Verify the exact Windows boot image and its prepared handoff. Successfully
   loading it is preparation, not evidence that it will run after mutation.
3. Observe an actual user-entered keyboard challenge before invocation.
   Pointer stability alone is insufficient. Log only challenge success/status,
   never arbitrary typed keys. Timeout or unexpected input cancels invocation.
4. Write and successfully flush the pre-state and one-shot intent to the USB
   before entering DisconnectController. Mark the attempt before the call.
   Failed logging prevents invocation. A firmware callback can hang; a polling
   timeout cannot interrupt it or guarantee recovery.
5. After any callback return, including error, independently collect its status,
   graph, shared state, full PCI configuration, resource/attribute queries and
   console identity. Do not short-circuit postchecks on callback failure.
6. Test actual keyboard input again with a challenge. Require all
   expected postconditions and durable result logging before considering the
   prepared Windows handoff. A failed challenge is not permission to retry.

Each input challenge has a 60-second polling budget, not a hard execution
deadline. ReadKeyStroke, OutputString, Stall or other firmware callbacks can
themselves hang. Input queues are drained with a 32-read bound before prompting;
only uppercase `E` before and uppercase `V` after are accepted. No typed keys
are written to the evidence log.

The predicted post-state has seven relevant active edges: only IMG08's EDID
BY_DRIVER edge disappears. The shared byte becomes zero; both pointer edges
and their counts remain `1/1`. The PCI owner, text/GOP consumers, console/input
identities, full PCI configuration, BAR descriptor and attributes stay unchanged.
These expectations require a new post-state checker; the current eight-edge
preflight correctly rejects this seven-edge graph and must not be weakened.

Host mocks must cover wrong identity, fingerprint and graph; stale/changed
observations; missing input and timeout; intent write/flush failure; callback
success and error with partial mutation; postcheck/logging failures; unexpected
extra closures; and attempted retry. Mock success cannot establish firmware
behavior or recoverability.

## Failure boundary and remaining work

One attempt only. No speculative reconnect, ConnectController, attribute
restoration, owner detach, MMIO, unlock or automatic reboot belongs in this
experiment. A callback error means possibly mutated state. Missing observations,
unexpected transitions, lost input or failed logging must block further driver
actions and automatic Windows handoff. Persist whatever evidence can still be
written, then stop for attended recovery; a hung callback may prevent that write.

Possible outcomes include lost display/input, a firmware hang, failure to boot
Windows and a required manual power-off/removal of the USB followed by normal
Windows Boot Manager. Power cycling is the intended recovery attempt, not a
tested guarantee. The host has no integrated GPU. Do not present this as
reliably recoverable merely because there are no intended persistent writes.
The operator must understand this boundary before the first mutation test.

Even a fully matching result establishes only this EDID lifecycle transition.
PCI-owner release, limited decode enable, cold-entry/protected-memory checks,
an unlock transaction and controlled Windows workloads remain separate gates.
