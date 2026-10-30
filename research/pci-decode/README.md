# PCI decode-cache synchronization research

The correction core and owned adapter are host-testable research libraries,
**not an executable fix**. No production build, probe, LOCAL profile gate or USB
staging script invokes correction. The generic diagnostic uses only the shared
read-only collector/predicates for optional observations. Current captures with
inconsistent PCI attributes remain refused by LOCAL admission.

Reports [#102](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/102),
[#121](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/121) and
[#129](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/129) describe a
GPU command register of `0x0002` but PciIo attributes of `0x300` (IO + memory).
Contributors report that disabling the IO attribute reconciled these values
before running their separate unlock workflow. Their results concern specific
firmware/builds; they do not establish a generic firmware correction.

The independent implementation here offers exactly one write capability:
`DisableIo`. It does not adopt the contributor pre-step's initial IO-enable attempt.
Its callback must eventually be bound to the reviewed GPU's
`PciIo->Attributes(Disable, IO)`. The research library adapter described below is
implemented and tested only against RAM-backed EFIAPI mocks; it is not connected
to any boot image and has not been authorized or tested on physical hardware.

`GuDecodeSync` requires an exact baseline GPU and 1–16 caller-verified immediate
parent bridges, including location, handle/interface tokens, all 64 config
dwords and full attribute/support values. It rejects aliasing, invalid BDF ranges,
inconsistent parent buses and unreviewed target identities. GPU command must be
exactly `2`; the only admitted discrepancy is raw attributes `0x300` instead of
`0x200`. An already consistent baseline is observed without a write.

Before the single Disable call, the core freezes its input/callback table, flushes
the intent log and checks a fresh complete snapshot. It compares the complete GPU
and every parent after the call and at four bounded logical follow-ups. Only the
GPU's IO attribute may clear; raw PCI config, memory/BME state, other attributes,
locations, interfaces and parents must remain identical. Only `EFI_SUCCESS` and
canonical boolean log success are accepted. A failed write still receives one
bounded observation because an error does not prove that state was unchanged.
There is no retry, rollback, IO/BME enable, config/MMIO write or chainload capability.

The caller must retain one zero-initialized transaction object and never reset
it to retry. Reentry using the same object refuses before a callback. The caller
must also serialize operations and prevent reuse through another object. The
snapshot callback's honesty, ownership and freshness are separate obligations;
tokens and a `Complete` flag are not authenticated evidence. Parent topology must
be verified from actual device paths by the future adapter, not inferred only
from matching bus numbers. Immediate host samples do not prove timed stability
or detect every change between observations. Hidden firmware state and real GPU
behavior are outside this mock model.

Run only the host model from an MSVC/Windows SDK developer environment:

```powershell
python -I research/pci-decode/test-decode-sync.py --mutations --clang C:\TOOLS\LLVM20\bin\clang.exe --output research/pci-decode/build/your-new-run
```

Use a new output directory. Tests exercise failures/warnings/partial output,
callback reentry and input mutation, log failure/drift, changes in every config
dword of the GPU and all 16 parents, full attribute/token/location drift, and
attempts to reuse a transaction. All protocol effects are RAM callbacks; no GPU,
firmware, boot setting or physical media is accessed. Passing this model grants
no hardware support, LOCAL write evidence or permission to run a correction.
The optional mutation checks compile twenty deliberately broken copies in the ignored
output directory and require the host tests to reject them. A compile failure does
not count as detecting a broken guard. Original source files remain unchanged.

## Research UEFI adapter

`efi-adapter.c` binds an exact caller-supplied baseline and PCI controller paths.
Initialization requires the caller's non-NULL `OwnerImage`, validates/copies inputs without firmware calls and cannot reset a
used context. Run freezes a private copy and spends the context before its first
callback, including against reentry through a fresh core transaction object.
Caller-owned policy changes cannot alter the active copy. Contexts must remain
private, zero-initialized and serialized; callers must not reset/recreate them to retry.

The adapter now observes an already-held exclusive claim on the GPU's PciIo
protocol. It never acquires or closes that claim. The caller must retain it for
the entire call, retain the input/context/transaction storage and all interface
lifetimes, and serialize other operations. The research-only initialization API
adds `OwnerImage` immediately after `Boot`; no shipped API or build uses it.

`OpenProtocolInformation` must return exactly one self-agent, NULL-controller,
`EXCLUSIVE`, `OpenCount=1` record. Additional observation records may have only
the exact passive attributes 1/2/4 and positive counts. Driver/child openers,
BY_DRIVER|EXCLUSIVE, foreign/duplicate exclusive records, missing output, warnings
and malformed counts refuse. Queries are bounded to 64 records; byte-range checks
reject known borrowed storage, misalignment and address overflow before access or
free. These checks cannot authenticate an arbitrary pointer as a pool allocation;
the firmware service's allocation contract remains a requirement. This follows the
[UEFI OpenProtocolInformation and pool-buffer contract](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html).

Ownership is sampled around full snapshots, after successful logging, and once
more before Disable(IO). The final query's pool buffer stays allocated until after
that call, avoiding an intervening FreePool callback. Holding this buffer does
**not** lock or extend protocol ownership. OpenProtocolInformation and FreePool
method pointers join the frozen whole-chain capability checks. Returned allocations
are freed once, including valid outputs accompanying an error; cleanup failure is
not retried and permanently marks `OwnershipFault`.

The context separately retains `OwnershipStatus`, `CleanupStatus` and the actual
`DisableIoStatus`. `OwnershipObserved`, `CleanupObserved` and `DisableInvoked`
distinguish an uncalled operation from a real EFI_NOT_STARTED return. First failures
survive subsequent successful observations. The core's Attempted/adapter WriteSpent
flags can be set before the actual protocol call; neither proves that Disable ran.
A failed cleanup after a write therefore remains an attempted write with uncertain
cleanup, even if the write itself succeeded.

This is a guard for one protocol's observed opener list, not ownership acquisition,
all-controller isolation or an atomic snapshot. In particular, a final ownership
query or cleanup callback that violates its read-only/device-state contract can
change GPU/parent state after an earlier read. Finite rechecks cannot exclude every
such window. Caller serialization, continued claim/lifetime retention and trusted
firmware API behavior remain necessary; passing mocks does not prove physical safety.

Production integration is still unresolved: full-release's CapturePermit rejects
the real mismatch before NVIDIA Stop, whereas its exclusive claim is acquired
after Stop in RestoreDecode. A self-exclusive adapter cannot simply replace the
contributor pre-step while NVIDIA still owns the protocol. This change does not
relax CapturePermit, stop a driver, force an exclusive OpenProtocol, restore decode,
stage an image or qualify any of the reported machines.

The GPU controller route uses the existing bounded PNP0A03/PNP0A08 parser. Two to
sixteen PCI nodes correspond to one to fifteen immediate parent bridges. Each
parent's path must be the exact corresponding prefix plus an End node; its PCI
device/function must match the last node. This adapter's parent limit is smaller
than the standalone core's sixteen-parent bound.

Each snapshot uses `HandleProtocol`, `LocateDevicePath`, `GetLocation`, a full
64-dword PCI read, and Attributes Get/Supported. It requires the pinned interface,
exact path bytes and exact Locate consumption at the End node. Two full chain
scans use different config initialization patterns, so successful partial reads
cannot pass merely by leaving the same sentinel twice. Method pointers and both
BootServices methods are pinned and rechecked, including all devices after the
last callback. Before the sole Disable(IO), the adapter performs another full
scan and callback-free whole-chain capability check. It does not expose generic
Attributes, config-write, MMIO, install/uninstall, disk or boot capabilities.

The mock adapter suite injects warning/error with valid output, absent output,
partial config, path/Locate-consumption errors, changed interfaces/methods,
early and late parent drift, Init/Run reentry, caller input changes and a failing
write that nevertheless changes state. Mutation checks also break Locate bounds,
the differing sentinels, whole-chain method checks and one-shot initialization.
Ownership fixtures cover callback-time claim loss, passive/managing combinations,
known borrowed/misaligned/overflowing buffers, output-less success, error with valid
output and pre/post-write cleanup failures. A reproduced EFI_NOT_STARTED status-loss
regression verifies that later successful queries cannot erase the first error.

This is not ownership acquisition, GPU qualification, a snapshot of all firmware
state, atomic observation across devices or proof of real-time stability. A future
experiment must independently establish authorized ownership, lifetime/freshness,
boot/display consequences, logging durability and physical validation. Existing
read-only probes, LOCAL admission and build-evidence rules remain unchanged.

## Current-boot baseline collector

Pure snapshot data/predicates now live in `snapshot.{h,c}` and EFI binding data in
`efi-binding.h`. The collector depends only on that read-only layer. The actuator
core and ownership adapter remain separate source units; a diagnostic must not
link them just to obtain snapshot helpers. This split preserves the existing
predicate bodies and ID/command admission without widening either envelope.

`efi-capture.c` supplies read-only baseline acquisition for the research adapter.
The caller supplies an exact GPU handle, segment/bus, PCI/subsystem IDs, BAR0 and
bounded controller-path bytes. The collector follows each immediate parent prefix
using LocateDevicePath, verifies exact End consumption and current live path bytes,
and reads GetLocation, all 64 PCI config dwords and Attributes Get/Supported.
It never calls OpenProtocolInformation, Allocate/FreePool, Attributes Enable/Disable,
config writes, MMIO, driver release, logging or boot services that launch an image.

Two full scans use different config sentinels and out-of-range location/attribute
sentinels. The first scan fixes every handle/interface/method; the second cannot
silently adopt replacements. Both must satisfy the caller's GPU pins, the existing
decode-core shape restrictions and exact full-snapshot equality. Whole-chain method
checks also run after callbacks, catching a late parent changing an earlier device's
method. `efi-path.h` shares controller validation with the existing adapter.

Use one private zero-initialized `GU_DS_CAPTURE`. It is spent before any service
callback and refuses reentry/reuse. Input/context/Boot overlap refuses without
clearing those aliased arguments. Failures during a fresh, disjoint capture publish
zero bindings/snapshot, Count=0 and Ready=FALSE; only success commits a complete result.
Reuse/reentry rejection preserves the old result. Always require this call's
EFI_SUCCESS, rather than interpreting an earlier Ready flag as a new capture. Request values are
copied privately before callbacks; interface and backing storage lifetimes still
belong to the caller. Protocol-returned pointers must obey the firmware API contract.

The current bound remains 2–16 PCI nodes (1–15 PciIo parent bridges), PNP0A03/PNP0A08
roots with captured UID, 16-bit segment and nonzero GPU bus. The decode-core ID scope
remains 2204/2203/2484/2487/2504, command=2, reviewed class/BAR shape and consistent
parent buses/attributes. In particular, production LOCAL's newer 24c9 admission
does not automatically widen this research correction scope; it is tested as refused.

Ready means a consistent observation under these rules. It proves neither absence
of another GPU nor chip/VRAM/PROM/driver-code qualification, ownership, atomicity or
timed stability. It does not authenticate a previously unknown device or authorize
an unlock. The returned binding array and snapshot can feed GuDsEfiInitialize only
after its independent held-claim/lifetime/serialization requirements are met.
The composition test does this against RAM EFIAPI mocks; no firmware code executes.
The production pre-NVIDIA-Stop integration issue described above is unchanged.

The collector uses bounded private per-image scratch rather than large automatic
WORK/snapshot arrays. A guard rejects callback reentry even through a fresh context
before that scratch can be overwritten. It is not a multiprocessor lock; callers
must still serialize operations. Results are copied into the caller's context and
do not retain pointers to the scratch arrays.

Before connecting this code to a shipped diagnostic, validate its actual EFI link,
not only COFF compilation. Run this from an ordinary host shell:

```powershell
python -I research/pci-decode/test-readonly-image.py --clang C:\TOOLS\LLVM20\bin\clang.exe --lld-link C:\TOOLS\LLVM20\bin\lld-link.exe --stack-mutation --output research/pci-decode/build/your-new-readonly-link
```

This builds a synthetic, invalid-target entry point with only the collector,
snapshot predicates and RAM runtime. It checks the real PE/map, refuses actuator
symbols, records compiler stack usage and requires each frame to stay within
2 KiB. The optional negative restores the old automatic arrays: it must compile,
then fail the freestanding link specifically for `__chkstk`, with a frame above
4 KiB. Do not hide that failure with a stack-probe stub or by disabling probing.
The tested clang 20 build reports 248 bytes maximum versus 13,960 bytes in the
negative. These are build observations, not physical firmware stack guarantees.
No image is executed, staged or an authorized package.

The generic diagnostic now links only this read-only layer for optional parent
baseline records, under `GU_PROBE_PARENT_BASELINE`. Its manifests bind the external
read-only files; LOCAL input hashes include the same files. Parent records use
distinct syntax, and the final target/capability check remains before bounded MMIO.
An unavailable optional observation never permits a changed/NULL method to be
called. Capability compromise ends the capture using frozen cleanup, without
continuing into graph services whose methods may have changed. Named probes do
not request this extra collection. The correction core and owned adapter remain
unconnected; no physical EFI or unlock qualification was performed.

Capture fixtures cover all parent depths, both root types, UID/segment boundaries,
stable wrong GPU pins, bad bus chains, interface replacement between scans, missing
parent resolution, partial output, strict status handling, mutable input, callback
reentry and overlapping arguments. Four extra compiled mutations remove GPU pinning,
two-scan equality, interface pinning and whole-chain capability checks and must fail.
