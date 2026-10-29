# Twenty-fourth candidate: sampled-version signature installation in RAM

[English](AD104-TWENTY-FOURTH-BOOT.en.md) | [한국어](AD104-TWENTY-FOURTH-BOOT.ko.md)

Test23 physically passed RAM patch preparation and automatic Windows recovery. Progress remains15/26 milestones (58% checklist), not an unlock probability. SS1 and exact-machine admission remain open.

## Finding from existing captures

The fixed register list already includes offset0x8241e0 (index20), twice per boot. Test23 recorded raw1 both times. The captured, pinned FWSEC descriptor has ucode_id9. The production vendor code computes0x8241c0+(ucode_id-1)*4, which equals0x8241e0. Its low16/highest-set-bit-plus-one conversion maps raw1 to version1; the production signature selector with versions3 and signature size384 chooses offset384, within the768-byte signature region.

[NVIDIA's published register map](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/main/src/common/inc/swref/published/ampere/ga100/dev_fuse.h) identifies0x8241e0 as GSP microcode9 version data15:0. The direct application here is also supported by the local production vendor/unlock_610.c address calculation and exact descriptor. This interpretation adds no register access and does not prove authentication.

A separate [Linux register definition](https://kernel.googlesource.com/pub/scm/linux/kernel/git/next/linux-next/+/2f03a3fe84216541ed29f9b7bc014acb5f1b676b/drivers/gpu/nova-core/fb/regs.rs) decodes VGA_BASE bit3 as address-valid and bits31:8 as a64KiB-scaled address. Observed0x10e09 therefore describes a valid workspace at0x10e0000, whereas reference1 has bit3 clear. This explains a concrete field difference, not why firmware left this workspace or whether it is safe to ignore. The reference gate stays intact; memory-layout and SS1 admission are not solved.

## Candidate scope

Repeat test23's exact GPU sequence: five stops, IO/MEM enable/disable,44register reads,262274PROM accesses and2chip bookends. No new GPU address or access, fuse read, reset, upload, execution, BME/VGA enable or transaction. Use batch2 only after both batches match; validate index/offset/status/count. The version sample was collected **before PROM**, not freshly read after capture and not authorization to execute.

After the existing exact-ROM/canary-protected scratch patch, accept only the known raw1 at the descriptor-derived address. Select the second384-byte signature from PROM0x45300..0x4547f, copy to scratch image0x10024..0x101a3, and independently compare every image byte. Only the previous44command+4mapper bytes and384signature bytes may differ. Verify source PROM and scratch canaries; record selected-byte and assembled-image hashes. Scratch is then overwritten by ordinary file readback, never persisted as executable firmware.

Unknown ROM or unsupported sample is an observation failure, retaining normal capture/recovery when integrity and hardware guards pass. Invalid sample metadata, source/canary corruption, I/O or hardware drift blocks handoff with normal buffer cleanup. No transaction context, authentication result, profile latch or execution authority is set.

Host actual-PROM positive:393changedbytes. Selected384-byte SHA256:
`7806dc110f1bbf58f3b41f4414c4e0426000b0d985a66013d035a6ea4d0e6dd5`.
Assembled RAM image SHA256:
`5004460ab7fea25b83cc95b6468456d830b98700926ed0c8696e49505eb19d07`.
Host tests exhaust all65536low16conversioninputs, high-bit masking, unsupported/sentinel samples, wrong offset, pinned hashes and inherited source/canary/error guards. Host proof already establishes assembly; EFI checks integration/reproducibility only.

Native EFI65,536bytes, AMD64/noimports, SHA256:
`bb384ad8668118450b56751a22c60e37b73cde48650ed176672e68afbf3a7e98`.
Physical test24 pending. Firmware execution and unlock remain false.

Save work, full shutdown, wait10seconds, UEFI USB, Shift+G once. Wait10seconds then tap Shift+V every2seconds up to3minutes. If no Windows after10minutes, power off/removeUSB/select normal Windows Boot Manager. Recovery is unproven for this run; run once, preserve captures, do not blind retry.

[Actual result / 실제 결과](AD104-TWENTY-FOURTH-RESULT.en.md).
