# TH18 v1.00a controller polling audit

目标程序：`local/th18.v1.00a/th18.exe`

- SHA256: `5aeb74b19939a29a8cf06e4fe7777aaf9139e93478507e039f43318171020e9f`
- PE32 image base: `0x400000`
- 输入更新函数：`0x401c50`
- XInput 手柄读取函数：`0x401860`
- DirectInput 手柄读取函数：`0x401650`
- 每帧调用点：`0x453579 -> 0x401c50`
- 输入更新内调用点：`0x401c6d -> 0x401860`
- `XINPUT1_3.dll` ordinal 2 IAT：`0x4ad280`

`0x401860` 用设备编号 0–3 循环调用 XInput，并将轴和按钮映射为输入位。
返回后，`0x401c50` 继续调用 `GetKeyboardState` 并合并键盘输入。因此把入口
`55 8b ec`（`push ebp; mov ebp, esp`）替换为 `31 c0 c3`
（`xor eax, eax; ret`），让该函数报告“没有 XInput 手柄输入”。

键盘合并完成后，`0x401e01` 调用 `0x401650`。该函数通过 DirectInput 设备的
`Poll`、`Acquire` 和 `GetDeviceState` 读取手柄；设备丢失时 `Acquire` 循环上限为
`0x190`（400）次。补丁把函数入口改为 `mov eax, [esp+4]; ret 4`，直接返回调用者
传入的键盘输入位。

WinMM 的 `joyGetPosEx` IAT 实际位于 `0x4ad278`，引用点 `0x471687`、`0x471696`
和 `0x474ab2` 属于启动/设置路径，不是每帧输入路径，因此本补丁不改 WinMM。
