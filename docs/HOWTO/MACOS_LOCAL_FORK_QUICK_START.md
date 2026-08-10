# macOS 二次修改版上手指南 / macOS Customized Fork Quick Start

本指南适用于 Apple Silicon Mac 上的《命令与征服：将军——零点行动》原生版二次修改分支。它涵盖从 Windows 取得自己合法持有的游戏文件、准备目录、构建和部署引擎，以及打包可双击 `.app` 的完整流程。

This guide covers the customized native Zero Hour fork on Apple Silicon macOS: transferring a lawfully owned Windows installation, preparing the runtime layout, building and deploying the engine, and packaging a double-clickable `.app`.

## 重要说明 / Important notice

### 中文

- 本仓库只包含开源引擎和构建脚本，不包含商业游戏资源。
- 你必须拥有合法的《将军》和《零点行动》副本。
- 不要把 `.big`、语音、视频、地图或私人字体提交到 GitHub。
- 《零点行动》是资料片。本移植版运行中文《零点行动》时仍会使用原版《将军》的部分资源，因此建议同时准备两个完整目录。
- 第一次测试应使用干净的官方资源。确认能运行后，再逐个加入非官方修复包、地图或 MOD，便于定位兼容问题。

### English

- This repository contains only the open-source engine and build scripts. It does not include commercial game data.
- You must own lawful copies of both Generals and Zero Hour.
- Never commit `.big` archives, audio, video, maps, or private fonts to GitHub.
- Zero Hour is an expansion and this port may still load resources from the base Generals installation. Prepare both complete directories.
- Test a clean retail data set first. Add unofficial patches, maps, or mods one at a time after the base game works so compatibility problems remain diagnosable.

## 1. 最终目录结构 / Required directory layout

选择一个运行根目录。默认位置是：

Choose a runtime root. The default is:

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

如果希望放在外接盘，先在终端设置：

For an external disk, set the root before running any helper script:

```bash
export GX_RUNTIME_ROOT="/Volumes/My Games/GeneralsX Runtime"
mkdir -p "$GX_RUNTIME_ROOT/Generals" "$GX_RUNTIME_ROOT/GeneralsZH"
```

路径可以包含空格或中文，但在命令中必须始终使用双引号。为了每次打开终端后都有效，可把 `export GX_RUNTIME_ROOT=...` 加到 `~/.zshrc`。

Paths may contain spaces or Chinese characters, but they must always be quoted in shell commands. Add the export to `~/.zshrc` if it should persist across Terminal sessions.

## 2. 从 Windows 取得游戏本体 / Copy the game data from Windows

### 中文

Steam、EA App、光盘版、十周年纪念版或其他零售版都可以作为来源。关键不是购买平台，而是你已经在 Windows 上合法安装并能读取完整资源。

#### 找到原版《将军》目录

在 Windows 文件资源管理器中搜索 `INI.big`。正确的原版根目录通常还同时包含：

- `W3D.big`
- `Window.big`
- `Textures.big`
- `Audio.big`
- `Generals.exe` 或 `Generals.dat`

把这个目录中的全部内容复制到 Mac 的 `Generals/`。不要只复制 `INI.big`；语音、纹理、模型、地图和界面分别位于不同归档中。

#### 找到《零点行动》目录

搜索 `INIZH.big`。正确的资料片根目录通常还包含：

- `W3DZH.big`
- `WindowZH.big`
- `TexturesZH.big`
- `AudioZH.big`
- `MapsZH.big`
- `Generals.exe`、`Generals.dat` 或资料片启动程序

把这个目录中的全部内容复制到 Mac 的 `GeneralsZH/`。

#### 从不同启动器定位

- Steam：在游戏库中右键游戏，选择“管理 → 浏览本地文件”。Steam 官方帮助也使用这一入口定位安装目录。
- EA App：从“已安装的游戏”打开游戏页面并查看管理或安装位置；如果界面版本没有直接打开目录的按钮，可在安装盘搜索 `INIZH.big`。
- 光盘版、The First Decade、The Ultimate Collection 或手工安装版：不要依赖固定的 `Program Files` 路径，直接搜索上述两个标记文件最可靠。

