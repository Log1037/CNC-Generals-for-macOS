# GeneralsX - macOS Build Instructions (Apple Silicon)

## Prerequisites

### System Requirements

- **macOS 15 (Sequoia) or later** on Apple Silicon (M1/M2/M3/M4)
- **Xcode Command Line Tools** 14+
- ~10 GB free disk space (build artifacts + DXVK Meson build)

### 1. Xcode Command Line Tools

```bash
xcode-select --install
```

### 2. Homebrew

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### 3. Build Tools

```bash
brew install cmake ninja meson python3 pkgconf ffmpeg glm
```

> **Note on `meson`**: The DXVK sub-project requires Meson >= 1.0. The Homebrew arm64
> bottle is sufficient. CMake overrides the build arches via `CFLAGS/CXXFLAGS=-arch arm64`.

### 4. Vulkan SDK (REQUIRED — NOT from Homebrew)

Download the **macOS Vulkan SDK** from LunarG. **Do not use the Homebrew `vulkan-headers` package**
— it lacks the MoltenVK ICD JSON that routes Vulkan calls to Metal.

1. Go to <https://vulkan.lunarg.com/sdk/home#mac>
2. Download the latest SDK installer (`.dmg`)
3. Run the installer — it installs to `~/VulkanSDK/<version>/macOS/`

After installation, verify:

```bash
ls ~/VulkanSDK/*/macOS/lib/libvulkan.dylib   # should list one file
ls ~/VulkanSDK/*/macOS/lib/libMoltenVK.dylib # should list one file
```

### 5. Game Files

Copy your retail Command & Conquer: Generals Zero Hour installation to:

```
~/GeneralsX/GeneralsZH/
```

Legacy fallback during migration is still supported:

```
~/GeneralsX/GeneralsMD/
```

Required files from the retail install:

- `generalszh.big`, `W3DZH.big`, `MapsZH.big` (and other `.big` archives)
- `AudioZH.big` (even though audio is not yet functional)

---

## Building

### Clone the Repository

```bash
git clone https://github.com/fbraz3/GeneralsX.git
cd GeneralsX
```

### Configure and Build

```bash
./scripts/build/macos/build-macos-zh.sh
```

This does:

1. Checks all prerequisites (cmake, ninja, meson, Vulkan SDK)
2. Runs `cmake --preset macos-vulkan` (fetches pinned DXVK fork commit and builds via Meson)
3. Builds `z_generals` target (Zero Hour executable)
4. Prints the binary path on success

**First run takes 5-10 minutes** because DXVK is fetched from git and compiled
via Meson. Subsequent builds reuse the Meson cache and finish in under a minute.

> **`--build-only` flag**: If you have already configured (cmake cache exists),
> skip configuration:
> ```bash
> ./scripts/build/macos/build-macos-zh.sh --build-only
> ```

### Manual cmake commands (equivalent)

```bash
cmake --preset macos-vulkan
cmake --build build/macos-vulkan --target z_generals -j$(sysctl -n hw.logicalcpu)
```

---

## Deploying

After a successful build, deploy the binary and Vulkan runtime to the game directory:

```bash
./scripts/build/macos/deploy-macos-zh.sh
```

This script:

- Copies `build/macos-vulkan/GeneralsMD/GeneralsXZH` to `~/GeneralsX/GeneralsZH/` (or `~/GeneralsX/GeneralsMD/` when legacy assets are detected)
- Detects the Vulkan SDK in `~/VulkanSDK/` and copies:
  - `libvulkan.dylib`, `libvulkan.1.dylib`
  - `libMoltenVK.dylib`
- Writes the `MoltenVK_icd.json` ICD manifest
- Generates a `run.sh` wrapper that sets `VK_ICD_FILENAMES` before launching
- Rebuilds the double-clickable app bundle (skip with `GX_SKIP_APP_BUNDLE=1`)

---

## The app bundle

Deploy already refreshes it, so a normal build/deploy cycle needs nothing extra.
To build it on its own:

```bash
./scripts/build/macos/package-macos-zh-app.sh
```

This is the entry point a player double-clicks. It is deliberately separate from
`bundle-macos-zh.sh`: that script builds a portable release bundle, while this one
builds the local Chinese-named launcher with the selected user-supplied asset path
baked in. Choose that path with `--game-dir`, `GX_GAME_DIRECTORY`, or
`GX_RUNTIME_ROOT`; no developer-specific absolute path is stored in source.

The bundle's `CFBundleExecutable` is `GeneralsXLauncher` (compiled from
`packaging/macos/GeneralsXLauncher.m`), not the engine. The launcher has to be the
entry point because it establishes everything the engine needs before `exec`:

