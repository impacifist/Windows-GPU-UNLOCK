# Prepare EVGA firmware assets on Windows

[English](NEXT-ASSETS.en.md) | [한국어](NEXT-ASSETS.ko.md)

The hardware state/PROM capture is complete. Prepare the five local files needed for the next runtime integration without another EFI boot. Update the branch through GitHub Desktop and run from the repository root:

```powershell
py -3.14 research/profile/prepare-evga3090-assets.py
```

Requires Git and Python3.14+, already used on the contributor machine. Downloads the pinned NVIDIA source and nvidia-utils package, verifies source hashes, generates/extracts the five outputs and verifies exact repository sizes/hashes. Uses pinned upstream revision0ebdb1c6308b6a2072be5bf04513472942541956. Windows native Python tar/zstd handling replaces only the generator's external zstd subprocess; pinned member validation, input/output checks and payload construction stay in use. No install/module loading, GPU access, EFI staging, USB/boot changes or reboot. Network downloads can be large and need disk space.

Default source/cache/output are under ignored research/profile/build/evga-assets/. A nonempty output is refused rather than replaced; preserve partial output and send the error if preparation fails. Existing output can be checked with --verify-only. Keep NVIDIA-LICENSE/NOTICE.md and review them before use. Send only console JSON/errors or verified-assets.json, never the five firmware files. No benchmark rebuild is needed.

Local execution completed generation of all five files and exact manifest verification. Separate verify-only passed; nonempty output and corrupt-asset rejection passed. This establishes local asset provenance/integrity, not GPU firmware acceptance or unlock. The contributor still needs locally prepared files; these are not redistributed through the PR.

The original generator first failed because external zstd was absent. The native adapter completed successfully on Python3.14; no upstream source was modified. Source: [pinned upstream firmware instructions](https://github.com/Zagitta/gpu-unlock/tree/0ebdb1c6308b6a2072be5bf04513472942541956/firmware). Runtime integration still must pin this exact EVGA identity/topology and entry selectors and preserve failure containment before an unlock EFI is supplied.
