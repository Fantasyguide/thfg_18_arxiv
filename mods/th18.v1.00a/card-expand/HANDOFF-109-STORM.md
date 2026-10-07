# 109（花映塚 × 獣王園）· TH19 侧「暴风圈」交接文档

> **版本**：TH19 v1.10c（`th19.exe`，imagebase `0x400000`，脱壳副本 md5 `dd0e1ff8c02327bf6408f763b8beedc9`）。
> 对照样本：**体験版**（`th19tr/th19.exe`，md5 `bb18a734b0a45e3756d50981025d9a1b`）＋ **1.00a 未打包 `data/`**（`data/th19_100a.ver`）；路径与 md5 见 §8。
> **姊妹文档**：[`HANDOFF-109.md`](HANDOFF-109.md)（TH09 半：场 / 连爆机制）、[`OWNER-FEEDBACK-109.md`](OWNER-FEEDBACK-109.md)（owner 原话台账）、[`TUNING.md`](TUNING.md)（自定值）、[`CARDS.md`](CARDS.md)。
> **本文只讲 TH19 那个圈本体**：画法 / 触发链 / 半径律 / 判据 / 下一步。TH09 侧不在本文。
> ⚠ **所有权**：本文是新文件。`OWNER-FEEDBACK-109.md` 只**追加**了一节（§追加，未改原文）；若有异议按 `CLAUDE.md` 的协作纪律处理。

## §0 一句话现状（2026-10-05）

**目标对象核出**：`ghost.anm` **script16（启动器）→ script17（唯一的可见圈）**，`script18` 是加算孪生、实际不显形。
**半径/寿命不是写死的**：来自敌人 ECL 的 `ins_1028`（半径，float）与 `ins_1031`（寿命帧数），经**内嵌 `EnemyData`**（`Enemy+0x1230`）落到 `Enemy+0x6340` / `Enemy+0x634c`，死亡链读它们。
⇒ 旧的「混合 ∈ {覆盖, 减}」不成立（是**普通 α** + 顶点色）；旧的 `enemy.anm 271–273` 是**灵那一档**的附属圈，不是小怪这个圈。
⇒ owner 的纠正（「灵和普通敌人半径不一致」）**成立**，来源见 §4。

## §1 画法（一手，逐字）

`ghost.anm` script17（`thanm -l 19 ghost.anm`）：

```
sprite(-1);  layer(6);  drawPoly(1.0f, 22);
color(255,128,64);  color2(0,0,0);
angleVel(0.017453292f, 0.0f, 0.017453292f);
alpha(1);           alpha2(220);
scale(1.0f, 1.0f);
stop();  interruptLabel(1);
alphaTime(10,0,0);  alpha2Time(10,0,0);
+10: delete();
```

| 元素 | 值 | 观感 |
| --- | --- | --- |
| 图元 | `drawPoly(1.0f, 22)`，`sprite(-1)` | 22 边形圆盘，**无贴图**（脚本自己画） |
| 圆心 | `color(255,128,64)` + `alpha(1)` | 橙，α≈0 ⇒ **透明** |
| 圆周 | `color2(0,0,0)` + `alpha2(220)` | 黑，实 |
| 径向 | 顶点色 RGBA 线性插值 | 外黑 → **中段暗橙（血豆腐）** → 芯透 |
| 混合 | 不写 = 默认 0 | **普通 α 混合**（不是覆盖/减） |
| 自转 | `angleVel(1°/帧, 0, 1°/帧)` | 22 边形能看出转 |
| 大小 | 脚本只给初值 1.0 | 由引擎每帧写（§2） |
| **淡出** | `interruptLabel(1)` 块 = `alphaTime(10,0,0); alpha2Time(10,0,0); +10: delete()` | **最后 10 帧两个 alpha 一起线性收到 0**（一手见 §2「淡出」行）|

`script16` = `alpha(0); scriptNew(17); scriptNew(18); stop(); interruptLabel(1); +20: delete();`
`script18` = 同形状的**加算**版（`color2(0,0,0)` 加算 ⇒ 加 0 ⇒ 等于不画）；它也有同样的 `interruptLabel(1)` 淡出块。
**"这个 anm 就是 ghost.anm"的一手证据**：`0x51d200` 里 `FUN_004ce870(0x3d, "ghost.anm")` → 指针存进 `GhostCtrlInf+0xc8`；建 VM 的 `0x51cc70` 取的就是它（`mov 0x5d1bd8,%ecx; mov 0xc8(%ecx),%ecx`）。

