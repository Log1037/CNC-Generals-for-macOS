# 《将军：零点行动》macOS 原生版二次修改总账——中文版

[中英对照总览 / Bilingual overview](MACOS_LOCAL_FORK_CHANGELOG.md) · [English full edition](MACOS_LOCAL_FORK_CHANGELOG.en.md)

- 整理日期：2026-08-10
- 上游基线：`ammaarreshi/Generals-Mac-iOS-iPad` 的 `main`，提交 `c5c8c4d3e`
- 主要平台：Apple Silicon macOS
- 主要游戏：《Command & Conquer: Generals — Zero Hour》简体中文资源版

> 本文是中文完整版本。英文完整版本为
> `docs/WORKDIR/reports/MACOS_LOCAL_FORK_CHANGELOG.en.md`；中英对照入口为
> `docs/WORKDIR/reports/MACOS_LOCAL_FORK_CHANGELOG.md`。

## 一、这批修改是什么

这不是一个从零开始的新移植项目，而是在已有开源移植链路上的第二阶段改造：

1. EA 公开的 GPLv3 游戏引擎源码；
2. TheSuperHackers 的现代化工作；
3. Fighter19 的 Unix、SDL3 和 DXVK 移植；
4. fbraz3/GeneralsX 的 Linux、macOS 整合；
5. ammaarreshi 的 Apple Silicon、iOS、iPadOS 分支；
6. 我们在实际安装、中文资源适配和长时间游玩中发现问题后形成的 macOS 二次修改。

我们的重点不是改变《零点行动》的玩法，而是让这套原生移植在现代 Retina Mac 上真正达到日常可玩的状态：画面清晰、窗口行为正常、速度可控、中文可读、过场不丢、应用包可复现，并且退出时不把 macOS 留在异常的黑色全屏空间里。

当前用户反馈是整体体验已经很好。不过代码仍主要保存在一个未提交工作区内，尚未拆成适合公开审查的提交，也没有完成另一台干净 Mac 上的完整安装验证。因此它目前应称为“本机体验良好的集成版”，还不能直接称作正式公开发行版。

## 二、公开内容与本机内容的边界

### 计划公开的内容

- 引擎和平台层修复；
- Retina、窗口、全屏及输入处理；
- 帧率和逻辑速度解耦；
- 游戏内设置面板；
- 中文字体查找、校验与回退逻辑；
- 可再分发字体的自动获取流程；
- 视频、渲染目标和特效修复；
- 外接盘路径与文件系统修复；
- macOS app 启动器、打包和部署脚本；
- 中英双语工程日志和构建说明。

### 永远不应公开的内容

- 原版或《零点行动》的任何 `.big`、语音、视频、地图等商业资源；
- 本机存档、录像、配置和缓存；
- 私人的 Windows 宋体文件；
- 本地 SDK、构建产物和外接盘归档；
- 写死本机用户名或卷宗路径的生成文件。

项目的 `游戏文件/` 和 `resources/fonts/` 已被 `.gitignore` 排除。本机目前可以通过忽略目录中的 `gx-font.conf` 选择苹方，而公开安装默认自动取得开源的 Noto Serif SC。两者使用同一套公开代码，不需要维护两个源代码版本。

## 三、Retina、HiDPI 与渲染分辨率

### 遇到的问题

- 游戏最初按 macOS 的逻辑点分辨率渲染，再放大到 Retina 像素，整个画面发糊。
- SDL 窗口尺寸、物理像素、引擎内部尺寸和 DXVK 交换链尺寸混在一起使用。
- 鼠标坐标有偏移，点击位置和画面不一致。
- 窗口改变大小后，DXVK 跟着窗口走，引擎仍停留在旧分辨率，造成二次缩放。
- 早期方案让“窗口尺寸从渲染尺寸推导”，同时又让“渲染尺寸从窗口推导”，在 50% 清晰度下形成反馈循环，出现窗口不断放大、画面只显示左上角等问题。
- 切换原生全屏后，Cocoa 和 SDL 已完成窗口变化，但游戏引擎仍认为自己处于旧模式。

### 最终修改

