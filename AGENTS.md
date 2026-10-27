# Repository working instructions

Read [the contribution policy](docs/CONTRIBUTION-POLICY.en.md) before reviewing
or implementing contributions. [한국어](docs/CONTRIBUTION-POLICY.ko.md).
Explicit current user instructions take precedence.

1. Prioritize shared unlock/diagnostic code and local-profile generation and
   validation. Do not implement every compatibility report as a new fixed profile.
   Board/BIOS/VBIOS constants alone are normally report/local-profile data.
2. Preserve existing reference profiles. New fixed profiles require the policy's
   documented maintainer exception; do not silently remove existing profiles or
   widen the reviewed generation envelope. Do not describe local generation as
   universal support.
3. Start with `git status --short`, preserve unrelated user work and artifacts,
   and use an isolated branch/worktree when needed. Inspect the complete diff,
   scripts/imports, effects, provenance and licenses before running contributed code.
4. Separate reusable fixes, hardware-specific evidence, research and operational
   tools. Read source and primary definitions rather than accepting a PR title,
   screenshot, model name or passing author-supplied test as proof.
5. Keep identity, firmware/code, ownership, MMIO/PCI, DMA and cleanup checks
   fail-closed. Generalization needs explicit boundaries and meaningful failure
   tests. Never weaken a check or expected result merely to make a new card pass.
6. Use host checks appropriate to the change. Documentation needs link/path/diff
   checks, not a hardware workload. Code review/changes do not authorize EFI boot,
   GPU reset, benchmark execution, existing-workload interruption, ESP/NVRAM or
   firmware changes. Obtain an explicit current request for hardware operations.
7. Distinguish inventory, host/synthetic tests, observed transactions, Windows
   recovery and matched performance evidence. Keep private captures, firmware,
   credentials and generated machine-specific artifacts out of Git.
8. Before merge, report one verdict: merge, split, needs evidence, hold/decline,
   or record only, with actual checks, limits and maintenance implications.
   Do not post comments, close issues, mark another contributor's Draft ready or
   merge without the project owner's explicit approval. Preserve a request to
   withhold publication until it is explicitly revoked.

Do not bulk-close existing contributions or start new hardware tests to implement
this policy. Important research/review decisions belong on GitHub; Discord is for
conversation and early findings. Maintainers do not promise a port for every report.
