# Shared ESP: read-only path check

[English](SHARED-ESP-PATHS.en.md) | [한국어](SHARED-ESP-PATHS.ko.md)

Before any shared-ESP implementation, inspect only the two reported boot files and two possible project directories on the existing system volume. The script asks Windows for the system volume's existing path, verifies it is FAT32, and checks file metadata/hashes and directory existence. It does not assign a drive letter, mount a partition, enumerate unrelated files, write to the ESP, change boot configuration or reboot.

After updating the branch, run from the repository root in PowerShell:

```powershell
powershell -NoProfile -File ./research/profile/inspect-shared-esp-files.ps1
```

Access may require an administrator PowerShell. Stop and send the error if the volume path is inaccessible. Review the JSON before sharing: it contains fixed EFI paths, file sizes and hashes. No volume GUID or file contents are printed. Missing paths or existing project directories are observations, not permission to overwrite them. The script refuses unexpected object types and reparse points at the final path; it does not validate every ancestor or prove future writes or firmware boot are safe.

Fixture tests covered present and missing files/directories and hashes without touching an ESP. The research machine's system volume was not reported as FAT32, so its live run refused as designed; actual access on Movian's machine remains untested.

Movian subsequently reported successful read-only access: both Windows Boot Manager and Limine files existed, while neither proposed project directory existed at the time. Their reported SHA-256 values were `2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec` and `a11eea5e0235427a14f12035a1960c2659e0f0fd963092025719305ef542a462`, respectively. This is contributor-supplied Windows output, not verification of firmware loading, ancestor paths, or write safety.

An offline graph inspection of the previously supplied EVGA19–EVGA23 captures found exactly one handle containing the pinned system partition's GPT device-path node and both Device Path and Simple File System protocol records in each of their two complete captures. The inspector passed synthetic target, wrong GUID, missing protocol and incomplete-capture cases. This replays recorded USB-boot graphs; it does not prove that an EFI image can load from that volume or that a future firmware boot will have identical topology.