- Apple 平台窗口请求 `SDL_WINDOW_HIGH_PIXEL_DENSITY`。
- 明确单位边界：
  - 窗口位置与尺寸使用逻辑点；
  - Retina drawable、DXVK swapchain 和引擎渲染目标使用物理像素；
  - 屏幕密度单独传递，任何路径都不能重复乘两次。
- DXVK 的 SDL3 WSI 改用 `SDL_GetWindowSizeInPixels` 获取交换链像素尺寸。
- 在第一个 D3D 设备建立前就按显示器大小创建全屏窗口，避免先建 1024×768 再强行拉伸。
- 新增 `GXRenderScalePercent`：
  - 100% 为原生 Retina；
  - 点对点模式按实际屏幕密度计算，2× 屏通常对应 50%；
  - 调整清晰度只改变渲染目标，不改变肉眼看到的窗口大小。
- 将窗口的逻辑点尺寸保存为明确状态，仅在启动、选择分辨率或用户拖动窗口时更新。
- 补上原来为空的 SDL 窗口 resize 处理，让引擎从最终窗口像素尺寸重新推导渲染分辨率。
- macOS 窗口大小统一由 SDL3 显示层管理，阻止 DX8Wrapper 再以物理像素冒充逻辑点执行第二次 resize。
- 实时改变渲染尺寸后，重建 HeaderTemplate、鼠标范围、字体、Shell 菜单、控制栏、自定义 UI 和 TacticalView。

## 四、窗口化与原生全屏

### 遇到的问题

- `Ctrl+Cmd+F` 和绿色窗口按钮本来能让 macOS 进入全屏空间，但游戏引擎没有监听结果。
- 退出全屏时读取窗口太早，拿到的仍是全屏或过渡中的尺寸。
- 最大窗口尺寸若按“可用桌面区域”设置，AppKit 会把同一限制施加到全屏内容，导致上下黑边。
- Cocoa 有时只缩小窗口的一个方向，破坏宽高比。
- 离开全屏后，标题栏可能跑到屏幕外。
- 台前调度、窗口恢复和系统动画不能靠自动截图或猜测解决。

### 最终修改

- 监听 SDL 的进入/离开全屏事件，让 macOS 负责过渡动画，引擎只负责跟随结果。
- 进入全屏后按当前显示器物理像素和清晰度比例重新建立渲染目标。
- 离开全屏时先更新模式，不立即取尺寸；等 Cocoa 发出恢复完成后的 resize 事件再计算。
- 使用非独占的原生 macOS 全屏 Space，不伪装成无边框窗口。
- 窗口最大尺寸改为显示器完整边界，而不是扣除菜单栏和标题栏后的可用区域，避免限制全屏 drawable。
- 窗口化时预留标题栏高度；若 Cocoa 只限制一边，则再按同一比例恢复宽高比。
- 检查窗口移动事件，把跑出屏幕的标题栏移回可见区域。
- 清晰度切换、窗口拖动和分辨率选择不再互相争夺窗口尺寸所有权。

## 五、退出游戏时的黑屏与孤立全屏 Space

### 遇到的问题

在内存紧张或 WindowServer 状态异常时，游戏进程退出后，原生全屏 Space 仍可能停留在最前面。此时进程已经不在“强制退出”列表中，屏幕却仍是一个没有窗口的黑色空间；单显示器机器尤其难以恢复。

### 最终修改

- 在引擎开始销毁之前、窗口仍是前台关键窗口时先退出全屏。
- 默认最多等待 12 秒，并按固定间隔重新请求，而不是只请求一次。
- 在低层 teardown 前启动独立退出 watchdog，防止 DXVK、MoltenVK、OpenAL 或其他工作线程死锁后长期占住屏幕。
- 如果等待结束后窗口仍是全屏：
  1. 关闭 SDL 窗口；
  2. 通过 CoreGraphics 临时切换显示模式再恢复，触发 WindowServer 重建显示配置；
  3. 这相当于软件方式模拟重新插拔显示器。
- 保留环境变量用于调整等待时间、关闭显示配置恢复或受控测试失败路径。

这套显示模式切换只在正常退出全屏彻底失败时触发，属于最后一道保险，不是常规退出路径。

## 六、帧率、逻辑速度与过场

### 遇到的问题

