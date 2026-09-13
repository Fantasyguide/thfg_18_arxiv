# TH18 原生敌机/Boss 掉落道具 ECL 全量审计

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **方法**：`thecl -d 18 -x -m local/vendor/eclmap/eclmap/th18.eclm` 全量反编译 `local/th18.v1.00a/dat/*.ecl`（22 个文件），
> 逐指令统计 `dropMain` / `dropExtra` / `unknown573` / `ins_574` / `dropItems` / `enmCreate*` 第 6 参；
> item type 语义由 `ItemManager__spawn_item` `0x446f40`、`Enemy__drop_items_and_notify_cards` `0x430510`、
> opcode handler `0x433a14`（dropClear）/`0x433a24`（dropExtra）/`0x433ae9`（dropMain）一手反汇编交叉核对。
> **可信度**：ECL 掉落清单 ✅ 一手（原生 `.ecl` 字节 + thecl）；type 5/7 的「没有」是**全量枚举**（22 个 ECL 无一处写 5/7）✅；
> 引擎侧旁路 ✅；TH17 对照 🟡。
> **产物**：[`02-item-drops.tsv`](02-item-drops.tsv) = 843 行数据（逐站点；含 type 0 与不可达/debug 行，用 `reachability`/`item_type` 列过滤）。

## 0. 一句话结论

**原版 TH18 敌人/Boss 的 ECL 掉落里没有 type 5（整残机道具）也没有 type 7（整炸弹道具）。** 六面 + EX 的 22 个 ECL 里，
带 item type 的掉落 write 只出现 `0x01`(P)、`0x02`(钱)、`0x03`(P 档，仅未调用模板)、`0x10`–`0x13`(卡道具)、`0x14`(无效)；
其中 `0x10`–`0x13` 才是「一整张资源卡」的掉落渠道（大妖精 / 中 Boss / Boss 相位，共 71 个站点）。

- **type 5（整残机）**：全 exe 唯一写 item type 的 spawner `0x446f40` 的 9 个 caller 中，**没有任何一处 spawn type 5** ⇒ 原版 TH18 连生成 type 5 的代码都没有（§6）。
- **type 7（整炸弹）**：ECL 零处；唯一生成点是玩家侧卡 **id 31 辉夜** 死亡时喷 3 颗（`0x40d30b` → `0x446f40`），不是敌机/Boss 掉落（§6）。
- **type 6（符卡碎片）**：只在 `default.ecl` 的 `BossItemPhase3()` 模板体里出现；该子程序在 22 个 ECL 中**零调用**（§3.4）。
- **type 4（命碎片）**：只在引擎 spellEnd 路径、满足 card 40 KISERU 条件时掉 1 颗（`0x434fec` / `0x435045`），也不是 ECL 掉落（§6）。

## 1. item type 表（与本审计相关）

| type | 语义 | 一手出处 |
| --- | --- | --- |
| `0x01` | P 火力道具 | [`05-shop-and-money.md`](../../card/th18/05-shop-and-money.md) §8 |
| `0x02` | 金钱：`MONEY += 1` | 同上 |
| `0x03` | P 火力加一档 | 同上 |
| `0x04` | 命碎片（spellEnd 条件掉落，非 ECL） | `0x435045`，§6 |
| `0x05` | 整残机：`LIVES_STOCK += 1` | 05-shop §8；**全 exe 无 spawn**，§6 |
| `0x06` | 符卡碎片：3 个进位 1 炸弹 | 05-shop §8；**ECL 唯一 write 是 dead template**，§3.4 |
| `0x07` | 整炸弹：`CURRENT_BOMBS += 1` | 05-shop §8；**仅卡 31 辉夜**，§6 |
| `0x08` | 火力全满 | 05-shop §8 |
| `0x09`–`0x0e` | 小分 | 05-shop §8 |
| `0x10` | card id 3 EXTEND2（残机卡） | `0x4b40a0` = 3 |
| `0x11` | card id 4 BOMB2（炸弹卡） | `0x4b40a8` = 4 |
| `0x12` | card id 5 PENDULUM（+50 金） | `0x4b40b0` = 5 |
| `0x13` | card id 6 DANGO（+0.5 火力） | `0x4b40b8` = 6 |
| `0x14`+ | spawner 直接 `return 0`（不生成道具） | `0x446f4a`：`cmp edi,0x14; jl` |

> `0x4b4020` stride 8 表的后一列是 card id；type `0x10`–`0x13` 的 card id 3/4/5/6 一手 objdump 已核对。
> **注意**：card id 3 EXTEND2 / 4 BOMB2 的卡面效果是「残机碎片 / 炸弹碎片」（[`08-catalog.md`](../../card/th18/08-catalog.md) §C），
> 与 type 5 / 7 的「整残机 / 整炸弹」不是一回事。

## 2. 全量直方图：type → 掉落 write 站点数

**站点定义**：一处会写 item type 的 ECL 指令 = `dropMain(N)`、`dropExtra(type,count)`、`unknown573(type,count)`（限时加速奖励，见 §5）、
`enmCreate*` 第 6 参、`@BossItem`/`@BossItemPhase` 展开后的每一次 `dropMain`/`dropExtra`。
`dropItems()`（结算 flush）、`dropClear()`、`dropArea()`、`ins_574(time)` 本身不带 type，不计入。
「正常可用」= 从各面 `main` 沿 `@call` / `enmCreate` 字符串 / `setInterrupt` / `setTimeout` 可达；debug22 入口与零引用子程序归入「仅 debug/未引用」。

