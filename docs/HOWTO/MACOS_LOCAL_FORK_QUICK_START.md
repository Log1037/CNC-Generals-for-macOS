# macOS 二次修改版上手指南

[中文](MACOS_LOCAL_FORK_QUICK_START.md) | [English](MACOS_LOCAL_FORK_QUICK_START.en.md)

本指南适用于 Apple Silicon Mac 上的《命令与征服：将军——零点行动》原生版个人需求分支，涵盖从 Windows 取得自己合法持有的游戏文件、准备目录、构建和部署引擎，以及打包可双击 `.app` 的完整流程。

## 重要说明

- 本仓库只包含开源引擎和构建脚本，不包含商业游戏资源。
- 你必须拥有合法的《将军》和《零点行动》副本。
- 不要把 `.big`、语音、视频、地图、私人字体或其他商业资源提交到 GitHub。
- 《零点行动》仍会使用原版《将军》的部分资源，因此应同时准备两个完整目录。
- 第一次测试建议使用干净的官方资源。确认能运行后，再逐个加入非官方修复包、地图或 MOD。

## 1. 准备运行目录

推荐结构如下：

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

如果希望放在外接盘，可以自行指定运行根目录：

```bash
export GX_RUNTIME_ROOT="/Volumes/My Games/GeneralsX Runtime"
mkdir -p "$GX_RUNTIME_ROOT/Generals" "$GX_RUNTIME_ROOT/GeneralsZH"
```

路径可以包含空格或中文，但在终端命令中必须使用双引号。希望每次打开终端都生效时，可把 `export GX_RUNTIME_ROOT=...` 加入 `~/.zshrc`。

## 2. 从 Windows 找到游戏本体

Steam、EA App、光盘版、The First Decade、The Ultimate Collection 或其他合法零售版都可以作为来源。关键不是购买平台，而是能取得完整的 Windows 游戏目录。

### 原版《将军》

在 Windows 文件资源管理器中搜索 `INI.big`。正确的原版根目录通常还包含：

- `W3D.big`
- `Window.big`
- `Textures.big`
- `Audio.big`
- `Generals.exe` 或 `Generals.dat`

把该目录的全部内容复制到 Mac 的 `Generals/`。不要只复制 `INI.big`，因为语音、纹理、模型、地图和界面位于不同归档中。

### 《零点行动》

搜索 `INIZH.big`。正确的资料片根目录通常还包含：

- `W3DZH.big`
- `WindowZH.big`
- `TexturesZH.big`
- `AudioZH.big`
- `MapsZH.big`
- `Generals.exe`、`Generals.dat` 或资料片启动程序

把该目录的全部内容复制到 Mac 的 `GeneralsZH/`。

### 从常见启动器定位

- Steam：在游戏库中右键游戏，选择“管理 → 浏览本地文件”。
- EA App：从“已安装的游戏”打开游戏页面，查看管理或安装位置；如果没有直接打开目录的按钮，可在安装盘搜索 `INIZH.big`。
- 光盘版、十周年纪念版、终极典藏版或手工安装版：不要依赖固定的 `Program Files` 路径，直接搜索 `INI.big` 和 `INIZH.big` 更可靠。

Windows 的 `.exe` 和 `.dll` 不会由原生 macOS 引擎执行，可以保留，也可以在确认资源完整后删除。必须保留 `.big`、`.dat`、地图、视频、语音和确实需要的松散 `Data/` 覆盖文件。

## 3. 把资源传到 Mac

可以使用：

- exFAT 移动硬盘；
- 局域网文件共享；
- 已确认完整下载的个人云盘目录。

复制整个游戏目录。不要使用仍是“按需下载”占位符的云端文件。

复制后检查两个标记文件：

```bash
export GX_RUNTIME_ROOT="${GX_RUNTIME_ROOT:-$HOME/GeneralsX}"

test -f "$GX_RUNTIME_ROOT/Generals/INI.big" \
  && echo "原版《将军》资源：正常" \
  || echo "原版《将军》资源：缺失"

test -f "$GX_RUNTIME_ROOT/GeneralsZH/INIZH.big" \
  && echo "《零点行动》资源：正常" \
  || echo "《零点行动》资源：缺失"
```

查看顶层归档：

```bash
find "$GX_RUNTIME_ROOT/Generals" -maxdepth 1 -iname '*.big' -print | sort
find "$GX_RUNTIME_ROOT/GeneralsZH" -maxdepth 1 -iname '*.big' -print | sort
```

不要把运行目录放进 Git 仓库。

## 4. 准备 macOS 构建环境

需要 Apple Silicon Mac、Xcode Command Line Tools、CMake、Ninja、Meson、Python、vcpkg、FFmpeg，以及完整的 LunarG Vulkan SDK。

```bash
xcode-select --install
brew install cmake ninja meson pkgconf python vcpkg ffmpeg glm
export VCPKG_ROOT="$(brew --prefix vcpkg)"
```

从 <https://vulkan.lunarg.com/sdk/home#mac> 安装 LunarG Vulkan SDK，并设置：

```bash
export VULKAN_SDK="$HOME/VulkanSDK/<version>/macOS"
```

当前构建脚本会检查 SDK 中的 `libvulkan.dylib` 和 `glslangValidator`，因此只安装 Homebrew Vulkan headers 不够。

第一次配置会下载和编译依赖，耗时明显长于后续增量构建。

## 5. 获取源码、构建和部署

```bash
git clone https://github.com/Log1037/CNC-Generals-for-iOS-macOS.git
cd CNC-Generals-for-iOS-macOS

export GX_RUNTIME_ROOT="${GX_RUNTIME_ROOT:-$HOME/GeneralsX}"

./scripts/build/macos/build-macos-zh.sh
./scripts/build/macos/deploy-macos-zh.sh
```

