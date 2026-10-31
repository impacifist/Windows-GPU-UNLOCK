# Shared ESP: read-only boot inspection

[English](SHARED-ESP-INSPECTION.en.md) | [한국어](SHARED-ESP-INSPECTION.ko.md)

Movian prefers a separate entry on his existing Windows/CachyOS ESP. The current EVGA EFI accepts USB media and directly starts the pinned Windows Boot Manager after the transaction. It therefore bypasses Linux selection for that invocation; that does not by itself delete or change the existing dual-boot configuration. Existing internal code rejects using the same partition for the unlock image and Windows. A shared-ESP EVGA profile is not implemented or tested.

Movian clarified that unlock is required for Windows only; normal CachyOS boot should remain available. A Windows-only unlock entry can therefore hand directly to Windows, provided existing Linux boot files and ordinary boot selection are preserved. No Linux unlock or post-unlock OS menu is requested.

The next inspection queries the existing system volume's filesystem and capacity/free bytes, and enumerates firmware boot entries via `bcdedit /enum firmware`. It does not mount the ESP, list or write its files, modify firmware entries, change boot order, or reboot. It may need an administrator PowerShell to read firmware entries; if refused, send the error. Stop on any error.

After updating the branch, run from the repository root:

```powershell
powershell -NoProfile -File ./research/profile/inspect-shared-esp.ps1
```

Review the output before sharing: firmware entries can contain identifiers, device details and boot paths. Redact anything private, retaining entry descriptions and EFI paths if comfortable. These are leads to the current boot topology; firmware enumeration does not prove loader behavior, safe shared writes or recovery. Do not resize partitions or install a launcher based on this output.

Host mocks passed success and enumeration-failure scenarios. A read-only attempt on the research machine did not succeed, so the command has not been verified on Movian's machine.

Movian subsequently supplied successful output: the system volume reports FAT32, 11,585,944,576 bytes total and 10,425,717,760 bytes free. Firmware enumeration lists a Limine entry first and a Windows Boot Manager entry, both pointing to the same reported volume, plus another Windows entry and a USB entry. The displayed paths are `\EFI\limine\limine_x64.efi` and `\EFI\Microsoft\Boot\bootmgfw.efi`. These are contributor-reported Windows views; they do not independently prove the files' contents, which boot path was actually used, or safe shared-ESP writes. The Windows-only unlock design must retain existing entries and normal CachyOS boot.