- 老引擎基本上每渲染一帧才跑一步逻辑，因此渲染 FPS 和游戏速度互相绑定。
- 提高游戏速度会被迫提高渲染上限；降低渲染帧率又会让游戏变慢。
- 老地图里的 `SET_FPS_LIMIT 20` 原本是给 2003 年硬件节省性能，却让现代机器上的开场和脚本过场降到 20 FPS，甚至把逻辑一起拖慢。
- 旧式快进通过跳过画面帧实现，画面不流畅。
- 摄像机旋转、俯仰和缩放按渲染帧计数，60 FPS 时会比作者设定快一倍。
- 曾经用缩放 `timeGetTime()` 的方式做加速，结果连 FFmpeg 视频、菜单动画、网络超时和各种真实计时都一起被改变。

### 最终修改

- 增加固定步长累积器：一帧画面内可以按实际欠下的时间连续运行多步逻辑。
- 渲染节奏和逻辑节奏彻底分开：
  - 渲染帧率可在 30–240 FPS 或不限帧之间调整；
  - 游戏速度以原版 30 Hz 为 1.0×，支持约 0.5×–6.0×。
- 网络对战继续由网络节奏控制，本地速度设置不能让某一客户端独自跑快。
- 脚本 `SET_TIME_MULTIPLIER` 改为乘到逻辑频率，不再通过跳帧伪造加速。
- 地图 `SET_FPS_LIMIT` 不能把渲染上限压到低于玩家设置。
- 客户端动画按照真实的“逻辑时间/渲染时间”比例推进。
- 脚本摄像机使用分数逻辑帧推进，时长不再依赖显示帧率。
- `timeGetTime()` 恢复为单调真实时间，并新增 64 位真实毫秒函数；预渲染视频继续按真实时间播放。
- 过场期间虽然游戏输入被锁定，但显示帧率、游戏速度和设置面板指令仍可使用。
- `-file` 直接载入地图的调试路径开放到 Release 构建，便于稳定复现某个战役过场。

### 当前实际模型

现在没有独立的“过场速度”引擎通道：

- 引擎内脚本过场和普通游戏共用同一个游戏速度；
- FFmpeg 播放的预渲染视频使用真实时间；
- 早期设计的 `GXCinematicSpeedTenths` / `GX_CINEMATIC_LOGIC_FPS` 因为没有真正接入 FramePacer，已从引擎移除。

发布整理时已经同步清理 Cocoa 启动器：不再读取或导出这两个旧变量，也不再为了逻辑速度强制抬高渲染 FPS。

## 七、游戏内“画面与速度设置”面板

### 遇到的问题

- 最初的 Extras 面板依赖一个未部署的 `.wnd` 文件，实际上根本无法正常打开。
- 全屏父窗口可能跟随 Shell 栈存活到游戏中，产生遮挡和崩坏。
- 自定义文字没有 CSF 条目，显示为 `MISSING`。
- 800×600 固定数值在高分辨率下让滑块按钮只有十几个像素，按钮无底图、行距不足、内容溢出。
- 只靠快捷键不符合用户想要“看得见、随时可调”的使用方式。

### 最终修改

- 整个面板由 C++ 动态创建，不再依赖松散 `.wnd` 和游戏文字包。
- 面板自己拥有 `WindowLayout`，在主菜单和游戏内都有明确生命周期。
- 文案有中英文内置后备，同时保留未来 CSF 翻译覆盖能力。
- 以 800×600 为设计坐标，按单一等比系数缩放尺寸和字体，避免 16:9 拉伸。
- 提供六项实时设置：
  - 渲染帧率；
  - 游戏速度；
  - 镜头俯角；
  - 卷屏速度；
  - 绘制距离；
  - 本地玩家资金。
- 提供“原生 HiDPI / 像素点对点”清晰度切换。
- 只有点击“保存”才写入画面、速度和视角偏好；资金永不保存，也不受“恢复默认值”影响。
- 资金功能在主菜单、录像和网络对战中禁用，避免破坏回放或联机锁步。
- 资金变更走 deposit/withdraw，不播放收入提示音，也不计入每分钟收入。
- 按钮使用原版真实存在的三段式图像；横向滑块没有可用图片资源，因此明确使用颜色绘制。
- 滑块命中、刻度和最大位置都按实际 thumb 尺寸计算，不再假设永远是 13×16。
- 面板吞掉自身鼠标事件，拖动滑块不会顺便在地图上拉选框。
- 游戏内退出菜单显示当前渲染帧率、逻辑 FPS、速度和资金。

