# GeneralsX macOS Build Guide

[中文](MACOS.md) | [English](MACOS.en.md)

This guide is for developers and advanced users who want to build `GeneralsXZH` from source. Users who mainly need to transfer their lawful game data and package an app should start with the [quick-start guide](../HOWTO/MACOS_LOCAL_FORK_QUICK_START.en.md).

## System requirements

- An Apple Silicon Mac;
- macOS 13 or later;
- Xcode Command Line Tools;
- approximately 10 GB of free space;
- lawfully owned data from both Generals and Zero Hour.

## 1. Install the build tools

```bash
xcode-select --install
brew install cmake ninja meson python pkgconf ffmpeg glm vcpkg
export VCPKG_ROOT="$(brew --prefix vcpkg)"
```

The build script checks these vcpkg locations:

- `$VCPKG_ROOT`;
- `vcpkg/` inside the repository;
- `~/vcpkg`;
- common Homebrew and `/opt/vcpkg` locations.

## 2. Install the LunarG Vulkan SDK

The current `build-macos-zh.sh` requires the complete LunarG macOS Vulkan SDK, including `libvulkan.dylib` and `glslangValidator`. Homebrew Vulkan headers alone are not sufficient.

1. Open <https://vulkan.lunarg.com/sdk/home#mac>.
2. Download and install the macOS SDK.
3. Export its path:

```bash
export VULKAN_SDK="$HOME/VulkanSDK/<version>/macOS"
```

Verify the installation:

```bash
test -f "$VULKAN_SDK/lib/libvulkan.dylib"
test -f "$VULKAN_SDK/lib/libMoltenVK.dylib"
test -x "$VULKAN_SDK/bin/glslangValidator"
```

The script also reads `$VULKAN_SDK_ROOT` and scans `~/VulkanSDK/*/macOS`.

## 3. Prepare the game data

The recommended layout is:

```text
${GX_RUNTIME_ROOT:-$HOME/GeneralsX}/
├── Generals/
│   └── INI.big
└── GeneralsZH/
    └── INIZH.big
```

External-disk example:

```bash
export GX_RUNTIME_ROOT="/Volumes/My Games/GeneralsX Runtime"
```

The data must come from the user's own lawful Windows copy. This repository does not download or distribute commercial assets. See the [quick-start guide](../HOWTO/MACOS_LOCAL_FORK_QUICK_START.en.md) for Windows-to-Mac transfer instructions.

## 4. Clone and build

```bash
git clone https://github.com/Log1037/CNC-Generals-for-iOS-macOS.git
cd CNC-Generals-for-iOS-macOS

./scripts/build/macos/build-macos-zh.sh
```

The script:

1. checks CMake, Ninja, Meson, Python, vcpkg, and the Vulkan SDK;
2. configures the `macos-vulkan` CMake preset;
3. fetches the pinned DXVK fork and builds it with Meson;
4. builds the `z_generals` target;
5. writes the full log to `logs/build_zh_macos-vulkan.log`.

The first build downloads and compiles dependencies. Later builds reuse the cache.

For an incremental build after configuration:

```bash
./scripts/build/macos/build-macos-zh.sh --build-only
```

Equivalent manual commands:

```bash
cmake --preset macos-vulkan
cmake --build build/macos-vulkan --target z_generals \
  -j"$(( ($(sysctl -n hw.logicalcpu) + 1) / 2 ))"
```

The engine is written to:

```text
build/macos-vulkan/GeneralsMD/GeneralsXZH
```

## 5. Deploy the runtime

```bash
export GX_RUNTIME_ROOT="${GX_RUNTIME_ROOT:-$HOME/GeneralsX}"
./scripts/build/macos/deploy-macos-zh.sh
```

The deploy script installs the engine, DXVK, MoltenVK, Vulkan loader, SDL3, OpenAL, FFmpeg-related runtime libraries, GameSpy compatibility libraries, Fontconfig configuration, and launch scripts. It does not delete or replace commercial game data.

Deployment also refreshes the local app by default. To skip app packaging:

```bash
GX_SKIP_APP_BUNDLE=1 ./scripts/build/macos/deploy-macos-zh.sh
```

## 6. Run from the command line

```bash
./scripts/build/macos/run-macos-zh.sh -win
```

Or run from the deployed directory:

```bash
cd "$GX_RUNTIME_ROOT/GeneralsZH"
./run.sh -win
```

Common options:

