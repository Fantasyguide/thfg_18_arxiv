# th17.v1.00b 交接报告 —— 动物灵 / 咆哮（HYPER）线

> **版本**：TH17 v1.00b（`th17.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：一手 Ghidra 反编译 + `objdump` 全量反汇编 + `thdat`/`thecl`/`thanm` 解包 + ExpHP `th-re-data` 符号对账。
> **可信度**：结构体 / 字段 / 常量 / 调用链 ✅ 一手（附地址）；设计意图 🟡；写入者未定位 ❓（§4）。
> **写于**：2026-09-10（TH17 主线在 2026-09-09 的会话里做完；本报告是**交接时的状态快照**，读这一页就能接着干）。

本页是**操作面**：环境怎么搭、结论在哪、还差什么、怎么复现。
**引擎语义不在本页**——全部在 [`engine/animal/th17/01`](../../engine/animal/th17/01-animal-spirits.md)（429 行，§0–§12）。

## 0. 一句话状态

动物灵 / 咆哮（霊撃）**全链已一手解完**：掉落通道 → `zToken` 运动/寿命/变形 → 24 px 收集 →
`TOKENS[5]` 队列 → 咆哮就绪（时长表）→ 霊撃（消弹 / 无敌 / 伤害源）→ 「中弹只退咆哮不扣命」→ HUD。
剩的是 **8 个局部 ❓**（写入者 / 设计意图，见 §4）和 **工程侧收尾**（符号回流、`FUN_` 命名，见 §5）。

## 1. 环境与样本（从零复现）

| 项 | 值 |
| --- | --- |
| 脱壳版 exe | `local/th17.v1.00b/th17.exe`，747,520 B，md5 `7a5b5bc1c231792098a28d8367928aaf` |
| 原包装版留档 | `local/th17.v1.00b/th17.exe.steamstub`，888,976 B，md5 `69a3bedc29172ba8d33cf0d73afc3f9d` |
| PE | i386，imagebase `0x400000`，时间戳 2019-08-10 17:36:44 UTC，入口 `0x47bb91` |
| 段 | `.text 0x401000`(0x98eba) / `.rdata 0x49a000`(0x1233a) / `.data 0x4ad000`(0x9c00) / `.rsrc 0x52b000` |
| 归档 | `local/th17.v1.00b/th17.dat`（THA1，79,861,613 B） |
| Ghidra 工程 | `local/th17.v1.00b/ghidra_projects/th17.exe.gpr`（程序名 `/th17.exe`）|
| 函数 dump | `local/th17.v1.00b/th17-funcs.json`（1967 条）|

- **样本是 Steam 包装版**，用本仓 `tooling/steamless/` 脱的壳（`local/` 全部 gitignored，换机器要自己放样本，见 `local/README.md`）。
- **脱壳版 md5 ≠ 原版版**（只重建了 PE 容器），但代码段字节是原版的 —— 抽检：ExpHP 的 **781 个函数地址里 780 个**精确落在同名函数入口（唯一例外 `0x421120` 是 Ghidra 切分差异），动物灵 13 个函数序言 13/13 合法。记录见 [`engine/animal/th17/01`](../../engine/animal/th17/01-animal-spirits.md) §1。
- 环境（缺什么先 `python3 tooling/doctor.py`）：Ghidra 12.x + JDK 21 + 带 pyghidra 的 python；`source tooling/env.sh` 导出 `GHIDRA_INSTALL_DIR` / `JAVA_HOME`。
- ⚠️ **本机 `thanm` 没有 `spec` 子命令**：脚本文本用 `-l VERSION`，贴图用 `-x VERSION -u`。

## 2. 已解清单（一行一个主题，结论全部在 `engine/animal/th17/01`）

| 主题 | 状态 | 结论在哪 | 关键地址 |
| --- | :---: | --- | --- |
| 管理器与全局指针（`zGoastManager` 0x54 B）| ✅ | §2.1 | `0x40e420`、`0x4b7684` |
| `zToken` 字段表（0x70 B，上限 40 只）| ✅ | §2.2 | `0x410380`、`0x4111a0` |
| 动物灵来源：只有 2 条 spawn 通道 | ✅ | §3.1 | `0x4348b2`、`0x40eca9` |
| item type 16..33 ↔ token type 1..17 全表 | ✅ | §3.2 | `0x434740` |
| ECL 掉落点普查（`dropMain(33)`/`(30..32)`/`(19)`/`(20)`/`(22)`/`(23..29)`）| ✅ | §3.3 | — |
| 稀有动物 × 7 与 trophy 33..39 | ✅ | §3.3 | `0x410525` |
| 出场散射（24 px + 同向 0.96 → 转 222.5°）| ✅ | §4.1 | `0x410570` |
| 运动/收集/寿命/变形（240→180 帧、type 循环）| ✅ | §4.2–4.3 | `0x410af0` |
| 「没有 PoC 自动收集」（负结论）| ✅ | §4.4 | `0x40f980` |
| 咆哮字段与 `HYPER_FLAGS` 位表 | ✅ | §5.1 | inner `0x4b59dc` |
| 充能 → 就绪（时长表 840/1140/1500、+480、+180）| ✅ | §5.2 | `0x40f980`、`0x40fb60` |
| 霊撃（40 帧、半径 `(1−f⁴)×200`、消弹、60 帧无敌）| ✅ | §5.3 | `0x40f6d0` |
| 结束与超时释放（释放等级 0/2/3）| ✅ | §5.4 | `0x40e920` |
| 「中弹只退咆哮不扣命」完整链 | ✅ | §6 | `0x449420` → `0x447bf8` → `0x4495e0` |
| HUD（`beast.anm` 脚本号表、槽缩放、可见性闩锁）| ✅ | §7 | `0x410200`、`0x410180` |
| 跨系统接口（ECL 全局 99/100、炸弹屏蔽）| ✅ | §8 | `0x427b0e`、`0x411d40` |
| 三处既有说法订正 | ✅ | §9 | — |

**§9 的三处订正**（别人可能还在引用旧说法，务必先看）：

1. 「咆哮按 C」❌ —— 实际是**炸弹键**（输入掩码 bit `0x2`），槽满时引擎主动屏蔽炸弹。
2. 「`TOKENS[5]` 是 5 类计数」❌ —— 实际是**存 type 的 5 格队列**。
3. （TH13 侧）「`562 dropItemsSp` 是另一套掉落」❌ —— 与 509 共用同一段尾代码。

## 3. 关键地址速查

**函数**（复核：`python3 tooling/ghidra/scripts/decompile_funcs.py th17.v1.00b <地址…>`）

| 地址 | 作用 |
| --- | --- |
| `0x40e420` | 管理器构造（ExpHP 名叫 `operator new`，实为 ctor；`push 0x54`）|
| `0x40e750` | 管理器 `on_tick`（token 帧、HUD、计时）|
| `0x40e920` | 咆哮超时释放的爆散 |
| `0x40ee70` | 咆哮主循环（读输入位 → 霊撃）|
| `0x40f6d0` | `do_hyper_cancel`（霊撃本体）|
| `0x40f980` | 收集进队列 |
| `0x40fb60` | 就绪判定（选咆哮类型 + 时长）|
| `0x410180` | HUD 槽缩放驱动 |
| `0x410200` | HUD 槽位图标层（写 `vm+0x490`）|
| `0x410380` | `spawn_token` |
| `0x410570` | 出场散射 |
| `0x410af0` | 单只 token 每帧（返回 1 = 出列）|
| `0x4111a0` | `TokenList::insert_at_back` |
| `0x434740` | `ItemManager::spawn_item`（item → token 分派）|
| `0x449420` | `Player::die`（state 4）|
| `0x4495e0` | `Player::cancel_impending_death` |
| `0x4491b0` | 真死路径（扣命 / 清咆哮槽）|
| `0x411d40` | `player_is_trying_to_bomb`（咆哮期间屏蔽炸弹）|

**常量 / 全局**

| 地址 | 含义 |
| --- | --- |
| `0x4b7684` | `GOAST_MANAGER_PTR` |
| `0x4b59dc` | `zGlobalsInner` 基址（咆哮字段 +0x84…+0xe8，见 §5.1 表）|
| `0x4a13cc`–`0x4a13d8` | 咆哮时长表（1140 / 840 / 1140 / 1500）|
| `0x4a13a4` | `"beast.anm"` 名字串 |
| `0x4a3db0` | 24 px 收集半径（距离² 576.0）|
| `0x4a3dd4` / `0x4a3b48` | 60 px 减速半径（距离² 3600.0）与 ×0.5 |
| `0x4a3b0c` | 吸收速度系数 0.25 |

**`beast.anm` 脚本号**：token 本体 0..13（`type`）、槽 0x1d、槽位空框 0xe–0x12、槽位图标 0x18–0x1c、
就绪光效 0x1e/0x1f、光环 0x20、霊撃 0x21、カワウソ光球 0x25–0x27、类型光环 `type+0x28`、超时释放 0x2c。

## 4. 未解清单（8 项，带「下一步」）

原表在 [`engine/animal/th17/01`](../../engine/animal/th17/01-animal-spirits.md) §10，这里补上动手建议：

| # | 事项 | 现状 | 下一步建议 |
| --- | --- | --- | --- |
| 1 | `goast+0x50` 的写入者 | ❓ 只找到自减 `0x40f0b6` 与读取 `0x40ef63` | 见下方「#1 已试过的死路」 |
| 2 | 稀有动物进队列后影响的完整设计意图 | 🟡 代码面已解（改释放等级），但「为何 2/3 档特殊」无文档 | 交叉 `trophy.txt` 文案 + 游戏内实测；或看 8..14 号 token 的 `HYPER_FLAGS` 高半字节消费点 |
| 3 | `Player::create_damage_source` 第三参语义 | ❓ 霊撃与カワソ光球都传 5 | 反编译该函数 + ExpHP `zPlayerDamageSource` 字段，看 `+0xf188` 是什么 |
| 4 | 伤害源 `+0x4`（逐帧写半径）的字段名 | ❓ ExpHP 无名字 | 交叉 `BulletManager::cancel_radius_as_bomb` 的读点，确认单位与语义 |
| 5 | 稀有动物条件里的 ECL 变量 `[-9926]` | ❓ 只见 `unless ([-9926]==0) goto skip` | 反查 `[-9926]` 的写入点（ECL 全局变量表 / 引擎写点）|
| 6 | 输入掩码 bit → 虚拟键完整表 | 🟡 已坐实 bit0 射击 / bit1 炸弹·霊撃 / bit3 低速 | 逐位解 `read_keyboard_input` `0x401ab0`（含 `0x50/0x60/0x90` 复合位）|
| 7 | `hyper_type==2`（カワウソ）3 光球的命中数值 | 🟡 伤害 2、每帧取消子弹；半径来源未定 | 反编译 `FUN_00410030` `0x410030` 与 `0x40f3a1` 区段 |
| 8 | 咆哮超时释放 vs trophy 文案的措辞差异 | 🟡 代码里只有「未用霊撃」才爆散 | 实机跑一次：满槽不用霊撃等超时，看是否 5 颗 |

**#1 已试过的死路**（省一次重复劳动）：`python3 tooling/ghidra/scripts/find_struct_writes.py th17 0x50 --global 0x4b7684`
只命中 `0x410d79` / `0x410da2`（都在 `0x410af0` 里），但那两处的 `ESI` 是 **`zToken`**
（同段还写 `[ESI+0x44]/[ESI+0x48]/[ESI+0x4c]/[ESI+0x54]` = offscreen 计时器五件套）⇒ **不是** manager 的 `+0x50`。
下一步：改扫「先把 `[0x4b7684]` 装进寄存器、再拿该寄存器当基址」的形态，或确认它是否只被构造函数里的 `memset(this,0,0x54)` 覆盖。

> 另有两条**证伪式负结论**（已写进正文，别再翻）：动物灵**不参与 PoC 自动收集**；咆哮**不改自机伤害倍率**
> 之外的效果（如收集物换外观/翻倍）在 TH17 侧**没有证据**。

## 5. 工程侧待办（符号回流 + 命名）

1. **本目录缺 `symbols.json`** —— 我们自己在 Ghidra 里加的命名只活在 `local/`（gitignored）。
   干完活跑 `python3 tooling/ghidra/symbols.py export th17.v1.00b` 落盘；
   `symbols.py status th17.v1.00b` 随时对账（细则见 `tooling/ghidra/README.md` 的「两层符号」）。
2. **动物灵区的 9 个 `FUN_` 建议按结论命名**（语义已写在 `engine/animal/th17/01`，工程里还是 `FUN_`）：

   | 地址 | 大小 | xrefs | 建议名 |
   | --- | --- | --- | --- |
   | `0x40dec0` | 1131 | 2 | `GoastManager__build_hud_gauge` |
   | `0x40f980` | 480 | 2 | `GoastManager__collect_token` |
   | `0x40fb60` | 1232 | 1 | `GoastManager__check_hyper_ready` |
   | `0x410030` | 331 | 2 | `GoastManager__otter_orbs` |
   | `0x410180` | 114 | 1 | `GoastManager__tick_hud_scale` |
   | `0x410200` | 375 | 4 | `GoastManager__refresh_hud_slots` |
   | `0x410640` | 631 | 1 | `GoastManager__convert_leftover_tokens` |
   | `0x4108d0` | 520 | 1 | `GoastManager__token_pickup_effects` |
   | `0x411210` | 149 | 3 | `TokenList__remove` |

3. **大函数未开工**（多数是菜单 / 结局 / 主循环）：`0x444bf0`(5327)、`0x490ea9`(4955)、
   `0x408610`(3530)、`0x453410`(3269)、`0x41c6b0`(3156)。
4. 若继续扩到别的子系统：样本已在位，`tooling/ghidra/bootstrap.py` 幂等可重跑（别忘 `symbols.py export`）。

## 6. 复现命令（可直接粘）

```bash
# 0) 到仓库根，然后
source tooling/env.sh                              # 导出 GHIDRA_INSTALL_DIR / JAVA_HOME / THDAT / THANM / THECL
export PYTHONPATH="$PWD/local/pyghidra-site"       # 本仓的 pyghidra 装在这里（缺依赖先跑 tooling/doctor.py）