### CJK 面板尺寸修复

后来发现，面板字体本身没有错，真正的问题是文字框仍按拉丁字形高度设计。游戏把 point 按 96 DPI 转成像素，而中文字体的 ascent+descent 又明显高于拉丁字体，因此 10pt 标签至少需要约 18 个设计单位，旧值只有 15；14pt 标题至少需要约 26，旧值只有 22。

最终做法：

- 标题、行标签、模式区、按钮区和行距全部按 CJK 实际高度重新计算；
- 修复漏掉的最底部状态栏高度；
- 状态栏不再只信固定常量，而是取 `max(设计高度, GameFont 实际高度)`；
- 面板总高度也不再写死，而是从状态栏真实高度和底部边距推导；
- 因此换成 Noto Serif SC、宋体、苹方或其他用户字体时，面板会随实际字框长高，不会再从底部截断。

### 当前快捷键

| 快捷键 | 作用 |
|---|---|
| `Ctrl+G` | 打开或关闭画面与速度面板 |
| `Ctrl+[` / `Ctrl+]` | 降低/提高渲染帧率 |
| `Shift+Ctrl+[` / `Shift+Ctrl+]` | 降低/提高游戏速度 |
| `Alt+N` | 非多人游戏中增加 10,000 资金 |

公开默认值和面板“恢复默认值”现已统一为 60 FPS / 1.0×。用户仍可保存自己习惯的 2.2× 或其他速度。

## 八、中文字体、字形与可读性

### 遇到的问题

- 个别汉字缺失、出现方框，或者被错误的无衬线字体替代。
- 从中文 `Language.ini` 读到的“宋体”是 GBK 原始字节，Fontconfig 无法直接把它当正常 UTF-8 家族名处理。
- `FcFontMatch()` 找不到目标时不会返回空，而是永远给一个“最接近”的字体，因此过去的“加载成功”并不代表真的找到了宋体。
- TTC 字体集合的 face index 被写死为 0；macOS `Songti.ttc` 的 face 0 是黑体重，Regular 实际在 face 6，导致文字异常粗重。
- 拉丁字体的 ascender/descender 指标不足以覆盖完整 CJK 字框，汉字底部可能被截断，看起来像另一个字。
- 全局放大字号会让一部分地方可读，却让固定高度菜单溢出。
- SDL3 路径计算了鼠标提示文字，但从未真正调用绘制；高 DPI 鼠标的微小抖动又不断重置提示延时。

### 最终修改

- 公开安装默认使用可再分发的 Noto Serif SC（SIL OFL 1.1），由部署脚本从固定 Git 提交下载到运行目录，不安装到系统。
- 下载 URL 固定到提交 `9b0f143`，并校验完整 SHA-256；先下载到 `.part`，验证后再原子移动，避免中断文件被误用。
- 若下载失败，部署仍继续，并明确提示四种恢复方案；引擎可退回主机上的 Songti SC。
- 每个候选字体都必须同时满足：
  - 存在 Unicode charmap；
  - 真正包含 U+6C49、U+4E2D、U+56FD 等汉字；
  - 自动探测时 OS/2 PANOSE 的 serifStyle 不能是无衬线范围 11–15。
- 检查字体的全部 `FC_FAMILY` 名称，而不是只看第一个本地化家族名。
- 自动查找时优先运行目录字体，其次主机宋体，再到开源宋体家族；显式指定的苹方等无衬线字体则按用户意愿接受。
- 明确选择 FreeType Unicode charmap。
- 可缩放字体的字框使用字体自身 bbox 扩展 ascent 和 descent，避免中文被裁切。
- 控制栏说明文字恢复正常的分辨率缩放，不再固定 12pt。
- 曾经加入的 `TooltipFontSizeAdjustment=1.25` 已在公开前撤回；现在不再自行制定全局字号审美，只修复错误的缩放和容器尺寸。
- SDL3Mouse 补上 cursor text 和 tooltip 的实际绘制。
- 鼠标静止判断改为带容差的锚点距离，现代鼠标微抖不会永远阻止提示出现。
- 删除大量 `[GX-ISSUE144]` 字体诊断，只保留 Unicode charmap 失败的一次性警告。

