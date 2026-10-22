# MarathonRecomp-Prospero — PS5 build setup

This checkout carries an **experimental, unofficial PlayStation 5 (Prospero) port** of MarathonRecomp. It follows the PS5 build shape used by `UnleashedRecomp-Prospero`: Linux host tools, the PS5 Payload SDK, PacBrew static libraries, a PS5 RADV build, manual cross-compilation, ELF conversion/FSELF signing, then optional FTP deployment. The upstream desktop CMake presets remain a separate build path. This fork is not endorsed by or intended for merge into upstream.

The game-specific source baseline in this workspace matches upstream commit [`6bcffacefc9363a1a3acfb513ce515cb9167eb52`](https://github.com/sonicnext-dev/MarathonRecomp/commit/6bcffacefc9363a1a3acfb513ce515cb9167eb52), with local PS5-only changes layered on top. Keep Marathon's own generated PPC/shader code and game patches; do not substitute Unleashed's game sources.

No Xbox 360 game files are included. Use files from your own legally acquired copy; keep them private and do not commit them.

## Host prerequisites

The documented build host is x86-64 Linux. The automatic setup downloads the pinned PS5 SDK, PacBrew ports, Mesa/RADV sources, and DXC's Linux x64 runtime/compiler into a cache outside this checkout:

```text
${XDG_CACHE_HOME:-$HOME/.cache}/MarathonRecomp/
```

You can override the dependency root with `PS5_DEPS_ROOT`. A first dependency bootstrap is large and needs network access and free disk space.

On Debian/Ubuntu, a typical starting set is:

```bash
sudo apt install build-essential clang lld llvm cmake meson ninja-build pkg-config \
  git wget curl unzip tar xz-utils rsync patch python3 python3-pip \
  python3-mako python3-ply python3-yaml glslang-tools libzstd-dev zlib1g-dev
```

Package names differ by distribution. The RADV build also needs `glslangValidator`, Meson, Ninja, Python `mako`/`ply`/`pyyaml`, and `rsync`. The host recompiler tools use Clang/Clang++ (including Microsoft's extension support), zstd development headers/library, and Git access to fetch the pinned Linux x64 DXC runtime and compiler. The pinned DXC CLI is staged at `bin/x64/dxc-linux`; the PS5 build uses it to generate renderer SPIR-V headers before C++ compilation. XenonRecomp's Windows-style anonymous aggregates are not GCC-compatible. Small utilities such as `x_decompress` use system GCC by default. The build probes Clang's C++ standard library, prefers a compatible installed Clang, and tries libc++ when available; Clang 18 with GCC 16's libstdc++ headers is incompatible. On Fedora, libc++ support is provided by `libcxx-devel` and `libcxxabi-devel`; install them with `sudo dnf install libcxx-devel libcxxabi-devel` if the probe reports no compatible standard library. Alternatively install Clang 19 or newer. Override Xenon/Xenos with `HOST_CXX` (or `CXX`) and small utilities with `HOST_HELPER_CXX` if needed.

## Source dependencies

For a fresh clone, initialize the repository's submodules recursively:

```bash
git clone --recurse-submodules <your-MarathonRecomp-Prospero-fork-url> MarathonRecomp-Prospero
cd MarathonRecomp-Prospero
```

The PS5 build needs the MarathonRecomp resources submodule as well as XenonRecomp and XenosRecomp. If you already cloned without submodules, run:

```bash
git submodule update --init --recursive
```

The PS5 pipeline uses PacBrew rather than the desktop vcpkg dependency prefix. To keep the working set compact, this workspace stores the optional desktop vcpkg checkout in `thirdparty/vcpkg.snapshot.tar.xz`; restore it before using the desktop CMake presets:

```bash
make restore-vcpkg
```

## Private game inputs

Provide three matching retail files:

- `default.xex` — game root
- `shader.arc` — `xenon/archives`
- `shader_lt.arc` — `xenon/archives`

The default input layout is:

```text
MarathonRecomp-Prospero/
└── ressources/
    └── game/
        ├── default.xex
        └── xenon/archives/
            ├── shader.arc
            └── shader_lt.arc
```

The recompiler searches case-insensitively below `ressources/game`. You may instead set `MARATHON_GAME_DIR` to the directory containing `game/`, or directly to the game directory. The three files can also be placed in `MarathonRecompLib/private/`; the build never downloads or distributes retail files. In the installed PS5 package, the game tree is mounted at `/app0/ressources`; persistent config and save data use `/data/MarathonRecomp`.

## Mod management

The Mods manager lets discovered folders in `/data/MarathonRecomp/mods/` be enabled regardless of whether they contain `xenon/`, `win32/`, both, or another layout. Enabled state and load priority are stored in `/data/MarathonRecomp/mods/mods.json`. When managed folders are found, the title screen's **Square** shortcut opens the dedicated Mods window and a startup notification reports how many folders were detected; neither indicates that a mod was verified to work. The Square prompt and notification are hidden when no managed folders are found. Schema-defined mod options are supported, and edits are staged until you confirm saving and relaunching. The existing **Options → Mods** page remains for quick enable/order changes.

**Not every mod works in this recompiled game.** The manager attempts to load enabled mods but does not test compatibility. Mods may target different game versions or asset paths, conflict with one another, or depend on features that are not implemented. Archive merging is inactive, and PS5 registration of brand-new custom archives remains disabled; see [`docs/MOD_MANAGER.md`](docs/MOD_MANAGER.md) for details. External Xenia patch files are not loaded.

## Build

This checkout defaults to the existing title ID `PPSA99903`, matching `sce_sys/param.json`; plain `make` uses it without an argument. Override it only if you need a different locally assigned `PPSA`/`PPSB` ID:

```bash
make
# Optional override:
make TITLE_ID=PPSA12345
```

You can persist settings in an ignored `.env` file; `.env.example` shows the available variables.

The first `make` performs the full sequence:

1. Runs `tools/prepare-assets.sh` to normalize the launcher icon, convert retained `pic0.source.*` / `pic1.source.*` images, encode supported `snd0.source.*` audio (as well as `snd0.wav`), and validate package-ready assets. No placeholder backgrounds or audio are created when sources are absent.
2. Bootstraps the PS5 Payload SDK/zlib, PacBrew FFmpeg/SDL archives, and static RADV if they are not already cached. The first PS5 build also builds a small FFmpeg 7.0.1 `XMAFRAMES` decoder into the external dependency cache: PacBrew's stock FFmpeg does not include Marathon's raw-XMA-frame decoder.
3. Builds host tools (`XenonRecomp`, Marathon-configured `XenosRecomp`, `u8extract`, and `file_to_c`), then recompiles PPC sources and both Xenos shader archives from the private inputs.
4. Uses the pinned DXC CLI to generate renderer SPIR-V headers, then converts the `MarathonRecompResources` CMake `BIN2C` manifest into embedded resource C files.
5. Cross-compiles the PS5 executable, links it with RADV and PacBrew's matching FFmpeg support libraries plus the patched `XMAFRAMES` decoder, signs `eboot.bin`, creates `sce_sys/param.json`, and stages `dist/<TITLE_ID>/` plus `pkg/<TITLE_ID>/`.

Useful incremental targets:

```bash
make assets                         # convert presentation assets only
make tools
make recomp MARATHON_GAME_DIR=ressources
make resources
make deps
make clean
```

`make deps` prebuilds the dependencies; the normal `make` path also bootstraps missing dependencies. Keep the launcher icon source at `sce_sys/icon0.source.*` (or supply a valid `sce_sys/icon0.png`); the icon converter normalizes the package icon to a real 512×512 PNG. A malformed or wrong-sized `icon0.png` is normalized from itself so a user's replacement is not overwritten by an older source, while a valid 512×512 icon is preserved unless a retained icon source is newer. No placeholder backgrounds or audio are created when source files are absent. Normal `make` or `make assets` automatically converts retained `sce_sys/pic0.source.*` and `sce_sys/pic1.source.*` images to 3840×2160 BC7 DDS files; if only one background source exists, it fills both PS5 presentation slots. Supported `sce_sys/snd0.source.*` audio files—including MP3—as well as `sce_sys/snd0.wav` are encoded to looped 48 kHz ATRAC9 `sce_sys/snd0.at9`. The source files remain in the project folder and are not copied into the package. Explicit overrides remain available through `tools/prepare-assets.sh --background PATH`, `--selection-background PATH`, `--launch-background PATH`, and `--audio PATH`. On PS5, the render scale remains 100% (1.0) by default and its in-game Video option is hidden; the FPS default is 60, with 30/60/90/120 still available. Existing PS5 saves at 120 FPS migrate to 60 once; saved 30/60/90 FPS selections are preserved. The empty `ressources/game/` input folder is present; add your private retail files there.

## Deploy and run

FTP folder deployment and payload launch are separate steps:

```bash
make deploy PS5_HOST=192.0.2.10
make launch PS5_HOST=192.0.2.10
```

These targets use the default `PPSA99903`; add `TITLE_ID=...` only when overriding it.

Set `FTP_PORT` (default `2121`), `PS5_ELF_PORT` (default `9021`), and optional `PS5_FTP_USER`/`PS5_FTP_PASSWORD` as needed. The deployment helper currently supports folder output only. To stop or remove the staged title:

```bash
make close PS5_HOST=192.0.2.10
make undeploy PS5_HOST=192.0.2.10
```

On PS5, the desktop-only Native File Dialog installer wizard and online update checker are disabled: the game tree must be staged into the package as described above, and the console build has no desktop folder picker or browser launcher.

## Current status and limitations

This is an in-progress port, not a tested release. The local workspace currently has no retail `default.xex`, `shader.arc`, or `shader_lt.arc`, so PPC/shader generation and the final PS5 build cannot be run here. A successful shell/CMake syntax check is not a PS5 runtime validation; hardware testing remains outstanding.