# 1) 全量反汇编（找「谁写了某字段」时最快）
objdump -d -M intel local/th17.v1.00b/th17.exe > /tmp/th17.asm

# 2) Ghidra 反编译（工程已在 local/，独占锁；别写死机器路径，靠 env.sh 探测）
python3 tooling/ghidra/scripts/decompile_funcs.py th17.v1.00b \
        0x40e420 0x40e750 0x40ee70 0x40f6d0 0x40f980 0x40fb60 0x410af0 0x434740

# 3) 按位移扫结构体写入（本会话新增的工具；示例：谁写了 goast+0x50）——同样需要上面的 PYTHONPATH
python3 tooling/ghidra/scripts/find_struct_writes.py th17 0x50

# 4) 解包资产（`$THDAT`/`$THANM`/`$THECL` 由第 0 步的 env.sh 导出；别忘了先 source）
"$THDAT" -x 17 local/th17.v1.00b/th17.dat /tmp/th17out/
"$THANM" -l 17 -m local/vendor/thpages/static/mapfile/v8.anmm /tmp/th17out/beast.anm
"$THECL" -d 17 -m local/vendor/eclmap/eclmap/th17.eclm /tmp/th17out/st01.ecl
```

## 7. 陷阱（踩过的）

- **脱壳版 md5 与原版不同**：别拿 md5 去对「是不是原版」；用**代码段对齐抽检**（§1）。
- **`thanm` 没有 `spec`**：脚本文本 `-l`、贴图 `-x -u`。
- **ExpHP 名字先当 🟡**：本次实测 `GoastManager::operator new` `0x40e420` 其实是**构造函数**；
  `on_draw` `0x40f970` 是纯 stub（`mov eax,1; ret`）。
- **字段位移**：TH17 的 `GOAST` 插在 inner `+0x1c`，导致 `CURRENT_LIVES` 在 `+0x64`、
  `CURRENT_BOMBS` 在 `+0x70` —— 拿别作的偏移硬套会错。
- **文档纪律**：地址写 `` `0x4b7684` ``（反引号、不带 `@`）；开头声明版本；
  写完跑 `python3 tooling/check-docs.py`（链接 / 地址 / 版本 / 表格四项）。
- **Ghidra 工程锁是独占的**：GUI / MCP / driver 同一时刻只能一个。

## 8. 相邻线接口

- **TH13 神灵 / 灵界**：机制同构（第三条资源轴 + 中弹保命），**代码不同源** —— 对照表在
  [`engine/animal/th17/01`](../../engine/animal/th17/01-animal-spirits.md) §11 与 [`engine/spirit/OVERVIEW.md`](../../engine/spirit/OVERVIEW.md)。
- **TH18**：**没有**动物灵 / 咆哮（残骸审计 [`engine/animal/th18/01`](../../engine/animal/th18/01-animal-residuals.md)）；
  唯一被继承的思路是 `Player::cancel_impending_death`（`th18:0x45d590`），我们的卡侧 deathbomb 钩子接在这条线上。
  **2026-09-10**：卡侧复刻已落地（card-expand id 117「鬼形兽」**Stage 2**）——
  [`mods/th18.v1.00a/card-expand/native/cards/animal.c`](../../mods/th18.v1.00a/card-expand/native/cards/animal.c) +
  `animal_core.c`（纯逻辑 116 断言），动物灵实体 = 自建 VM 池 + 原版 `beast1.png` 切图（DVD 式弹跳 /
  24 px 吸收 / 60 px 减速 / 变色 / 版边消失），审计 `AUDIT.md` §AL、实跑清单 `NEXT.md` §1v；
  本文 §4/§5 的掉落 / 队列 / 时长表 / 霊撃 / 中弹链数值被逐条照抄。
  **咆哮的 shot 组**（狼 = `set 11..14`、鹫 = `16..19`）也已解码并追加进 TH18 的 `pl0X.sht`：布局 / 全解码 /
  func 与 anm 映射见 [`engine/sht/th17/01`](../../engine/sht/th17/01-file-layout-and-shooterset-index.md)、
  [`02`](../../engine/sht/th17/02-hyper-shot-groups.md)。⚠️ 那两组**分机体**（霊夢 / 魔理沙 / 妖夢 字节全不同）、
  **不分火力档也不分 shot type**；TH18 只有霊夢 / 魔理沙能同源，**咲夜 / 早苗 目前是替身** ——
  待定方案（A 保持 / B 按本人原版弹幕放大 / C 借妖夢）与四个机体的原版弹幕对照写在
  [`mods/th18.v1.00a/card-expand/NEXT.md`](../../mods/th18.v1.00a/card-expand/NEXT.md) §1v-补。
- **跨版本台账**：[`engine/_shared/version-residuals.md`](../../engine/_shared/version-residuals.md)。
- **本作登记与导航**：[`INDEX.md`](INDEX.md)（样本 / 符号覆盖 / 待挖图）。

## 9. 证据与复核入口

- 一手正文：[`engine/animal/th17/01`](../../engine/animal/th17/01-animal-spirits.md)（§0 模型、§1 地址对齐、§2 字段、
  §3 掉落、§4 运动、§5 咆哮、§6 中弹链、§7 HUD、§8 跨系统、§9 对账、§10 未答、§12 复现）。
- 跨版本矩阵：[`engine/animal/OVERVIEW.md`](../../engine/animal/OVERVIEW.md)。
- 样本登记：[`INDEX.md`](INDEX.md)。
- 数据侧：`local/th17.v1.00b/th17.dat` → `thdat -x 17` → `beast.anm` / `st*.ecl` / `trophy.txt`。
