# Related issues and what transfers to EVGA3090

[English](RELATED-ISSUES.en.md) | [한국어](RELATED-ISSUES.ko.md)

Reviewed issue bodies and relevant comments on 27 October 2026. These are comparisons of reported evidence; raw captures from the other machines were not independently revalidated here.

| Report | Shared evidence | Difference and consequence |
|---|---|---|
| [#95: ASRock B760 / ZOTAC3090](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/95) | One physical display controller, two GOPs, pathless zero-framebuffer aggregate, missing ConOut variables, topology_scope refusal. | Different BIOS and console binaries. Matching another report's PROM/nvgop does not qualify its console callbacks or Movian's EVGA card. Closest independent topology comparison. |
| [#75: ASUS B660M / 3090 FE](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/75) | Two GOPs including pathless aggregate with zero framebuffer/info size; shared system console, missing ConOut, generator refusal. | Different firmware/card and reported volatile registers. Similar aggregate shape is not exact code or release equivalence. |
| [#52: ASUS Z690 / RTX3090](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/52#issuecomment-6427173800) | Physical GOP → combo agent → virtual GOP/text → system console; separate EDID BY_DRIVER edge. Same combo-file GUID as Movian's IMG17. | Reported combo image size 0x2160 versus Movian's 0x2180, despite the same GUID and Stop RVA. File GUID/offset alone cannot substitute for exact executable hashes. Missing ConOut and unsupported aggregate layout are separate blockers. |
| [#80: MSI Z690 / RTX3060 Ti](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/80) | Early preparation refusal before transaction; ownership expectations do not match the reported graph. | Physical GOP and text share a handle, with no GOP child open. This is not Movian's two-GOP chain. Do not require an invented child or remove checks to force a common release sequence. |
| [#87: ASUS B760 / ZOTAC3060 GA104](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/87) | Pathless aggregate and single physical GPU. | Two NVIDIA display children: GOP+EDID and EDID-only sibling. Adds a separate cleanup obligation; outside our one-membership test. Capture also reports an unreviewed-envelope refusal. |
| [#32: EVGA3090 / ASUS Z790](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/32) | Same vendor/GPU family and Z790 platform; report includes a successful transaction and Tensor benchmarks. | Subsystem 3842:3982 versus Movian's 3842:3387; different board/BIOS, iGPU display route and experimental binary. Success is exact-machine evidence, not a ready profile for Movian. |
| [#4: ASUS B660M / RTX3070 Ti](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/4) | Reported hardware-verified aggregate-console release and Windows handoff. | Different GPU and pinned firmware. Demonstrates feasibility of a reviewed aggregate route, not universal AMI support. |

## What this means for our implementation

Two GOP handles can represent a physical GOP plus firmware aggregation, not two physical GPUs. A missing ConOut NVRAM variable does not mean the system-table console is absent. The generator's non-aggregate admission refusal and the live lifecycle harness's protocol queries are separate failures and should remain separately diagnosed.

The existing aggregate implementation in `internal-boot/boot/prepare-console.c` distinguishes ComboFile, GfxFile and text owners, checks exact bindings, and models additional child/runtime relationships. Movian's IMG17/IMG16/IMG13 roles follow this broad layering. Its profile constants and complete release sequence must not be copied to Movian's machine without qualification.

Our current correction uses fresh ProtocolsPerHandle membership to require zero GOP and exactly one text GUID on the system console, while retaining physical/virtual GOP and owner/code checks. None of the reviewed issue reports establishes the exact EVGA03/04 discrepancy: HandleProtocol(GOP) returning success with a pointer while fresh membership lists no GOP. Its underlying cause remains unknown. The new absence-evidence method is not evidence that firmware APIs generally malfunction or that Stop is safe.

The next useful hardware result is therefore still the existing single IMG13 membership Stop, conditional on all remaining live guards and fresh G, followed by edge verification and Windows handoff. These comparisons do not justify adding IMG16/17, EDID or parent stops to that test.

## Recovery correction from the Ada discussion

[Yakold's latest correction in #54](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/54#issuecomment-6455834943) retracts the claimed tested PciIo mapping and failure-specific halt/FLR finalizer. He cannot supply the tested EFI, paired logs or original ESP assets; the cleanup fragment is not tied to a known executed binary. The supplied asset/version and upstream identifiers are useful research leads, not verified recovery evidence. Do not use that earlier finalizer claim to reduce the disclosed EVGA recovery risk. This is also a different architecture/transaction issue, not an explanation for the current console query discrepancy.
