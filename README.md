# CNC Generals for macOS

[中文](README.md) | [English](README.en.md)

<img width="500" height="281" alt="《零点行动》在 Apple 平台运行" src="https://github.com/user-attachments/assets/aeaf6692-36e6-40c8-b9f8-8066d014ec4b" />

> **上游说明：** 本仓库不是从零开始的独立移植。Apple 平台原生移植的直接基础来自 [`ammaarreshi/Generals-Mac-iOS-iPad`](https://github.com/ammaarreshi/Generals-Mac-iOS-iPad)；**CNC Generals for macOS** 只代表在该上游基础上，针对维护者个人使用情境所做的增强、修复和打包工作。上游的完整项目介绍与移植历程请直接阅读其 README。

本项目让《命令与征服：将军——零点行动》在 Apple Silicon Mac 上原生运行。它不是 Windows 模拟器：游戏引擎直接编译为 ARM64，原有 DirectX 8 渲染经过 DXVK、Vulkan 和 MoltenVK 转换到 Metal。本 fork 根据 Apple Silicon Mac、中文游戏资源、外接盘安装和日常游玩中实际遇到的问题，加入了一批 Bug 修复、兼容性改进和少量个人游玩体验调整。macOS 是当前唯一继续推进的目标平台；iOS / iPadOS 代码保留在仓库中，但本 fork 暂停维护和验证，不据此判断直接上游的实现状态。

> 本仓库及其 GitHub Releases 不包含《将军》或《零点行动》的商业游戏资源。你必须自行拥有并提供合法的 Windows 版游戏文件。分支中提交的 `.big` 文件仅为本项目实际使用的社区模组及本地兼容性覆盖文件，不包含游戏本体资源。

## 发行分支

| 分支 | 用途 | 内容边界 |
|---|---|---|
| [`main`](https://github.com/Log1037/CNC-Generals-for-macOS/tree/main) | 原版《零点行动》与通用改进 | macOS 引擎修复、界面缩放、Control Bar 布局、快捷栏和通用模组覆盖 |
| [`progen`](https://github.com/Log1037/CNC-Generals-for-macOS/tree/progen) | ProGen 26 版本 | 继承 `main`，再加入 ProGen 专属源码、Tomahawk Storm、激光卡曼奇修复和 ProGen 模组包 |

`progen` 是建立在 `main` 之上的增量分支，二者不会混入商业游戏本体。模组范围、文件边界和致谢见[模组范围与致谢](docs/MOD_SCOPE_AND_CREDITS.zh-CN.md)。

## 这个分支增加了什么

- 改进 Retina、HiDPI、窗口化、macOS 原生全屏和安全退出行为。
- 将画面渲染帧率与游戏逻辑速度分离；公开默认值为 60 FPS、1.0 倍游戏速度。
- 增加游戏内“画面与速度设置”面板，集中管理分辨率、渲染比例、速度、镜头和本地单机辅助功能。
- 改进中文字体回退、字形选择、字号和界面可读性。
- 修复视频、过场、缩放特效、离屏渲染和远景单位材质等实际游玩问题。
- 改进 SDL 输入、鼠标捕获、菜单稳定性和 macOS 窗口切换体验。
- 提供可移植的资源路径识别和 `.app` 打包流程，适合把游戏放在外接盘。

完整记录见[中文版修改日志](docs/WORKDIR/reports/MACOS_LOCAL_FORK_CHANGELOG.md)。

## 平台状态

| 平台 | 状态 | 说明 |
|---|---|---|
| Apple Silicon macOS | 主要使用平台 | 支持本地构建、命令行运行和双击 `.app` |
| iOS / iPadOS | 本 fork 已暂停 | 保留上游及既有代码，但不再推进、发布或验证；不据此判断直接上游版本 |
| Linux | 共享引擎仍保留 | 本分支的 macOS 修改尚需更完整的跨平台回归测试 |

## 快速开始

### 1. 准备游戏文件

需要同时准备完整的原版《将军》和《零点行动》目录：

```text
GeneralsX Runtime/
├── Generals/
│   └── INI.big
└── GeneralsZH/
    └── INIZH.big
```

游戏文件可以来自 Steam、EA App、光盘版、The First Decade、The Ultimate Collection 或其他合法拥有的 Windows 安装。购买平台并不重要，完整资源才重要。

如何从 Windows 找到并复制 Steam、EA App、光盘版或典藏版资源，请看 [macOS 二次修改版上手指南](docs/HOWTO/MACOS_LOCAL_FORK_QUICK_START.md)。

### 2. 准备构建环境

```bash
xcode-select --install
brew install cmake ninja meson pkgconf python vcpkg ffmpeg glm
export VCPKG_ROOT="$(brew --prefix vcpkg)"
export VULKAN_SDK="$HOME/VulkanSDK/<version>/macOS"
```

其中 Vulkan SDK 需要从 LunarG 安装，不能只用 Homebrew 的 Vulkan headers 代替。完整依赖说明见 [macOS 构建指南](docs/BUILD/MACOS.md)。

### 3. 克隆、构建和部署

```bash
git clone https://github.com/Log1037/CNC-Generals-for-macOS.git
cd CNC-Generals-for-macOS

export GX_RUNTIME_ROOT="$HOME/GeneralsX Runtime"
./scripts/build/macos/build-macos-zh.sh
./scripts/build/macos/deploy-macos-zh.sh
```

### 4. 打包双击 App

```bash
./scripts/build/macos/package-macos-zh-app.sh \
  --game-dir "$GX_RUNTIME_ROOT/GeneralsZH" \
  --generals-dir "$GX_RUNTIME_ROOT/Generals" \
  --runtime "$GX_RUNTIME_ROOT/GeneralsZH" \
  --output "$HOME/Applications/将军：零点行动.app"
```

启动器支持：

- 自动识别常见位置和上次成功使用的位置；
- 直接选择 `INIZH.big`、`INI.big`、游戏目录或共同父目录；
- 从 Finder 拖入文件或目录；
- 在原生选择窗口中按 `⇧⌘G` 手动输入路径；
- 按住 Option 打开 App，重新选择游戏资源。

## 常用控制

| 快捷键 | 功能 |
|---|---|
| `Ctrl+G` | 打开或关闭画面、速度与本地单机设置面板 |
| `Ctrl+[` / `Ctrl+]` | 调整渲染帧率 |
| `Shift+Ctrl+[` / `Shift+Ctrl+]` | 调整游戏速度 |
| `Cmd+G` | 释放或重新捕获鼠标 |
| `Ctrl+Cmd+F` | 切换 macOS 原生全屏 |
| `Alt+N` | 本地单机模式增加 10,000 资金 |

本地单机辅助功能只面向离线游戏，不用于联机对战。

## 随便说两句

这首先是一个按自己需要慢慢修改的个人 fork。因为其中一些修复和功能或许也能帮到别人，所以把源码放在这里；它不算成熟发行版，也没有固定的维护计划。

遇到问题、想到什么功能，或者自己改出了一点东西，都可以直接开 [Issue](https://github.com/Log1037/CNC-Generals-for-macOS/issues/new/choose) 或 Pull Request。能顺手写明系统版本、分辨率和具体现象就更好了，但不用按正式项目提工单。唯一需要注意的是不要上传商业游戏资源、包含资源的 App 或私人签名资料。

想一起改代码的话，可以再看一眼这份简短的[参与说明](CONTRIBUTING.md)。

## 文档

- [macOS 上手指南](docs/HOWTO/MACOS_LOCAL_FORK_QUICK_START.md)
- [macOS 构建指南](docs/BUILD/MACOS.md)
- [二次修改工程日志](docs/WORKDIR/reports/MACOS_LOCAL_FORK_CHANGELOG.md)
- [模组范围与致谢](docs/MOD_SCOPE_AND_CREDITS.zh-CN.md)
- [修改与来源声明](NOTICE.md)
- [移植工程手册](docs/port/PORTING_PLAYBOOK.md)
- [移植方法总结](docs/port/PORTING_PATTERNS.md)

## 来源与致谢

本项目建立在以下工作之上：

- Electronic Arts 发布的 [GPLv3 游戏引擎源码](https://github.com/electronicarts/CnC_Generals_Zero_Hour)
- [TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode)
- [Fighter19/CnC_Generals_Zero_Hour](https://github.com/Fighter19/CnC_Generals_Zero_Hour)
- [fbraz3/GeneralsX](https://github.com/fbraz3/GeneralsX)
- 直接上游 [ammaarreshi/Generals-Mac-iOS-iPad](https://github.com/ammaarreshi/Generals-Mac-iOS-iPad)
- ProGen 模组作者与维护者
- GenTool，以及本地配置中使用的 Control Bar Pro 内容
- DXVK、MoltenVK、SDL3、OpenAL Soft、FFmpeg、Fontconfig、FreeType 等开源组件

详细继承关系和修改边界见 [NOTICE.md](NOTICE.md)。

## 许可证与免责声明

源码按照仓库中的 [GPLv3 许可证及 EA 附加条款](LICENSE.md)发布。第三方组件和模组文件继续适用各自的许可证、声明与权利归属。

本项目不是 Electronic Arts、Westwood、EA Pacific 或任何上游社区项目发布的官方版本。游戏名称、剧情、美术、音频、地图和其他商业资源的权利属于其各自权利人。
