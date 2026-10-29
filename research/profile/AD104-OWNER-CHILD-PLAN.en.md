# Next candidate: PCI-owner child cleanup

English | [한국어](AD104-OWNER-CHILD-PLAN.ko.md)

## Correction and implemented candidate

The original DisconnectController proposal below is superseded. EDK II's
[CoreDisconnectController](https://github.com/tianocore/edk2/blob/master/MdeModulePkg/Core/Dxe/Hand/DriverSupport.c)
can append zero-child parent Stop when the selected child is the only child.
Thus a non-NULL child does not isolate the child callback. This is a reference
implementation finding, not proof of this OEM core's exact code.

The new optional candidate directly invokes the freshly pinned binding's
`Stop(binding, gpu, 1, &child)`. The bundled DriverBinding.h permits direct calls
subject to valid started-controller/created-child restrictions. Fresh BY_DRIVER
and sole BY_CHILD_CONTROLLER relationships, valid enumerated handles, exact
image/code/Stop pins and two matching precollections establish the checked
runtime prerequisites. No Start is called and historical Start execution is
inferred from those current relationships, not independently traced.

See [twelfth test contract](AD104-TWELFTH-BOOT.en.md). The implementation, 193
RAM lifecycle checks and native build are complete; hardware execution is not.
USB staging awaits acceptance of the new direct-call/GOP-removal risk boundary.
The earlier investigation proposal is retained below to explain the correction.

Status: investigation contract only. No implementation, staging or physical owner
callback is claimed. The [eleventh result](AD104-ELEVENTH-RESULT.en.md) establishes
consumer removal; it leaves PCI command `0007` and owner/child relationships active.

The next smallest candidate is exactly
`DisconnectController(gpu, pinned_IMG29_binding, discovered_child)` after fresh
verification of the three-consumer result. The bundled UEFI declaration in
`deps/edk2/MdePkg/Include/Uefi/UefiSpec.h` distinguishes a non-NULL ChildHandle
from NULL (all children). Confirm the actual firmware dispatch path before use;
never substitute `DisconnectController(gpu, binding, NULL)` here.

[Static lifecycle inspection](AD104-LIFECYCLE.en.md) identifies child cleanup at
IMG29 RVA `0xcd0`, with protocol teardown and legacy INT10 services. The separate
zero-child Stop at `0x7c4` can request Disable mask `(Supported & 0x700) | 0x18`,
predicted `0x718` on this machine. Parent stop and decode enable are separate
experiments; they must not be silently included in child cleanup.

Required design before a test:

- Fresh code identities, exact BDF/controller/binding/child, four-edge graph,
  shared flag zero, pointer counts one each, full PCI state and pinned Windows
  boot image must agree twice; flush one-shot intent and recheck before calling.
- A new collector must survive a removed GOP child and never dereference saved
  GOP, mode, text or driver-private allocations after the call. The current
  display collector requires a live GOP and cannot serve as the postcollector.
- Expected relevant graph is three edges: PCI BY_DRIVER ownership and both
  pointer edges remain; PCI BY_CHILD_CONTROLLER is absent. Query relevant child
  protocol absence with fresh firmware enumeration, not stale interface reads.
- Command `0007`, attributes `0718`, supported `1b77b` and full PCI configuration
  should remain unchanged for this candidate. This is a prediction to test.
- Even an error callback requires independent repeated observations; partial
  protocol removal, wrong edge, pointer/input drift, configuration drift or
  logging failure blocks advancement and Windows handoff. No retry/reconnect,
  attribute writes, MMIO, reset, parent stop or unlock.
- RAM fault tests must cover precondition drift, each partial cleanup outcome,
  freed-interface non-access, callback errors, logging and final-input failures.

New risk boundary: child cleanup may reinitialize legacy video and remove/free
GOP interfaces used by subsequent boot code. Display/input may disappear, firmware
may hang, and Windows may fail to start even though consumer-only tests returned.
Manual power-off and USB removal may be required; recovery is not established.
Present this concrete boundary before staging the attended owner-child test.

A successful child-only result would still leave PCI ownership and BME enabled.
Only a separately admitted parent release and independent cold-entry readback can
advance the remaining [unlock proof gates](AD104-UNLOCK-EVIDENCE.en.md).