| type | 语义 | 全部 22 ECL 站点 | 正常可用（可达） | 仅 debug/未引用 |
| --- | --- | ---: | ---: | ---: |
| `0x01` | P（火力） | 304 | 266 | 38 |
| `0x02` | 钱 | 279 | 233 | 46 |
| `0x10` | EXTEND2（残机卡） | 19 | 19 | 0 |
| `0x11` | BOMB2（炸弹卡） | 36 | 36 | 0 |
| `0x12` | PENDULUM（+50 金） | 6 | 6 | 0 |
| `0x13` | DANGO（+0.5 火力） | 10 | 10 | 0 |
| `0x14` | 无效：spawner 拒收 | 2 | 2 | 0 |

> 读法：一个敌机可能同时有 `enmCreate` 主掉落 write 和自己的 `dropMain` write，二者都计入；
> 但主掉落只有最后一次 write 会真的生成（§5.2）。因此本表是**上界**，不是「实际生成颗数」。

## 3. 关键问题的正面回答

### 3.1 type 5 / 7：没有（可证伪的全量枚举）

全 22 个 ECL 的**全部** drop 指令 + `enmCreate*` 第 6 参里，item type 的字面值只有：

- `dropMain(...)` 第一参：`1`、`2`、`6`、`16`、`19`、`$A`、`$C`（后两者是 `default.ecl` 模板变量，§3.4/§4.2 展开后为 0/1/2/16/17/18/19/20）。
- `dropExtra(...)` 第一参：`1`、`2`、`3`。
- `unknown573(...)` 第一参：`1`、`2`。
- `enmCreate*` 第 6 参：`0`、`1`、`2`、`16`、`17`、`18`、`19`。

**没有 4、5、7，也没有 8/9–0x0e。** 复现命令与原始结果见 §7。
其中唯一的 `dropMain(6)` 在 `default.ecl` 的 `BossItemPhase3()` 模板体里（`+0x5d8`），该子程序零调用 ⇒ 不是实际掉落。

### 3.2 实际观测到的 type 汇总

见 §2 直方图。人工可读版：
- **P/钱**：`0x01` / `0x02` 是主体，几乎每个杂兵都有。
- **卡道具**：`0x10`–`0x13` 共 **71** 个可达站点，全部来自大妖精 `GirlC/D`、中 Boss、Boss 相位/终符（逐站点见 §4.1）。
- **无效**：`0x14` 只有 `st02bs BossCard1` 两处 `@BossItem(20,0,0)`，被 `0x446f4a` 拒收，等于不掉。
- **type 6**：唯一 write 是零调用模板，实际为 0。
- **type 3**：只有 `default.ecl BossItemPhase2` 的 `dropExtra(3,$A)`（零调用模板）；实际为 0。

### 3.3 卡道具 0x10–0x13 会掉，而且分布很广

**会。** 完整逐站点表见 §4.1；按资源卡汇总：

| 卡道具 | card_id | 出现位置（可达站点数） |
| --- | --- | --- |
| `0x10` EXTEND2（残机卡） | 3 | 全 7 面：大妖精 + st01/03/04/07 Boss 终符 + st03/05/06/07mbs 中 Boss（19 个站点） |
| `0x11` BOMB2（炸弹卡） | 4 | 全 7 面：大妖精 + 各面 Boss 多数相位（st07bs 就有 8 处；36 个站点） |
| `0x12` PENDULUM（+50 金） | 5 | 6 个站点：st02/03/04/06/07 大妖精 + st05bs Boss2 / st06bs Boss4 |
| `0x13` DANGO（+0.5 火力） | 6 | 10 个站点：st02/03/04/07 大妖精 + st04bs Boss2 / st05bs Boss4 / st06bs Boss3/6 |

### 3.4 boss 相位/符卡边界：用固定卡道具 + P/钱，不用命/炸弹

- Boss 相位（`BossN`）与终符结束（`BossDead`）走 `default.ecl` 的 `@BossItem(main_type, power, money)`：
  `dropMain(main_type)` + `dropExtra(1,power)` + `dropExtra(2,money)` + `dropItems()`；`main_type` 多为 `0x10`–`0x13`，少数为 `0`（不掉主资源）。
- 中 Boss 死亡走 `@BossItemPhase(power, money, main_type, area)`：`dropMain(main_type)` + `dropExtra(1,power)` + `dropExtra(2,money)`。
- `st04mbs MBossEscape` 是**超时逃走路**：`setTimeout(1, "MBossEscape")` 进入，此时 `$TIMEOUT != 0`，
  `@BossItemPhase(10,20,16,48)` 在模板开头被 `unless ($TIMEOUT == 0) goto end;` 跳过；紧接着的
  `dropClear(); dropMain(16); dropExtra(1,20); dropExtra(2,20); dropItems();`（`+0x1594`–`+0x1600`）才是实际掉落。
  ⇒ **stage 4 中 Boss 只有超时逃跑才掉 `0x10`；击破走 `MBossDead` 的 `@BossItemPhase(30,20,0,48)`，主资源不掉。**
- `st02bs BossCard1` 的两处 `@BossItem(20,0,0)` = 无效 type，实际无掉落。
- `BossEscape` 在各 `*bs` 文件里都是零引用子程序；`BossItemPhase2/3` 在 default 里零引用。

## 4. 逐站点记录

> 完整机器可读清单：[`02-item-drops.tsv`](02-item-drops.tsv)（843 行；
> 列 = stage / file / sub / dump_line / ecl_offset / call / item_type / count / reachability / text）。
> 下面只把「卡道具」与「Boss 相位」两类全部展开；P/钱的大宗重复站点见 §4.3 汇总与 TSV。

### 4.1 卡道具（`0x10`–`0x14`）逐站点

