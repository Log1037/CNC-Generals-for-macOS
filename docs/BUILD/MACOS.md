# GeneralsX macOS 构建指南

[中文](MACOS.md) | [English](MACOS.en.md)

本文面向需要从源码构建 `GeneralsXZH` 的开发者和高级用户。只想复制正版游戏资源并打包 App 的用户，也可以先阅读[上手指南](../HOWTO/MACOS_LOCAL_FORK_QUICK_START.md)。

## 系统要求

- Apple Silicon Mac；
- macOS 13 或更新版本；
- Xcode Command Line Tools；
- 约 10 GB 可用空间；
- 自行合法拥有的原版《将军》和《零点行动》资源。

## 1. 安装构建工具

```bash
xcode-select --install
brew install cmake ninja meson python pkgconf ffmpeg glm vcpkg
export VCPKG_ROOT="$(brew --prefix vcpkg)"
```

构建脚本会自动尝试以下 vcpkg 位置：

- `$VCPKG_ROOT`；
- 仓库内的 `vcpkg/`；
- `~/vcpkg`；
- 常见 Homebrew 和 `/opt/vcpkg` 位置。

## 2. 安装 LunarG Vulkan SDK

当前 `build-macos-zh.sh` 需要完整的 LunarG macOS Vulkan SDK，包括 `libvulkan.dylib` 和 `glslangValidator`。仅安装 Homebrew 的 Vulkan headers 不够。

1. 打开 <https://vulkan.lunarg.com/sdk/home#mac>。
2. 下载并安装 macOS SDK。
3. 设置 SDK 路径：

```bash
export VULKAN_SDK="$HOME/VulkanSDK/<version>/macOS"
```

检查：

```bash
test -f "$VULKAN_SDK/lib/libvulkan.dylib"
test -f "$VULKAN_SDK/lib/libMoltenVK.dylib"
test -x "$VULKAN_SDK/bin/glslangValidator"
```

构建脚本也会读取 `$VULKAN_SDK_ROOT`，并自动扫描 `~/VulkanSDK/*/macOS`。

## 3. 准备游戏资源

推荐把原版和资料片分别放在：

```text
${GX_RUNTIME_ROOT:-$HOME/GeneralsX}/
├── Generals/
│   └── INI.big
└── GeneralsZH/
    └── INIZH.big
```

外接盘示例：

```bash
export GX_RUNTIME_ROOT="/Volumes/My Games/GeneralsX Runtime"
```

资源必须来自用户自己合法拥有的 Windows 副本。本仓库不下载或分发商业资源。如何从 Windows 查找和复制两个完整目录，请看[上手指南](../HOWTO/MACOS_LOCAL_FORK_QUICK_START.md)。

## 4. 克隆与构建

```bash
git clone https://github.com/Log1037/CNC-Generals-for-iOS-macOS.git
cd CNC-Generals-for-iOS-macOS

./scripts/build/macos/build-macos-zh.sh
```

脚本会：

1. 检查 CMake、Ninja、Meson、Python、vcpkg 和 Vulkan SDK；
2. 使用 `macos-vulkan` preset 配置 CMake；
3. 获取固定版本的 DXVK fork 并通过 Meson 构建；
4. 构建 `z_generals` 目标；
5. 把完整日志写入 `logs/build_zh_macos-vulkan.log`。

第一次构建需要下载和编译依赖，后续会复用缓存。

已经配置过，只想增量构建时：

```bash
./scripts/build/macos/build-macos-zh.sh --build-only
```

等价的手工命令：

```bash
cmake --preset macos-vulkan
cmake --build build/macos-vulkan --target z_generals \
  -j"$(( ($(sysctl -n hw.logicalcpu) + 1) / 2 ))"
```

引擎程序位于：

```text
build/macos-vulkan/GeneralsMD/GeneralsXZH
```

## 5. 部署运行时

```bash
export GX_RUNTIME_ROOT="${GX_RUNTIME_ROOT:-$HOME/GeneralsX}"
./scripts/build/macos/deploy-macos-zh.sh
```

部署脚本会把引擎、DXVK、MoltenVK、Vulkan loader、SDL3、OpenAL、FFmpeg 相关运行库、GameSpy 兼容库、Fontconfig 配置和运行脚本放入运行目录。它不会删除或替换用户的商业资源。

默认还会刷新本地 App。跳过 App 打包：

```bash
GX_SKIP_APP_BUNDLE=1 ./scripts/build/macos/deploy-macos-zh.sh
```

## 6. 从命令行运行

```bash
./scripts/build/macos/run-macos-zh.sh -win
```

或者：

```bash
cd "$GX_RUNTIME_ROOT/GeneralsZH"
./run.sh -win
```

常用参数：

| 参数 | 作用 |
|---|---|
| `-win` | 窗口化运行，推荐用于排错 |
| `-fullscreen` | 全屏运行 |
| `-noshellmap` | 跳过动态主菜单背景 |
| `-xres 1280 -yres 720` | 指定逻辑分辨率 |

