# TH06 基线与 TH06NC ECL 差异

状态：✅ 文件布局和主要运行时结构已在 NC 样本静态验证。
opcode 200/201 的语义**已收口**（201 是被证伪的），逐项对照见 [`02-th06-baseline-comparison.md`](02-th06-baseline-comparison.md)。

## 基线匹配

NC 的 ECL 记录具有 TH06 风格的 16-byte 文件头、12-byte 指令头和 8-byte timeline 记录。`RunEcl` 保留 current instruction、time gate、difficulty mask、负变量槽和 next-offset 控制流；timeline 仍是 0–12 的独立 switch。TH06 社区对应源码位于 `local/vendor/th06-re/src/th06_102h.exe/EclManager/` 与 `EnemyManager/`。

## NC 新增范围

| 范围 | 观察 | 置信度 |
| --- | --- | --- |
| 主 opcode 136–199 | 跳表逐项解出：**全部落默认** ✅。另有旧 opcode 127 `DEBUGWATCH` 也落默认，**TH06 原版同样如此** |
| 主 opcode 200 | **真新指令** ✅：TH06 `SHOOTOFFSET` 的极坐标版，`shoot_offset = (cos(a)·r, sin(a)·r, 0)`；名字仍暂定 🟡 |
| 主 opcode 201 | **不是新弹型** ✅：与 67–75 共用 handler，`aim_mode = 134` 在消费侧 switch（只实现 0–8、无 default）里无分支 | ✅ |
| timeline opcode 13 | loader 扫描并保存 checkpoint；不在 0–12 runtime switch 内 | 🟡 |
| timeline opcode 14 | 样本中仅出现一次（ecldata7，time 8499），8-byte 无参数记录；runtime 按默认 advance | 🟡 |

主跳表位于 `0x140028d78`，opcode byte-map 位于 `0x140028f5c`；timeline 跳表起点为 `0x140036f14`。不要把现代 TH16 的 300+ 游戏 opcode 模型套到这里。

## 样本与验证

- 7 个 NC ECL 文件均可由 `thecl -g 6` 解码；未知主 opcode 200 共 4 次、201 共 13 次，timeline 14 仅 1 次。
- 当前全局锚点：`g_EclManager=0x140a6eb70`、`g_EclFile=0x140a6eb78`、`g_EclSubTable=0x140a6eb80`。
- 复现：`source tooling/env.sh && python tooling/ghidra/apply_th06nc_ecl.py --dry-run`，然后重新导出 HTML。

## 下一步

1. ~~对 opcode 201 追到 BulletManager~~ —— 已收口，见 [`02`](02-th06-baseline-comparison.md) §3。
2. 🟡 **实机验证 opcode 201**：静态上它的 aim_mode 无分支实现，但样本里用了 13 次，
   一条明显退化的指令被反复使用是可疑的。在实机看到之前不下「NC 有坏指令」的结论。
3. ❓ 补齐 ECL loader 的精确函数边界和子表结构。
4. ❓ 为 enemy context 建立字段级结构（目前只有 `bullet_props` `enemy+0x26c`
   与 `shoot_offset` `enemy+0xc5c`），再比较 TH06 社区 `Enemy`。
