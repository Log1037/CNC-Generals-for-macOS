# GeneralsX macOS 二次修改工程日志 / macOS Local Fork Engineering Log

- 最后整理 / Last consolidated: 2026-08-10
- 上游基线 / Upstream baseline: `ammaarreshi/Generals-Mac-iOS-iPad` `main` at `c5c8c4d3e757033d9ab464f6bd6e15e91e0e742f`
- 主要目标 / Primary target: Command & Conquer: Generals — Zero Hour on Apple Silicon macOS

完整版本 / Full editions:

- [中文完整版本](MACOS_LOCAL_FORK_CHANGELOG.zh-CN.md)
- [Full English edition](MACOS_LOCAL_FORK_CHANGELOG.en.md)
- 调查过程与被否决的方案 / Investigation history and rejected approaches: `docs/DEV_BLOG/2026-07-DIARY.md`

## 文档目的 / Purpose

### 中文

本文记录在现有 GeneralsX Apple Silicon 移植基础上完成的第二阶段改造。它不是按时间排列的调试流水账，而是面向审查和发布的工程总览：每一类修改都说明实际遇到的问题、最终保留的方案、主要实现位置，以及公开前仍需处理的事项。

当前本机安装版已经获得良好的实际游玩反馈，但源码修改仍主要存在于未提交工作区中。它应被视为“体验良好的本地集成版”，尚不是已经整理完成的公开发行分支。

### English

This document records the second-stage macOS work built on the existing GeneralsX Apple Silicon port. It is a release-oriented engineering overview rather than a chronological debugging transcript. Each area identifies the observed problem, the solution retained in the working tree, the main implementation locations, and the remaining publication work.

The installed local build now has good play-test feedback, but most source changes still live in an uncommitted working tree. It should therefore be treated as a good local integration build, not yet as a reviewable public release branch.

## 项目来源与范围 / Lineage and scope

### 中文

本项目延续以下开源工作链路：EA 公开的 GPLv3 引擎源码、TheSuperHackers 的现代化工作、Fighter19 的 Unix/SDL3/DXVK 移植、fbraz3/GeneralsX 的 Linux 与 macOS 整合、ammaarreshi 的 Apple Silicon 与 iOS/iPadOS 分支，以及本次由中文资源、外接盘安装和真实游玩问题驱动的 macOS 二次修改。

本仓库不包含商业游戏资源。用户仍需自行提供合法持有的《命令与征服：将军》和《零点行动》文件。

### English

The work extends a chain that includes EA's GPLv3 engine release, TheSuperHackers' modernization, Fighter19's Unix/SDL3/DXVK port, fbraz3/GeneralsX's Linux and macOS integration, ammaarreshi's Apple Silicon and iOS/iPadOS fork, and this macOS-focused customization layer driven by Chinese assets, external-disk installation, and real play sessions.

No commercial game assets are included. Users must still provide their own lawfully obtained Generals and Zero Hour data.

## 公开范围 / Public and local-only boundaries

### 中文

计划公开的是源代码、平台修复、打包脚本、字体选择逻辑和可再分发字体的获取流程。绝不能公开的是原版 `.big`、语音、视频、地图、存档、录像、私有字体、本机构建产物以及写死个人路径的生成文件。

`游戏文件/` 与 `resources/fonts/` 已由 `.gitignore` 排除。本机可以通过忽略目录中的 `gx-font.conf` 选择苹方；公开安装默认下载经过版本与 SHA-256 固定的 Noto Serif SC。两者共用同一套公开代码。

### English

The intended public set consists of source changes, platform fixes, packaging scripts, font-selection logic, and the retrieval flow for redistributable fonts. Retail `.big` files, audio, video, maps, saves, replays, private fonts, local build products, and generated files containing personal paths must never be published.

`游戏文件/` and `resources/fonts/` are ignored. This machine may select PingFang through an ignored `gx-font.conf`, while a public install retrieves a pinned and SHA-256-verified Noto Serif SC asset. Both use the same committed code.

