# AD104 release contract: host model only

English | [한국어](AD104-RELEASE-CONTRACT.ko.md)

The [fifth capture](AD104-FIFTH-RESULT.en.md) establishes current attributes
`0x718`, supported attributes `0x1b77b` and command `0007`. Static Stop analysis
predicts a possible Disable mask of `0x718`. Neither successful driver release
nor its postconditions have been observed on this host.

`lifecycle-model.py` defines an exploratory host-only contract. It has no EFI
adapter, is not linked into an EFI build and grants no runtime authority. Its
configuration and four named graph roles are synthetic fixtures, not an OEM
profile or complete interpretation of the captured protocol graph.

## Proposed sequence to evaluate

| Step | Synthetic expected state | Failure response |
| --- | --- | --- |
| Initial observation | exact device/BDF and owner fingerprint, command 7, attributes `0x718`, expected graph, keyboard and pinned boot image | zero mutation attempts |
| Text consumer callback | only its expected abstract edge removed; configuration/attributes unchanged | stop, no retry/handoff |
| Graphics consumer callback | only its expected abstract edge removed; configuration/attributes unchanged | stop, no retry/handoff |
| PCI-owner callback | expected owner/child edges absent; predicted command 0 and attributes 0; remaining config unchanged | stop, no retry/handoff |
| Limited decode enable | only original I/O and memory enable mask `0x300`; predicted command 3 and attributes `0x300` | stop, no retry/handoff |

Command 0 / attributes 0 after the owner callback are deliberately strict
**predictions**, not established hardware behavior. Exact checks may reject
benign changes; refusal is not evidence of damage. A callback error may follow
partial mutation and must never be treated as an unchanged machine. The attempt
is marked before each callback. There is no automatic rollback or retry.

Each successful mock callback requires a new observation: fresh generation,
full 256-byte configuration, BAR descriptor, attributes, supported mask, BDF, graph, keyboard,
pinned boot image and captured fingerprint provenance. Extra edges, partial
closure or lost input refuse advancement. These comparisons are not an atomic
firmware snapshot and their host implementation is not a live graph verifier.

The decode step is **not restoration of the original state**. It must not enable
BME (`0x400`) or VGA (`0x18`), rewrite BARs, reset the GPU or execute firmware.
It does not restore console ownership or display functionality. The existing
GA102 decode policy is unchanged and is not silently reused for BME-on AD104.

## Verification and remaining gates

Run `python -m unittest discover -s research/profile -p 'test_*.py'`.
Twenty-five Python tests pass. The new model tests inject failures at each
callback and reject successful callbacks followed by stale observations, partial
or unexpected edges, lost keyboard, changed boot image, altered configuration,
changed BAR descriptors/BDF/support masks, and unexpected BME/VGA state. Extra
initial command bits are refused. Callback exceptions and capture exceptions
after an attempt return an unknown-state result and never advance; an initial
capture exception causes zero attempts. An error in the decode
callback stops just like an error in a release callback. Even the fully matching
synthetic sequence returns Windows, unlock and runtime authorization as false.

Before a state-changing EFI can be prepared, remaining work includes:

- A complete capture-specific graph model, including the null-image binding
  whose Stop lies inside IMG29, shared consumers and legacy-service dependencies.
- Validated ownership of each target callback and a narrowly fingerprinted
  runtime preflight, rather than a renamed native-NVIDIA release profile.
- A defined post-release GPU cold-state check. A successful decode transition
  alone cannot establish WPR2 state or unlock admission.
- An attended failure path for partial Stop, lost display/input and failed decode
  enable, plus separately tested Windows-loader eligibility after release.

No Stop, DisconnectController, attribute-changing operation, GPU MMIO, unlock,
reboot or USB replacement was performed for this model. The USB still contains
the fifth query diagnostic; repeating it does not test this contract. No new
EFI boot is required for the remaining offline work.