| stage | ECL 文件 | 子程序 | dump 行 | 指令 | 掉落 | 置信 |
| --- | --- | --- | --- | --- | --- | --- |
| st01 | `st01.ecl` | `MainSub06` | 1675 (+0x963c) | `enmCreate.item` `GirlD02` | `0x10` EXTEND2 ×1 | ✅ |
| st01 | `st01.ecl` | `MainSub06` | 1677 (+0x9680) | `enmCreate.item` `GirlD02` | `0x11` BOMB2 ×1 | ✅ |
| st01 | `st01.ecl` | `MainSub06` | 1678 (+0x96b0) | `enmCreate.item` `GirlD02` | `0x11` BOMB2 ×1 | ✅ |
| st01 | `st01bs.ecl` | `Boss2` | 174 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st01 | `st01bs.ecl` | `BossDead` | 712 (—) | `helper:BossItem.main` | `0x10` EXTEND2 ×1 | ✅ |
| st01 | `st01mbs.ecl` | `MBossDead` | 183 (—) | `helper:BossItemPhase.main` | `0x11` BOMB2 ×1 | ✅ |
| st02 | `st02.ecl` | `MainSub03` | 1168 (+0x62f8) | `enmCreate.item` `GirlD01` | `0x13` DANGO ×1 | ✅ |
| st02 | `st02.ecl` | `MainSub03` | 1173 (+0x63b0) | `enmCreate.item` `GirlD03` | `0x12` PENDULUM ×1 | ✅ |
| st02 | `st02.ecl` | `MainSub03` | 1174 (+0x63e0) | `enmCreate.item` `GirlD03` | `0x12` PENDULUM ×1 | ✅ |
| st02 | `st02.ecl` | `MainSub07` | 1269 (+0x6fc0) | `enmCreate.item` `GirlD03` | `0x11` BOMB2 ×1 | ✅ |
| st02 | `st02.ecl` | `MainSub07` | 1270 (+0x6ff0) | `enmCreate.item` `GirlD03` | `0x10` EXTEND2 ×1 | ✅ |
| st02 | `st02.ecl` | `MainSub07` | 1271 (+0x7020) | `enmCreate.item` `GirlD03` | `0x11` BOMB2 ×1 | ✅ |
| st02 | `st02bs.ecl` | `Boss2` | 180 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st02 | `st02bs.ecl` | `Boss3` | 341 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st02 | `st02bs.ecl` | `BossCard1` | 380 (—) | `helper:BossItem.main` | `0x14` 无效（spawner 拒收） ×1 | ✅ |
| st02 | `st02bs.ecl` | `BossCard1` | 381 (—) | `helper:BossItem.main` | `0x14` 无效（spawner 拒收） ×1 | ✅ |
| st02 | `st02bs.ecl` | `BossDead` | 900 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st02 | `st02mbs.ecl` | `MBossDead` | 181 (—) | `helper:BossItemPhase.main` | `0x11` BOMB2 ×1 | ✅ |
| st03 | `st03.ecl` | `MainSub05` | 1205 (+0x6444) | `enmCreate.item` `GirlD01` | `0x11` BOMB2 ×1 | ✅ |
| st03 | `st03.ecl` | `MainSub05` | 1207 (+0x6488) | `enmCreate.item` `GirlD01` | `0x11` BOMB2 ×1 | ✅ |
| st03 | `st03.ecl` | `MainSub05` | 1209 (+0x64cc) | `enmCreate.item` `GirlD01` | `0x13` DANGO ×1 | ✅ |
| st03 | `st03.ecl` | `MainSub07` | 1246 (+0x6910) | `enmCreate.item` `GirlD02` | `0x10` EXTEND2 ×1 | ✅ |
| st03 | `st03bs.ecl` | `Boss2` | 202 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st03 | `st03bs.ecl` | `Boss3` | 368 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st03 | `st03bs.ecl` | `BossDead` | 1002 (—) | `helper:BossItem.main` | `0x10` EXTEND2 ×1 | ✅ |
| st03 | `st03mbs.ecl` | `MBossDead` | 138 (—) | `helper:BossItemPhase.main` | `0x10` EXTEND2 ×1 | ✅ |
| st04 | `st04.ecl` | `GirlD01` | 445 (+0x1f9c) | `dropMain` | `0x13` DANGO ×1 | ✅ |
| st04 | `st04.ecl` | `MainSub02` | 911 (+0x3fc0) | `enmCreate.item` `GirlC01` | `0x12` PENDULUM ×1 | ✅ |
| st04 | `st04.ecl` | `MainSub05` | 1006 (+0x4c08) | `enmCreate.item` `GirlD01` | `0x11` BOMB2 ×1 | ✅ |
| st04 | `st04.ecl` | `MainSub07` | 1030 (+0x4eb0) | `enmCreate.item` `GirlD02` | `0x10` EXTEND2 ×1 | ✅ |
| st04 | `st04.ecl` | `MainSub07` | 1032 (+0x4ef4) | `enmCreate.item` `GirlD02` | `0x11` BOMB2 ×1 | ✅ |
| st04 | `st04.ecl` | `MainSub07` | 1033 (+0x4f24) | `enmCreate.item` `GirlD02` | `0x11` BOMB2 ×1 | ✅ |
| st04 | `st04bs.ecl` | `Boss2` | 202 (—) | `helper:BossItem.main` | `0x13` DANGO ×1 | ✅ |
| st04 | `st04bs.ecl` | `Boss3` | 367 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st04 | `st04bs.ecl` | `BossDead` | 1055 (—) | `helper:BossItem.main` | `0x10` EXTEND2 ×1 | ✅ |
| st04 | `st04mbs.ecl` | `MBossEscape` | 247 (—) | `helper:BossItemPhase.main` | `0x10` EXTEND2 ×1 | ✅ |
| st04 | `st04mbs.ecl` | `MBossEscape` | 249 (+0x15a4) | `dropMain` | `0x10` EXTEND2 ×1 | ✅ |
| st05 | `st05.ecl` | `MainSub08` | 1195 (+0x67e4) | `enmCreate.item` `GirlD03` | `0x10` EXTEND2 ×1 | ✅ |
| st05 | `st05bs.ecl` | `Boss2` | 172 (—) | `helper:BossItem.main` | `0x12` PENDULUM ×1 | ✅ |
| st05 | `st05bs.ecl` | `Boss3` | 306 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st05 | `st05bs.ecl` | `Boss4` | 481 (—) | `helper:BossItem.main` | `0x13` DANGO ×1 | ✅ |
| st05 | `st05mbs.ecl` | `MBossDead` | 115 (—) | `helper:BossItemPhase.main` | `0x10` EXTEND2 ×1 | ✅ |
| st06 | `st06.ecl` | `MainSub07` | 1339 (+0x6edc) | `enmCreate.item` `GirlD01` | `0x11` BOMB2 ×1 | ✅ |
| st06 | `st06.ecl` | `MainSub07` | 1341 (+0x6f20) | `enmCreate.item` `GirlD01` | `0x11` BOMB2 ×1 | ✅ |
| st06 | `st06.ecl` | `MainSub07` | 1342 (+0x6f50) | `enmCreate.item` `GirlD01` | `0x11` BOMB2 ×1 | ✅ |
| st06 | `st06bs.ecl` | `Boss3` | 328 (—) | `helper:BossItem.main` | `0x13` DANGO ×1 | ✅ |
| st06 | `st06bs.ecl` | `Boss4` | 405 (—) | `helper:BossItem.main` | `0x12` PENDULUM ×1 | ✅ |
| st06 | `st06bs.ecl` | `Boss5` | 462 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st06 | `st06bs.ecl` | `Boss6` | 505 (—) | `helper:BossItem.main` | `0x13` DANGO ×1 | ✅ |
| st06 | `st06mbs.ecl` | `MBossDead` | 131 (—) | `helper:BossItem.main` | `0x10` EXTEND2 ×1 | ✅ |
| st07 | `st07.ecl` | `MainSub02b` | 1241 (+0x68e4) | `enmCreate.item` `GirlD01` | `0x13` DANGO ×1 | ✅ |
| st07 | `st07.ecl` | `MainSub03` | 1247 (+0x6948) | `enmCreate.item` `GirlD01` | `0x10` EXTEND2 ×1 | ✅ |
| st07 | `st07.ecl` | `MainSub03` | 1249 (+0x698c) | `enmCreate.item` `GirlD01` | `0x11` BOMB2 ×1 | ✅ |
| st07 | `st07.ecl` | `MainSub03` | 1250 (+0x69bc) | `enmCreate.item` `GirlD01` | `0x11` BOMB2 ×1 | ✅ |
| st07 | `st07.ecl` | `MainSub05` | 1300 (+0x6f54) | `enmCreate.item` `GirlD01` | `0x11` BOMB2 ×1 | ✅ |
| st07 | `st07.ecl` | `MainSub05` | 1301 (+0x6f84) | `enmCreate.item` `GirlD01` | `0x13` DANGO ×1 | ✅ |
| st07 | `st07.ecl` | `MainSub05` | 1302 (+0x6fb4) | `enmCreate.item` `GirlD01` | `0x11` BOMB2 ×1 | ✅ |
| st07 | `st07.ecl` | `MainSub07` | 1366 (+0x7748) | `enmCreate.item` `GirlD02` | `0x13` DANGO ×1 | ✅ |
| st07 | `st07.ecl` | `MainSub07` | 1369 (+0x77d8) | `enmCreate.item` `GirlD02` | `0x12` PENDULUM ×1 | ✅ |
| st07 | `st07.ecl` | `MainSub07` | 1378 (+0x7988) | `enmCreate.item` `GirlD02` | `0x11` BOMB2 ×1 | ✅ |
| st07 | `st07.ecl` | `MainSub10` | 1508 (+0x8814) | `enmCreate.item` `GirlD03` | `0x10` EXTEND2 ×1 | ✅ |
| st07 | `st07bs.ecl` | `Boss10` | 169 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st07 | `st07bs.ecl` | `Boss2` | 246 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st07 | `st07bs.ecl` | `Boss3` | 329 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st07 | `st07bs.ecl` | `Boss4` | 416 (—) | `helper:BossItem.main` | `0x10` EXTEND2 ×1 | ✅ |
| st07 | `st07bs.ecl` | `Boss5` | 504 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st07 | `st07bs.ecl` | `Boss6` | 593 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st07 | `st07bs.ecl` | `Boss7` | 681 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st07 | `st07bs.ecl` | `Boss8` | 769 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st07 | `st07bs.ecl` | `Boss9` | 857 (—) | `helper:BossItem.main` | `0x11` BOMB2 ×1 | ✅ |
| st07 | `st07mbs.ecl` | `MBoss2` | 114 (—) | `helper:BossItem.main` | `0x10` EXTEND2 ×1 | ✅ |
| st07 | `st07mbs.ecl` | `MBoss3` | 157 (—) | `helper:BossItem.main` | `0x10` EXTEND2 ×1 | ✅ |
| st07 | `st07mbs.ecl` | `MBossDead` | 430 (—) | `helper:BossItemPhase.main` | `0x10` EXTEND2 ×1 | ✅ |

