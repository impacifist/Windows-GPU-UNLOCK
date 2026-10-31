# EVGA06 result and next combined graphics-consumer release

[English](NEXT-GRAPHICS-RELEASE.en.md) | [한국어](NEXT-GRAPHICS-RELEASE.ko.md)

EVGA06 contains successful IMG13 and IMG16 Stop returns, their postchecks, two complete captures and ready-windows. Movian reports the same automatic Windows behavior as EVGA05. Full recorded-graph comparison finds no handle additions/removals; the virtual GOP loses its IMG16 active GOP open and text protocols 387477c2-69c7-11d2-8e39-00a0c969723b / e1e4a857-c970-4075-a4da-e9c41b69adfc. The IMG13-associated d3b36f2c GUID also disappears, as before. The removed text protocol contains the prior IMG13 opens. Other recorded active opens remain. Both GOPs survive. [Sanitized result](seventh-efi-run-result.json). No unlock occurred.

## Next test, built but not hardware run

The new mode repeats both proven Stops and their checks, then calls the exact pinned IMG17 Stop(binding, physicalGop, 1, &virtualGop) once. It requires the two physical-GOP combo opens absent afterward while retaining the two GOP handles/shape, absent IMG16 text interfaces/open, system console text and driver code pins. Final verification runs again before preloaded Windows. Unexpected state or callback failure after a Stop holds. No retry, parent Stop or reset is appended.

IMG17 one-child path checks its stored child and controller list, closes two GOP references, removes/frees a member node and rebuilds a mode list through helper0x594. Its last-member helper0x156c allocates a display-sized buffer and calls an interface at offset0x10, then updates aggregate mode fields. The indirect target's complete behavior is not qualified; this is not proof of GPU inactivity or harmlessness. A successful Stop alone is insufficient; the harness checks fresh relationships. It does not call the zero-child IMG17 branch. EDID and NVIDIA driver ownership are retained; their removal remains later work.

**New risk:** this reaches a new OEM last-member/helper path with allocation and an indirect graphics-interface call. It may change graphics state, lose display/input, hang firmware/Windows or prevent GPU initialization. Manual power-off/USB removal may be needed. Recovery is unproven; there is no automatic restoration. No direct GPU MMIO/PCI attribute writes, GPU reset, firmware upload/execution or unlock is added by the harness, but callbacks may have indirect hardware effects. Read the risk and choose whether to run; no separate acknowledgement is required.

Update through GitHub Desktop, then from the repo root:

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -ReleaseGraphics
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

The build automatically includes the two earlier stages. A separate EVGA-GFX-RELEASE.ARMED marker prevents retry; keep both older markers. Staging requires the exact reviewed EVGA06 hash, validates mode/source/EFI/ESP and preserves captures and numbered backups. Stop on an error; do not bypass or delete markers.

Full shutdown → one-time UEFI USB boot → fresh Shift+G once after Live guards passed. No Shift+V. After G the display is not a progress indicator. If Windows has not started within three minutes, power off, remove USB and use normal Windows Boot Manager. Preserve the marker and do not repeat. Send EVGA07 or the next EVGAnn log, build/stage output and automatic/manual Windows return. Offline verification will be done here.

Local validation: 25 original membership + 7 two-Stop + 8 three-Stop host checkpoints and 9 capture regressions passed. Third-stage cases cover success/final no-log verification, third Stop failure, missing edge removal, earlier Stop failures blocking continuation, marker and code-hash rejection. Warnings-as-errors EFI build/PE verification passed: 22528 bytes, SHA2564f7b17c9a1c4804aaa9e74f10c0fb61af8fbe15decaac1df2cbb737a556fb477, AMD64/no imports/NX/dynamic base. These are mocks/build checks, not hardware validation; contributor toolchain output may differ.
