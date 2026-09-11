# TH06NC 的「SHT」

⚠️ **这一作没有 `.sht` 文件。** `.sht` 格式从 **TH07** 才有（`sht-webedit` 的 struct 表
最早是 `struct_07.js`，`thtk` / `truth` 更是完全不支持 sht）。TH06 与 TH06NC 的自机数据
**烧在 exe 里**，是一组静态表。所以本目录记的是「自机射击数据」这个**子系统**在这一作的形态，
不是某个文件格式。

| 入口 | 地址 | 当前命名 |
| --- | --- | --- |
| 自机型表（速度 + 发弹回调） | `0x1403de3b0` | `g_CharData`（4 项，步长 `0x20`） |
| 共同发弹实现 | `0x14006a580` | `Player__FireBulletFromTable` |
| ReimuA 火力表 | `0x1403dcdb0` | `g_CharacterPowerDataReimuA`（9 档） |
| ReimuB 火力表 | `0x1403dd530` | `g_CharacterPowerDataReimuB` |
| MarisaA 火力表 | `0x1403dcbd0` | `g_CharacterPowerDataMarisaA` |
| MarisaB 火力表 | `0x1403dc8a0` | `g_CharacterPowerDataMarisaB` |
| 当前火力 | `0x1404f1e88` | `g_GameManager__currentPower`（按 word 读） |

详见 [exe 内的自机火力表](01-in-exe-shot-tables.md)。表可以**离线**读出来，不用开 Ghidra：

```sh
python3 tooling/ghidra/th06nc_sht_dump.py            # NC 全表
python3 tooling/ghidra/th06nc_sht_dump.py --diff     # 与 TH06 原版逐字段比对
```

Ghidra 应用脚本为 [`tooling/ghidra/apply_th06nc_sht.py`](../../../tooling/ghidra/apply_th06nc_sht.py)，
要求样本 MD5 `b0cc28ec904e9efce2439ad857c938b5`。