> `helper:*` 各行的 `dump 行` 是 thecl `-x` 文本的行号；`enmCreate.item` 行同时给出 `.ecl` 内偏移（`+0x…`）。
> 0x10–0x13 的可达站点共 71 个；其中 `st04mbs MBossEscape` 的 helper 行在运行时因 `$TIMEOUT != 0` 被跳过（见 §3.4），故**实际执行**的卡道具 write 是 70 个。
> `st04mbs MBossEscape` 的 helper 行被 `$TIMEOUT` 跳过，实际掉落是紧随其后的 `dropMain(16)`（见 §3.4）。

### 4.2 Boss / 中 Boss 相位掉落展开（全部 `@BossItem` / `@BossItemPhase` 可达调用）

| stage | ECL 文件 | 子程序 | dump 行 | 调用 | 展开后的掉落 | 置信 |
| --- | --- | --- | --- | --- | --- | --- |
| st01 | `st01bs.ecl` | `Boss2` | 174 | `@BossItem(17, 10, 10)` | 主BOMB2×1, 额外P×10, 额外钱×10 | ✅ |
| st01 | `st01bs.ecl` | `BossDead` | 712 | `@BossItem(16, 10, 10)` | 主EXTEND2×1, 额外P×10, 额外钱×10 | ✅ |
| st01 | `st01mbs.ecl` | `MBossDead` | 183 | `@BossItemPhase(10, 10, 17, 48.0f)` | 主BOMB2×1, 额外P×10, 额外钱×10 | ✅ |
| st02 | `st02bs.ecl` | `Boss2` | 180 | `@BossItem(17, 15, 15)` | 主BOMB2×1, 额外P×15, 额外钱×15 | ✅ |
| st02 | `st02bs.ecl` | `Boss3` | 341 | `@BossItem(17, 15, 15)` | 主BOMB2×1, 额外P×15, 额外钱×15 | ✅ |
| st02 | `st02bs.ecl` | `BossCard1` | 380 | `@BossItem(20, 0, 0)` | 主0x14(无效)×1, 额外P×0, 额外钱×0 | ✅ |
| st02 | `st02bs.ecl` | `BossCard1` | 381 | `@BossItem(20, 0, 0)` | 主0x14(无效)×1, 额外P×0, 额外钱×0 | ✅ |
| st02 | `st02bs.ecl` | `BossDead` | 900 | `@BossItem(17, 20, 20)` | 主BOMB2×1, 额外P×20, 额外钱×20 | ✅ |
| st02 | `st02mbs.ecl` | `MBossDead` | 181 | `@BossItemPhase(10, 10, 17, 48.0f)` | 主BOMB2×1, 额外P×10, 额外钱×10 | ✅ |
| st03 | `st03bs.ecl` | `Boss2` | 202 | `@BossItem(17, 20, 20)` | 主BOMB2×1, 额外P×20, 额外钱×20 | ✅ |
| st03 | `st03bs.ecl` | `Boss3` | 368 | `@BossItem(17, 20, 20)` | 主BOMB2×1, 额外P×20, 额外钱×20 | ✅ |
| st03 | `st03bs.ecl` | `BossDead` | 1002 | `@BossItem(16, 30, 30)` | 主EXTEND2×1, 额外P×30, 额外钱×30 | ✅ |
| st03 | `st03mbs.ecl` | `MBossDead` | 138 | `@BossItemPhase(10, 20, 16, 48.0f)` | 主EXTEND2×1, 额外P×10, 额外钱×20 | ✅ |
| st04 | `st04bs.ecl` | `Boss2` | 202 | `@BossItem(19, 40, 15)` | 主DANGO×1, 额外P×40, 额外钱×15 | ✅ |
| st04 | `st04bs.ecl` | `Boss3` | 367 | `@BossItem(17, 50, 20)` | 主BOMB2×1, 额外P×50, 额外钱×20 | ✅ |
| st04 | `st04bs.ecl` | `BossDead` | 1055 | `@BossItem(16, 80, 30)` | 主EXTEND2×1, 额外P×80, 额外钱×30 | ✅ |
| st04 | `st04mbs.ecl` | `MBossDead` | 202 | `@BossItemPhase(30, 20, 0, 48.0f)` | 主无×1, 额外P×30, 额外钱×20 | ✅ |
| st04 | `st04mbs.ecl` | `MBossEscape` | 247 | `@BossItemPhase(10, 20, 16, 48.0f)` | 主EXTEND2×1, 额外P×10, 额外钱×20 | ✅ |
| st05 | `st05bs.ecl` | `Boss2` | 172 | `@BossItem(18, 20, 10)` | 主PENDULUM×1, 额外P×20, 额外钱×10 | ✅ |
| st05 | `st05bs.ecl` | `Boss3` | 306 | `@BossItem(17, 20, 20)` | 主BOMB2×1, 额外P×20, 额外钱×20 | ✅ |
| st05 | `st05bs.ecl` | `Boss4` | 481 | `@BossItem(19, 10, 20)` | 主DANGO×1, 额外P×10, 额外钱×20 | ✅ |
| st05 | `st05bs.ecl` | `BossDead` | 1179 | `@BossItem(0, 80, 20)` | 主无×1, 额外P×80, 额外钱×20 | ✅ |
| st05 | `st05mbs.ecl` | `MBossDead` | 115 | `@BossItemPhase(30, 20, 16, 48.0f)` | 主EXTEND2×1, 额外P×30, 额外钱×20 | ✅ |
| st06 | `st06bs.ecl` | `Boss2` | 244 | `@BossItem(0, 30, 40)` | 主无×1, 额外P×30, 额外钱×40 | ✅ |
| st06 | `st06bs.ecl` | `Boss2` | 246 | `@BossItem(2, 50, 10)` | 主钱×1, 额外P×50, 额外钱×10 | ✅ |
| st06 | `st06bs.ecl` | `Boss3` | 328 | `@BossItem(19, 10, 40)` | 主DANGO×1, 额外P×10, 额外钱×40 | ✅ |
| st06 | `st06bs.ecl` | `Boss4` | 405 | `@BossItem(18, 20, 20)` | 主PENDULUM×1, 额外P×20, 额外钱×20 | ✅ |
| st06 | `st06bs.ecl` | `Boss5` | 462 | `@BossItem(17, 30, 40)` | 主BOMB2×1, 额外P×30, 额外钱×40 | ✅ |
| st06 | `st06bs.ecl` | `Boss6` | 505 | `@BossItem(19, 50, 40)` | 主DANGO×1, 额外P×50, 额外钱×40 | ✅ |
| st06 | `st06bs.ecl` | `BossDead` | 1901 | `@BossItem(0, 0, 60)` | 主无×1, 额外P×0, 额外钱×60 | ✅ |
| st06 | `st06mbs.ecl` | `MBossDead` | 131 | `@BossItem(16, 50, 50)` | 主EXTEND2×1, 额外P×50, 额外钱×50 | ✅ |
| st07 | `st07bs.ecl` | `Boss10` | 169 | `@BossItem(17, 50, 50)` | 主BOMB2×1, 额外P×50, 额外钱×50 | ✅ |
| st07 | `st07bs.ecl` | `Boss2` | 246 | `@BossItem(17, 40, 40)` | 主BOMB2×1, 额外P×40, 额外钱×40 | ✅ |
| st07 | `st07bs.ecl` | `Boss3` | 329 | `@BossItem(17, 40, 40)` | 主BOMB2×1, 额外P×40, 额外钱×40 | ✅ |
| st07 | `st07bs.ecl` | `Boss4` | 416 | `@BossItem(16, 40, 40)` | 主EXTEND2×1, 额外P×40, 额外钱×40 | ✅ |
| st07 | `st07bs.ecl` | `Boss5` | 504 | `@BossItem(17, 40, 40)` | 主BOMB2×1, 额外P×40, 额外钱×40 | ✅ |
| st07 | `st07bs.ecl` | `Boss6` | 593 | `@BossItem(17, 40, 40)` | 主BOMB2×1, 额外P×40, 额外钱×40 | ✅ |
| st07 | `st07bs.ecl` | `Boss7` | 681 | `@BossItem(17, 40, 40)` | 主BOMB2×1, 额外P×40, 额外钱×40 | ✅ |
| st07 | `st07bs.ecl` | `Boss8` | 769 | `@BossItem(17, 40, 40)` | 主BOMB2×1, 额外P×40, 额外钱×40 | ✅ |
| st07 | `st07bs.ecl` | `Boss9` | 857 | `@BossItem(17, 40, 40)` | 主BOMB2×1, 额外P×40, 额外钱×40 | ✅ |
| st07 | `st07mbs.ecl` | `MBoss2` | 114 | `@BossItem(16, 50, 50)` | 主EXTEND2×1, 额外P×50, 额外钱×50 | ✅ |
| st07 | `st07mbs.ecl` | `MBoss3` | 157 | `@BossItem(16, 50, 50)` | 主EXTEND2×1, 额外P×50, 额外钱×50 | ✅ |
| st07 | `st07mbs.ecl` | `MBossDead` | 430 | `@BossItemPhase(50, 50, 16, 48.0f)` | 主EXTEND2×1, 额外P×50, 额外钱×50 | ✅ |