## 1. Retina、HiDPI 与分辨率 / Retina, HiDPI, and resolution

### 中文

原实现混用了 macOS 逻辑点、Retina 物理像素、引擎渲染尺寸和 DXVK 交换链尺寸，造成画面发糊、鼠标偏移、窗口裁切、黑边以及 resize 后的二次缩放。最终实现明确规定窗口几何使用逻辑点，drawable、swapchain 和引擎目标使用物理像素，并把屏幕密度作为独立量传递。

Apple 平台窗口启用 `SDL_WINDOW_HIGH_PIXEL_DENSITY`，DXVK 的 SDL3 WSI 使用 `SDL_GetWindowSizeInPixels`。新增 `GXRenderScalePercent`，允许在原生 Retina 与较低内部渲染比例之间连续调整，同时保持可见窗口尺寸不变。窗口 resize 后会重建所有依赖分辨率的 UI、鼠标和战术视图资源。

### English

The original path mixed macOS points, Retina pixels, the engine render size, and the DXVK swapchain extent. This caused soft output, pointer offsets, cropped windows, black bars, and double scaling after resize. The retained implementation makes the unit contract explicit: window geometry uses logical points, while the drawable, swapchain, and engine targets use physical pixels; display density is carried separately.

Apple windows request `SDL_WINDOW_HIGH_PIXEL_DENSITY`, and DXVK's SDL3 WSI uses `SDL_GetWindowSizeInPixels`. `GXRenderScalePercent` provides continuous control between native Retina and reduced internal resolution without changing the visible window size. Resolution-dependent UI, pointer, and tactical resources are rebuilt after live size changes.

主要位置 / Main areas: `GeneralsMD/Code/Main/SDL3Main.cpp`, `GeneralsMD/Code/GameEngineDevice/Source/SDL3GameEngine.cpp`, `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp`, `Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp`, `cmake/apply-dxvk-macos-retina.cmake`.

## 2. 窗口化与原生全屏 / Windowed and native fullscreen behavior

### 中文

早期版本虽然能由 macOS 进入全屏 Space，但游戏引擎没有正确跟随进入、离开和恢复窗口后的尺寸变化。现在由 Cocoa/SDL 负责原生动画，引擎监听进入与离开事件；退出全屏后等待最终 resize 再计算目标尺寸。全屏采用非独占 Space，窗口化时保留宽高比、标题栏可见性和台前调度兼容性。

### English

Earlier builds could enter a native macOS fullscreen Space, but the engine did not correctly follow entry, exit, and restored-window sizing. Cocoa and SDL now own the native transition while the engine follows the resulting events; after leaving fullscreen, it waits for the final resize before deriving the new target. Fullscreen remains non-exclusive, and windowed recovery preserves aspect ratio, title-bar reachability, and Stage Manager compatibility.

## 3. 安全退出与黑色全屏 Space 恢复 / Safe exit and black-Space recovery

### 中文

在 WindowServer 或内存压力下，进程退出后可能遗留一个没有窗口的黑色全屏 Space。现在引擎会在低层销毁前主动离开全屏，默认等待 12 秒并重试；独立 watchdog 防止图形或音频 teardown 死锁。只有常规退出彻底失败时，才关闭窗口并通过 CoreGraphics 临时重建显示配置。

### English

Under WindowServer or memory pressure, the process could disappear while leaving an empty black fullscreen Space behind. The engine now leaves fullscreen before low-level teardown, waits up to 12 seconds with retries, and arms an independent watchdog against graphics or audio shutdown deadlocks. Only if normal recovery fails does it close the window and use a temporary CoreGraphics display reconfiguration as a last resort.

主要位置 / Main areas: `GeneralsMD/Code/GameEngineDevice/Source/SDL3GameEngine.cpp`, `GeneralsMD/Code/Main/SDL3Main.cpp`, `GeneralsMD/Code/Main/MacDisplayKick.cpp`.

