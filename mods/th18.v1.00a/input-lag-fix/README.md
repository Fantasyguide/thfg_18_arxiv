# TH18 controller polling patch

这是一个仅适用于《东方虹龙洞》v1.00a 的可选 thcrap 补丁，用来规避
XInput / DirectInput 手柄轮询被 vJoy 等虚拟设备拖慢的问题。

补丁让 `0x401860` 的 XInput 读取函数直接返回 0，并让 `0x401650` 的
DirectInput 读取函数原样返回已经收集的键盘输入。前者每帧依次查询 4 个控制器；
后者在设备丢失时可能单帧重试 `Acquire` 最多 400 次。两段都位于键盘读取之外。

## 行为边界

- 禁用 TH18 的 XInput 和 DirectInput 手柄输入。
- 保留键盘输入，以及 thcrap 的其他补丁和 DLL 插件。
- 不修改 D3D9、帧率限制或 dgVoodoo 配置。
- 默认不启用；需要传统手柄输入的玩家不要勾选。

## 发布

开发源位于 `patch/`。运行：

```bash
python3 mods/th18.v1.00a/input-lag-fix/release.py
```

会校验源文件并同步到本地 `th18_modkit` 发布仓库，但不会提交或推送。

## 验证

```bash
python3 mods/th18.v1.00a/input-lag-fix/test_patch.py -v
```

测试会核对零售 EXE 哈希、入口原字节、补丁内容、默认关闭状态和发布副本。

静态检查不能替代 Windows 上的实际设备测试。发布验收时保持会触发问题的 vJoy/手柄环境，
分别确认：启用补丁后卡顿消失且键盘/鼠标可用；禁用补丁后手柄输入恢复；
`thcrap/logs/thcrap_log.txt` 中两条 binhack 均显示 `OK`。