### 4.3 直掉落指令（`dropMain` / `dropExtra` / `unknown573`）按文件汇总

不展开每个重复站点，只给「文件 × 指令 × type × 站点数」；逐站点见 TSV。

| ECL 文件 | 指令 | type | 站点数 |
| --- | --- | --- | ---: |
| `st01.ecl` | `dropExtra(type,count)` | `0x01` P | 6 |
| `st01.ecl` | `dropExtra(type,count)` | `0x02` 钱 | 5 |
| `st01.ecl` | `dropMain(N)` | `0x01` P | 8 |
| `st01.ecl` | `dropMain(N)` | `0x02` 钱 | 5 |
| `st01.ecl` | `unknown573(type,count)` | `0x01` P | 2 |
| `st01.ecl` | `unknown573(type,count)` | `0x02` 钱 | 8 |
| `st01mbs.ecl` | `unknown573(type,count)` | `0x02` 钱 | 1 |
| `st02.ecl` | `dropExtra(type,count)` | `0x01` P | 6 |
| `st02.ecl` | `dropExtra(type,count)` | `0x02` 钱 | 6 |
| `st02.ecl` | `dropMain(N)` | `0x01` P | 3 |
| `st02.ecl` | `dropMain(N)` | `0x02` 钱 | 2 |
| `st02.ecl` | `unknown573(type,count)` | `0x02` 钱 | 8 |
| `st02mbs.ecl` | `unknown573(type,count)` | `0x02` 钱 | 1 |
| `st03.ecl` | `dropExtra(type,count)` | `0x01` P | 3 |
| `st03.ecl` | `dropExtra(type,count)` | `0x02` 钱 | 5 |
| `st03.ecl` | `dropMain(N)` | `0x01` P | 2 |
| `st03.ecl` | `dropMain(N)` | `0x02` 钱 | 1 |
| `st03.ecl` | `unknown573(type,count)` | `0x01` P | 1 |
| `st03.ecl` | `unknown573(type,count)` | `0x02` 钱 | 7 |
| `st04.ecl` | `dropExtra(type,count)` | `0x01` P | 4 |
| `st04.ecl` | `dropExtra(type,count)` | `0x02` 钱 | 7 |
| `st04.ecl` | `dropMain(N)` | `0x01` P | 4 |
| `st04.ecl` | `dropMain(N)` | `0x13` DANGO | 1 |
| `st04.ecl` | `unknown573(type,count)` | `0x02` 钱 | 8 |
| `st04mbs.ecl` | `dropExtra(type,count)` | `0x01` P | 1 |
| `st04mbs.ecl` | `dropExtra(type,count)` | `0x02` 钱 | 1 |
| `st04mbs.ecl` | `dropMain(N)` | `0x10` EXTEND2 | 1 |
| `st05.ecl` | `dropExtra(type,count)` | `0x01` P | 7 |
| `st05.ecl` | `dropExtra(type,count)` | `0x02` 钱 | 5 |
| `st05.ecl` | `dropMain(N)` | `0x01` P | 1 |
| `st05.ecl` | `unknown573(type,count)` | `0x02` 钱 | 9 |
| `st06.ecl` | `dropExtra(type,count)` | `0x01` P | 9 |
| `st06.ecl` | `dropExtra(type,count)` | `0x02` 钱 | 5 |
| `st06.ecl` | `unknown573(type,count)` | `0x02` 钱 | 6 |
| `st07.ecl` | `dropExtra(type,count)` | `0x01` P | 15 |
| `st07.ecl` | `dropExtra(type,count)` | `0x02` 钱 | 13 |
| `st07.ecl` | `dropMain(N)` | `0x01` P | 1 |
| `st07.ecl` | `unknown573(type,count)` | `0x02` 钱 | 9 |