## 4. 渲染帧率、逻辑速度与过场 / Render cadence, game speed, and cinematics

### 中文

原引擎把“一帧画面”和“一步游戏逻辑”绑定在一起，导致提高速度必须提高帧率、降低帧率又会拖慢游戏。现在使用固定步长累积器，一帧画面可以执行多步逻辑：渲染帧率可在 30–240 FPS 或不限帧之间选择，游戏速度以原版 30 Hz 为 1.0×，范围约为 0.5×–6.0×。

脚本 `SET_TIME_MULTIPLIER` 作用于逻辑速率，`SET_FPS_LIMIT` 不再把玩家的渲染上限压低。脚本摄像机使用分数逻辑时间，`timeGetTime()` 保持真实单调时间，FFmpeg 预渲染视频不跟随游戏加速。引擎内没有独立的过场速度通道；脚本过场与普通游戏共用游戏速度。

### English

The legacy engine coupled one rendered frame to one simulation step, so faster play required a higher render rate and a lower render cap slowed the game itself. A fixed-step accumulator now permits multiple logic steps per rendered frame. Rendering can be set from 30–240 FPS or unlimited, while gameplay speed is based on the original 30 Hz simulation and ranges from roughly 0.5× to 6.0×.

`SET_TIME_MULTIPLIER` now affects logic rate, and `SET_FPS_LIMIT` no longer pushes rendering below the player's cap. Scripted cameras use fractional logic time, `timeGetTime()` remains real monotonic time, and FFmpeg video remains wall-clock based. There is no separate cinematic-speed channel in the engine; scripted in-engine cinematics share normal game speed.

主要位置 / Main areas: `Core/GameEngine/Include/Common/FramePacer.h`, `Core/GameEngine/Source/Common/FramePacer.cpp`, `Core/GameEngine/Source/Common/FrameRateLimit.cpp`, `GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp`, `GeneralsMD/Code/GameEngine/Source/GameLogic/ScriptEngine/ScriptActions.cpp`.

## 5. 游戏内设置与修改面板 / In-game settings and trainer panel

### 中文

原 Extras 菜单依赖未部署的 `.wnd` 文件，无法可靠打开。现在面板完全由 C++ 创建，在主菜单和游戏内拥有明确生命周期，提供渲染帧率、游戏速度、镜头俯角、卷屏速度、绘制距离、单机资金以及原生 HiDPI/点对点清晰度切换。

画面与速度设置只有点击保存后才持久化；资金不保存，在主菜单、录像和网络对战中禁用。面板使用统一等比缩放，并根据实际 CJK 字体高度动态计算标题、行距、状态栏和总高度，避免中文截断或溢出。

### English

The original Extras menu depended on an undeployed `.wnd` file and could not open reliably. The replacement is built programmatically in C++, has an explicit lifecycle in both shell and in-game contexts, and exposes render FPS, game speed, camera pitch, scroll speed, draw distance, single-player money, and native-HiDPI versus point-for-point clarity.

Display and speed settings are persisted only after Save; money is never persisted and is disabled in the shell, replays, and network games. Uniform scaling and live CJK font metrics determine the title, rows, footer, and total panel height so Chinese labels do not clip or overflow.

快捷键 / Shortcuts: `Ctrl+G`, `Ctrl+[` / `Ctrl+]`, `Shift+Ctrl+[` / `Shift+Ctrl+]`, `Alt+N`.

主要位置 / Main area: `GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/ExtrasMenu.cpp`.

## 6. 中文字体与可读性 / Chinese fonts and legibility

### 中文

字体问题包括缺字方框、错误字形、宋体被无衬线替代、TTC face index 错误以及 CJK 字框被拉丁字体高度裁切。新的字体解析顺序为：`GX_CJK_SERIF_FONT`、`gx-font.conf`、运行目录字体、系统宋体候选、开源后备字体。