Steam 常见根目录是 `C:\Program Files (x86)\Steam\steamapps\common\`，EA 版本则取决于安装时选择的位置。路径只是线索，`INI.big` 与 `INIZH.big` 才是判断目录是否正确的标记。

#### 传输到 Mac

可使用 exFAT 移动硬盘、局域网共享或可信的个人云盘。建议复制整个目录，不要让云盘“按需下载”留下只有占位符的文件。

Windows 的 `.exe` 和 `.dll` 不会由原生引擎执行，可以保留，也可以在确认资源完整后删除。必须保留 `.big`、`.dat`、地图、视频、语音以及你确实需要的松散 `Data/` 覆盖文件。

### English

Steam, EA App, disc releases, The First Decade, The Ultimate Collection, and other retail editions can all be used. The purchase platform is not the important part; what matters is that you lawfully own and can read a complete Windows installation.

#### Locate the base Generals directory

Search Windows for `INI.big`. The correct base-game root normally also contains `W3D.big`, `Window.big`, `Textures.big`, `Audio.big`, and a `Generals.exe` or `Generals.dat`. Copy the entire directory contents into the Mac `Generals/` folder. Do not copy only `INI.big`; audio, textures, models, maps, and UI data live in separate archives.

#### Locate the Zero Hour directory

Search for `INIZH.big`. The correct expansion root normally also contains `W3DZH.big`, `WindowZH.big`, `TexturesZH.big`, `AudioZH.big`, `MapsZH.big`, and a game executable or `Generals.dat`. Copy the entire directory contents into the Mac `GeneralsZH/` folder.

#### Find the folder from common launchers

- Steam: right-click the game in the Library and choose **Manage → Browse local files**.
- EA App: open the title under Installed games and inspect its management or install-location controls. If the current client does not expose the folder directly, search the selected install drive for `INIZH.big`.
- Disc, First Decade, Ultimate Collection, and manually installed editions: do not rely on a fixed `Program Files` path. Searching for the two marker files is more reliable.

Steam commonly stores games below `C:\Program Files (x86)\Steam\steamapps\common\`, while EA locations depend on the destination chosen during installation. Treat those paths as hints; `INI.big` and `INIZH.big` are the actual directory markers.

Transfer the complete folders through an exFAT drive, a network share, or trusted personal cloud storage. Avoid cloud placeholders that have not been downloaded locally. Native GeneralsX does not execute the Windows `.exe` or `.dll` files, so they may be kept or removed after verification. Keep all `.big`, `.dat`, map, video, audio, and required loose `Data/` override files.

## 3. 在 Mac 上检查资源 / Verify the copied assets

默认目录：

Default runtime root:

```bash
export GX_RUNTIME_ROOT="${GX_RUNTIME_ROOT:-$HOME/GeneralsX}"

test -f "$GX_RUNTIME_ROOT/Generals/INI.big" \
  && echo "Generals assets: OK" \
  || echo "Generals assets: MISSING"

test -f "$GX_RUNTIME_ROOT/GeneralsZH/INIZH.big" \
  && echo "Zero Hour assets: OK" \
  || echo "Zero Hour assets: MISSING"
```

快速查看根目录归档：

List the top-level archives:

```bash
find "$GX_RUNTIME_ROOT/Generals" -maxdepth 1 -iname '*.big' -print | sort
find "$GX_RUNTIME_ROOT/GeneralsZH" -maxdepth 1 -iname '*.big' -print | sort
```

不要把运行目录放进 Git。仓库的 `.gitignore` 已排除项目内的 `游戏文件/`，但放在其他位置的资源仍由使用者自行负责。

Do not add the runtime directory to Git. The repository ignores its own `游戏文件/` directory, but users remain responsible for assets stored elsewhere.

## 4. 准备 macOS 构建环境 / Prepare the macOS build environment

需要 Apple Silicon Mac、Xcode Command Line Tools、CMake、Ninja、Meson、Python、vcpkg，以及 Vulkan loader + MoltenVK。

An Apple Silicon Mac, Xcode Command Line Tools, CMake, Ninja, Meson, Python, vcpkg, and a Vulkan loader with MoltenVK are required.

```bash
xcode-select --install
brew install cmake ninja meson pkgconf python vcpkg vulkan-loader molten-vk
export VCPKG_ROOT="$(brew --prefix vcpkg)"
```

也可以安装 LunarG Vulkan SDK，并设置：

Alternatively install the LunarG Vulkan SDK and export:

```bash
export VULKAN_SDK="$HOME/VulkanSDK/<version>/macOS"
```

`build-macos-zh.sh` 会在真正编译前检查主要依赖。第一次配置需要下载并构建依赖，耗时明显长于后续增量构建。

`build-macos-zh.sh` validates the main dependencies before compiling. The first configure downloads and builds dependencies and is substantially slower than later incremental builds.

## 5. 获取源码、构建和部署 / Clone, build, and deploy

```bash
git clone https://github.com/Log1037/Generals-Mac-iOS-iPad.git GeneralsX
cd GeneralsX