## §2 触发链（死亡 → 圈）

| 步 | 地址（1.10c）| 做什么 |
| --- | --- | --- |
| 敌人死亡 | `0x50c1f0` | 读 `Enemy+0x6340`(float) / `0x6344` / `0x634c` / `0x6350`（读点 `0x50c41b`…`0x50c443`）|
| 生成 | `0x51d4e0` | 造「灵」对象；`switch(uVar8)`（类别：0/1/3/4，来自 `+0x63a0 & 0x700000`）|
| 爆风本体 | `0x51cc70` | 建 VM：anm=`ghost.anm`、**script `0x10`=16**、位置=死亡点；建伤害源 `FUN_005200f0`；挂缩放插值 |
| 插值 | `FUN_004044b0` | `(vm+0x1f8, 时长=ins_1031, mode=5, from=vm+0x54(=1.0), to=ins_1028)` |
| 每帧 | `0x51c8c0` | tick 插值 → 当前半径写进伤害源 `+0x18`；消半径内的**小弹**（`bullet+0x1044==1 && bullet+0x1078<4`）|
| **淡出** | `0x51c8c0` 开头 | `if (frame == lifetime-10) FUN_004cb830(vm, 1)` ⇒ 给爆风 VM 及两个子 VM 设 interrupt label `1`（下一帧生效）|
| 结束 | `0x51c7f0` | `lifetime <= frame` → tick 返回 1 → 解链 / 卸 VM / 销毁伤害源 / 释放（**正好是淡出结束那一帧**）|

⇒ 一次性覆盖 owner 的 5 条观感：死亡点出现 ✔ 消小弹+伤敌 ✔ 芯透 ✔ 外黑→中褐→透 ✔ 不到一秒长到最大（= `ins_1031` 帧数）✔ **最后 10 帧淡出** ✔
* 淡出的落地：`script17/18` 的 `interruptLabel(1)` 块 = `alphaTime(10,0,0); alpha2Time(10,0,0); +10: delete()` ⇒ 两个 alpha 10 帧线性收到 0；`script16`（不画东西）的 label-1 块是 `+20: delete()`。
* 寿命 30 帧（小妖精 A）⇒ 第 20–30 帧淡出；50 帧 ⇒ 第 40–50 帧；灵 40 帧 ⇒ 第 30–40 帧。
* 淡出期间**缩放插值仍在跑**（`FUN_00402c30()` 在分支之后每帧都调）⇒ 是「一边微涨一边淡出」，不是先涨完再淡。
* `FUN_004cb830`＝给 VM **及子 VM** 设 label（不立刻重跑解释器）；`FUN_004cb7a0`＝同一件事**外加**立刻 `FUN_004bd660()` 跑一次（两者语义已用 `ghost.anm script1` 反证：script1 是随机粒子爆发，若是 SetScript(1) 会变成粒子而非淡出）。

## §3 谜题：为什么静态找不到「写点」——`Enemy` 内嵌了 `EnemyData`

死亡代码在**外层 `Enemy`** 基址上读，而 ECL 跑在**内嵌的 `EnemyData`** 上，两者相差一个固定偏移 ⇒ 同一块内存两种视角。

| | 1.10c 正式版 | 体験版 |
| --- | --- | --- |
| 内嵌偏移 | `EnemyData = Enemy + 0x1230` | `Enemy + 0x1234` |
| 一手证据 | 12 处 `lea 0x1230(%e..),%ecx` + `call 0x509030`/`0x411320`（按 EnemyData 调方法）| `0x421b7d`/`0x42d230`/`0x42ecbd`/`0x431fd7`… 的 `lea 0x1234` |
| 交叉校验 | flags `0x63a0` − ECL 写 `0x5170` = `0x1230` | flags `0x63a4` − 同字段 = `0x1234` |
| 死亡处读 | `movss 0x6340(%esi),%xmm3`（`0x50c421`）| `movss 0x6344(%edi),%xmm3`（`0x4327d0`）|

ECL 指令 ↔ 字段 ↔ 死亡链用途：

| ECL | EnemyData | 1.10c 外层 | 用途 |
| --- | --- | --- | --- |
| `ins_1028(float)` | `+0x5110` | **`+0x6340`** | 圈的目标半径（插值 target）|
| `ins_1029(int)` | `+0x5114` | `+0x6344` | 伤害源 kind（`FUN_005200f0` 第 5 参）|
| `ins_1030(int)` | `+0x5118` | `+0x6348` | 体験版当参数用；正式版未读 |
| `ins_1031(int)` | `+0x511c` | **`+0x634c`** | 寿命帧数（= 插值时长 = 伤害源 HP）|
| `ins_1032(int)` | `+0x5120` | `+0x6350` | `FUN_00549780(arg)` |