### 字体选择顺序

从高到低：

1. 环境变量 `GX_CJK_SERIF_FONT`；
2. 运行目录 `gx-font.conf`；
3. `<runtime>/fonts` 中的 `NotoSerifSC-Regular.otf`、`song.otf`、`simsun.ttc` 等；
4. macOS 自带 Songti SC；
5. Fontconfig 可找到的其他开源宋体。

一次启动可这样指定：

```bash
GX_CJK_SERIF_FONT="PingFang SC" ./run.sh
```

长期指定则在 `gx-font.conf` 放一行字体家族名。命令行启动器和双击 app 的 Cocoa 启动器现在都会读取同一个文件，避免两种启动方式使用不同字体。

### 分发说明

- 不提交、不分发私有宋体；
- Noto Serif SC 只在部署时下载，不把 11.6MB 二进制塞入 Git 历史；
- 使用 Simplified Chinese Subset OTF，而不是 24MB 的全 CJK 大包；
- 部署脚本补充了 macOS `AssetsV2` 和 `Fonts/Supplemental` 搜索路径，使现代系统字体对项目 Fontconfig 可见。

## 九、视频、将军挑战和过场内容

### 遇到的问题

- D3D8 能力表可能说某种老纹理格式可用，但通过 DXVK/MoltenVK 建立或锁定后仍失败，导致视频缓冲区为空。
- 将军挑战有一条低内存静态路径；部分任务的 voice length 为 0，于是选将军后的 VS 动画和播报几乎一帧内就消失。
- 早期游戏时间缩放让 FFmpeg 视频与音频错位。

### 最终修改

- 视频缓冲区按候选格式逐一尝试，必须同时通过纹理创建和 lock 才接受；Apple 平台优先 32 位 XRGB。
- `valid()` 同时检查包装对象和底层 D3D 纹理。
- FFmpeg 使用 64 位单调真实时钟。
- macOS 强制走完整的将军挑战动画流程，不再因旧式低内存判断丢过场。
- 对极短视频避免除零，并记录打开、缓冲区建立、播放、跳过和结束信息。

## 十、光照、屏幕滤镜和渲染目标

### 遇到的问题

- 远景或低 LOD 单位，尤其步兵，会异常变黑，拉近后恢复。
- 屏幕滤镜在设备初始化时记住的是 swapchain backbuffer，而实际场景在 pillarbox/缩放模式下画到另一个离屏目标；滤镜结束后恢复错表面，UI 和效果会被后续清除覆盖。
- 微波坦克等热扰动从上一帧已放大的 backbuffer 取样，在 50% 渲染比例下 UV 只覆盖左上区域，形成错位的小地图式残影。
- 损坏或缺少树木绘制类型时，单位进入分区可能解引用空指针。

### 最终修改

- 回移植 DXVK 上游 `95d591a8d3f2` 的固定管线 `COLOR1` 行为：
  - fixed-function vertex shader key 加入 `SpecularEnabled`；
  - `D3DRS_SPECULARENABLE` 变化时也重建顶点着色器；
  - 关闭高光时保留原始顶点 `COLOR1`，而不是输出接近零的计算高光色。
- CMake 对本地和远端 DXVK 源都自动应用同一补丁。
- 每次 RTT pass 开始时记录当下真实 render target/depth surface，结束后恢复同一组表面。
- RTT 纹理随实时场景目标尺寸和格式变化。
- Smudge 从当前绑定的场景表面复制，并在分辨率变化后自愈重建背景纹理。
- 树类型索引和 draw-module data 在使用前做范围和空值保护。
- `D3DRS_PATCHSEGMENTS` 未实现警告从每材质一次改成每进程一次，避免同步日志 I/O 拖垮帧率。

这里需要保留措辞上的谨慎：早期交接时 `COLOR1` 方案仍被标记为候选，当前整体体验虽已很好，但本次整理没有再做单独的前后 A/B 截图。因此公开说明应写成“已实装并在本地使用”，不要伪称已经建立独立自动回归测试。

## 十一、SDL 输入、鼠标与工具提示

### 遇到的问题