| Option | Effect |
|---|---|
| `-win` | Windowed mode, recommended for troubleshooting |
| `-fullscreen` | Fullscreen mode |
| `-noshellmap` | Skip the animated main-menu background |
| `-xres 1280 -yres 720` | Set the logical resolution |

## 7. Package the local app

```bash
./scripts/build/macos/package-macos-zh-app.sh \
  --game-dir "$GX_RUNTIME_ROOT/GeneralsZH" \
  --generals-dir "$GX_RUNTIME_ROOT/Generals" \
  --runtime "$GX_RUNTIME_ROOT/GeneralsZH" \
  --output "$HOME/Applications/Generals Zero Hour.app"
```

`GeneralsXLauncher` is the app's real entry point. It:

- resolves the Zero Hour and base Generals data;
- sets the engine working directory and asset environment;
- points the engine at the bundled DXVK, Vulkan, and MoltenVK libraries;
- restores the saved render cadence and game speed;
- writes output to `~/Library/Logs/GeneralsX/ZeroHour.log`;
- reports missing assets and libraries instead of failing silently.

Path sources are handled in this order:

1. environment overrides;
2. remembered successful locations;
3. package-time locations;
4. `$GX_RUNTIME_ROOT` and standard directories;
5. the native file picker.

The picker accepts marker files, game directories, or their common parent, with Finder drag-and-drop and manual `⇧⌘G` entry.

## 8. Verify the app

```bash
APP="$HOME/Applications/Generals Zero Hour.app"

plutil -lint "$APP/Contents/Info.plist"
codesign --verify --deep --strict "$APP"
"$APP/Contents/MacOS/GeneralsXLauncher" --check
```

`--check` does not launch the game. The default build is ad-hoc signed and not Apple-notarized, so an `spctl` rejection is not the same as a failed `codesign` verification.

## 9. macOS window shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+Cmd+F` | Toggle native macOS fullscreen |
| `Cmd+G` | Release or recapture the cursor |

The launch mode still comes from command-line options and `Options.ini`. These shortcuts change only the current session.

## 10. DXVK source model

The macOS build uses the pinned remote DXVK fork configured in `cmake/dx8.cmake`. Do not patch `build/_deps/` directly.

For local DXVK development:

```bash
cmake --preset macos-vulkan -DSAGE_DXVK_USE_LOCAL_FORK=ON
```

## 11. Troubleshooting

### Vulkan SDK not found

Confirm that `$VULKAN_SDK/lib/libvulkan.dylib` and `$VULKAN_SDK/bin/glslangValidator` exist. Do not install only the Homebrew Vulkan headers.

### vcpkg not found

```bash
brew install vcpkg
export VCPKG_ROOT="$(brew --prefix vcpkg)"
```

### DXVK Meson still uses stale build state

```bash
rm -rf build/macos-vulkan/_deps/dxvk-src-fbraz3 \
       build/macos-vulkan/_deps/dxvk-build-macos
cmake --preset macos-vulkan
```

### `VK_ERROR_INCOMPATIBLE_DRIVER` at runtime

Redeploy, confirm the runtime contains the correct `libMoltenVK.dylib` and ICD JSON, and inspect `VK_ICD_FILENAMES`.

### The app cannot find the game data

Select `INIZH.big` and `INI.big` directly, or hold Option while opening the app to reselect both locations. Do not select only a `Data/` subdirectory.

### Black bars or incorrect fullscreen size

Run with `-win` first to verify assets and rendering. Then inspect the log's drawable, window, and display-bounds values. Retina builds must keep AppKit points separate from drawable pixels.

## 12. Current validation boundary

Verified locally:

- a full `z_generals` build;
- syntax checks for deployment, run, and packaging scripts;
- isolated app plist, deep-signature, and launcher `--check` validation;
- direct-directory, common-parent, and marker-file path inputs.

Still needed:

- an end-to-end build on another clean Apple Silicon Mac;
- Linux, iOS, and base-Generals regressions;
- replay determinism and network lockstep checks.

## Related files

| File | Purpose |
|---|---|
| `scripts/build/macos/build-macos-zh.sh` | Configure and build `GeneralsXZH` |
| `scripts/build/macos/deploy-macos-zh.sh` | Deploy the engine and runtime libraries |
| `scripts/build/macos/run-macos-zh.sh` | Run with the correct environment |
| `scripts/build/macos/package-macos-zh-app.sh` | Package the local double-clickable app |
| `packaging/macos/GeneralsXLauncher.m` | App launcher |
| `cmake/dx8.cmake` | DXVK fetch and build configuration |