* 1.10c 写点：`0x504e1b` / `0x504e42` / `0x504e67` / `0x504e8c` / case `0xb9`（→`0x5120`）；默认值 `0x50ac22`…。
* 体験版写点：`0x428c16` / `0x428c2c` / `0x428c40` / `0x428c54` / `0x428c68`；默认 `0x42fd03`…。

## §4 一手数值（`.ecl`，1.00a ↔ 1.10c 对差一致）

| 档 | `ins_1028` 半径 | `ins_1031` 寿命 | `ins_1029` kind | 出处 |
| --- | --- | --- | --- | --- |
| 小妖精 A | **30.0** | **30 帧（0.5 s）** | 6 | `WaveNN_GirlA01`（多数波次；部分有第二次 40/20 帧/kind 4）|
| 小妖精 B | **50.0** | 50 帧 | 6 | `WaveNN_GirlB01`（部分有第二次 80/60 帧 或 70/40 帧）|
| 纯灵 / 动物灵 | **80.0** | **40 帧（0.67 s）** | 10 | `default.ecl` → `GhostPureBlast` |
| 灵（测试形态）| 32.0 | 30 帧 | 10 | `default.ecl` → `GhostTest` |
| 未设时的默认 | 2.0 | 30 帧 | 10 | 机器码默认值（`0x50abb0`）|

* ⇒ **80 / 30 ≈ 2.67×**，对上 owner 口径「**灵 ≥ 小怪的 2 倍**」；小怪 A 的 30 帧 = **0.5 s**，对上「不到一秒长到最大」。
* **1.00a 与 1.10c 对差**：带爆风参数的子程序 54（1.00a） vs 56（1.10c）；只有 `GhostPure2b` / `GhostPure2G` 的 `ins_1029/1030` 有差；`ins_1028` 首值分布 **30.0×32 / 50.0×20 / 80.0×3 / 32.0×1** 完全一致 ⇒ **档位是稳定设计，不是补丁残留**。
* 灵的「多一圈」是**另一件事**：`0x51d4e0` 的 `case 1`（类别 1）才额外调 `0x51bb00`，其 VM 用 **`ghost.anm` 19/22/25/28**（暗圈·减算 + 亮圈·加算、scale 13/20、`scale(0.75)`、**会呼吸**）⇒ owner 的 A6「暴风不会呼吸抖动」排除的正是这一族，**别把它当成半径分档**。

### §4.1 伤害（2026-10-05 核查，含一次自我更正）

**伤害源的建点**（一手，`0x51cc70` 里）：`0x51cd74` 调 `FUN_005200f0`，实参 =（`out`、位置指针 = 死亡点、`param_4 = 0`（角度）、**`param_5 = *(obj+0x68)` = `ins_1031`**、**`param_6 = ins_1029`**），`xmm3 = 1.0f`（半径初值；之后 tick 每帧改成当前半径）。

**工厂落字段 / 源 tick 维护**（一手）：

| 字段 | 初值 | 每帧（源 tick `FUN_0051fa30`）| 语义（**由消费者读法定**）|
| --- | --- | --- | --- |
| `+0x84`(float) | `param_5` | `-= 1.0`；`(int)<1` ⇒ 删源（`FUN_00520320`）| **寿命**（帧）|
| `+0x80`(int) | `param_5` | `= (int)+0x84` | 本帧寿命（int）|
| `+0x7c`(int) | `param_5 - 1` | `= +0x80`（存上一帧）| 上一帧寿命 |
| `+0x98`(int) | `param_6` | 不变 | **伤害**（敌人侧直接当 dmg 读）|
| `+0xa4` | `1` | 不变 | **结算模数**（`+0x80 % +0xa4 == 0`）|
| `+0xac` | `0` | `-= 1` | 1 帧冷却（结算后由 `FUN_0051e5f0` 置 1）|
| `+0x9c/+0xa0` | `0 / 9999999` | `+0x9c += 伤害`；`≥ +0xa0` 删源 | 累计/上限 |

**敌人侧消费者 = `FUN_00520450`**（一手；敌人每帧调它算自己吃多少伤害）核心条件：

