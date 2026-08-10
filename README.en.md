# Command & Conquer: Generals — Zero Hour for Apple Platforms

[中文](README.md) | [English](README.en.md)

<img width="500" height="281" alt="Zero Hour running on an Apple platform" src="https://github.com/user-attachments/assets/aeaf6692-36e6-40c8-b9f8-8066d014ec4b" />

This community project runs Command & Conquer: Generals — Zero Hour natively on Apple Silicon Macs, iPhone, and iPad. It is not a Windows emulator: the game engine is compiled directly for ARM64, while the original DirectX 8 renderer reaches Metal through DXVK, Vulkan, and MoltenVK.

This repository is a personal-use-driven fork of [`ammaarreshi/Generals-Mac-iOS-iPad`](https://github.com/ammaarreshi/Generals-Mac-iOS-iPad). It retains the upstream Apple-platform port and adds macOS usability fixes and local single-player features based on real use with an Apple Silicon Mac, Chinese game data, an external-disk installation, and day-to-day play.

> This repository does not include commercial assets from Generals or Zero Hour. You must own and supply a lawful Windows copy of the game data.

## What this fork adds

- Improved Retina, HiDPI, windowed-mode, native macOS fullscreen, and safe-exit behavior.
- Independent render cadence and simulation speed, with public defaults of 60 FPS and 1.0x game speed.
- An in-game Display & Speed panel for resolution, render scale, timing, camera, and local single-player options.
- Better Chinese font fallback, glyph selection, text sizing, and UI legibility.
- Fixes for videos, cinematics, scaled effects, offscreen rendering, and distant-unit materials encountered during normal play.
- Improved SDL input, cursor capture, menu robustness, and macOS window switching.
- Portable asset discovery and `.app` packaging suitable for external-disk installations.

See the [English engineering log](docs/WORKDIR/reports/MACOS_LOCAL_FORK_CHANGELOG.en.md) for the complete record.

## Platform status

| Platform | Status | Notes |
|---|---|---|
| Apple Silicon macOS | Primary use platform | Supports local builds, command-line runs, and a double-clickable `.app` |
| iPhone / iPad | Inherited from the direct upstream | Requires full Xcode, a signing team, and the iOS packaging workflow |
| Linux | Shared engine retained | The macOS customization still needs broader cross-platform regression testing |

## Quick start

### 1. Prepare the game data

Prepare complete copies of both the base game and Zero Hour:

```text
GeneralsX Runtime/
├── Generals/
│   └── INI.big
└── GeneralsZH/
    └── INIZH.big
```

The files may come from Steam, EA App, a disc release, The First Decade, The Ultimate Collection, or another lawfully owned Windows installation. The purchase platform is less important than having a complete data set.

For Windows-to-Mac transfer instructions, see:

- [macOS customized-fork quick start](docs/HOWTO/MACOS_LOCAL_FORK_QUICK_START.en.md)
- [General game-file guide](docs/HOWTO/GETTING_THE_GAME_FILES.md)

### 2. Prepare the build environment

```bash
xcode-select --install
brew install cmake ninja meson pkgconf python vcpkg ffmpeg glm
export VCPKG_ROOT="$(brew --prefix vcpkg)"
export VULKAN_SDK="$HOME/VulkanSDK/<version>/macOS"
```

Install the Vulkan SDK from LunarG; Homebrew Vulkan headers alone are not sufficient. See the [macOS build guide](docs/BUILD/MACOS.en.md) for the complete dependency notes.

### 3. Clone, build, and deploy

```bash
git clone https://github.com/Log1037/Generals-Mac-iOS-iPad.git GeneralsX
cd GeneralsX

export GX_RUNTIME_ROOT="$HOME/GeneralsX Runtime"
./scripts/build/macos/build-macos-zh.sh
./scripts/build/macos/deploy-macos-zh.sh
```

### 4. Package a double-clickable app

```bash
./scripts/build/macos/package-macos-zh-app.sh \
  --game-dir "$GX_RUNTIME_ROOT/GeneralsZH" \
  --generals-dir "$GX_RUNTIME_ROOT/Generals" \
  --runtime "$GX_RUNTIME_ROOT/GeneralsZH" \
  --output "$HOME/Applications/Generals Zero Hour.app"
```

The launcher supports:

- automatic discovery of common and previously successful locations;
- direct selection of `INIZH.big`, `INI.big`, either game directory, or their common parent;
- Finder drag-and-drop into the native picker;
- manual path entry with `⇧⌘G`;
- forced asset reselection by holding Option while opening the app.

## Common controls

| Shortcut | Action |
|---|---|
| `Ctrl+G` | Toggle the display, speed, and local single-player panel |
| `Ctrl+[` / `Ctrl+]` | Adjust render FPS |
| `Shift+Ctrl+[` / `Shift+Ctrl+]` | Adjust game speed |
| `Cmd+G` | Release or recapture the cursor |
| `Ctrl+Cmd+F` | Toggle native macOS fullscreen |
| `Alt+N` | Add 10,000 credits in local single-player |

Local helper features are intended for offline single-player use only.

## Documentation

- [macOS quick-start guide](docs/HOWTO/MACOS_LOCAL_FORK_QUICK_START.en.md)
- [macOS build guide](docs/BUILD/MACOS.en.md)
- [Customization engineering log](docs/WORKDIR/reports/MACOS_LOCAL_FORK_CHANGELOG.en.md)
- [Modification and attribution notice](NOTICE.en.md)
- [Porting playbook](docs/port/PORTING_PLAYBOOK.md)
- [Porting patterns](docs/port/PORTING_PATTERNS.md)

## Lineage and credits

This project builds on:

- Electronic Arts' [GPLv3 engine source release](https://github.com/electronicarts/CnC_Generals_Zero_Hour)
- [TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode)
- [Fighter19/CnC_Generals_Zero_Hour](https://github.com/Fighter19/CnC_Generals_Zero_Hour)
- [fbraz3/GeneralsX](https://github.com/fbraz3/GeneralsX)
- The direct upstream, [ammaarreshi/Generals-Mac-iOS-iPad](https://github.com/ammaarreshi/Generals-Mac-iOS-iPad)
- DXVK, MoltenVK, SDL3, OpenAL Soft, FFmpeg, Fontconfig, FreeType, and other open-source components

See [NOTICE.en.md](NOTICE.en.md) for the detailed lineage and modification boundary.

## License and disclaimer

Source is distributed under the repository's [GPLv3 license and EA additional terms](LICENSE.md). Third-party components retain their respective licenses.

This is not an official release by Electronic Arts, Westwood, EA Pacific, or any upstream community project. Game names, story, artwork, audio, maps, and all other commercial assets remain the property of their respective rights holders.