候选字体必须拥有 Unicode charmap、常用汉字覆盖，并在自动检测时通过 OS/2 PANOSE 衬线分类。TTC 会枚举 face 而不是固定使用 face 0。公开部署下载固定版本并校验 SHA-256 的 Noto Serif SC；私有 Windows 字体不进入仓库。之前的自定义 tooltip 字号补偿已撤回，字体统一使用引擎正常的分辨率调整。

### English

The font failures included missing-glyph boxes, incorrect forms, Song requests resolving to sans-serif faces, incorrect TTC face indices, and CJK boxes clipped by Latin-oriented metrics. The new resolution order is `GX_CJK_SERIF_FONT`, `gx-font.conf`, runtime fonts, host Song candidates, and an open-font fallback.

Candidates must provide a Unicode charmap and common ideograph coverage, and automatic detection additionally checks the OS/2 PANOSE serif classification. TTC faces are enumerated instead of assuming face zero. Public deployment retrieves a pinned, SHA-256-verified Noto Serif SC; private Windows fonts are never committed. The custom tooltip-size compensation was removed so all fonts use the engine's normal resolution adjustment.

主要位置 / Main areas: `Core/Libraries/Source/WWVegas/WW3D2/render2dsentence.cpp`, `Core/Libraries/Source/WWVegas/WW3D2/render2dsentence.h`, `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/GUI/W3DGameFont.cpp`, `scripts/build/macos/deploy-macos-zh.sh`.

## 7. 视频与过场内容 / Video and presentation continuity

### 中文

将军挑战开场曾缺失或崩溃，视频纹理也可能因为格式与锁定假设不成立而黑屏。现在 FFmpeg 视频路径会验证纹理格式、锁定结果与回退路径；macOS 将军挑战使用真实时间播放完整动态开场，而不是跳过内容或用静态替代。

### English

Challenge introductions could be missing or crash, and video textures could go black when format or lock assumptions failed. The FFmpeg path now validates texture formats, lock results, and fallbacks. On macOS, General's Challenge retains the full real-time animated introduction instead of skipping content or replacing it with a static screen.

## 8. 光照、屏幕特效与离屏目标 / Lighting, screen effects, and offscreen targets

### 中文

远处单位和步兵曾在低细节阶段异常变黑。保留的 DXVK 固定功能修复在关闭镜面反射时继续传递原始顶点 `COLOR1`，避免 SAGE 把通用第二顶点色读成零。屏幕滤镜和热浪效果也改为采样真实的实时场景目标，并在离屏渲染后正确恢复颜色与深度目标。

这部分实现已包含在当前体验良好的整合版中，但本次文档整理没有重新执行逐项 A/B 测试，因此发布说明应把“当前整体验证良好”和“每个底层假设已独立证实”分开表述。

### English

Distant units and infantry could become abnormally dark at lower detail levels. The retained DXVK fixed-function patch preserves the original vertex `COLOR1` when specular is disabled, preventing SAGE materials from reading a zeroed generic secondary color. Screen filters and heat distortion now sample the live scene target and correctly restore color and depth targets after offscreen work.

These changes are part of the currently good integration build, but this documentation pass did not repeat isolated A/B tests. Public notes should distinguish overall integration validation from independent proof of every low-level hypothesis.

主要位置 / Main areas: `cmake/apply-dxvk-macos-retina.cmake`, `Core/Libraries/Source/WWVegas/WW3D2/dx8renderer.cpp`, `Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp`, `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DSmudge.cpp`, `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp`.

## 9. 输入、鼠标与菜单稳定性 / Input, cursor, and menu robustness

### 中文

SDL3 键盘映射补充了小键盘键；鼠标静止判断增加容差，避免高分屏上的细小抖动；`Cmd+G` 可释放或重新捕获鼠标并显示菜单栏。主菜单在焦点恢复后刷新地图背景，空 `TEXT` 脚本项不再破坏解析，将军挑战流程保留用户渲染上限，滑块在缺少图片资源时采用明确颜色后备。

### English

