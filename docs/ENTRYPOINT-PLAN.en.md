# Repository entry points and cleanup proposal

**English** | [한국어](ENTRYPOINT-PLAN.ko.md)

Inventory against main commit `1f8776e` (2026-10-30). This is a proposal for the Kanban card **Inventory repository entry points and plan cleanup**, assigned to DeclanFrampton. It does not move, remove or deprecate any command. Maintainer agreement is required before applying a cleanup map.

## Keep the public boundaries explicit

| Entry point | Current purpose and side effects | Proposed disposition |
|---|---|---|
| [README](../README.md), [compatibility](COMPATIBILITY.en.md) | Choose an exact supported route; not an execution command | Keep as navigation; link one canonical guide per route |
| [collect_compatibility.py](../tools/collect_compatibility.py) | Windows inventory; prints reviewed data, no upload or unlock | Keep as inventory-only entry |
| [check-compatibility.py](../research/profile/check-compatibility.py) | Offline assessment of existing captures; no probe execution or profile generation | Keep as offline checker; do not merge with a staging command |
| [new-machine.ps1](../research/profile/new-machine.ps1) | Resumable probe/profile/test/package workflow; can stage USB files; unlock staging requires `-StageUnlock` | Keep existing command while reviewing PR112; do not advertise it as only an inventory tool |
| [build-probe.ps1](../research/profile/build-probe.ps1) | Builds generic diagnostic EFI files locally; booting that EFI is a separate hardware operation | Keep low-level diagnostic builder; frontends should call it rather than duplicate it |
| [Configure.ps1](../internal-boot/Configure.ps1), [Build.ps1](../internal-boot/Build.ps1) | Configure exact internal profiles and build manual/automatic EFI pairs | Keep separate configuration and build boundaries |
| [Headless.ps1](../internal-boot/Headless.ps1), [Install.ps1](../internal-boot/Install.ps1) | Existing dedicated-ESP installation/registration and automatic-boot policy actions; may change boot configuration/settings | Keep explicit installation commands; never invoke from inventory or offline review |
| [prepare_usb.py](../tools/prepare_usb.py) | Constructs a new local package tree from manifests/assets; does not format or write a selected USB | Keep package creation distinct from physical staging |
| [unlock/build.ps1](../unlock/build.ps1) | Legacy USB build with its own profile/experimental options | Retain alongside the [legacy guide](GUIDE.en.md); do not treat it as interchangeable with internal headless or LOCAL builds |
| [benchmark/run-benchmark.ps1](../benchmark/run-benchmark.ps1) | Runs compute measurements; independent of proof of pre-boot transaction | Keep benchmark setup/build/run/comparison separate |
| [research/headless](../research/headless/README.ko.md), [research/rtx3080](../research/rtx3080/README.md) | Experimental route-specific work | Keep under research; coordinate any shared helper extraction with active PRs |

## Similar names do not establish duplication

The legacy `unlock` and `internal-boot/unlock` trees use different wrappers and boot contracts. Their builders and PE verifiers must not be deleted or merged based on filenames. First compare accepted defines, profile selection, source manifests, output layout and failure behavior; preserve tests for both routes.

Likewise, capture build manifests, machine-local profile evidence and the top-level [SOURCE-MANIFEST.json](../SOURCE-MANIFEST.json) have different scopes. The top-level manifest describes a historical public v0.3.0 inventory; it is not an automatically refreshed current-tree manifest. Do not silently regenerate it as part of unrelated documentation or staging work.

Generated `build` directories, assets, captured BIN files, `research/profile/local`, private profile headers and internal installation state are excluded by [.gitignore](../.gitignore). Do not move them into published documentation or replace exclusions during cleanup. A clean checkout does not contain the user's private assets or machine-specific generated profile.

## Proposed sequence for review

1. Agree navigation and command ownership before changing paths. Keep existing commands working throughout any later migration.
2. Reconcile [PR112](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/112), which owns the contributor doctor/capture launcher. Its branch has additional local acceptance work; do not copy that frontend into another PR or describe it as merged main behavior.
3. Implement a small, route-specific change only after agreement. If a public command is eventually replaced, first add a documented compatibility wrapper with a bounded migration period; no deprecation is approved here.
4. Add checks appropriate to that change: documented command resolution, parameter compatibility, output/manifest contracts and refusal paths. Host mocks do not establish USB selection, EFI execution or Windows recovery.

No `.github/workflows` directory exists at this inventory revision. The CI card needs a separate inventory of local test prerequisites and private-asset dependencies before introducing runners; absence of hosted workflow files does not mean host tests are absent.

## Acceptance still outstanding

This PR supplies the map for review. Keep the card in Review until the maintainers agree the keep/deprecate/move decisions. No file movement, runtime consolidation, physical boot, new support qualification or beginner walkthrough is completed by this inventory.
