# Reusable profile technical review — 2026-10-30

**Acceptance update:** [EVP11/EVP12 completed the exact-machine SharedEsp acceptance sequence](EVP12-RESULT.en.md). This supersedes the pending first-shared-package test below. Maintainer approval remains pending; Usb/SeparateEsp reusable packaging remains build/mock-only.

[한국어](REUSABLE-PROFILE-REVIEW.ko.md) · [Instructions](REUSABLE-PROFILE.en.md)

Implementation review is complete; maintainer approval and live validation of this new packaging remain pending. Earlier SULK02 evidence demonstrates the attended transaction on the captured machine, not this installer or every layout.

The three explicit profiles select SharedEsp, Usb or SeparateEsp. All retain the same exact-machine runtime pins and fresh Shift+G requirement. They do not enter the generic automatic/headless profile catalogue. SeparateEsp requires an existing internal FAT32 ESP distinct from Windows; the controller never creates or formats one.

Reviewed boundaries include immutable per-attempt journals, package inventory and PE verification, exact volume identities, fresh contiguous slots, previous evidence hashes, one-shot receipts, USB loader restoration and exact-owned firmware cleanup. BootNext is selected last; BootOrder is never written. A partial failure preserves state and refuses blind rearm. Recovery is not guaranteed.

## Checks actually executed

- Six EFI packages compiled: SharedEsp slots 10/11, Usb 8/9, SeparateEsp 8/9. PE checks confirm AMD64 EFI applications with no imports, IAT or delay imports, and NX/dynamic-base flags.
- All three routes passed host media admission/rejection and six receipt gate cases.
- Actual controller Install/WhatIf, incomplete refusal, simulated transaction, Collect/cleanup, Rearm and Cancel paths passed with fake storage/firmware in PowerShell 7 and Windows PowerShell 5.1.
- Sixteen malformed-package, identity, previous-evidence tampering, firmware-race and cleanup-policy checks passed in both shells.
- Legacy second shared receipt tests passed six cases, preparation eight cases, firmware selection/cleanup 32 cases.

These checks do not invoke native firmware APIs or access a GPU. They establish software behavior against mocks, not hardware success. Build artifacts are local under `research/profile/build/reusable-v4`; package manifests record source commit, dirty state, input hashes and payload inventory. Assets are not redistributed in the ZIP.

## Maintainer review focus

Review media admission, package provenance, installation/cleanup refusal semantics and legacy marker coexistence before merge. The first new hardware acceptance test is SharedEsp slot 10: install, USB-absent attended boot, Windows return, collection and exact-owned cleanup. A later successful-run rearm is separate live proof. Dedicated separate-ESP behavior remains build/mock-only. Secure Boot signing/enrollment, unattended/permanent unlock and other machines are outside this qualification.
