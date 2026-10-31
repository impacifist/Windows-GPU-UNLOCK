# Check your PC. Share what you find.

**English** · [한국어](START-HERE.ko.md) · [Back to project](../README.md)

You can help this project **without attempting an unlock**. Start with a Windows inventory, or check capture files you already have.

> [!TIP]
> **New here? Choose option 1.** You do not need a USB stick, compiler, firmware files, administrator terminal or BIOS changes to submit an inventory report.

| What you have | Start here | What you get |
| :--- | :--- | :--- |
| A Windows PC and no capture files | [**1 · Describe your PC**](#1--describe-your-pc) | Hardware and driver details for a report |
| Existing `DIAGnn.TXT`, `IMGnn.BIN` and `PROM.BIN` files from your PC | [**2 · Check existing files**](#2--check-existing-files) | An offline result with reasons |
| A request to collect new EFI captures | [**3 · Plan a capture**](#3--plan-a-capture) | A separate attended boot procedure |

## Before you start · Open the project folder

1. On the [project page](https://github.com/impacifist/Windows-GPU-UNLOCK), select **Code → Download ZIP**.
2. Right-click the downloaded ZIP → **Extract All**. Open the extracted folder containing `README.md`, `tools` and `research`. Do not run commands inside the ZIP preview.
3. In File Explorer's address bar, type `powershell` and press **Enter**. This opens a normal terminal in that folder.
4. Check Python:

   ```powershell
   python --version
   ```

Use **Python 3.10 or newer** for both options below. If the command opens the Store, is not recognized or shows an older version, install Python from [python.org](https://www.python.org/downloads/windows/), then close and reopen the terminal. If your installation uses `py`, try `py -3 --version` and use `py -3` instead of `python` in the commands below. Prefer not to install anything? Enter your hardware details manually in the report form.

## 1 · Describe your PC

Copy this one command into the terminal and press Enter:

```powershell
python -I .\tools\collect_compatibility.py
```

It reads Windows hardware information and prints a report. It does not run an unlock, change settings, write firmware or upload anything. You should see JSON: text beginning with `{`, containing sections such as `hardware` and `assessment`.

**Expected:** `assessment.status` is `not-assessed` and `can_unlock` is `false`. This is normal, even on a reported working card. The inventory describes your PC; it cannot certify unlock compatibility. If some sections are missing, you can still report what was collected and fill in the gaps manually.

**Next:** review the output, then [share your findings](#share-your-findings). You are finished with the Windows-only check; no EFI boot is needed.

## 2 · Check existing files

Use files from **your own PC**, kept together in their original capture folder. Replace the example `D:\RTX3090-UNLOCK` with that folder's path. The historical folder name is also used for other cards; it does not identify your GPU.

```powershell
python -I .\research\profile\check-compatibility.py "D:\RTX3090-UNLOCK" --json
```

This reads local files only. It does not scan the running GPU, execute a captured file, generate an unlock profile or upload data. No compiler is needed. Missing files are a result you can report; do not download another person's captures to fill the gaps.

| Result | Plain meaning | What to report |
| :--- | :--- | :--- |
| `candidate` | Existing files pass this route's offline checks | Share the report; it is **not approval to unlock** |
| `needs_data` | Required data is missing | Share which data is missing |
| `outside_scope` | This automatic route does not accept the recorded configuration | Share the reason; other routes may differ |
| `invalid_data` | Files or observations could not be accepted | Share the reason; keep the originals |
| `error` | The checker could not load correctly | Share the error and downloaded source version |

Exit code `2` is expected for a non-candidate result; it does not automatically mean the program crashed. See [checker details](CHECK-COMPATIBILITY.en.md) for its limits and comparison tool.

## Share your findings

1. Search [existing issues](https://github.com/impacifist/Windows-GPU-UNLOCK/issues) for your earlier report. Update that issue if this is the same machine and investigation.
2. Otherwise open the [compatibility report form](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/new?template=compatibility.yml). A GitHub account is needed to submit an issue; the checks above work without one.
3. If you ran option 1, paste the **reviewed inventory output** into the inventory field. If you ran option 2, paste its JSON into **Offline compatibility report**; enter hardware details manually if you did not collect an inventory. These are different reports; keep their fields separate.
4. Add your board/BIOS, physical card model and where the monitor cable is connected. Unknown is a valid answer. Say whether no EFI was run, or describe the actual Windows/display outcome of an existing run, including any manual power-off.
5. Mention the source commit if known; otherwise say you used the main ZIP and give the download date. Include the command and error text if something failed.

> [!IMPORTANT]
> Share the reviewed **text report**, not `PROM.BIN`, `IMGnn.BIN`, firmware assets or raw system dumps. Remove personal paths, serials, UUIDs, recovery keys and anything you do not want public. No upload happens automatically.

<details>
<summary><strong>Optional · Save the displayed report as a text file</strong></summary>

The simplest way is to copy the terminal's report into Notepad and save it under a new filename. This lets you review it before posting. Be careful not to overwrite an existing file. You can also paste the reviewed text directly into the issue form; no saved file is required.

</details>

## 3 · Plan a capture

An EFI capture collects information before Windows starts. It is **not** the Windows-only inventory above. Even a diagnostic that avoids GPU writes can hang or fail to return to Windows, requiring manual power-off. Do not disable security features or boot firmware merely to complete an issue form.

If you choose to proceed, use the [new-machine capture guide](NEW-MACHINE.en.md) and review its prerequisites and recovery limits first. That page also describes later profile/build stages; **stop at the capture boundary** when the goal is only to collect evidence. No unlock run is required for a compatibility report.

The guided `Start-Capture.cmd` launcher is being developed in [PR #112](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/112); it is not part of this main snapshot. Do not expect a downloaded main ZIP to contain it yet.

---

**Looking for unlock instructions?** Read the [exact supported configurations](COMPATIBILITY.en.md) first. A shared GPU name, inventory output or `candidate` result does not qualify a new machine.
