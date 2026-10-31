# EVGA12 NVIDIA parent release

[English](THIRTEENTH-EFI-RESULT.en.md) | [한국어](THIRTEENTH-EFI-RESULT.ko.md)

EVGA12 records successful NVIDIA parent Stop after six consumer Stops and the NVIDIA three-child Stop. The post-parent PCI command word is `0x0000`; postchecks pass, both captures complete and the ready-for-Windows event succeeds. The operator reports "Same as last": automatic Windows return after a delay.

Independent comparison of the recorded graphs finds 549 handles before and 546 afterward, exactly three child handles removed and no additions. The parent private protocol and NVIDIA PCI BY_DRIVER open are removed, along with the parent device-path child opens. Only the virtual GOP remains; EDID-discovered handles are absent. This is a major ownership-release milestone on this exact machine, not proof of unlock or qualification of every unlock prerequisite. Recorded checks do not establish that all indirect hardware effects are understood.

No unlock transaction, firmware upload or explicit GPU reset was included. The final verification immediately before Windows intentionally writes no log; the recorded ready event and operator report must be distinguished from a trace of that final check.

[Sanitized result](thirteenth-efi-run-result.json). Raw EVGA12 remains ignored. Next: [locked-state benchmark](NEXT-BENCHMARK.en.md), before any step4 unlock attempt. Preserve the same executable and settings for the later comparison.