```c
if (*(int *)(src + 0x80) != *(int *)(src + 0x7c)          /* 寿命这一帧变了 */
 && *(int *)(src + 0x80) % *(int *)(src + 0xa4) == 0      /* 到结算点（+0xa4=1 ⇒ 每帧） */
 && *(int *)(src + 0xac) < 1                              /* 不在结算冷却里 */
 && *(int *)(src + 0x98) != 0                             /* 伤害非 0 */
 && FUN_0051e670(...)) {                                  /* 形状/OBB 重叠 */
      iVar3 = *(int *)(src + 0x98);                       /* ★ 伤害 = +0x98 */
      if (*(int *)(src + 0xb8) != 0)                      /* 可选：+0xb8 索引处理器表 */
          iVar3 = (*(&DAT_0059af54 + (+0xb8) * 4))(src, …);  /* 覆盖伤害（-1 = 不覆盖）*/
      local_48[0] += iVar3;                               /* 按类别取最强后累加 */
}
```

⇒ **伤害 = `+0x98` = 建源时传的 `param_6` = `ins_1029`**，而 `ins_1031` 是**寿命**。上一次说「伤害 = `ins_1031`」是**错的**（当时只看了工厂、没读消费者）；此处更正。

| 档 | 半径 `ins_1028` | **伤害 `ins_1029`/帧** | 寿命 `ins_1031` | 整段总伤害 |
| --- | :-: | :-: | :-: | :-: |
| 小妖精 A | 30.0 | **6** | 30 帧 | 180 |
| 小妖精 B | 50.0 | **6** | 50 帧 | 300 |
| 纯灵 / 动物灵 | 80.0 | **10** | 40 帧 | 400 |
| 部分档（`ins_1029(4)`）| — | 4 | — | — |
| `GhostPure2G` / `GhostPure2b` | 80.0 | **0**（条件要求 `+0x98 != 0` ⇒ 不造成伤害）| 40 帧 | 0 |

* ⇒ 卡侧现在写的 **`dmg = 1/帧` 偏软 6–10 倍**：照抄一手应是 **6/帧（小妖精）/ 10/帧（灵）**，寿命仍用 `ins_1031`（30/50/40 帧）。
* `ins_1029` 是**双重身份**：①敌人侧拿它当**伤害**（上面）；②暴风 tick 里 `switch (*(obj+0x6c))` 也用它决定「消掉的弹掉什么」（道具 / 分数，`FUN_0051c650` / `FUN_00549780`）。
* 对照（同一工厂，TH19 自身）：`AuraPL16Inf` 的 tick 在 `0x42b318` 传 `param_5 = 1`（寿命 1 帧）、`param_6 = 4`（伤害 4），且**每 7 帧重造一次** ⇒ 光环 ≈ 4 伤害 / 7 帧，与它的观感一致 ✓。

## §5 撤回与复核（请 owner 过目）

1. **撤回**我 2026-10-05 上一轮的结论「`Enemy+0x6340/+0x634c` 全 exe 无写点 ⇒ 半径对所有敌人一致」。
   错因：只在外层 `Enemy` 基址上找写点，没意识到 `EnemyData` 内嵌在 `Enemy+0x1230`。正确写点见 §3。
2. **A20（「anm 的 `color()` 是 B,G,R」）建议复核**。一手证据链：
   * `color`（opcode 404）处理器 `0x4bf419`：arg1→`vm+0x4ae`、arg2→`0x4ad`、arg3→`0x4ac`；`alpha` 处理器 `0x4bf3d2`：→`0x4af`。
   * 绘制把 `0x4ac` 整个 dword 当 D3DCOLOR 交给顶点（例 `0x4c54f9`）；D3DCOLOR 内存序 = B,G,R,A —— [th15 勘误](../../../engine/player/th15/01-graze-and-item-slowdown.md) 已确立这一点。
   * ⇒ `color(a,b,c)` = **R=a, G=b, B=c**。
   * owner 的两条观感（271 青 / 273 蓝）**与 RGB 不冲突**：271/273 走 `blendMode(2)` = **减算**（`0x4ca0b4` 档 2：`SRCALPHA / ONE / REVSUBTRACT` ⇒ `dst − src·α`），减掉橙/红自然剩青/蓝。
   * 交叉验证：`aura.anm` script11 = `color(255,255,255)` + `color2(255,255,0)`，owner 自己的描述是「白色半透明的圆 + 外面**黄色**的外发光」——只有 RGB 读成立。
   * ⇒ 本仓凡基于 A20 的色相结论都要翻回复核（109 现用配色首当其冲）。

## §6 卡侧动作（建议，待 owner 拍）