export GX_RUNTIME_ROOT="${GX_RUNTIME_ROOT:-$HOME/GeneralsX}"

./scripts/build/macos/build-macos-zh.sh
./scripts/build/macos/deploy-macos-zh.sh
```

部署脚本会把以下内容复制到 `GeneralsZH/`，但不会删除或覆盖商业资源：

The deploy script places the following beside the Zero Hour assets without deleting or replacing the retail archives:

- `GeneralsXZH`
- SDL3, OpenAL, DXVK, Vulkan, MoltenVK and GameSpy runtime libraries
- `run.sh`, `dxvk.conf`, Fontconfig configuration, and the open Chinese font fallback

部署结束时默认还会刷新可双击 App。只想部署命令行版本时：

By default deployment also refreshes the double-clickable app. To deploy only the command-line runtime:

```bash
GX_SKIP_APP_BUNDLE=1 ./scripts/build/macos/deploy-macos-zh.sh
```

## 6. 先从命令行测试 / Test from the command line first

```bash
GX_RUNTIME_ROOT="$GX_RUNTIME_ROOT" ./scripts/build/macos/run-macos-zh.sh -win
```

或者：

Or:

```bash
cd "$GX_RUNTIME_ROOT/GeneralsZH"
./run.sh -win
```

窗口化测试成功后再打包和测试全屏，能更容易区分资源、图形和 macOS 窗口问题。

Verify windowed mode before packaging and fullscreen testing. This makes asset, renderer, and macOS window failures easier to distinguish.

## 7. 打包可双击 App / Package a double-clickable app

最简命令：

Simplest command:

```bash
./scripts/build/macos/package-macos-zh-app.sh \
  --game-dir "$GX_RUNTIME_ROOT/GeneralsZH" \
  --generals-dir "$GX_RUNTIME_ROOT/Generals" \
  --runtime "$GX_RUNTIME_ROOT/GeneralsZH"
```

默认输出在仓库的上一级目录，名称为 `将军：零点行动.app`。指定位置：

The default output is `将军：零点行动.app` beside the repository. To choose another destination:

```bash
./scripts/build/macos/package-macos-zh-app.sh \
  --game-dir "$GX_RUNTIME_ROOT/GeneralsZH" \
  --generals-dir "$GX_RUNTIME_ROOT/Generals" \
  --runtime "$GX_RUNTIME_ROOT/GeneralsZH" \
  --output "$HOME/Applications/将军：零点行动.app"
```

可选参数：

Optional arguments:

| 参数 / Option | 用途 / Purpose |
|---|---|
| `--game-dir <dir>` | 包含 `INIZH.big` 的用户资源目录 / User asset directory containing `INIZH.big` |
| `--generals-dir <dir>` | 包含 `INI.big` 的原版资源目录 / Base-game directory containing `INI.big` |
| `--runtime <dir>` | 包含已部署 `.dylib` 的目录 / Directory containing deployed `.dylib` files |
| `--engine <file>` | 指定 `GeneralsXZH` 二进制 / Select a specific engine binary |
| `--icon <file>` | 使用 PNG 或 ICNS 图标 / Use a PNG or ICNS icon |
| `--output <app>` | 指定输出 App / Select the output app path |
| `--sign <identity>` | 指定签名身份；默认 `-` 为本地 ad-hoc 签名 / Signing identity; `-` is local ad-hoc signing |

验证 App 结构和签名：

Verify the app structure and signature:

```bash
APP="$HOME/Applications/将军：零点行动.app"

