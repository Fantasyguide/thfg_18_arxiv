# dgVoodoo compatibility package

供 `th18_modkit` 启动器按需部署到 `game/` 的图形兼容组件，默认关闭。

- dgVoodoo 版本：2.86.2
- `D3D9.dll`：32 位 Direct3D 9 代理
- SHA256：`0c5704900f7b24ed1bc376f7b377f4df3e7edf3cd8506761eb959be8580b3e2d`
- 上游发布：<https://github.com/dege-diosg/dgVoodoo2/releases/tag/v2.86.2>
- 上游说明：<https://www.dege.freeweb.hu/dgVoodoo2/ReadmeGeneral/>

配置启用保持纵横比的无边框全屏、`flip_discard` 呈现和最大内部解析度。
这部分只改变图形代理和显示方式，手柄轮询卡顿由独立的
`th18_disable_controller_polling` thcrap 补丁处理。

如果游戏目录已有 ReShade、vpatch 或其他项目提供的 `d3d9.dll`，不要启用本组件，
除非已经确认代理链的加载方法。
