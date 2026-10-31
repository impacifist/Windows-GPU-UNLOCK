# Maintainer ownership and interface review

**English** | [한국어](MAINTAINER-WORKFLOW.ko.md)

This working agreement implements the agreed ownership split. Changes to shared
interfaces still need both maintainers' review; this document does not record that
review as already complete. Track implementation in the
[project board](https://github.com/users/impacifist/projects/5).

| Area | Lead | Current interfaces |
|---|---|---|
| Unlock sequence, hardware admission and profiles | @impacifist | `internal-boot/unlock/`, `research/profile/proven.json`, generated profile header/report and fixture binding |
| Driver release, recovery and Windows handoff | @impacifist | `internal-boot/boot/`, PCI state contracts, refusal/continuation boundaries |
| Setup, build orchestration and contributor tools | @DeclanFrampton | `research/profile/new-machine.ps1`, build/stage wrappers, `tools/` |
| Evidence tooling, host checks, packaging and EN/KO guides | @DeclanFrampton | capture parsers, test reports, manifests, documentation and release automation |

Directory ownership is not exclusive: a parser or build wrapper can enforce a core
invariant. Request cross-area review whenever a change affects the following contracts.

## Contracts to preserve

- **Capture versus unlock:** capture-only orchestration must stop before profile
  generation, firmware-asset replay or unlock packaging/staging. Generic probe
  compilation retains its diagnostic-only defines. Booting a probe is an attended
  hardware operation, not a host test or a promise of recovery.
- **Admission:** `proven.json`, profile constants, image hashes and topology checks
  remain the core owner's qualification boundary. Tooling must expose refusals,
  not translate them into warnings or invent an override.
- **Evidence:** retain raw captures. File completeness, parser acceptance, host
  replay, Windows return and measured unlock effects are different evidence levels.
  Never treat `capture-complete=1` as compatibility or unlock proof.
- **Schema/versioning:** inventory consumers before changing diagnostic keys,
  profile JSON, manifests or host-test evidence. Update producers, parsers,
  fixtures and EN/KO explanations together. Reject unknown/incompatible input
  explicitly; do not silently reinterpret old evidence. There is no new global
  schema version introduced by this agreement.
- **Build/stage:** prerequisite checks inspect host tools only. They do not install
  software, persist PATH changes, stage USB files or change boot settings. Staging
  remains an explicit operation with existing target checks and loader backups.
- **Release:** packaging verification does not qualify hardware support. The core
  owner reviews the exact hardware evidence and support scope before release.

## Delivery process

One lead owns each card and names dependencies before implementation. Start from
current main on an isolated branch; avoid moving shared files across active PRs.
Link existing issues and experiments rather than creating competing implementations.
Keep a small PR focused on one interface change, with checks actually run and
remaining unknowns. Put it in Review when the result is concrete; mark Done only
after acceptance criteria, required reviews and integration are complete.

The first tooling changes are a host-only prerequisite doctor and an explicit
capture-only path. They do not widen the proven hardware envelope or authorize a
new hardware experiment. Existing AD104 and EVGA research keeps its own evidence
and test approvals.
