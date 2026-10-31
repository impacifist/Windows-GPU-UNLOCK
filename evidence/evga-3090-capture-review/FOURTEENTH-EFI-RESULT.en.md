# EVGA13 post-release access result

[English](FOURTEENTH-EFI-RESULT.en.md) | [한국어](FOURTEENTH-EFI-RESULT.ko.md)

EVGA13 (828614 bytes, SHA256 4c381605ffb0a25e07414388758d6bb2cb542b4ab4e04e34610d8fd4a9ec3864) records all eight Stop callbacks and postchecks, successful memory-only enable and command2 check, two BAR0 chip values b72000a1, successful memory disable and two command0/restored-attribute checks. Both captures complete and ready-Windows succeeds. The operator uploaded the log/marker without explicitly stating automatic/manual Windows return; that outcome remains unconfirmed.

This establishes bounded post-release chip access and successful recorded cleanup on this run. It does not establish the remaining cold registers, post-release PROM identity, DMA mapping or unlock. The code's final no-log checks are not independently recorded. Raw capture remains ignored; [sanitized result](fourteenth-efi-run-result.json).
