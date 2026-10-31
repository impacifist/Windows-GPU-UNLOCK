# Next test: saved console-driver code fingerprints

[English](NEXT-CODE-TEST.en.md) | [한국어](NEXT-CODE-TEST.ko.md)

After the nine console-chain checks passed on both captures, this test follows the actual BY_DRIVER owners to their saved image dumps. It validates AMD64 PE structure, bounds and the Stop executable section, rejects relocations within that section, then compares section hashes, Stop RVAs, versions and dump names across captures. Repeated firmware identities alone are not sufficient: multiple bindings can share an identity, so ownership selects the instance.

On the updated PR96 branch, run from the repository root:

```powershell
python -I research/profile/inspect-evga3090-console-code.py "D:\RTX3090-UNLOCK"
```

Expected: console_code_matches_across_captures=true; hardware_accessed=false, write_eligible=false and unlock_proven=false. Send the complete JSON output. Missing or malformed logs/images, ambiguous ownership, relocations inside the selected section or differing fingerprints fail the test. Preserve files on failure; do not force a profile.

Both uploaded captures passed locally. Graphics-console IMG17 has Stop RVA 0x88c and 6544 hashed code bytes; text-console IMG13 has Stop RVA 0x104c and 22048 code bytes. Exact hashes are in [saved results](console-code-test-results.json). This establishes reproducible saved executable code, not authenticated provenance or safe Stop semantics. Both captures share the same saved IMG files: agreement does not independently prove the code was unchanged between boots. This test cannot qualify a hardware lifecycle or authorize unlock.

No reboot, new EFI, BIOS changes, firmware execution or GPU access is needed. The script reads saved files and prints JSON. The topology admission refusal remains unchanged. Next work is static analysis of these exact console implementations and an offline lifecycle model before proposing any new attended hardware test.