## 5. 指令语义（一手反汇编）

### 5.1 opcode → handler

TH18 的 ECL 游戏 opcode 在 `FUN_00430d30` `0x430d30` 里用两级表分发：
`opcode - 0x12c` 查字节表 `0x4366f4`，再查跳转表 `0x436438`。解析出的 drop 指令 handler：

| opcode | eclmap 名 | handler | 落到 |
| --- | --- | --- | --- |
| 506 | `dropClear` | `0x433a14` | `0x42d180`：memset `zEnemyDrop+4` 0x50 字节，清 timed 窗口 |
| 507 | `dropExtra` | `0x433a24` | `0x42d010`：`zEnemyDrop[type] = count` |
| 508 | `dropArea` | `0x433a8a` | `0x42d090`：写掉落位置/面积，不带 type |
| 509 | `dropItems` | `0x433abf` | `0x42d1e0`（取敌人位置）→ `0x42d0c0`（撒主掉落 + `Enemy__drop_items_and_notify_cards`） |
| 510 | `dropMain` | `0x433ae9` | `0x42d0b0`：`zEnemyDrop[0] = type`（主掉落） |
| 562 | `dropItemsSp` | `0x433ad1` | 同 `dropItems` 尾段（TH18 ECL 零使用） |
| 573 | `unknown573` | `0x433a4a` | `0x42d020`：`zEnemyDrop + 0x50 + type*4 = count`（限时加速奖励，§5.2） |
| 574 | `ins_574` | `0x433a70` | `0x42d040`：写限时窗口帧数 + 复位计时 |

