# Reported working setups and profile coverage

[한국어](CONFIRMED-SETUPS.ko.md) | [Compatibility](COMPATIBILITY.en.md)

Snapshot reviewed 2026-10-29: **105 open/closed issues and 73 comments**, excluding pull requests. These are contributor reports about historical builds, not fresh physical tests of current main or model-wide compatibility guarantees.

**A fixed profile and a LOCAL profile are different.** The six existing fixed profiles remain in the compatibility guide. LOCAL derives a private header from the owner’s captures and requires that owner’s host tests against the exact source, assets and fixtures. This registry is not a build input or permission to use another PC’s header.

Source revisions, measurements, published artifact hashes and every audited issue are recorded in the [machine-readable register](../evidence/compatibility-audit/setups.json) and [issue-by-issue audit](ISSUE-AUDIT.en.md). No firmware/EFI binaries or raw captures are included.

## Existing profile routes

| Evidence | GPU / PCI / subsystem | Board / BIOS / CPU | Reported route and outcome |
|---|---|---|---|
| [#4](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/4) | MSI RTX 3070 Ti 8GB · `10de:2482` / `1462:5052` | ASUS TUF GAMING B660M-E D4 / 4003 / i7-12700F | b660m-3070ti internal. Logged transaction, Windows/display return; repeated boots reported |
| [#12](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/12) | MSI RTX 3090 24GB · `10de:2204` / `1462:3881` | MSI B550M PRO-VDH WIFI / 2.O1 / Ryzen 7 5700X | LOCAL; no EDID driver; bus 2b. Logged clean transaction and Windows/display return |
| [#81](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/81) | ZOTAC RTX 3090 24GB · `10de:2204` / `19da:1613` | ASUS TUF GAMING B650-PLUS / 3287 / Ryzen 9 7950X3D | LOCAL-220410de-161319da-a95a2b89 USB. One full-shutdown transaction and Windows/display return; no tracked diff |
| [#84](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/84) | ASUS Turbo RTX 3090 24GB · `10de:2204` / `1043:87d5` | ASUS PRIME B550M-A WIFI II / 3621 / Ryzen 5 5600X | LOCAL-220410de-87d51043-f4c45666 USB. One logged cold transaction, Windows/display return; downloaded ZIP commit unknown |
| [#87](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/87) | ZOTAC RTX 3060 GA104 12GB · `10de:2487` / `19da:2630` | ASUS PRIME B760M-A WIFI / 1820 / i7-14700F | LOCAL USB; two-child aggregate. Latest comment supersedes initial probe refusal: three successful boots including warm restart |
| [#91](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/91) | EVGA RTX 3070 XC3 8GB · `10de:2484` / `3842:3755` | Gigabyte B550M DS3H / F21c / Ryzen 5 5600X | LOCAL-248410de-37553842-e177a27c USB. One logged cold USB transaction and Windows recovery; current tuple covered via #113 |
| [#105](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/105) | Gigabyte RTX 3060 Ti LHR 8GB · `10de:24c9` / `1458:40cd` | Gigabyte X870 EAGLE WIFI7 / F11 / Ryzen 9 9950X | LOCAL USB; exact 24c9/2000/60015 tuple. One clean restart-based transaction and Windows return; captures not all full shutdowns |
| [#113](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/113) | Zotac RTX 3070 8GB · `10de:2484` / `19da:2617` | Gigabyte B460M D2VX SI / FE / i5-10400F | LOCAL-248410de-261719da-b0dc28e6 USB. One logged cold transaction and Windows/display return |
| [#124](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/124) | Gainward RTX 3090 24GB · `10de:2204` / `10b0:2204` | Gigabyte Z790 GAMING X AX / F15 / i7-13700K | LOCAL-220410de-220410b0-e4482647 USB. One logged transaction/Windows return; one capture after reset, only one full shutdown |
| [#133](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/133) | EVGA RTX 3090 Ti FTW3 Ultra 24GB · `10de:2203` / `3842:4985` | MSI PRO Z690-P DDR4 / A.20 / i7-12700K | LOCAL-220310de-49853842-79d1fad6 USB. Two logged USB boots; one manual and one default USB-first; Windows/display return |
| [#136](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/136) | MSI RTX 3090 SUPRIM 24GB · `10de:2204` / `1462:3882` | MSI MAG B650 TOMAHAWK WIFI / 1.A0 / Ryzen 7 7800X3D | LOCAL-220410de-38821462-a1201d9a USB. One logged full-shutdown transaction and both displays return; no tracked diff |

## New LOCAL coverage and historical fork candidates

| Evidence | GPU / PCI / subsystem | Board / BIOS / CPU | Reported route and outcome |
|---|---|---|---|
| [#50](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/50) | RTX 3060 GA106 12GB · `10de:2504` / `10de:1536` | Gigabyte Z690 UD DDR4 / F27 / i5-12400F | historical z690ud-3060 fork; current LOCAL candidate. Two full-shutdown USB transactions and Windows return; do not reuse fixed fork header |
| [#73](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/73) | Gigabyte RTX 3080 12GB · `10de:220a` / `1458:40a2` | ASUS TUF GAMING B550-PLUS / 3611 / Ryzen 9 5900X | LOCAL tuple added by this PR. One logged USB transaction; Windows/HDMI return; DP required monitor power-cycle/replug |

## Successful private/research builds that still need integration

| Evidence | GPU / PCI / subsystem | Board / BIOS / CPU | Reported route and outcome |
|---|---|---|---|
| [#7](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/7) | ASUS RTX 3080 Ti 12GB · `10de:2208` / `1043:8803` | MSI MPG X570 GAMING PLUS / A.N0 / Ryzen 9 3900X | private e1adcc5; PR139. One USB transaction and Windows return; decode pre-step; public adapter not hardware-tested |
| [#28](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/28) | Maxsun RTX 3080 LHR 10GB · `10de:2216` / `7377:2000` | Maxsun MS-Terminator B760M D4 / not stated / i5-12490F | private b760m-3080lhr internal. One clean transaction and Windows return; extra exclusive owner/private EDID topology |
| [#32](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/32) | EVGA RTX 3090 24GB · `10de:2204` / `3842:3982` | ASUS ROG MAXIMUS Z790 DARK HERO / 2202 / not stated; UHD770 display | experimental separate-display UEFI. One logged transaction and Windows benchmarks; repeated boots untested |
| [#43](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/43) | RTX 3060 GA106 12GB · `10de:2504` / `10de:1536` | MSI MAG B550M MORTAR MS-7C91 / not stated / Ryzen 5 5600X | private GA106 manual/automatic SD route. Transaction signature and Windows return reported; source diff not published |
| [#86](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/86) | RTX 3060 Ti 8GB GA104-200 · `10de:2414` / `10de:2414` | Gigabyte Z590 VISION G / F8 / i7-11700KF | LOCAL-241410de-241410de-2036183f internal. Windows/display return and benchmarks; different b73000a1 chip/6000a driver; not current GA104 tuple |
| [#90](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/90) | Palit RTX 3090 24GB · `10de:2204` / `1569:f278` | Gigabyte B550M AORUS PRO-P / F15a / Ryzen 9 5900X | private LOCAL; SS1 preserved. Windows/display return reported; accepted captures after CSM disabled; local gate changes not reviewed here |
| [#93](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/93) | Dell OEM RTX 3090 24GB · `10de:2204` / `1028:3880` | MSI MAG B760M MORTAR WIFI II / 1.90 / i5-14600K | private separate Intel-display USB. One cold transaction and Windows return; Intel GOP/ownership changes need separate review |
| [#102](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/102) | Gigabyte RTX 3090 24GB · `10de:2204` / `1458:403a` | MSI MPG X570 GAMING PRO CARBON WIFI / 1.Q2 / Ryzen 9 3950X | private decode pre-step + LOCAL USB. One transaction/Windows boot from restart; nvgop6000a and decode correction need review |
| [#106](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/106) | Dell RTX 3090 24GB · `10de:2204` / `1028:3880` | Gigabyte X570 AORUS ULTRA / F30 / Ryzen 5 5600X | private parent-bridge BME wrapper. Clean transaction and Windows return reported; bridge scope not a generic LOCAL fix |
| [#121](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/121) | Reported MSI RTX 3090 24GB · `10de:2204` / `1458:4043` | MSI MAG X570S TORPEDO MAX / A.B3 / Ryzen 9 5950X | contributor decode pre-step + LOCAL USB. One clean transaction/Windows return; retain reported manufacturer/subsystem discrepancy |
| [#129](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/129) | Dell OEM RTX 3090 24GB · `10de:2204` / `1028:3880` | ASRock X570 Taichi / not stated / Ryzen 7 5800X3D | external decode pre-step + LOCAL USB. One cold transaction and Windows/display return; reflashed VBIOS disclosed |
| [#131](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/131) | ZOTAC RTX 3090 24GB · `10de:2204` / `19da:1613` | ASUS ROG CROSSHAIR VIII HERO WI-FI / 5601 / Ryzen 9 5950X | private x570-3090 one-shot USB. Operator Windows/NVIDIA/display recovery; source candidate and full transaction logs private |
| [#102](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/102#issuecomment-6504092640) | RTX 3090 24GB (reported subsystem 1b4c:1454) · `10de:2204` / `1b4c:1454` | MSI MPG X570 GAMING PLUS / not stated / Ryzen 9 5900X | private decode pre-step + generated LOCAL USB; PR138. One clean transaction and Windows recovery reported in a separate comment/setup |

## External or incomplete success reports

| Evidence | GPU / PCI / subsystem | Board / BIOS / CPU | Reported route and outcome |
|---|---|---|---|
| [#29](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/29) | MSI RTX 3090 24GB · `10de:2204` / `1462:3884` | Gigabyte model not stated / not stated / not stated | entryLatchedSs1Full internal. Logged transaction and Windows handoff reported; board/CPU/source incomplete |
| [#36](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/36) | RTX 3050 Laptop 4GB GA107M · `10de:25a2` / `1043:13fc` | ASUS TUF F15 FX506HCB / 316 / i5-11400H | independent yuksew-derived manual USB. One transaction/Windows boot; laptop decode/cleanup changes outside this implementation |
| [#99](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/99) | Palit RTX 3090 24GB · `10de:2204` / `1569:2204` | HP Z640 / M60 / not stated | modified legacy UEFI. Transaction/benchmark claim; partial patch relaxes aggregate checks; Windows handoff not independently established |
| [#140](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/140) | ASUS TUF RTX 3080 Ti 12GB · `10de:2208` / `1043:8803` | withheld / withheld / withheld | private manual USB; Secure Boot policy modified. Windows success and benchmarks reported; host/log/hash evidence incomplete; Secure Boot only likely on |
| [#2](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/2) | Inno3D RTX 4070 Ti · `10de:2782` / `10de:16e1` | ASRock Z690M-ITX/ax / 20.01 / not stated | Zihao-Wu06 Ada port. External port reports selectors/post-FLR, Windows return and daily use; not support in this repository |
| [#13](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/13) | PNY RTX 3080 LHR (comment, not original Gigabyte reporter) · `10de:2216` / `not stated` | not stated / not stated / not stated | unidentified private Windows port. A different commenter reports success with a screenshot; do not attribute it to the original X570 machine |

## Important boundaries and next actions

- #73 adds only `220a / b72000a1 / 3000 MiB` with the existing `60010` NVIDIA GOP code. It does not admit 10 GB/LHR 3080, 3080 Ti or other chip/driver combinations. [Integration evidence](../evidence/b550-3080-12gb-local/README.md).
- #125 reports a clean transaction followed by power-off before a usable Windows session. It is not a working Windows setup. Closed #35 was a read-only preflight with attempted=0, not an unlock.
- The PNY success commenter in #13 is not the original Gigabyte/X570 reporter. The #102 follow-up is also a different GPU/CPU/board and has its own record.
- #86 reports PCI `2414` and chip `b73000a1`; do not normalize these to `24c9 / b74000a1` from its marketing name. #121’s reported MSI branding and Gigabyte subsystem `1458:4043` remain an unresolved identity discrepancy.
- Retain #73’s manual DP recovery, #90’s small prefill/no decode improvement and #106’s lack of material Drama3 gain. Benchmarks use different protocols and are not cross-card comparisons or promises of application speedups.
- Decode pre-steps, parent-bridge BME changes, external Ada/laptop implementations and Secure Boot-policy changes are not silently imported into the standard flow.
- Regenerating a LOCAL header on current source invalidates old host-test evidence. Use the [current workflow](NEW-MACHINE.en.md); this audit is not a request for another boot.
- A custom `resolved` label remains unavailable to this READ-only account. Instead, [PR #142](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/142) uses `Resolves` keywords for #12, #73, #81, #84, #87, #91, #105, #113, #124, #133 and #136. They close automatically only when the PR is merged into default `main`; no issue has been closed yet. #4 is already closed. Private-policy, incomplete and failed-handoff reports remain open.