codesign --verify --deep --strict "$APP"
"$APP/Contents/MacOS/GeneralsXLauncher" --check
```

`--check` 只检查资源目录、`INIZH.big`、Frameworks 和引擎程序，不会启动游戏。App 默认是本地 ad-hoc 签名，没有 Apple 公证；`spctl` 显示 `rejected` 不代表 `codesign --verify` 失败。

`--check` verifies the asset root, `INIZH.big`, Frameworks, and engine without launching the game. The app is ad-hoc signed and not Apple-notarized by default; an `spctl` rejection is not the same as a failed `codesign --verify`.

## 8. 第一次打开 / First launch

1. 确认保存资源的外接盘已经挂载。
2. 双击 App。
3. 启动器会先尝试环境变量、上次保存的位置、打包时的位置和标准目录。识别不到时会依次弹出《零点行动》和原版《将军》的原生选择窗口。
4. 可以选择标记文件本身、游戏文件夹或它们的上一层目录；也可以从 Finder 拖入，或在窗口中按 `⇧⌘G` 手动输入路径。选中后会自动寻找 `INIZH.big` / `INI.big` 并记住有效位置。
5. 若要主动重新选择两个目录，按住 Option 键打开 App，或从终端运行 `GeneralsXLauncher --choose-game-dir`。
6. 如果 macOS 阻止运行，在“系统设置 → 隐私与安全性”中确认打开，或在 Finder 中按住 Control 点击 App 后选择“打开”。
7. 如果 App 无法读取外接盘，在系统隐私设置中允许访问可移动宗卷。

1. Mount the external disk that holds the assets.
2. Double-click the app.
3. The launcher checks environment overrides, remembered locations, package-time paths, and standard directories. If discovery fails, native panels ask for Zero Hour and base Generals separately.
4. Select the marker file, its game directory, or their common parent. A folder can be dragged from Finder, and `⇧⌘G` opens manual path entry. The launcher searches for `INIZH.big` / `INI.big` and remembers valid selections.
5. Hold Option while opening the app, or run `GeneralsXLauncher --choose-game-dir`, to replace both remembered locations.
6. If macOS blocks it, confirm the launch under **System Settings → Privacy & Security**, or Control-click the app in Finder and choose **Open**.
7. If the app cannot read an external disk, allow removable-volume access in macOS privacy settings.

日志位置 / Log location:

```text
~/Library/Logs/GeneralsX/ZeroHour.log
```

## 9. 本分支的常用控制 / Fork-specific controls

| 快捷键 / Shortcut | 功能 / Action |
|---|---|
| `Ctrl+G` | 打开画面、速度与单机辅助面板 / Toggle the display, speed, and single-player helper panel |
| `Ctrl+[` / `Ctrl+]` | 调整渲染帧率 / Adjust render FPS |
| `Shift+Ctrl+[` / `Shift+Ctrl+]` | 调整游戏速度 / Adjust game speed |
| `Cmd+G` | 释放或重新捕获鼠标 / Release or recapture the cursor |
| `Alt+N` | 单机模式增加 10,000 资金 / Add 10,000 credits in local single-player |

公开默认值是 60 FPS 渲染、1.0× 游戏速度。你仍可在面板中保存 2.2× 或其他个人速度。

The public default is 60 FPS rendering at 1.0× game speed. A personal 2.2× or other speed can still be saved through the panel.

## 10. 常见问题 / Troubleshooting

### `INIZH.big` not found

`--game-dir` 指向了错误层级，或者只复制了某个子目录。它必须直接指向含有 `INIZH.big` 的资料片根目录。

`--game-dir` points at the wrong level or only a subdirectory was copied. It must directly name the Zero Hour root containing `INIZH.big`.

### 原版战役资源、语音或过场缺失 / Base resources, audio, or cinematics are missing

确认 `Generals/` 与 `GeneralsZH/` 是相邻目录，并且两个目录都是完整复制，不是只挑选了几个 `.big`。

Verify that `Generals/` and `GeneralsZH/` are siblings and that both are complete copies rather than a small selection of archives.

### 加入 Windows 修复包后崩溃 / Crash after adding a Windows patch or mod

先把新增文件移到单独备份目录，用干净资源验证。Windows 修改器中的 `.exe`、注入 DLL、注册表修改和 DirectX wrapper 不能直接用于原生 macOS 引擎；纯 `.big`、地图和松散 `Data/` 覆盖也可能依赖特定 Windows 补丁版本。

Move the added files to a backup directory and retest clean assets. Windows trainer executables, injected DLLs, registry changes, and DirectX wrappers cannot be used directly by the native macOS engine. Even pure `.big`, map, and loose `Data/` overrides may depend on a specific Windows patch level.

### App 能检查通过但 Finder 打不开 / Check passes but Finder launch fails

先运行 `codesign --verify --deep --strict`，再查看 `ZeroHour.log`。如果 App 在移动后仍写死旧资源路径，请用正确的 `--game-dir` 重新打包。

Run `codesign --verify --deep --strict` and then inspect `ZeroHour.log`. If a moved app still refers to an old asset path, rebuild it with the correct `--game-dir`.

## 相关文档 / Related documents

- [中英对照工程日志 / Bilingual engineering log](../WORKDIR/reports/MACOS_LOCAL_FORK_CHANGELOG.md)
- [通用资源获取说明 / General game-file guide](GETTING_THE_GAME_FILES.md)
- [macOS 构建细节 / macOS build details](../BUILD/MACOS.md)
- [许可证 / License](../../LICENSE.md)
- [修改与来源声明 / Modification and attribution notice](../../NOTICE.md)
