# Active Windows display routing

**Observed result: Windows reports one active, available desktop display path
on the inventoried RTX 3080 through external DisplayPort.** The path's source
and target resolve to the same adapter.

The query ran on 2026-10-26 from 14:46:54.873Z to 14:46:55.312Z, completed in
approximately 0.437 seconds and exited with code 0 under a 10-second external
deadline. It did not create a D3D device, submit a graphics/compute workload or
change the display configuration.

## Recorded observation

| Field | Result |
|---|---|
| Active display paths returned | 1 |
| Distinct target adapters among those paths | 1 |
| Path active / target available | true / true |
| Windows output technology | 10: external DisplayPort |
| Source and target on the same adapter | true |
| Source adapter IDs | `10de:2216` / `1458:403f` |
| Target adapter IDs | `10de:2216` / `1458:403f` |
| Source and target marked as software adapters | false |
| Inventory GPU IDs matched | true |
| Query attempts | 1 of at most 3 |

[observation.json](observation.json) contains the result, timing, compiler
options and source/executable hashes. It embeds the parsed native JSON without
semantic changes; formatting is normalized for readability. The original
stdout hash is retained. The executable itself is excluded from publication.

This supplies a direct Windows path-to-adapter association, rather than
inferring the connection from a separate list of installed controllers.
An earlier CIM query reported one active monitor connection with technology
10, but [WmiMonitorConnectionParams](https://learn.microsoft.com/en-us/windows/win32/wmicoreprov/wmimonitorconnectionparams)
has no adapter relationship.

## Method and limits

The [collector source](collect_display_paths.cpp) uses the documented
[QueryDisplayConfig](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-querydisplayconfig)
active-path query with virtual-mode awareness. It passes the path's target
adapter identifier to
[IDXGIFactory4::EnumAdapterByLuid](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgifactory4-enumadapterbyluid)
and reads the typed hardware fields from
[DXGI_ADAPTER_DESC1](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_adapter_desc1).
Source and target adapters are resolved separately when their identifiers differ.

The helper keeps adapter LUIDs and descriptions in memory only. Output is
restricted to numeric hardware IDs, counts, booleans and fixed labels. No
monitor names, serials, EDID, device paths, LUID values, hostnames or usernames
are serialized. Device-interface strings are not parsed.

The query permits at most 64 paths, 256 mode records and three sizing/query
attempts. These are application limits, not Windows topology limits. A
configuration change that exhausts the retries or an API failure produces a
fixed stage label and numeric status. Access failures are reported as failures.
The required mode array is allocated, but mode-index unions are not interpreted.

Microsoft identifies
[output technology 10 as external DisplayPort](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ne-wingdi-displayconfig_video_output_technology).
Its [display-target documentation](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_path_target_info)
also distinguishes display targets from physical connectors. Accordingly:

- The observation is a snapshot of Windows' active desktop routing.
- Active and available are separate observations; an active path can briefly
  outlive an unplugged target.
- One target adapter among active paths is not a complete inventory of
  installed, disabled, disconnected or spare GPUs.
- Matching device/subsystem IDs does not uniquely identify a physical card.
- The reported output type does not identify a particular rear-panel socket,
  monitor, adapter, switch or cable chain.
- Windows routing does not establish pre-boot firmware topology, recovery
  behavior, or compatibility with an unlock profile.

## Build and collection record

The collector compiled with MSVC 19.29.30159 for x64, selected through toolset
14.29. All warnings were treated as errors:

```text
/EHsc /std:c++17 /O2 /W4 /WX
User32.lib Dxgi.lib
```

The source and executable hashes were checked before execution. The source
hash was checked again against the file included in this contribution. The
recorded `repository_base_commit` is the preceding research commit; the new
collector is identified by its own SHA-256 because it was compiled before
this follow-up was committed.

Source SHA-256:
`2999b10f2c75e30bfd30eabf320105d85c0759b8adf8cae1c89bb8a84be03d6d`.

An independent source review found no blocking mapping, bounds or privacy
issues before execution. The resulting JSON was parsed and checked against
the source/binary hashes and the actual exit status. No additional GPU
benchmark was run for this follow-up.

To build from an x64 Visual Studio developer terminal with a Windows 10/11 SDK,
starting at the repository root:

```powershell
New-Item -ItemType Directory -Force research/rtx3080/display-topology/build | Out-Null
cl.exe /EHsc /std:c++17 /O2 /W4 /WX research/rtx3080/display-topology/collect_display_paths.cpp /Feresearch/rtx3080/display-topology/build/collect_display_paths.exe /Foresearch/rtx3080/display-topology/build/collect_display_paths.obj /link User32.lib Dxgi.lib
```

The following Python 3.11 command runs only the read-only collector with a
10-second deadline and prints its JSON:

```powershell
py -3.11 -c "import subprocess; p = subprocess.run([r'research\rtx3080\display-topology\build\collect_display_paths.exe'], timeout=10, check=True, capture_output=True, text=True); print(p.stdout, end='')"
```

These records support the limited observation above. Hashes and JSON
consistency do not provide signed hardware or execution attestation.