- 数字键盘 scancode 落到 `KEY_NONE`，原版小键盘旋转、缩放和复位镜头都失效。
- 全屏捕获鼠标后，只能切走应用才能把鼠标交回桌面。
- 单纯调用 releaseCapture 会在下一次焦点或模式刷新时被自动抓回。
- 即使鼠标已释放，菜单栏仍被 SDL 的独占全屏策略彻底隐藏。

### 最终修改

- Generals 和 Zero Hour 两套 SDL3Keyboard 都补齐数字键盘 0–9、运算符、Enter、Num Lock/Clear 映射。
- 在鼠标捕获策略中增加“用户主动释放”原因，不会被焦点变化无意清除。
- `Cmd+G` 在键盘设备之前被拦截，只执行释放/重新捕获，不把游戏里的 `G` 热键传下去。
- 释放鼠标时允许全屏菜单栏在顶端悬停出现，重新捕获后再次隐藏，避免影响边缘卷屏。
- 分辨率和全屏模式变化后重新计算鼠标范围和捕获策略。

## 十二、主菜单、退出菜单和 UI 稳定性

### 遇到的问题

- 切出窗口后回到游戏，3D 主菜单可能变黑，虽然地图和摄像机对象仍存在。
- `QuitMenu.wnd` 的 `TEXT = ""` 会让解析器对空 `strtok` 结果执行 `strlen`，直接崩溃。
- 将军挑战启动时强制把渲染上限改回 30。
- 图片滑块尚未完整初始化时可能直接解引用空图像。

### 最终修改

- macOS 恢复焦点后延迟刷新 shell map，并明确跟踪 Shell 是否仍拥有当前背景游戏。
- `.wnd` 文本解析限制引号扫描范围，并把空文字当作合法内容。
- Challenge 启动时保留用户的渲染上限。
- 图片不完整时退回安全的颜色滑块绘制。
- 移除 Options 菜单里旧的动态 Extras 按钮以及废弃的松散 `ExtrasMenu.wnd`，统一使用自持有面板。

## 十三、外接盘、BIG 文件与本地文件系统

### 遇到的问题

- 原项目默认运行目录固定在 `~/GeneralsX`，实际正版资源和应用都在外接 APFS 盘。
- BIG 文件搜索递归钻进 Data、字体和其他目录，外接盘元数据解析时可能卡几分钟。
- 本地文件列表只比较扩展名，Windows 的 `*`、`?` 规则不完整，递归方式也较脆弱。
- Data 目录内的旧 BIG 副本可能被重复加载。

### 最终修改

- 下载、部署和运行脚本支持 `GX_RUNTIME_ROOT`，未设置时保持上游 `~/GeneralsX` 默认。
- 支持根目录 `GeneralsX.biglist` 显式列出加载档案。
- Unix 下没有 manifest 时只用 `opendir` 扫描安装根目录的 `.big`，不递归整个外接盘。
- 本地文件系统补齐大小写不敏感的 `*`、`?` 通配匹配，以及可选的安全递归迭代。
- Cocoa 启动器直接检查固定必需文件 `INIZH.big`，不枚举大型可移动目录。
- 提供旧 BIG 条目抽取工具，用于受控恢复单个资源。

## 十四、macOS app、启动器与部署

### 遇到的问题

- 中文 app 最初手工拼装，重新编译引擎后 app 内仍可能保留旧二进制和旧 DXVK。
- `CFBundleExecutable` 曾直接指向游戏引擎，绕过了运行所需的环境变量和工作目录。
- 临时部署目录与真正游玩的资源目录各自有一套运行库，容易出现“明明修了但启动后还是旧问题”。
- Vulkan 发现只认 LunarG 布局，对 Homebrew loader/MoltenVK 支持不完整。
- 把 ICD JSON 放进 `Contents/Frameworks` 会被 codesign 当作未签名嵌套代码。
- 命令行 `run.sh` 与双击 app 曾读取不同的字体配置。

### 最终修改

- 新增编译的 Cocoa 启动器，负责在 `exec` 引擎前设置：
  - 零点行动和原版资源路径；
  - 工作目录；
  - DXVK bare-name `dlopen` 所需的 `DYLD_LIBRARY_PATH`；
  - Vulkan ICD；
  - Fontconfig；
  - 帧率和逻辑速度环境；
  - `~/Library/Logs/GeneralsX/ZeroHour.log`。
