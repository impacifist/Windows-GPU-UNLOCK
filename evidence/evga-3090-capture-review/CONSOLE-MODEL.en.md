# Console dependency model and service initialization

[English](CONSOLE-MODEL.en.md) | [한국어](CONSOLE-MODEL.ko.md)

## What changed

The CPU-only model in `research/profile/evga3090-console-model.py` expresses five selected ownership records and three ordered removals: text splitter, virtual-GOP text consumer (IMG16), graphics splitter. These names are symbolic roles, not authenticated source-module names. It refuses a missing/duplicate/unexpected record, nonunit open count, different order, callback failure, incomplete supplied post-call observations or an unchanged/unexpected post-call graph. Success never enables writes or claims unlock. This is an assumption model, not instruction emulation or a driver safety proof.

Both saved logs were parsed and their selected GOP/SimpleTextOut edges normalized to the model's symbolic roles; each matched the exact five-edge set. [Saved model results](console-model-results.json) record the input hashes and retain the unmodeled active EDID edge. Model removal synthesizes the expected edge changes; these are not captured post-Stop observations. No actual firmware call occurred.

Nine new regression cases passed; the complete Python suite passed 78 tests. Run from the repository root:

```powershell
python -m unittest discover -s research/profile -p 'test_evga3090_console_model.py'
```

## Service-table initialization evidence

Static decoding of the same loaded images found the following reads and stores. Standard UEFI x64 entry convention supplies the System Table in RDX and its BootServices member at offset 0x60 (vendored `deps/edk2/MdePkg/Include/Uefi/UefiSpec.h`).

| Image | Initialization evidence | Service global RVA used by Stop |
| --- | --- | --- |
| IMG13 | Entry RVA 0x350: instruction 0x364 reads [RDX+0x60], 0x36b stores to 0x6230. Additional init 0xb11 reads [RDX+0x60], 0xb15 stores to 0x6240. | 0x6240 |
| IMG16 | Entry 0x350 calls 0x37c and 0x444. At 0x386/0x38a, [RDX+0x60] is stored at 0x3810; 0x45e/0x462 initializes 0x3820 the same way. | 0x3820 |
| IMG17 | Entry 0x370 calls 0x424 with RDX unchanged; 0x42e reads [RDX+0x60] and 0x432 stores at 0x1e60. | 0x1e60 |

This strengthens the standard Boot Services interpretation of the Stop calls. IMG13 has two distinct service globals; the initialization path supplying the Stop global still needs complete reachability/call-argument review. Runtime overwrite targets, helper function semantics, indirect callbacks and object lifetimes have not been exhaustively qualified. The earlier conditional analysis is narrowed, not converted into hardware admission.

## Separate physical-GOP dependency

Both logs also record an EDID Discovered BY_DRIVER open, count 1, on the physical GOP by IMG07 (binding identity c7a7030c-c3d8-45ee-bed9-5d9e76762953). The selected five-edge model deliberately does not remove it, and reports it as remaining work. NVIDIA's parent/child ownership is also outside this model. No whole-graph quiescence is claimed.

The next step is to qualify the EDID and NVIDIA paths, helper/indirect targets and aggregate-console lifetimes before widening the model or proposing a new attended EFI boundary. The contributor does not need another capture or reboot merely to rerun analysis of these existing files. Existing profile admission remains refused.