SDL3 keyboard handling now includes keypad mappings; mouse-still detection tolerates tiny high-density jitter; and `Cmd+G` releases or recaptures the cursor while exposing the menu bar. The shell refreshes its map background after focus restoration, empty `TEXT` script entries no longer break parsing, Challenge flows preserve the user's render cap, and sliders use an explicit color fallback when image assets are absent.

## 10. 外接盘与文件系统 / External disks and filesystem behavior

### 中文

运行时资源不再隐含依赖仓库位置。`GX_RUNTIME_ROOT` 可指定外接盘上的游戏根目录；BIG 扫描限制在根目录并支持可选 `GeneralsX.biglist`，本地文件系统补齐通配符枚举。启动前检查 `INIZH.big`，缺少正版资源时给出明确提示。

### English

Runtime assets no longer implicitly depend on the repository location. `GX_RUNTIME_ROOT` selects the external-disk game root; BIG scanning is root-scoped and can use an optional `GeneralsX.biglist`; and the local filesystem now supports wildcard enumeration. Startup preflights `INIZH.big` and reports a clear error when retail assets are absent.

## 11. macOS App、启动器与部署 / macOS app, launcher, and deployment

### 中文

正式应用包使用 Cocoa 启动器作为入口，隐藏脚本和裸二进制细节。打包流程使用临时目录、原子替换和深度签名，将引擎、DXVK、Vulkan ICD、OpenAL、FFmpeg 与所需资源纳入 `.app`。启动器和 `run.sh` 共享字体配置规则，并支持 `--check` 静态检查。

部署脚本为依赖下载提供镜像、校验、损坏文件修复和失败后的人工恢复说明。运行目录仍需由用户提供商业资源，App 包不应包含这些文件。

### English

The formal app bundle uses a Cocoa launcher as its entry point, hiding scripts and bare executable details. Packaging stages into a temporary location, replaces atomically, signs deeply, and bundles the engine, DXVK, Vulkan ICD, OpenAL, FFmpeg, and required redistributable resources. The launcher and `run.sh` share font-configuration behavior and support a static `--check` mode.

Deployment provides mirrors, checksums, corrupted-file repair, and manual recovery instructions for failed dependency downloads. Retail game data remains user-supplied and must not be embedded in the app bundle.

主要位置 / Main areas: `packaging/macos/GeneralsXLauncher.m`, `scripts/build/macos/package-macos-zh-app.sh`, `scripts/build/macos/deploy-macos-zh.sh`, `scripts/build/macos/run-macos-zh.sh`.

## 12. 配置项与诊断入口 / Preferences and diagnostics

### 中文

主要持久设置包括 `GXRenderFPS`、`GXGameSpeedTenths`、`GXRenderScalePercent`、镜头与卷屏参数。主要运行时入口包括 `GX_RUNTIME_ROOT`、`GX_CJK_SERIF_FONT` 以及全屏恢复诊断开关。配置项必须区分用户偏好、一次性诊断参数与已经废弃的旧实验变量。

### English

The main persistent preferences include `GXRenderFPS`, `GXGameSpeedTenths`, `GXRenderScalePercent`, and camera/scroll settings. Important runtime controls include `GX_RUNTIME_ROOT`, `GX_CJK_SERIF_FONT`, and fullscreen-recovery diagnostics. User preferences, one-shot diagnostic switches, and obsolete experimental variables must remain clearly separated.

## 13. 当前验证状态 / Current validation status

### 中文

已有证据包括成功构建、打包、签名检查、启动器静态检查、字体冷下载与损坏修复、路径与依赖检查，以及用户对当前安装版的实际游玩反馈。本次文档整理没有启动或自动操作游戏，也没有把静态检查误写成人工游玩确认。

### English

Retained evidence includes successful builds, packaging and signature checks, launcher static checks, font cold-download and corrupted-file repair tests, path and dependency checks, and user play-test feedback for the installed build. This documentation pass did not launch or automate the game and does not present static checks as manual play confirmation.

## 14. 发布整理状态 / Publication preparation status

