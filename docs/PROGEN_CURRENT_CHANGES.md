# ProGen 分支当前修改

`progen` 是 `main` 的增量分支，用于本地 ProGen 26 配置。它先继承原版《零点行动》与两个版本共用的 macOS 修复，再加入以下 ProGen 专属内容。

## 源码修改

- 新增 `SPECIAL_TOMAHAWK_STORM` 特殊能力类型，并接入 Zero Hour 的能力名称表。
- 允许 Tomahawk Storm 通过位置、目标和一般特殊能力检查。
- 将 Tomahawk Storm 接入超级武器建造完成、就绪和敌方提示流程。
- 将 Tomahawk Storm 状态接入游戏内超级武器提示绘制路径。
- 在 `ProGen_TomahawkStormMissile` 末端俯冲时，按真实物理速度调整客户端模型方向，使导弹模型沿实际弹道飞行；该修改不改变位置、轨迹、爆炸或伤害逻辑。

## ProGen 模组与覆盖文件

分支在 `游戏文件/GeneralsZH-ProGen26/` 中保存当前配置实际使用的模组文件和 `GeneralsX.biglist`：

- ProGen 26 的 Art、Data、English、Maps、Scripts 和 Window 包。
- `!000_ProGenLaserComancheFix.big`，用于当前 ProGen 配置中的激光卡曼奇修复。
- `!MenuMusicOriginal.big`，用于恢复本地配置使用的原版菜单音乐。
- GenTool / Control Bar Pro 相关的 4K Control Bar 覆盖文件。
- Boss AI 包，以及与 `main` 共用的 Expanded LAN Lobby Menu 和 DecalsZH 引用。

`GeneralsX.biglist` 同时列出运行所需的原版 Zero Hour BIG 文件，但这些商业游戏资源没有提交到 Git。该目录不是一份可以脱离正版游戏直接运行的完整游戏副本；使用者必须自行提供合法的原版文件。

## 修改目的

这里的大部分修改来自本地实际游玩中遇到的 Bug、兼容性问题或模组与 GeneralsX/macOS 运行环境之间的差异。少量内容属于个人单机体验调整，例如界面缩放、Control Bar 布局、快捷操作、镜头和菜单音乐。

感谢 ProGen 的作者与维护者提供 ProGen 的玩法和资源内容。感谢 GenTool，以及本地配置中使用的 Control Bar Pro 内容。本 fork 只主张这里新增的兼容性修复和个人调整，不主张第三方模组原始内容的作者身份。

## 验证边界

这些内容来自已进行过本地问题排查和分项测试的工作树；当前 GitHub 发布分支已经完成源码差异、BIG 文件边界、文件体积和载入清单检查，但尚未在这个最终组合提交上重新完成整包构建与游戏内回归。因此，仓库状态不等同于一个已经全面验收的二进制发行版。