- 外接盘未挂载、`INIZH.big` 缺失、Frameworks 或引擎缺失时显示中文错误框。
- `--check` 只做启动前检查，不运行游戏。
- `package-macos-zh-app.sh` 自动完成：
  1. 编译启动器；
  2. 暂存引擎和 dylib；
  3. 把 ICD manifest 放到 Resources；
  4. 写入 Info.plist 和图标；
  5. 从内到外签名；
  6. 运行 preflight；
  7. 验证后原子替换正式 app。
- 部署脚本复制 OpenAL、DXVK D3D8/D3D9、Vulkan、MoltenVK、Fontconfig、字体、GameSpy 和 wrapper。
- 同时接受完整 LunarG SDK 与 Homebrew Vulkan loader + MoltenVK。
- Vulkan dylib 使用 `install -m 644`，避免第一次复制出的只读文件阻止第二次部署。
- `GX_MIRROR_DIRS` 或自动探测可把“引擎生成的 payload”同步到真实游玩目录，但绝不覆盖正版 `.big`、Data、存档和录像。
- 默认部署后刷新 app，可用 `GX_SKIP_APP_BUNDLE=1` 跳过。
- Cocoa 启动器与 `run.sh` 统一读取 `gx-font.conf`。

### 已确认的“假失败”

有两次看似“修复没用”，实际都是旧产物：

- app 内引擎比 build 输出旧，因此从 app 测试一直重现已经修好的面板问题；
- RelWithDebInfo 下 `DEBUG_LOG` 是空操作，不能期待用某条 DEBUG 日志证明面板代码已经运行。

发布或测试时应比较实际 app 内二进制与最新构建产物，并重新打包，不要仅凭部署目录或不存在的 DEBUG 日志判断。

## 十五、主要配置与诊断变量

### 持久配置

| 配置 | 含义 | 当前范围/默认值 |
|---|---|---|
| `GXRenderFPS` | 渲染帧率上限 | 30–240，默认 60 |
| `GXGameSpeedTenths` | 游戏速度，十分之一为单位 | 5–60，启动器默认 10 |
| `GXRenderScalePercent` | 相对原生 drawable 的渲染比例 | 50–100，默认 100 |

### 运维和诊断环境变量

| 变量 | 作用 |
|---|---|
| `GX_RUNTIME_ROOT` | 指定外接盘或其他运行根目录 |
| `GX_MIRROR_DIRS` | 冒号分隔的部署镜像目录 |
| `GX_SKIP_APP_BUNDLE` | 不自动刷新本地 app |
| `SAGE_PATCH_ENABLED` | 主动启用可选 SagePatch interposer |
| `GX_STATE_PROBE` | 打开节奏/过场状态诊断 |
| `GX_EXIT_UNFULLSCREEN_SECONDS` | 调整退出全屏等待时间 |
| `GX_EXIT_WATCHDOG_SECONDS` | 调整退出 watchdog 时间 |
| `GX_EXIT_DISPLAY_KICK` | 设为 0 时关闭最后的显示配置恢复 |
| `GX_EXIT_TEST_REFUSE` | 故意制造退出全屏失败以测试恢复链路 |

`GX_EXIT_TEST_REFUSE` 是受控故障测试开关，不是普通玩家设置。

## 十六、现有验证证据

开发日记中已经记录：

- 面板在窗口化和 Retina 全屏下的构建、关闭和重复打开；
- 滑块几何、刻度和数值往返；
- 60 FPS 显示与多步逻辑同时运行；
- 4.0× 逻辑速度不再改变渲染上限；
- 实时清晰度切换；
- 窗口拖动 resize 后 pillarbox 不再二次缩放；
- Finder 启动重新打包和签名后的 app；
- 原生全屏往返、鼠标释放和菜单栏；
- 修正窗口 ceiling 后全屏黑边消失；
- 退出菜单不再因空文字崩溃；
- Challenge 视频建立和播放路径；
- 强制制造退出全屏失败后，显示配置恢复链路可以救回桌面；
- 字体高频诊断清理；
- Noto Serif SC 冷下载、SHA-256 校验、重复部署、损坏修复和下载失败回退；
- `gx-font.conf` 在命令行和 app 两种启动方式下生效。