### 中文

本轮发布整理已经完成：建立独立分支、移除本机绝对路径、清除废弃的独立过场速度变量、取消渲染/逻辑 FPS 强制耦合、同步构建文档，并把公开默认值统一为 60 FPS / 1.0×。资金面板作为这个个人需求驱动分支明确保留，但只允许本地单机使用。App 默认改用上游仓库已有图标，自制 ICNS 留在忽略目录。

源码发布后仍建议继续完成：

1. 将 DXVK 修复进一步整理成上游 fork 中的独立正式提交；当前 CMake patch 已注明对应上游提交并保证可复现。
2. 检查跨层 macOS helper，能通过小型平台接口收口的继续收口。
3. 验证共享 `Core/` 改动对原版 Generals、Linux、iOS、录像确定性和网络锁步的影响。
4. 在另一台没有本项目历史配置的 Apple Silicon Mac 上完成全新安装测试。
5. 制作二进制 Release 前再次核对第三方库、字体、图标许可证和上游署名。

### English

This publication-preparation pass created a dedicated branch, removed machine-specific absolute paths, deleted the obsolete independent-cinematic variables, removed render/logic FPS coupling, synchronized the build documentation, and standardized public defaults at 60 FPS / 1.0×. The money panel is intentionally retained as a personal-fork feature but remains restricted to local single-player play. Public packaging now uses the icon already tracked by upstream, while the local custom ICNS is ignored.

Recommended follow-up after the source branch is published:

1. Move the DXVK fixes into a dedicated formal commit in the DXVK fork; the current reproducible CMake patch already identifies its upstream relationship.
2. Continue consolidating cross-layer macOS helpers behind a small platform interface where practical.
3. Validate shared `Core/` changes against base Generals, Linux, iOS, replay determinism, and network lockstep.
4. Perform a clean install on another Apple Silicon Mac without this machine's historical configuration.
5. Recheck third-party library, font, icon licensing, and upstream attribution before publishing binary release assets.

## 15. 建议提交拆分 / Suggested commit series

| 顺序 / Order | 中文范围 | English scope |
|---:|---|---|
| 1 | 外接盘与文件系统 | External-disk and filesystem support |
| 2 | Retina、DXVK WSI 与窗口尺寸 | Retina, DXVK WSI, and window sizing |
| 3 | 原生全屏与安全退出 | Native fullscreen and safe exit |
| 4 | 渲染帧率与逻辑速度解耦 | Render/logic cadence separation |
| 5 | 视频、纹理和渲染目标修复 | Video, texture, and render-target fixes |
| 6 | SDL 输入、鼠标和菜单稳定性 | SDL input, cursor, and menu robustness |
| 7 | 中文字体与 CJK 布局 | Chinese fonts and CJK layout |
| 8 | 游戏内设置面板 | In-game settings panel |
| 9 | macOS 启动器、打包与签名 | macOS launcher, packaging, and signing |
| 10 | 文档、许可证与发布说明 | Documentation, licensing, and release notes |

每个提交都应单独说明问题、根因、修改边界和验证方式；不要把当前工作区一次性压成一个不可审查的大提交。

Each commit should identify the problem, established cause, scope, and validation evidence. The current working tree should not be collapsed into one unreviewable bulk commit.

## 当前结论 / Closing status

### 中文

这批改动已经从零散修补发展成一套完整的 macOS 本地发行层，覆盖显示、窗口、时序、中文、过场、输入、资源路径和 App 打包。它目前最大的风险不是“没有效果”，而是所有改动尚未经过发布工程化：分支、提交、路径清理、许可证检查和跨机器验证仍需完成。

### English

The work has grown from isolated fixes into a coherent local macOS distribution layer covering display behavior, window management, timing, Chinese text, cinematics, input, asset paths, and app packaging. Its primary remaining risk is no longer lack of functionality, but lack of release engineering: branching, commit separation, path cleanup, license review, and cross-machine validation remain outstanding.
