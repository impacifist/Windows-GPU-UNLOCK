# EVGA19 unlock transaction result

[English](EVGA19-RESULT.en.md) | [한국어](EVGA19-RESULT.ko.md)

Movian reported screen flashes followed by an automatic Windows boot after Shift+G. The supplied EVGA19 log records admission, firmware execution, selector values SS0 `0x88888888` and SS1 `0x8` before and after PCIe FLR, and transaction result zero with no recorded fault, quiesced resources and no retained mappings. Cleanup and ready-Windows status also returned success. The post-FLR SS1 value is split across two log lines.

This is a successful recorded transaction and reported Windows handoff on this machine; it does not establish a performance gain or independently verify post-boot GPU correctness. The next step is the same benchmark executable and settings as the [baseline](BENCHMARK-BASELINE.en.md), in the same Windows session if possible.

The raw log is not published; its SHA-256 is `dfd6a0e0e58b8bc794e87a6c1b0e774ac674d5b483985ba36439b53fbc597537`.
