# EVGA21 attended profile result

[English](EVGA21-RESULT.en.md) | [한국어](EVGA21-RESULT.ko.md)

Movian supplied EVGA21 and the slot-5 marker after running the first numbered profile build. The log records slot 05, admission, the expected selectors before and after FLR, transaction result zero, no recorded fault, quiesced resources, no retained maps, asset cleanup, two complete captures and ready-Windows success. The post-FLR SS1 value is split across two log lines. The capture alone does not establish that Windows started or that performance improved after this run.

The exact EVGA21 capture was used in a local mock-storage slot-6 staging fixture, which passed the previous-slot evidence checks. A local slot-6 EFI was built and PE-verified; actual rearming and firmware execution on Movian's machine remain untested. Raw EVGA21 SHA-256: `10c8c23267072a91356efba6a1c56ec304f2839a86317a8b01d5fc1459a823ee`. The contributor reported typical Qwen performance around 33 tokens/s compared with an observed approximately 37.4 tokens/s screenshot, but matching settings and controlled before/after runs are not established.
