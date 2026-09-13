# th17.v1.00b —《東方鬼形獣 ～ Wily Beast and Weakest Creature.》逆向工作区

> **版本**：TH17 v1.00b（`th17.exe`，imagebase `0x400000`）。本页只做**登记与导航**，
> 引擎结论一律写在 [`engine/`](../../engine/README.md) 对应子系统下，本页指回去。
>
> ★ **接着干看 [`HANDOVER.md`](HANDOVER.md)**：环境复现、已解清单、8 个未解项与下一步、符号回流待办、陷阱。

## 样本与工程

| 项 | 值 |
| --- | --- |
| 脱壳版 exe | `local/th17.v1.00b/th17.exe`，747,520 B，md5 `7a5b5bc1c231792098a28d8367928aaf` |
| 原包装版留档 | `local/th17.v1.00b/th17.exe.steamstub`，888,976 B，md5 `69a3bedc29172ba8d33cf0d73afc3f9d` |
| PE | i386，PE32，imagebase `0x400000`，时间戳 2019-08-10 17:36:44 UTC，入口 `0x47bb91` |
| 段 | `.text 0x401000`(0x98eba) / `.rdata 0x49a000`(0x1233a) / `.data 0x4ad000`(0x9c00) / `.rsrc 0x52b000` |
| 归档 | `local/th17.v1.00b/th17.dat`（THA1，79,861,613 B，`thdat -x 17` 解出 254 项） |
| Ghidra 工程 | `local/th17.v1.00b/ghidra_projects/th17.exe.{gpr,rep}` |
| 函数 dump | `local/th17.v1.00b/th17-funcs.json`（1967 条：`addr`/`name`/`size`/`xrefs`）|

> ✅ **Steamless 脱壳说明**：本机样本是 **Steam 包装版**（`.steamstub`），已用 Steamless 脱壳成
> `th17.exe`（747,520 B = 原版版大小）。脱壳会重建 PE 容器 → **文件 md5 与原版版不同**，但代码段字节是原版的。
> **抽检结论**：ExpHP `th-re-data` v1.00b 的 781 个函数地址里 **780 个**在脱壳版 Ghidra 库中精确落在同名函数入口；
> 动物灵 13 个函数序言逐一手检 13/13 合法。详细抽检记录见
> [`engine/animal/th17/01`](../../engine/animal/th17/01-animal-spirits.md) §1。

## 符号覆盖（读 `th17-funcs.json`）

| 来源 | 数量 |
| --- | --- |
| exe 内函数总数 | **1967** |
| 已命名（ExpHP + Ghidra 自动） | **1299** |
| 仍是 `FUN_` | **668** |
| ExpHP 函数名对齐 | 781 地址 / **780 命中**（1 例切分不同） |

> ⚠️ 本页统计来自 `th17-funcs.json`（2026-09-09 建库）；我们自己在 Ghidra 里新增的命名尚未 export 到
> `symbols.json`（该文件本目录暂缺）。

## 本作特有机制（只列名与指针，结论看 engine）

| 机制 | 一手文档 |
| --- | --- |
| 动物灵（token）掉落 / 运动 / 收集 / 5 格队列 | [`engine/animal/th17/01`](../../engine/animal/th17/01-animal-spirits.md) |
| 咆哮（ロアリング）/ 霊撃（HYPER）充能-激活-结束 | 同上 §5 |
| 「中弹只退咆哮不扣命」 | 同上 §6 |
| 咆哮 HUD（`beast.anm` 脚本号 / 槽位） | 同上 §7 |
| 跨版本动物灵矩阵 | [`engine/animal/OVERVIEW.md`](../../engine/animal/OVERVIEW.md) |
| 三兽灵选择（狼 / 獺 / 鷲）与 `GOAST` 字段 | 同上 §5.1 |
| 稀有动物灵 × 7（水母/牛/雏/龟/埴轮/埴轮马/三雏）与 ECL 落点 | 同上 §3.3 |

## 待挖图

1. **动物灵区的 `FUN_` 命名**：语义已在 [`engine/animal/th17/01`](../../engine/animal/th17/01-animal-spirits.md) 里写明，
   但工程里仍是 `FUN_`。建议按该文命名（地址 / 大小 / 交叉引用数）：

   | 地址 | 大小 | xrefs |
   | --- | --- | --- |
   | `0x40dec0` | 1131 | 2 |
   | `0x40f980` | 480 | 2 |
   | `0x40fb60` | 1232 | 1 |
   | `0x410030` | 331 | 2 |
   | `0x410180` | 114 | 1 |
   | `0x410200` | 375 | 4 |
   | `0x410640` | 631 | 1 |
   | `0x4108d0` | 520 | 1 |
   | `0x411210` | 149 | 3 |

2. **未命名的大函数**（按体积排序的前几名，多数是菜单/结局/主循环，未开工）：

   | 地址 | 大小 | xrefs |
   | --- | --- | --- |
   | `0x444bf0` | 5327 | 1 |
   | `0x490ea9` | 4955 | 3 |
   | `0x408610` | 3530 | 1 |
   | `0x453410` | 3269 | 4 |
   | `0x41c6b0` | 3156 | 1 |

3. **未解清单**：动物灵侧的 ❓ 项（写入者未定位 / 字段语义未定）集中在
   [`engine/animal/th17/01`](../../engine/animal/th17/01-animal-spirits.md) §10。
4. **符号回流**：干完活跑 `tooling/ghidra/symbols.py export th17.v1.00b` 存进本目录 `symbols.json`
   （见 [`tooling/ghidra/README.md`](../../tooling/ghidra/README.md) 的「两层符号」）。
