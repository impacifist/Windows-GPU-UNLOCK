# Actual unlock: current admission gaps

Date: 2026-10-25. Objective: a verified unlock on this B550 / RTX 4070 Ti,
with state evidence and matched numerical/performance measurements. The
[fixed-binary result](BINARY-TENSOR-RESULT.md) is a stock control, not that result.

## Why another live Windows reset is not the next unlock test

The reviewed AD104 implementation runs in UEFI before the Windows GPU driver.
Its `vendor/unlock_610.c` cold-entry path rejects enabled WPR2 as already
initialized since power-on, explicitly describing restarting FWSEC/Booter in
that state as untested. It also requires exact permission, boot, CPU-control,
selector and profile checks before firmware execution.

The local driver restart proved a vendor-reported restart and later usable
compute. It did not capture WPR2 or establish those preboot entry conditions.
No claim is made that this machine's current WPR2 value was read. Administrative
access and acceptance of desktop disruption do not supply a validated warm-state
unlock path. No such transaction was attempted here.

## Reference versus this machine

| Requirement | Reference | Local evidence / missing input |
|---|---|---|
| GPU and subsystem | 10de:2782 / 1462:5132 | Match |
| Platform | MSI Z490 / BIOS A.E0 | ASUS B550-F / BIOS 3205; different topology |
| VBIOS | 95.04.3c.40.fd | 95.04.31.00.f0; different firmware |
| ROM admission | Exact 1 MiB SHA-256 | Physical PROM not captured/validated |
| FWSEC admission | Exact image layout/signatures | Local image not parsed/validated |
| PCI location and BARs | Captured preboot values | Windows bus 0B observed; preboot location/resources unknown |
| Cold state / display owners | Captured registers and UEFI graph/code | Not captured on this platform |
| Windows handoff | Exact ESP/image/geometry pins | No local handoff profile qualified |

The reference capture diagnostic itself currently requires bus 1, BAR0
`0x80000000`, length `0x01000000`, memory decoding enabled and bus mastering
disabled before its selected register sampling. Its `sample.c` guard cannot be
assumed suitable here. Windows bus 0B is a lead, not proof of preboot enumeration.
Changing the guard to accept everything would defeat capture admission.

## Next execution gate

The existing no-PC-reboot boundary does not accommodate the reference's cold
manual UEFI route. A separate scope decision is required before shutdown or
manual boot. Even with that scope expanded, the first proposed boot is a
capture-only diagnostic adapted and reviewed for this host, not the unlock EFI.

Preparation must preserve current boot files, boot priority and recovery access.
Initial capture should discover PCI resources and ownership with explicit
completeness reporting; selected MMIO/PROM sampling needs reviewed local bounds.
Raw ROM and loaded code remain private. After capture, derive a separate local
profile and run exact admission/ownership/transaction mocks before proposing an
unlock attempt. A firmware failure may require a full power-off; reboot-free
recovery cannot be promised.

Success requires actual transaction completion, expected selector/effective
readback, cleanup evidence, Windows return and matched fresh-process numerical
and throughput checks. No benchmark percentage, build success or selector write
alone meets that criterion. No unlock outcome is established at this checkpoint.

## Source anchors

Inspected external checkout:
`JustAResearcher/gpu-unlock-ada@4e7fefcc4923f9e083d5324dc75a7d2f906af77f`.

- [Reference procedure and recovery boundary](https://github.com/JustAResearcher/gpu-unlock-ada/blob/4e7fefcc4923f9e083d5324dc75a7d2f906af77f/windows/ad104-msi/README.md)
- [Cold-entry checks](https://github.com/JustAResearcher/gpu-unlock-ada/blob/4e7fefcc4923f9e083d5324dc75a7d2f906af77f/windows/ad104-msi/vendor/unlock_610.c)
- [Pinned ROM/FWSEC/profile checks](https://github.com/JustAResearcher/gpu-unlock-ada/blob/4e7fefcc4923f9e083d5324dc75a7d2f906af77f/windows/ad104-msi/profile.c)
- [Diagnostic resource guard](https://github.com/JustAResearcher/gpu-unlock-ada/blob/4e7fefcc4923f9e083d5324dc75a7d2f906af77f/windows/ad104-msi/diagnostic/src/sample.c)
