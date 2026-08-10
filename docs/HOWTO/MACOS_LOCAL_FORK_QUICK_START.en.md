# macOS Customized Fork Quick Start

[中文](MACOS_LOCAL_FORK_QUICK_START.md) | [English](MACOS_LOCAL_FORK_QUICK_START.en.md)

This guide covers the personal-use-driven native Zero Hour fork on Apple Silicon macOS: transferring a lawfully owned Windows installation, preparing the runtime layout, building and deploying the engine, and packaging a double-clickable `.app`.

## Important notice

- This repository contains only the open-source engine and build scripts. It does not include commercial game data.
- You must own lawful copies of both Generals and Zero Hour.
- Never commit `.big` archives, audio, video, maps, private fonts, or other commercial assets to GitHub.
- Zero Hour still uses some data from the base game, so prepare two complete directories.
- Test clean retail data first. Add unofficial patches, maps, and mods one at a time after the base game works.

## 1. Prepare the runtime layout

The recommended structure is:

```text
~/GeneralsX/
├── Generals/
│   ├── INI.big
│   ├── W3D.big
│   ├── Window.big
│   └── ...
└── GeneralsZH/
    ├── INIZH.big
    ├── W3DZH.big
    ├── WindowZH.big
    └── ...
```

To use an external disk, choose a runtime root:

```bash
export GX_RUNTIME_ROOT="/Volumes/My Games/GeneralsX Runtime"
mkdir -p "$GX_RUNTIME_ROOT/Generals" "$GX_RUNTIME_ROOT/GeneralsZH"
```

Paths may contain spaces or Chinese characters, but quote them in shell commands. Add the export to `~/.zshrc` if it should persist across Terminal sessions.

## 2. Locate the game data on Windows

Steam, EA App, disc releases, The First Decade, The Ultimate Collection, and other lawful retail editions can all be used. The purchase platform is not the important part; what matters is obtaining complete Windows game directories.

### Base Generals

Search Windows File Explorer for `INI.big`. The correct base-game root normally also contains:

- `W3D.big`
- `Window.big`
- `Textures.big`
- `Audio.big`
- `Generals.exe` or `Generals.dat`

Copy the entire directory into the Mac `Generals/` folder. Do not copy only `INI.big`; audio, textures, models, maps, and UI data live in separate archives.

### Zero Hour

Search for `INIZH.big`. The correct expansion root normally also contains:

- `W3DZH.big`
- `WindowZH.big`
- `TexturesZH.big`
- `AudioZH.big`
- `MapsZH.big`
- `Generals.exe`, `Generals.dat`, or the expansion launcher

Copy the entire directory into the Mac `GeneralsZH/` folder.

### Common launchers

- Steam: right-click the game in the Library and choose **Manage → Browse local files**.
- EA App: open the title under Installed games and inspect its management or install-location controls. If the current client does not expose the folder, search the install drive for `INIZH.big`.
- Disc, First Decade, Ultimate Collection, and manually installed editions: do not depend on a fixed `Program Files` location. Search for `INI.big` and `INIZH.big` instead.

The native macOS engine does not execute the Windows `.exe` and `.dll` files. They may be kept or removed after the data set has been verified. Keep all `.big`, `.dat`, map, video, audio, and required loose `Data/` override files.

## 3. Transfer the data to the Mac

Suitable transfer methods include:

- an exFAT external disk;
- a local network share;
- a trusted personal cloud folder whose contents are fully downloaded.

Copy each complete game directory. Do not rely on cloud placeholder files.

Verify the two marker files:

```bash
export GX_RUNTIME_ROOT="${GX_RUNTIME_ROOT:-$HOME/GeneralsX}"

test -f "$GX_RUNTIME_ROOT/Generals/INI.big" \
  && echo "Generals assets: OK" \
  || echo "Generals assets: MISSING"

test -f "$GX_RUNTIME_ROOT/GeneralsZH/INIZH.big" \
  && echo "Zero Hour assets: OK" \
  || echo "Zero Hour assets: MISSING"
```

List the top-level archives:

```bash
find "$GX_RUNTIME_ROOT/Generals" -maxdepth 1 -iname '*.big' -print | sort
find "$GX_RUNTIME_ROOT/GeneralsZH" -maxdepth 1 -iname '*.big' -print | sort
```

Do not place the runtime directory inside the Git repository.

## 4. Prepare the macOS build environment

You need an Apple Silicon Mac, Xcode Command Line Tools, CMake, Ninja, Meson, Python, vcpkg, FFmpeg, and the complete LunarG Vulkan SDK.

```bash
xcode-select --install
brew install cmake ninja meson pkgconf python vcpkg ffmpeg glm
export VCPKG_ROOT="$(brew --prefix vcpkg)"
```

Install the LunarG Vulkan SDK from <https://vulkan.lunarg.com/sdk/home#mac> and export:

```bash
export VULKAN_SDK="$HOME/VulkanSDK/<version>/macOS"
```

The current build script checks the SDK for `libvulkan.dylib` and `glslangValidator`, so Homebrew Vulkan headers alone are not sufficient.

The first configure downloads and compiles dependencies and takes substantially longer than later incremental builds.

## 5. Clone, build, and deploy

```bash
git clone https://github.com/Log1037/CNC-Generals-for-iOS-macOS.git
cd CNC-Generals-for-iOS-macOS

export GX_RUNTIME_ROOT="${GX_RUNTIME_ROOT:-$HOME/GeneralsX}"

./scripts/build/macos/build-macos-zh.sh
./scripts/build/macos/deploy-macos-zh.sh
```