公开发行前仍应在干净环境手工完成：

1. 原生和低比例 Retina 全屏；
2. 窗口启动、拖动、清晰度切换；
3. 绿色按钮和 `Ctrl+Cmd+F` 双路径全屏往返；
4. 正常退出与受控恢复测试；
5. 战役脚本摄像机、快进和速度还原；
6. 将军挑战动画、播报和视频音画；
7. 远景步兵光照、屏幕滤镜和热扰动；
8. 中文菜单、建造说明、鼠标提示和混合文字；
9. 单机资金、录像回放和网络安全冒烟；
10. 另一台没有本项目历史配置的 Apple Silicon Mac 全新安装。

## 十七、GitHub 发布整理状态

### 本轮已经完成

1. 建立 `agent/macos-personal-customizations` 安全分支。
2. 正版资源、私有字体、存档、SDK、日志、本机归档和自制 ICNS 均不进入公开文件集。
3. 去除引擎和 Cocoa 启动器里的本机外接盘默认路径。
4. 清理启动器中已废弃的独立过场速度变量和渲染/逻辑 FPS 强制耦合。
5. 同步更新 macOS 构建文档，公开默认值统一为 60 FPS / 1.0×。
6. 加入可记忆的原生路径选择器：支持选择标记文件、游戏目录或共同上级目录，并分别识别《零点行动》和原版《将军》。
7. 增加中英双语 README、来源声明、工程日志和上手指南。
8. 公开打包改用上游已经跟踪的图标；本机自制 ICNS 已排除。

### 源码发布后的后续工作

- 将 DXVK 修复进一步整理为 DXVK fork 中的独立正式提交；当前 CMake patch 保证复现并注明对应上游提交。
- 检查跨层 macOS helper，能通过小型平台接口收口的继续收口。
- 验证共享 `Core/` 改动对原版 Generals、Linux、iOS、录像确定性和网络锁步的影响。
- 在另一台没有本项目历史配置的 Apple Silicon Mac 上按上手指南完成全新安装。
- 制作二进制 Release 前再次核对第三方库、字体、图标许可证和上游署名。

### 本分支保留的产品选择

- 公开默认速度为 1.0×，用户可自行保存 2.2×。
- 金钱滑块和 `Alt+N` 作为个人需求驱动功能保留，但只允许本地单机使用。
- CoreGraphics 显示配置恢复继续作为强保护下的最后手段。

## 十八、建议的提交拆分

建议按以下顺序组织 Git 历史，而不是一个五千多行的大提交：

1. `fix(filesystem): make asset discovery external-disk safe`
2. `build(macos): make local app packaging reproducible`
3. `fix(macos-hidpi): separate points, pixels, and render scale`
4. `fix(macos-window): follow native fullscreen and window resize`
5. `fix(macos-shutdown): recover stranded fullscreen spaces`
6. `fix(timing): decouple rendering from fixed-step simulation`
7. `fix(cinematics): make camera and video timing frame-rate independent`
8. `feat(extras): add localized display and speed panel`
9. `feat(trainer): add guarded single-player cash controls`
10. `fix(fonts): improve CJK fallback, metrics, and panel sizing`
11. `fix(input): restore keypad and SDL cursor overlays`
12. `fix(rendering): repair scaled render-target effects`
13. `fix(dxvk-macos): backport fixed-function color and pixel sizing`
14. `docs(macos): publish setup, controls, validation, and limitations`

资金修改应保持独立提交，使上游能够接受平台与正确性修复而不必同时接纳 trainer。

## 十九、当前结论

这批二次修改已经不只是“让程序能启动”，而是形成了完整的 macOS 使用层：真正的 Retina 渲染、可控窗口与原生全屏、独立帧率和速度、中文设置面板、可靠的字体方案、可复现 app 打包，以及一批在真实游玩中发现的渲染、输入、过场和退出修复。

下一阶段的核心不是继续堆功能，而是把已经好用的状态变成可维护的公开项目：冻结当前成果、清除本机路径和旧变量、把私有内容挡在 Git 外、按逻辑拆分提交，并在另一台干净机器上完成安装与验证。
