# EVGA RTX 3090 contributor capture review

[English](README.en.md) | [한국어](README.ko.md)

This separate branch starts at upstream main c403e8f0002ec69fc162a145d8172b2782ec9645. The contributor reports an ASUS Z790/i9-14900K/EVGA3090 machine; those motherboard/CPU descriptions are operator context, not independently authenticated by these files.

The uploaded ZIP contains50 files: two distinct426367-byte diagnostics,46 loaded-image dumps,1MiB PROM and README. Both diagnostics contain capture-complete=1 and diagnostic-result success. The offline checker identifies one NVIDIA display controller220410de, subsystem33873842, GA102 b72000a1,24576MiB at0000:01:00.0. It finds no other PCI display controller but two GOP handles. The saved PROM SHA256 matches the value logged in both captures.

The current automatic local-profile route refuses both captures with topology_scope. Direct read-only parser analysis identifies its first refusal: without an integrated GPU, the RTX must own the only GOP in the graph; two were found. This does not establish an integrated GPU is physically absent or identify the second GOP's role. Missing ConOut variable records are also present; later gates have not all been evaluated. Do not bypass this refusal, copy another profile, or infer unlock compatibility from the GPU model.

[Sanitized results](capture-summary.json) preserve hashes and the checker output. Full diagnostics and firmware binaries remain private/ignored; no executable from the archive was run. Distinct capture hashes do not independently prove separate full shutdowns or Windows arrival. No profile generation, assets, GPU writes, firmware execution or unlock test was performed during this review.

Next: obtain the contributor's confirmation of two full-shutdown capture runs and Windows outcomes, then inspect the two GOP device paths/owners and console state offline. Any widened topology requires a separately reviewed implementation and replay evidence; these results alone authorize no write build. The4070Ti branch and its staged test are separate and unchanged.

[Next test and topology findings / 다음 시험과 토폴로지 결과](NEXT-TEST.en.md).
