# EVP11 completed transaction; cleanup pending

## Same-session benchmark and next cleanup

The contributor confirmed Shift+Shut down, USB absent throughout and no intervening Windows reboot. Benchmark ZIP SHA256 `b31e920052a7cc8f5ac291984a2d45d0b7be45d1397ebca6f54d142b388b819f` has matching completion hashes for context and all three trials. Protocol 2, PCI 0000:01:00.0 and per-mode reference_checked=true are present in each trial. Recomputed medians: FP32 26.504291, FP16 123.135526, BF16 122.758938, TF32 64.172952 TFLOPS. FP32 ranged 16.566–26.512144 across trials; report that variability rather than implying uniform performance. These GEMM checks do not establish all-workload correctness or long-term stability.

Inspection shows Boot0000/0008/000C unchanged and Boot000D absent from the current order. Its raw-variable absence versus retained unchanged value is checked live before cleanup; the inspection alone did not distinguish them. The exact reviewed old USB-entry digest may be absent from order, but a changed present value still refuses. No USB entry is deleted or recreated.

Run `./research/profile/manage-evga-profile.ps1 -Action Cleanup` in administrator PowerShell. It verifies the successful evidence, checks the live firmware snapshot, saves a backup and deletes only the exact owned temporary option. It preserves the original result JSON and writes a separate hash-bound cleanup receipt. BootOrder is never written; no GPU access or reboot. On any error stop and report it. Do not rerun Collect, delete state or rearm yet. Send the JSON. Firmware deletion can fail; no automatic rollback/retry is promised.

The separate Cleanup action and immutable-receipt rearm recognition pass mocked integration tests in PS7/5.1. Pure policy tests cover the exact reviewed USB identity and reject changed values, names, attributes or hashes. Other hardware compatibility and unattended/permanent operation remain unproven.

[한국어](EVP11-RESULT.ko.md)

The contributor's manually packaged results contain the expected four files. EVP11 log SHA256 `386a2b17d30aa2eb18e36fd8d742e84518be69e2093214a863f40c3191b5f768`, receipt `00bf38e9f2d2e2fe5a3b83293d7c4d57ce741d118366628f2eea988efe8bf58a` and runtime marker `c81ed25e0ed465df4a3813ac40781f6d0fb46f06fe899d4ed4fb6b4666c70aca` match the reported collection. Package EFI hash is `c4f3360abeab91c1345d5fe35e9d2992b1dec87a99a78357dddb6b29604e2609`.

Entry admission passed with SS1=7. Pre-FLR selectors were SS0=88888888 and SS1=8. Transaction result=0, contained=0, started=1, quiesced=1, fault=0, retained=0, maps=0 and FLR=1. Successful transaction-return, close-return, asset release, Windows-ready and two captures are recorded. The operator reports Windows loaded after Shift+G and reported removing USB beforehand. Full shutdown versus restart must still be explicitly confirmed; one success does not establish causation or general reliability.

Collection reports Success=true but CleanupCompleted=false because an existing boot entry changed. Do not weaken cleanup or rearm until the current entry comparison is reviewed. The operator reported no automatically visible ZIP and manually archived the result directory; why the printed ZIP was not visible is not established.

Next, without rebooting, administrator PowerShell at the repository root:

```powershell
./research/profile/check-evp11-session.ps1
```

This checks the exact successful log/receipt/marker, preserves or reconstructs the collected ZIP if missing, reads the current firmware entry comparison and runs the existing hash-pinned GPU benchmark labelled EVP11. It prints inspection and benchmark ZIP paths; send both. No firmware writes, cleanup, rearm or reboot occur. The benchmark does use the GPU and can fail like the existing benchmark; preserve partial output rather than retry automatically. Report any intervening reboot, full-shutdown method and USB absence. If the script fails, send the error and any ZIP already produced.

The combined workflow passes with fake firmware/storage and a synthetic benchmark in PowerShell 7 and Windows PowerShell 5.1. These checks do not substitute for the live benchmark or cleanup result.
