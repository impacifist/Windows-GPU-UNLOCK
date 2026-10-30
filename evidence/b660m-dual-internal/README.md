# B660M / dual RTX (3090 Ti + 3070 Ti) headless internal-ESP evidence

## Provenance and build scope

These results come from one machine, reported by the contributor, on 2026-10-28.
The maintainer has not run a hardware test for them. The EFI hashes in
[summary.json](summary.json) are the files that were booted. They were built from
this PR revision with `research/headless/build-unlock.ps1 -HardwareProfile b660m-dual`.
The wrapper embeds the builder's Windows ESP GUID, so its hash differs on other machines.

Same physical machine and AMI BIOS 4003 as the reviewed `B660M_3090TI`/`B660M_3070TI`
entries, now with both cards installed: the Colorful RTX 3090 Ti on the first
x16 root port and the MSI RTX 3070 Ti on the second. The multi-card profile
orders non-console targets first and processes every card through the same
guarded pipeline. This is the first multi-target (`GU_MULTI_TARGET`) profile.

## Machine

ASUS TUF GAMING B660M-E D4 (AMI BIOS 4003), Core i7-12700F (no iGPU):

- card 0: Colorful RTX 3090 Ti 24 GiB (GA102, `10de:2203` / `1301:7377`, BAR0 0x84000000)
- card 1: MSI RTX 3070 Ti 8 GiB (GA104, `10de:2482` / `5052:1462`, BAR0 0x82000000)

Windows 11 Pro 22000, driver 616.92. Secure Boot off, BitLocker off, Fast
Startup off. AMI aggregate console topology, same fingerprints as B660M_3070TI.

Note: with a second adapter populated, the firmware reassigned the 3090 Ti
BAR0 from `0x82000000` to `0x84000000`; the pre-release checks read the BAR
base from config space at run time, so no constant needed changing.

## What this run validated that single-card did not

1. Two independent transactions in one boot: card 0 claimed exclusively
   (no firmware driver bound); card 1's firmware-bound `nvgop-ga1xx` driver
   was released through the guarded `DisconnectController` path and the card
   was re-claimed, all gates re-verified between release and transaction.
2. The handoff driver-graph comparison tolerates exactly that fingerprinted
   `nvgop` release on a non-console GPU; every other edge change still stops.
3. Windows Boot Manager started after both transactions and the driver
   reported both cards (`nvidia-smi`: 3090 Ti 24564 MiB, 3070 Ti 8192 MiB).

## Recorded hardware runs

| Run | Boot | Cards | Result |
|---|---|---|---|
| AUTO000029 | full shutdown | both | both transactions result=0; wrapper stopped at graph match before the release tolerance existed (fixed in this revision) |
| AUTO000030 | full shutdown | both | `FINAL status=0 clean=1`, one FLR per card, wrapper `verify-complete`, Windows started |

After the run: each card `SS0=0x88888888 SS1=0x8`, GFW boot completed, WPR2
cleared, BME off, no retained mappings (`transaction result=0 containment=0
quiesced=1 fault=0 retained=0 maps=0 FLR=1`).

Excluded: PROM images, raw probe/graph logs, driver dumps, partition
identifiers, EFI images. Not established: repeated cold-boot reliability,
the reverse card order (3070 Ti in slot 1), booting with only one of the two
cards, sleep/hibernate resume, long-term stability and a controlled locked
baseline.

See [summary.json](summary.json) and [report-excerpt.txt](report-excerpt.txt).