The deploy script places the engine and runtime libraries beside the Zero Hour data without deleting or replacing commercial assets. The deployed files include:

- `GeneralsXZH`;
- SDL3, OpenAL, DXVK, Vulkan, MoltenVK, and GameSpy runtime libraries;
- `run.sh`, `dxvk.conf`, Fontconfig configuration, and an open Chinese fallback font.

Deployment also refreshes the double-clickable app by default. To deploy only the command-line runtime:

```bash
GX_SKIP_APP_BUNDLE=1 ./scripts/build/macos/deploy-macos-zh.sh
```

## 6. Test the command-line runtime first

```bash
GX_RUNTIME_ROOT="$GX_RUNTIME_ROOT" ./scripts/build/macos/run-macos-zh.sh -win
```

Or run from the deployed directory:

```bash
cd "$GX_RUNTIME_ROOT/GeneralsZH"
./run.sh -win
```

Verify windowed mode before testing fullscreen and app packaging. This makes asset, renderer, and macOS window failures easier to distinguish.

## 7. Package a double-clickable app

```bash
./scripts/build/macos/package-macos-zh-app.sh \
  --game-dir "$GX_RUNTIME_ROOT/GeneralsZH" \
  --generals-dir "$GX_RUNTIME_ROOT/Generals" \
  --runtime "$GX_RUNTIME_ROOT/GeneralsZH" \
  --output "$HOME/Applications/Generals Zero Hour.app"
```

Common options:

| Option | Purpose |
|---|---|
| `--game-dir <dir>` | Zero Hour directory containing `INIZH.big` |
| `--generals-dir <dir>` | Base Generals directory containing `INI.big` |
| `--runtime <dir>` | Runtime directory containing the deployed `.dylib` files |
| `--engine <file>` | Select a specific `GeneralsXZH` engine binary |
| `--icon <file>` | Select a PNG or ICNS icon |
| `--output <app>` | Choose the output app path |
| `--sign <identity>` | Choose the signing identity; `-` means local ad-hoc signing |

Verify the app:

```bash
APP="$HOME/Applications/Generals Zero Hour.app"

codesign --verify --deep --strict "$APP"
"$APP/Contents/MacOS/GeneralsXLauncher" --check
```

`--check` validates the asset roots, marker files, Frameworks, and engine without launching the game. The app is not Apple-notarized by default, so an `spctl` rejection is not the same as broken code signing.

## 8. First launch and path selection

1. Mount the external disk that holds the game data.
2. Double-click the app.
3. The launcher checks environment overrides, remembered locations, package-time locations, and standard directories.
4. If automatic discovery fails, it asks for Zero Hour and base Generals separately.

The native picker accepts:

- `INIZH.big` or `INI.big` directly;
- a game directory containing the marker file;
- a common parent containing `Generals/` and `GeneralsZH/`;
- a file or directory dragged from Finder;
- a manually entered path through `⇧⌘G`.

Successful selections are remembered. To replace them, hold Option while opening the app or run:

```bash
"Generals Zero Hour.app/Contents/MacOS/GeneralsXLauncher" --choose-game-dir
```

The log is stored at:

```text
~/Library/Logs/GeneralsX/ZeroHour.log
```

## 9. Common controls

| Shortcut | Action |
|---|---|
| `Ctrl+G` | Toggle the display, speed, and local single-player panel |
| `Ctrl+[` / `Ctrl+]` | Adjust render FPS |
| `Shift+Ctrl+[` / `Shift+Ctrl+]` | Adjust game speed |
| `Cmd+G` | Release or recapture the cursor |
| `Ctrl+Cmd+F` | Toggle native macOS fullscreen |
| `Alt+N` | Add 10,000 credits in local single-player |

The public defaults are 60 FPS rendering and 1.0x game speed. A personal 2.2x or other speed can still be saved through the panel. Local helper features are intended for offline single-player use only.

## 10. Troubleshooting

### `INIZH.big` was not found

The selected directory is at the wrong level, or only a subdirectory was copied. Select `INIZH.big`, the Zero Hour root, or the common parent that contains both game directories.

### Base-game resources, audio, or cinematics are missing

Confirm that both `Generals/` and `GeneralsZH/` are complete copies rather than a small selection of `.big` files. They do not have to be siblings, but the launcher must be able to resolve both locations.

### The game crashes after adding a Windows patch or mod

Move the added files to a backup directory and retest clean assets. Windows trainer executables, injected DLLs, registry changes, and DirectX wrappers cannot be used by the native macOS engine. Even pure `.big`, map, and loose `Data/` overrides may depend on a specific Windows patch level.

### The app passes `--check` but will not open from Finder

Run `codesign --verify --deep --strict`, inspect `ZeroHour.log`, confirm that the external disk is mounted, and check whether macOS has granted removable-volume access.

### The asset paths must be selected again

Hold Option while opening the app or use `--choose-game-dir`. This replaces both remembered locations.

## Related documents

- [macOS build guide](../BUILD/MACOS.en.md)
- [Customization engineering log](../WORKDIR/reports/MACOS_LOCAL_FORK_CHANGELOG.en.md)
- [General game-file guide](GETTING_THE_GAME_FILES.md)
- [Modification and attribution notice](../../NOTICE.en.md)
- [License](../../LICENSE.md)
