# EVGA14 cold-state and PROM result

[English](FIFTEENTH-EFI-RESULT.en.md) | [한국어](FIFTEENTH-EFI-RESULT.ko.md)

Both batches match: GFW PLM8b8f, BOOT3ff, CPUCTL10, WPR2_HI0, feature PLMffffff8f, SS0 30053014, SS1 7, memory6000. Full1MiB PROM hash matches the original. Decode disable/restoration checks, both captures and ready-Windows pass. The operator confirmed normal Windows loading after Shift+G at 01:16. This establishes operator-reported arrival in addition to the recorded ready event.

The observed non-selector cold values meet app.c ColdCheck comparisons, but fixed stock selectors do not match SS0/SS1. port.c GuLatchEntrySelectors accepts the observed selector ranges for an entry-latched policy; that is offline policy compatibility, not successful upload, selector modification or unlock. Never replace this baseline with another machine's fixed selectors. PCI/BAR/configuration, ownership handoff and complete runtime integration still require qualification.

[Sanitized result](fifteenth-efi-run-result.json). Raw log ignored. Next useful preparation is CPU-only asset generation; no repeat EFI is requested for this completed state capture.
