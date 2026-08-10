# CNC Generals for iOS/macOS：修改与来源声明

[中文](NOTICE.md) | [English](NOTICE.en.md)

**CNC Generals for iOS/macOS** 是本个人增强分支的公开名称，不表示本项目独立完成了 Apple 平台移植。本仓库是一个经过修改的 GPLv3 分支，不是 Electronic Arts、Westwood、EA Pacific 或任何上游社区项目发布的官方版本。

## 直接上游

- **[`ammaarreshi/Generals-Mac-iOS-iPad`](https://github.com/ammaarreshi/Generals-Mac-iOS-iPad)**
- 本次个人需求版从上游提交 `c5c8c4d3e757033d9ab464f6bd6e15e91e0e742f` 开始整理。

本仓库的 Apple 平台原生移植基础、iOS/macOS 基础设施和相应成果直接继承自该上游。直接上游的完整功能介绍、移植历程和原始说明请阅读其自己的 README；本仓库不复制那份 README，也不把上游成果重新表述为本分支原创，只说明后来新增和修改的内容。

## 更早的主要来源

- [Electronic Arts GPLv3 源码发布](https://github.com/electronicarts/CnC_Generals_Zero_Hour)
- [TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode)
- [Fighter19/CnC_Generals_Zero_Hour](https://github.com/Fighter19/CnC_Generals_Zero_Hour)
- [fbraz3/GeneralsX](https://github.com/fbraz3/GeneralsX)

## 本分支的修改范围

本分支在 2026 年根据维护者自己的 Apple Silicon macOS、中文游戏资源、外接盘安装和实际游玩需要进行了修改。新增内容主要包括：

- Retina、HiDPI、窗口化和 macOS 原生全屏处理；
- 独立的渲染帧率与游戏逻辑速度控制；
- 游戏内画面、速度、镜头和本地单机设置面板；
- 中文字体回退、字形、字号和界面可读性调整；
- 视频、过场、光照、缩放特效和离屏渲染修复；
- SDL 输入、鼠标捕获、菜单稳定性和安全退出修复；
- 游戏资源路径识别、外接盘运行和本地 `.app` 打包。

这不是直接上游作者发布的官方更新。本分支特有问题应提交到[本仓库的 Issues](https://github.com/Log1037/CNC-Generals-for-iOS-macOS/issues)。

## 游戏资源边界

本仓库不提供《命令与征服：将军》或《零点行动》的商业游戏资源。使用者必须自行拥有并提供合法副本。

游戏名称、剧情、美术、音频、地图、视频和其他商业资源的权利属于其各自权利人。不得把 `.big`、语音、视频、地图、私人字体或其他商业资源提交到本仓库。

个人 iPhone 或 iPad 构建可以在打包阶段嵌入使用者自己合法持有的游戏资源，以便在自己的设备上安装。包含这些商业资源的 `.ipa` 或 `.app` 属于私人构建产物，不得提交到源码仓库、附加到 GitHub Releases 或对外分发。

## 许可证

源码继续按照仓库中的 [GPLv3 许可证及 EA 附加条款](LICENSE.md)发布。第三方组件继续适用其各自许可证。