- `zEnemyDrop` = `zEnemyData+0x5010`（= `zEnemy+0x623c`）：`+0x00 main_drop`，`+0x04 ex_drop[type-1]`（19 项，type 1..0x13），`+0x50` 起 `timed_ex_drop[type]`，`+0xa4` = 限时窗口帧数。
- `Enemy__drop_items_and_notify_cards` `0x430510`：循环 `ebx=0..0x12`，读 `zEnemyDrop+4+ebx*4`，调 `ItemManager__spawn_item` 时 type = `ebx+1`（`0x43061b`–`0x430624`）⇒ **`dropExtra(type,count)` 就是 type 型道具 count 颗**。
- `ItemManager__spawn_item` `0x446f40`：`cmp edi,0x14; jl ok; xor eax,eax; ret 0x20`（`0x446f4a`）⇒ **type ≥ 0x14 直接不生成**。
- `enmCreate*` 第 6 参写入主掉落：构造函数 `0x42d7d0` 读参数结构 `+0x10` → `enemy+0x623c`（`0x42d833`）；thecl 的 6 参数签名来自 [`th10.c`](../../../local/vendor/thtk/thecl/thecl10.c) 的 `mffSSS`（string,float,float,int,int,int）。
- `spellEnd` op 523 handler `0x434fec`：spell 活跃 + 有奖励 + `card_exists(40)` ⇒ `0x435045` spawn type 4（命碎片）1 颗；与 ECL `spellEnd()` 无关的 type 5/7 都不会出现（§6）。

### 5.2 主掉落 / 限时奖励的“最后一次写生效”

- 每个敌人出生时 `main_drop` 由 `enmCreate*` 第 6 参写一次；敌人自己的 ECL 后续若 `dropMain`/`dropClear` 会覆盖/清掉它。
- `unknown573(type,N)` 写 `timed_ex_drop[type]` + `ins_574(T)` 设窗口；`0x430510` 开头（`0x430544`–`0x430573`）按 `T` 剩余帧线性衰减后并入 `ex_drop`。TH18 只见到 `unknown573(1,..)` 与 `unknown573(2,..)`（§4.3）。

## 6. 非 ECL 的引擎侧 item type 来源（为什么 type 5/7 结论仍是“敌机不掉”）

全 exe 对 `ItemManager__spawn_item` `0x446f40` 的调用点只有 9 处；逐处看 item type 实参：

| spawner | type 实参 | 说明 |
| --- | --- | --- |
| 敌人死亡主掉落 `FUN_00430240` `0x430493`；`dropItems` 主掉落 `FUN_0042d0c0` `0x42d0ee` | `enemy+0x623c` | ECL `enmCreate`/`dropMain`（本文主体） |
| `Enemy__drop_items_and_notify_cards` `0x430624` | `ex_drop[type-1]` → type 1..0x13 | ECL `dropExtra`/timed |
| `FUN_00430d30` `0x435045`（op 523 spellEnd） | `0x04` | 持 card 40 KISERU 时命碎片 ×1 |
| `gen_items_from_et_cancel` `0x4291a7` / `0x429243` | `0x09` / `0x02` | 消弹生成小分/钱 |
| `CardYachie__recharge` `0x40d713` | `0x02` | 卡 34 额外金钱 |
| `Player__on_tick__body` `0x45c20a` | 表驱动（7 颗） | 玩家死亡撒道具，非敌机掉落 |
| `CardKaguya__on_player_death_after_deathbomb_frame_2` `0x40d30b` | `0x07`×3 | **整炸弹的唯一来源；玩家侧卡 31，不是敌机/Boss** |