- `DYLD_LIBRARY_PATH` pointing at `Contents/Frameworks` — the engine `dlopen`s
  `libdxvk_d3d8.dylib` by bare name, which dyld never resolves against `LC_RPATH`
- `VK_ICD_FILENAMES` / `VK_DRIVER_FILES` for the bundled MoltenVK
- `CNC_GENERALS_ZH_PATH` and the working directory, so loose `Data/INI` overrides resolve
- `GX_RENDER_FPS` and `GX_LOGIC_FPS`, read from `Options.ini`
- stdout/stderr redirected to `~/Library/Logs/GeneralsX/ZeroHour.log`

It also preflights the asset root and shows a Chinese alert on failure rather than
dying silently — useful because the assets live on a removable volume. Run that
preflight without starting the game:

```bash
"将军：零点行动.app/Contents/MacOS/GeneralsXLauncher" --check
```

Signing is ad-hoc (`--sign -`), which is enough for local Gatekeeper. `spctl`
will still report `rejected` because the bundle is not notarized; that is expected
and does not prevent launching a locally built app.

For a complete Chinese/English walkthrough covering Windows asset transfer,
deployment, and app packaging, see
[`docs/HOWTO/MACOS_LOCAL_FORK_QUICK_START.md`](../HOWTO/MACOS_LOCAL_FORK_QUICK_START.md).

---

## Running from the command line

```bash
./scripts/build/macos/run-macos-zh.sh -win
```

Or use the generated wrapper in the deploy directory:

```bash
~/GeneralsX/GeneralsZH/run.sh -win -noshellmap
```

Legacy fallback path also works:

```bash
~/GeneralsX/GeneralsMD/run.sh -win -noshellmap
```

Common flags:

| Flag | Effect |
|------|--------|
| `-win` | Windowed mode (recommended for debugging) |
| `-fullscreen` | Fullscreen mode |
| `-noshellmap` | Skip the animated main menu shell map |
| `-xres 1280 -yres 720` | Set resolution |

---

## macOS window and cursor hotkeys

| Keys | Effect |
|------|--------|
| `Ctrl+Cmd+F` | Toggle fullscreen (also the green zoom button) |
| `Cmd+G` | Release the cursor to the desktop, or take it back |

Both are macOS-only and neither is persisted: the launch window mode still comes from `-fullscreen`
/ `-win` or `Windowed` in `Options.ini`.

Fullscreen is macOS's own transition — the window is `SDL_WINDOW_RESIZABLE` and SDL3 uses native
fullscreen Spaces — so the engine only reacts to it. On
`SDL_EVENT_WINDOW_ENTER_FULLSCREEN` / `LEAVE_FULLSCREEN` it re-derives the render resolution for the
new window size at the current `GXRenderScalePercent` and updates the engine's windowed flag. Without
that the render resolution stayed at its windowed value while DXVK's swapchain grew to the panel, and
the pillarbox stretched the difference.

`Cmd+G` matters most in fullscreen, where SDL never posts `MOUSE_LEAVE` — there is nowhere to leave
to — so before this the only way to free the grabbed cursor was to switch away from the game. It is
recorded as a `CursorCaptureBlockReason`, not a bare `releaseCapture()`, so a focus or mode change
does not silently take the cursor back. `Cmd` is not a modifier the game uses, and the key is consumed
before the keyboard device sees it, so the retail key map is unaffected.

While the cursor is released in fullscreen the macOS menu bar is reachable by moving to the top of the
screen, and it hides again when the cursor is recaptured. This needs the window to be in
*non-exclusive* fullscreen: SDL marks fullscreen exclusive whenever a fullscreen display mode is set,
and the Cocoa backend then requests `NSApplicationPresentationHideMenuBar`, which is a hard hide that
hovering cannot reveal. The window therefore clears its fullscreen mode before the transition, and
`SDL_HINT_VIDEO_MAC_FULLSCREEN_MENU_VISIBILITY` is toggled with the cursor state. The hint's `auto`
value does not help here — it means visible only when fullscreen was entered from the title-bar button,
and the engine can enter it programmatically at launch.

The window's maximum size is set from the **full display bounds**, never the usable area. SDL passes a
maximum to Cocoa as `-setContentMaxSize:`, and AppKit applies it to fullscreen content too, so a lower
ceiling letterboxes the fullscreen picture. See Troubleshooting below.

---

## DXVK macOS Source Model

DXVK for macOS is consumed from the project fork as a **pinned commit** configured in
`cmake/dx8.cmake` (`DXVK_REMOTE_REF`).

- No local `PATCH_COMMAND` is executed in the current workflow.
- macOS fixes are expected to exist in the fork commit itself.
- For local DXVK development, use `-DSAGE_DXVK_USE_LOCAL_FORK=ON`.