只动本卡自己的文件：[`native/cards/pofv.c`](native/cards/pofv.c)、[`native/cards/pofv_core.h`](native/cards/pofv_core.h)、[`native/tests/test_pofv_core.c`](native/tests/test_pofv_core.c)（若动 `assets/ability/scripts/**` 的 layer 归属，按 `CLAUDE.md` 的共享件声明）。

| 项 | 现状（enemy.anm 271–273 那版）| 一手（TH19 ghost.anm 16/17）|
| --- | --- | --- |
| 图元 | `drawPoly(1.2f, 12)` | `drawPoly(1.0f, 22)` |
| 混合 | `blendMode(2)`（减） | 不写（普通 α）|
| 内/外 | `alpha(224)` 芯实 / `alpha2(1)` 边透（反的）| `alpha(1)` 芯透 / `alpha2(220)` 边实 |
| 颜色 | 按 A20 读的色相 | `color(255,128,64)` / `color2(0,0,0)`（**R,G,B**）|
| 呼吸 | 23↔25 吞吐 | **无**（A6 已否）|
| 大小 | 卡侧每帧写半径 | 同构：初值 1.0 → 目标，`mode=5` |
| 半径/寿命 | 自定 | 小怪 **30 / 30 帧**；小怪 B **50 / 50 帧**；灵 **80 / 40 帧** |

**owner 一句话可验的判据**：无贴图无高光；外黑边；中段血豆腐褐；芯透；~0.5 s 长到最大；不呼吸；不自转得多（1°/帧）。

## §7 仍未做 / 下一步

1. §6 的落地 + `cd native && make check` 全绿 + 实跑判据（发版流程见 [`HANDOFF-112.md`](HANDOFF-112.md) §1，同一套）。
2. 灵的「额外对象」（`0x51d4e0` case 1 → `0x51bb00` → `ghost.anm` 19/22/25/28）**要不要复刻** —— owner 拍（复刻 = 灵那档多一圈且会呼吸）。
3. 一手回流 `engine/versus/th19/`（§1–§4 的地址表）：本工单遵 owner 要求**未读** engine 侧内容，故**没有**合并，避免与既有结论打架；回流时以本文 §1–§4 为准并互相核对。
4. 可选（非必需）：运行时 dump 一次 `Enemy+0x6340` 复核；静态链已闭环。

## §8 样本与复现

| 样本 | 路径 | md5 / 备注 |
| --- | --- | --- |
| 正式版（Steam，加壳原样）| `D:/SteamLibrary/steamapps/common/th19/th19.exe` | `8147b8d2bd8fe67b8d186e7fb3b5804d`，2,163,416 B |
| 脱壳副本（本仓用）| `local/th19.v1.10c/th19.exe` | `dd0e1ff8c02327bf6408f763b8beedc9` |
| 数据（1.10c）| `local/th19.v1.10c/th19.dat` = `D:/SteamLibrary/steamapps/common/th19/th19.dat` | `11a7e35dbc3cd267cfc3988810ba9299`（两边同 md5）|
| 1.00a 未打包数据 | `D:/SteamLibrary/steamapps/common/th19/data/`（43 个 `.ecl`；`th19_100a.ver`）| §4 的 1.00a↔1.10c 对差就是它 |
| 体験版 exe | `D:/117/桌面/迫真车万/th19tr/th19.exe` | `bb18a734b0a45e3756d50981025d9a1b`，867,840 B（§3 的 `0x1234` 对照）|
| TH09 样本（109 另一半）| `D:/117/桌面/迫真车万/[th09] 东方花映塚 (汉化版+日文版)/th09.exe`；仓内 `local/th09.v1.50a/` | 本次未用 |

```bash
# anm 脚本（thtk 需给版本号 + anmmap）
local/vendor/thtk/build/thdat/thdat -x 19 local/th19.v1.10c/th19.dat -C /tmp/ext ghost.anm aura.anm
local/vendor/thtk/build/thanm/thanm -l 19 /tmp/ext/ghost.anm \
    -m local/vendor/thpages/static/mapfile/v8.anmm > /tmp/ext/ghost.txt
# ecl（无 th19.eclm，用原始 opcode 名）
local/vendor/thtk/build/thecl/thecl -d 19 /tmp/ext/default.ecl > /tmp/ext/default.txt
# 机器码核对（不需要 Ghidra；PE 直接 objdump）
objdump -d --start-address=0x50c1f0 --stop-address=0x50c4a0 local/th19.v1.10c/th19.exe
```