部署脚本会把引擎和运行库放到 `GeneralsZH/`，但不会删除或替换商业资源。主要部署内容包括：

- `GeneralsXZH`；
- SDL3、OpenAL、DXVK、Vulkan、MoltenVK 和 GameSpy 运行库；
- `run.sh`、`dxvk.conf`、Fontconfig 配置和开源中文字体回退。

默认情况下，部署完成后还会刷新可双击 App。只想部署命令行版本时：

```bash
GX_SKIP_APP_BUNDLE=1 ./scripts/build/macos/deploy-macos-zh.sh
```

## 6. 先测试命令行版本

```bash
GX_RUNTIME_ROOT="$GX_RUNTIME_ROOT" ./scripts/build/macos/run-macos-zh.sh -win
```

也可以进入运行目录：

```bash
cd "$GX_RUNTIME_ROOT/GeneralsZH"
./run.sh -win
```

建议先验证窗口化模式，再测试全屏和 App 打包，这样更容易区分资源、渲染和 macOS 窗口问题。

## 7. 打包可双击 App

```bash
./scripts/build/macos/package-macos-zh-app.sh \
  --game-dir "$GX_RUNTIME_ROOT/GeneralsZH" \
  --generals-dir "$GX_RUNTIME_ROOT/Generals" \
  --runtime "$GX_RUNTIME_ROOT/GeneralsZH" \
  --output "$HOME/Applications/将军：零点行动.app"
```

常用参数：

| 参数 | 用途 |
|---|---|
| `--game-dir <dir>` | 包含 `INIZH.big` 的《零点行动》目录 |
| `--generals-dir <dir>` | 包含 `INI.big` 的原版《将军》目录 |
| `--runtime <dir>` | 包含已部署 `.dylib` 的运行目录 |
| `--engine <file>` | 指定 `GeneralsXZH` 引擎程序 |
| `--icon <file>` | 指定 PNG 或 ICNS 图标 |
| `--output <app>` | 指定输出 App 路径 |
| `--sign <identity>` | 指定签名身份；默认 `-` 为本地 ad-hoc 签名 |

验证 App：

```bash
APP="$HOME/Applications/将军：零点行动.app"

codesign --verify --deep --strict "$APP"
"$APP/Contents/MacOS/GeneralsXLauncher" --check
```

`--check` 只检查资源目录、标记文件、Frameworks 和引擎程序，不会启动游戏。App 默认没有 Apple 公证，因此 `spctl` 显示 `rejected` 不等同于签名损坏。

## 8. 第一次打开和路径选择

1. 确认保存游戏资源的外接盘已经挂载。
2. 双击 App。
3. 启动器会依次检查环境变量、上次保存的位置、打包时的位置和标准目录。
4. 自动识别失败时，会分别要求选择《零点行动》和原版《将军》资源。

选择窗口允许以下输入：

- 直接选择 `INIZH.big` 或 `INI.big`；
- 选择含有标记文件的游戏目录；
- 选择同时包含 `Generals/` 和 `GeneralsZH/` 的共同父目录；
- 从 Finder 把文件或目录拖入选择窗口；
- 按 `⇧⌘G` 手动输入完整路径。

识别成功后会记住选择。需要更换位置时，按住 Option 打开 App，或从终端运行：

```bash
"将军：零点行动.app/Contents/MacOS/GeneralsXLauncher" --choose-game-dir
```

日志位于：

```text
~/Library/Logs/GeneralsX/ZeroHour.log
```

## 9. 常用控制

| 快捷键 | 功能 |
|---|---|
| `Ctrl+G` | 打开或关闭画面、速度与本地单机设置面板 |
| `Ctrl+[` / `Ctrl+]` | 调整渲染帧率 |
| `Shift+Ctrl+[` / `Shift+Ctrl+]` | 调整游戏速度 |
| `Cmd+G` | 释放或重新捕获鼠标 |
| `Ctrl+Cmd+F` | 切换 macOS 原生全屏 |
| `Alt+N` | 本地单机模式增加 10,000 资金 |

公开默认值为 60 FPS 渲染、1.0 倍游戏速度。可以在面板中保存 2.2 倍或其他个人速度。本地辅助功能只面向离线单机游戏。

## 10. 常见问题

### 提示找不到 `INIZH.big`

选择的目录层级不正确，或者只复制了某个子目录。重新选择 `INIZH.big` 本身、《零点行动》根目录或同时包含两个游戏目录的共同父目录。

### 原版战役资源、语音或过场缺失

确认 `Generals/` 和 `GeneralsZH/` 都是完整复制，而不是只挑选了几个 `.big`。两者不必强制放在一起，但必须分别能被启动器识别。

### 加入 Windows 修复包后崩溃

先把新增文件移到单独备份目录，用干净资源验证。Windows 修改器的 `.exe`、注入 DLL、注册表修改和 DirectX wrapper 不能直接用于原生 macOS 引擎；纯 `.big`、地图和松散 `Data/` 覆盖也可能依赖特定 Windows 补丁版本。

### App 检查通过，但 Finder 无法打开

先运行 `codesign --verify --deep --strict`，再查看 `ZeroHour.log`。同时检查外接盘是否挂载，以及 macOS 是否允许 App 访问可移动宗卷。

### 需要彻底重新选择资源

按住 Option 打开 App，或使用 `--choose-game-dir`。这会覆盖上次记住的两个有效位置。

## 相关文档

- [macOS 构建指南](../BUILD/MACOS.md)
- [二次修改工程日志](../WORKDIR/reports/MACOS_LOCAL_FORK_CHANGELOG.md)
- [通用游戏文件获取说明](GETTING_THE_GAME_FILES.md)
- [修改与来源声明](../../NOTICE.md)
- [许可证](../../LICENSE.md)
