# Prepare the firmware assets

**English** | [한국어](ASSETS.ko.md)

NVIDIA firmware, VBIOS/PROM dumps and driver DLLs are not included. Exact sizes and SHA-256 hashes of the required files are in [`unlock/assets.json`](../unlock/assets.json).

Use legally available original inputs and follow the upstream [firmware instructions at the pinned commit](https://github.com/Zagitta/gpu-unlock/tree/0ebdb1c6308b6a2072be5bf04513472942541956/firmware). This case uses the **GA102 / 610.43.03** firmware family. This is **not** an instruction to install Windows driver 610.43.03; the successful Windows case used 617.14.

## Generate with the upstream tools

In WSL/Linux, prepare Git, make, tar and **Python 3.14 or newer**. The pinned upstream tool requires Python 3.14 for `.tar.zst` input handling. This differs from the Windows EFI build/packaging Python requirement.

Run in a new working directory. These commands download inputs and generate assets; they do not install or load the Linux GPU module. `sudo`, `make install-*` and DKMS installation are unnecessary for this Windows procedure.

```sh
git clone https://github.com/Zagitta/gpu-unlock.git gpu-unlock-firmware-source
cd gpu-unlock-firmware-source
git checkout --detach 0ebdb1c6308b6a2072be5bf04513472942541956
make inputs firmware PYTHON=python3.14 INPUTS="$PWD/inputs" ASSETS="$PWD/assets"
```

### Windows without WSL

The same pinned scripts also run on Windows with Python 3.14 (for example `py -3.14` or a uv-managed
3.14). `make` and an external `zstd` binary are not needed. In a new working directory:

```powershell
git clone https://github.com/Zagitta/gpu-unlock.git gpu-unlock-firmware-source
cd gpu-unlock-firmware-source
git checkout --detach 0ebdb1c6308b6a2072be5bf04513472942541956
py -3.14 -I firmware\fetch_artifacts.py "$PWD\inputs"
New-Item -ItemType Directory assets.src,assets | Out-Null
tar -xf inputs\NVIDIA-kernel-module-source-610.43.03.tar.xz -C assets.src
py -3.14 -I <this repo>\tools\generate_v67_windows.py "$PWD" "$PWD\inputs\nvidia-utils-610.43.03-1-x86_64_v3.pkg.tar.zst" "$PWD\assets"
py -3.14 -I firmware\extract_boot_assets.py "$PWD\assets.src\NVIDIA-kernel-module-source-610.43.03" "$PWD\assets"
```

`tools/generate_v67_windows.py` only replaces upstream's `zstd -dc` pipe with Python's built-in zstd
reader. All of upstream's input, payload and license hash checks still run. This route produced all five files with
the expected sizes and hashes on Windows 11 (2026-10-27, `b550-aorus-3090-usb` evidence).

The upstream `fetch_artifacts.py` checks pinned NVIDIA source and nvidia-utils inputs by SHA-256; generation also verifies output hashes. Copy these five `.bin` files from the generated `assets/` directory into this repository's local `assets/`:

```text
booter_load_ga102.bin
v67-ga102-610.43.03.bin
gsp_ga10x.bin
gsp_boot_ga102.bin
gsp_boot_desc_ga102.bin
```

Review and retain the upstream-generated `NVIDIA-LICENSE` and [firmware/NOTICE.md](https://github.com/Zagitta/gpu-unlock/blob/0ebdb1c6308b6a2072be5bf04513472942541956/firmware/NOTICE.md). During the original public preparation, asset generation was not rerun; existing verified assets were checked against all five expected sizes and hashes using the packaging tool. This documentation update also does not claim a new asset-generation run.

Do not substitute similar versions. Both packaging and the UEFI app reject mismatched hashes. Preparation is incomplete if you cannot establish the input source, generation method and license terms.