## 7. 打包本地 App

```bash
./scripts/build/macos/package-macos-zh-app.sh \
  --game-dir "$GX_RUNTIME_ROOT/GeneralsZH" \
  --generals-dir "$GX_RUNTIME_ROOT/Generals" \
  --runtime "$GX_RUNTIME_ROOT/GeneralsZH" \
  --output "$HOME/Applications/将军：零点行动.app"
```

`GeneralsXLauncher` 是 App 的真正入口。它负责：

- 识别《零点行动》和原版《将军》资源；
- 设置引擎工作目录和资源环境变量；
- 指向 App 内的 DXVK、Vulkan 和 MoltenVK；
- 读取已保存的渲染帧率和游戏速度；
- 把输出写入 `~/Library/Logs/GeneralsX/ZeroHour.log`；
- 在资源或运行库缺失时显示可读错误，而不是静默退出。

路径来源按以下优先级处理：

1. 环境变量；
2. 上次成功保存的位置；
3. 打包时写入的位置；
4. `$GX_RUNTIME_ROOT` 和标准目录；
5. 原生文件选择窗口。

选择器接受标记文件、游戏目录或共同父目录，并支持 Finder 拖入和 `⇧⌘G` 手动输入。

## 8. 验证 App

```bash
APP="$HOME/Applications/将军：零点行动.app"

plutil -lint "$APP/Contents/Info.plist"
codesign --verify --deep --strict "$APP"
"$APP/Contents/MacOS/GeneralsXLauncher" --check
```

`--check` 不启动游戏。默认使用 ad-hoc 签名，没有 Apple 公证，因此 `spctl` 的 `rejected` 结果不等同于 `codesign` 验证失败。

## 9. macOS 窗口快捷键

| 快捷键 | 功能 |
|---|---|
| `Ctrl+Cmd+F` | 切换 macOS 原生全屏 |
| `Cmd+G` | 释放或重新捕获鼠标 |

窗口模式仍由启动参数和 `Options.ini` 决定。这两个快捷键只改变当前运行状态，不写回启动模式。

## 10. DXVK 源码模型

macOS DXVK 默认使用 `cmake/dx8.cmake` 中固定的远程 fork 版本。普通构建不应直接修改 `build/_deps/`。

需要开发本地 DXVK fork 时：

```bash
cmake --preset macos-vulkan -DSAGE_DXVK_USE_LOCAL_FORK=ON
```

## 11. 常见问题

### 找不到 Vulkan SDK

确认 `$VULKAN_SDK/lib/libvulkan.dylib` 和 `$VULKAN_SDK/bin/glslangValidator` 存在。不要只安装 Homebrew 的 Vulkan headers。

### 找不到 vcpkg

```bash
brew install vcpkg
export VCPKG_ROOT="$(brew --prefix vcpkg)"
```

### DXVK Meson 构建仍使用旧缓存

```bash
rm -rf build/macos-vulkan/_deps/dxvk-src-fbraz3 \
       build/macos-vulkan/_deps/dxvk-build-macos
cmake --preset macos-vulkan
```

### 运行时提示 `VK_ERROR_INCOMPATIBLE_DRIVER`

重新部署，确认运行目录中有正确的 `libMoltenVK.dylib` 和 ICD JSON，并检查 `VK_ICD_FILENAMES`。

### App 找不到游戏资源

直接选择 `INIZH.big` 和 `INI.big`，或者按住 Option 打开 App 重新选择。不要只选择某个 `Data/` 子目录。

### 全屏出现黑边或窗口尺寸异常

先以 `-win` 运行，确认资源和渲染正常；再检查日志中的 drawable、window 和 display bounds。Retina 环境下必须区分 AppKit 点尺寸和实际 drawable 像素。

## 12. 当前验证边界

已经验证：

- `z_generals` 在本机完整构建；
- 部署、运行和打包脚本通过语法检查；
- 隔离 App 通过 plist、深度签名和启动器 `--check`；
- 直接目录、共同父目录和标记文件三种路径输入可以被识别。

仍需补充：

- 另一台干净 Apple Silicon Mac 的端到端构建；
- Linux、iOS 和原版《将军》的跨平台回归；
- 录像确定性和网络锁步检查。

## 相关脚本

| 文件 | 用途 |
|---|---|
| `scripts/build/macos/build-macos-zh.sh` | 配置并构建 `GeneralsXZH` |
| `scripts/build/macos/deploy-macos-zh.sh` | 部署引擎和运行库 |
| `scripts/build/macos/run-macos-zh.sh` | 使用正确环境运行游戏 |
| `scripts/build/macos/package-macos-zh-app.sh` | 打包本地双击 App |
| `packaging/macos/GeneralsXLauncher.m` | App 启动器 |
| `cmake/dx8.cmake` | DXVK 获取与构建配置 |
