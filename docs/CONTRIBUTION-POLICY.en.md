# Contribution and review policy

[한국어](CONTRIBUTION-POLICY.ko.md)

The project's development priority is reusable unlock/diagnostic code and tools
for creating and validating local profiles. Compatibility reports are welcome;
they do not commit maintainers to porting, testing or supporting every machine.

## What belongs in main

| Contribution | Default decision |
|---|---|
| Reproducible bug or safety fix | Review with focused regression coverage |
| Shared diagnostic, profile-generation, validation or recovery improvement | Review the reusable change and its failure paths |
| New topology or architecture support | Review a bounded shared implementation, evidence and representative fixtures |
| Project documentation or benchmark improvement | Review relevance, reproducibility and maintenance cost; reuse existing tools |
| Constants or a hard-coded branch for one more board/BIOS/card | Record the report; use a local profile where the reviewed workflow permits it |
| Successful run with no reusable code change | Keep a sanitized compatibility report; no code merge is needed |
| Duplicated implementation, broad operational-tool bundle or large raw log collection | Request a focused split, reduce the scope, or decline |
| Removed/relaxed checks solely to make a configuration pass | Hold pending a justified shared design and negative tests |

One tested machine can reveal a reusable defect or capability. Judge that
change by its shared value and evidence; do not require access to several
machines merely to propose a fix. Separate the fix from machine-specific data.

## Machine-specific data and exceptions

Do not grow the production profile list for every motherboard, BIOS or VBIOS.
Keep generated partition settings, profile headers and raw captures local.
Share a small sanitized issue summary with source/build identity, relevant
observations, test conditions and limitations. A short, reviewed evidence
summary may accompany a reusable change when needed to explain or reproduce it.

The existing [local-profile workflow](NEW-MACHINE.en.md) has a restricted,
[reviewed envelope](../research/profile/proven.json). It is not a universal
installer. Outside that envelope, submit research inputs; do not bypass checks,
invent constants or claim that a profile can already be generated safely.

Existing fixed profiles remain reference configurations. A new fixed reference
profile is an exception requiring an explicit maintainer decision explaining
why it is needed for a reusable capability or reference test and why the current
local-profile format is insufficient. Include the maintenance implications.
One owner's successful boot alone does not justify the exception.

## Review before execution or merge

1. Identify the reusable problem and the smallest contribution that solves it.
   Split independent research notes, benchmark tools, diagnostics and hardware
   control. Declare prerequisite PRs and review the base first.
2. Read the complete diff, dependencies and execution paths before running
   contributor code. Inspect downloads, external communication, generated files,
   privileged operations, binaries, attribution and licenses. Tests and hashes
   supplied by an author are inputs to review, not proof of authenticity.
3. Preserve exact identity, firmware/code fingerprints, ownership, MMIO/PCI,
   DMA and cleanup guards. Generalization needs explicit boundaries and negative
   tests, rather than board-specific bypasses or a blanket acceptance flag.
4. Run checks appropriate to the change and document what was not run. Host
   simulations and synthetic builds do not establish hardware safety or support.
   Keep diagnostic collection, transaction, Windows recovery and performance
   evidence separate using the [validation record](VALIDATION.en.md).
5. Give a concrete verdict: **merge**, **split**, **needs evidence**, **hold/decline**,
   or **record only**. Explain the reusable benefit, evidence, remaining risk and
   maintenance cost. A merge recommendation is not permission to execute an EFI.

Review/implementation does not authorize posting comments, closing issues,
marking another contributor's Draft ready, or merging a PR. Agents obtain the
project owner's explicit approval for those actions. A general request to
continue work does not replace a requested publication/merge approval.

## Community and maintenance

GitHub issues/PRs hold substantial results, reproducible reports, review and
decisions. Discord is for conversation, early findings and coordination; useful
outcomes should be summarized on GitHub. Maintainers are not expected to extract
every technical decision from chat or provide continuous device-specific support.

Triage regressions and unsafe existing behavior first, then small shared fixes
and prerequisite changes, then bounded new capabilities and research. Review a
manageable queue instead of promising acceptance in arrival order. Inventory-only
reports may remain recorded without an implementation commitment. Apply these
criteria to open PRs individually; do not bulk-close contributions because the
policy changed, and do not request a new hardware run merely to satisfy triage.