---

## Troubleshooting

### Black bars above and below the picture in fullscreen

The window's maximum size is clamping the fullscreen drawable. Two log lines identify it:

```
INFO: window ceiling set to WxH points (display bounds)
INFO: entered fullscreen: ... drawable WxH, window WxH points, max WxH
```

If `drawable` is short of the panel's pixel size while `max` is non-zero, the ceiling is the cause, and
the first line says what set it. The ceiling must be `SDL_GetDisplayBounds`, not the usable bounds and
not usable-minus-title-bar — SDL hands it to Cocoa as `-setContentMaxSize:`, which caps fullscreen
content as well as the window's. Clearing the ceiling when fullscreen is entered does **not** fix it:
`ENTER_FULLSCREEN` is posted after Cocoa has already sized the frame, and a native toggle gives no
earlier hook.

### "Vulkan SDK not found"

```
ERROR: Vulkan SDK not found at ~/VulkanSDK/
```

Install from <https://vulkan.lunarg.com/sdk/home#mac>. The SDK must be in
`~/VulkanSDK/<version>/macOS/lib/libvulkan.dylib`.

### "meson: command not found"

```bash
brew install meson
```

### DXVK Meson build fails with linker error

If you see `--version-script` linker errors, the DXVK source being built likely
does not include the darwin linker guard fix in its commit history.
Clean the DXVK build cache and reconfigure:

```bash
rm -rf build/macos-vulkan/_deps/dxvk-src-fbraz3 build/macos-vulkan/_deps/dxvk-build-macos
cmake --preset macos-vulkan
```

### `VK_ERROR_INCOMPATIBLE_DRIVER` in logs

This is addressed by the portability-enumeration fix included in the pinned fork
commit. If you see it:

1. Ensure the Vulkan SDK is installed via LunarG installer (not Homebrew)
2. Ensure `scripts/build/macos/deploy-macos-zh.sh` was run (MoltenVK ICD JSON must be present)
3. Verify `VK_ICD_FILENAMES` points to the correct JSON in the runtime dir

### `VK_ERROR_FEATURE_NOT_PRESENT` — robustBufferAccess2 / nullDescriptor

```
[mvk-error] VK_ERROR_FEATURE_NOT_PRESENT: vkCreateDevice(): Requested physical
device feature specified by the 1st flag in VkPhysicalDeviceRobustness2FeaturesKHR
is not available on this device.
```

This is addressed in the pinned fork commit. If you see this, the DXVK dylib in
the game directory is stale or from a different DXVK source revision. Rebuild and
redeploy:

```bash
./scripts/build/macos/build-macos-zh.sh --build-only
./scripts/build/macos/deploy-macos-zh.sh
```

### Game crashes at startup (SIGSEGV)

Run with verbose MoltenVK output:

```bash
cd ~/GeneralsX/GeneralsZH
VK_ICD_FILENAMES=./MoltenVK_icd.json MVK_CONFIG_LOG_LEVEL=4 ./GeneralsXZH -win
```

### "Feature not present" Vulkan validation error

The pinned DXVK commit masks core features against what the physical device
actually supports.
If you still see this, MoltenVK may need an update. Re-running
`scripts/build/macos/deploy-macos-zh.sh` after updating the Vulkan SDK copies the
new `libMoltenVK.dylib` to the runtime dir.

---

## Current Status

| Feature | Status |
|---------|--------|
| CMake configure | Working |
| DXVK compile via Meson | Working (fork-pinned source model) |
| GeneralsXZH binary | Builds successfully |
| Vulkan device init | Working (MoltenVK -> Metal) |
| 3D rendering | Under active testing |
| Audio (OpenAL) | In progress (Phase 2) |
| Video (FFmpeg/Bink replacement) | In progress (Phase 3 planning/spike pending) |

---

## Related Scripts

| Script | Purpose |
|--------|---------|
| `scripts/build/macos/build-macos-zh.sh` | Configure + build `GeneralsXZH` |
| `scripts/build/macos/deploy-macos-zh.sh` | Deploy binary + Vulkan runtime to game dir |
| `scripts/build/macos/run-macos-zh.sh` | Launch with correct environment |
| `cmake/dx8.cmake` | DXVK ExternalProject build (pinned fork commit) |
| `cmake/dxvk-macos-patches.py` | Deprecated legacy helper (not used by current build flow) |
| `CMakePresets.json` (`macos-vulkan`) | Build preset (arm64, MoltenVK, SDL3, OpenAL, ffmpeg) |

---

*See the [Dev Blog](../../DEV_BLOG/) for detailed session-by-session technical notes.*