⇒ **没有任何一处 spawn type 5**；type 7 只在玩家侧卡 31 出现。故「敌机/Boss 会掉整残机/整炸弹吗」答案是**不会**。
（另：`0x435045` 的 type 4 命碎片是唯一 ECL 外的敌侧资源碎片；`spellEnd` 是 Boss 符卡结束路径，但它掉的是“碎片”type 4，不是整命 type 5。）

## 7. 复现方法（命令 + 原始结果）

```bash
# 1) 反编译全部 22 个原生 ECL（-x 带偏移注释，-m 用 Priw8 eclmap 命名）
cd /home/nipporita/renkolab
mkdir -p /tmp/th18ecl/x
for f in local/th18.v1.00a/dat/*.ecl; do
  b=$(basename "$f" .ecl)
  local/vendor/thtk/build/thecl/thecl -d 18 -x -m local/vendor/eclmap/eclmap/th18.eclm "$f" "/tmp/th18ecl/x/$b.ecl.txt"
done

# 2) 列出全部 drop 指令的字面 type（type 5/7 的“没有”可直接 grep 出来）
grep -rhoE '(dropMain|dropExtra|unknown573)\([^)]*\)' /tmp/th18ecl/x/*.txt | sort | uniq -c
# 结果（节选）：dropMain(1)/(2)/(6)/(16)/(19)/$A/$C；dropExtra(1|2|3,...)；unknown573(1|2,...)

# 3) enmCreate* 第 6 参分布
python3 - <<'PY'
import re,glob,collections
pat=re.compile(r'\b(enmCreate[A-Za-z]*)\s*\(\s*"((?:[^"\\]|\\.)*)"\s*,([^;]*)\)')
c=collections.Counter()
for fp in glob.glob('/tmp/th18ecl/x/*.txt'):
    for line in open(fp,encoding='latin-1'):
        for m in pat.finditer(line):
            a=[x.strip() for x in m.group(3).split(',')]
            if len(a)>=5: c[a[4]]+=1
print(dict(sorted(c.items())))
PY
# 结果：{'0': 183, '1': 157, '2': 89, '16': 7, '17': 17, '18': 4, '19': 5}
# 没有 5/7/4/8/9..0xe；0/16..19 见 §2/§4。

# 4) 引擎 handler 反汇编（可选）
objdump -d -M intel --start-address=0x433a14 --stop-address=0x433b00 local/th18.v1.00a/th18.exe
objdump -d -M intel --start-address=0x446f40 --stop-address=0x447000 local/th18.v1.00a/th18.exe
```

> 逐站点 TSV 的生成脚本：`local/_scratch/audit_final.py`（本仓工作副本，未入库）。

## 8. TH17 对照（🟡，只作设计参考）

TH17 的 [`animal/th17/01-animal-spirits.md`](../../animal/th17/01-animal-spirits.md) §4 记录 `dropMain(19/20/33/30/32/22)` 是**动物灵 token 索引**（19/20 = 炸弹/残机碎片灵）。
TH18 的 ECL 掉落**没有动物灵**：`dropMain` / `dropExtra` 的第一参是**直接 item type**（1..0x13），
语义由 `0x42d0b0` / `0x42d010` + `0x430510` 的循环决定（§5.1）。两作的数字空间不能互相翻译：
TH18 的 `dropMain(19)` = `0x13` DANGO 卡道具，而不是 TH17 的残机碎片灵。卡扩展 id 117 若要复刻 TH17 的
「鱼→道具」行为，应在 TH18 item type 表（`0x4b4020`）上发 `0x10`/`0x11` 等卡道具，而不是套用 TH17 的 19/20。

## 9. 证据地址清单

| 事实 | 地址 |
| --- | --- |
| ECL drop opcode 跳转表 | 字节表 `0x4366f4` / 跳转表 `0x436438`（opcode 基址 `0x12c`） |
| `dropExtra` handler | `0x433a24` → `0x42d010` |
| `dropMain` handler | `0x433ae9` → `0x42d0b0` |
| `dropItems` handler | `0x433abf` → `0x42d0c0` |
| `dropClear` handler | `0x433a14` → `0x42d180` |
| `unknown573` / `ins_574` | `0x433a4a` → `0x42d020` / `0x433a70` → `0x42d040` |
| 主掉落循环 type 1..0x13 | `Enemy__drop_items_and_notify_cards` `0x430510`（`0x430544`–`0x4306cf`） |
| item spawner + type 上限 | `ItemManager__spawn_item` `0x446f40`（type ≥ `0x14` 拒收 `0x446f4a`） |
| `enmCreate` 第 6 参 → `enemy+0x623c` | `0x42d7d0`（`0x42d833`） |
| spellEnd type 4 | `0x434fec`（`0x435045`） |
| CardKaguya type 7 ×3 | `0x40d30b` |
| item type → card id 表 | `0x4b4020` stride 8；`0x4b40a0`/`0x4b40a8`/`0x4b40b0`/`0x4b40b8` = 3/4/5/6 |

## 10. 开放项

- 🟡 「正常可用可达性」是静态 call/字符串图推断；`debug22` 与零引用子程序已单列，但 `$SPELL_ID`（符卡练习）分支按可达计。
- 🟡 type `0x14`：`st02bs BossCard1` 写两次，但 spawner 拒收；ZUN 原意（TH17 残留？）未考。
- 🟡 `BossItemPhase2/3` 在 TH18 零引用：是 TH17/早期版本残留，未在原版 TH18 起效。
