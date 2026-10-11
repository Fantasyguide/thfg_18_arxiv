# 108 · 发射口使魔「只建一侧」——引擎发几次的一手判定 + 多原点（同帧 2/4/5 次）落地

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引 TH08 处写成 `th08:0x…`。
> **方法**：① 逐指令反汇编 `local/th18.v1.00a/th18.exe` 的 `etOn` handler（`local/_scratch/thdis.py`，capstone 32 位）；
> ② 实跑 log 原文（`/mnt/d/项目/幻想指南/18/th18_modkit/game/th18_card_expand.log`，build `Oct 11 2026 03:28:24`，log:586）；
> ③ 22 份 ECL dump 的 `wait(` 分段普查（`local/_scratch/c108/th18ecl2/*.txt`，由委托人扫出 9 处）；
> ④ 卡侧改动 + 主机单测（`native/tests/test_youkai_core.c`）。
> **可信度**：§0/§1/§2 为 ✅（逐指令 + log 原文 + dump 原文）；§6 的「不回归」按闸与代码结构论证（未实跑）；
> 标 🟡 的集中在 §8。
> **被查对象**：`native/cards/youkai.c` · `native/cards/youkai_core.c` · `native/cards/youkai_core.h` ·
> `native/tests/test_youkai_core.c`（全部属卡 108 自己；**未动任何共享件**，见 §7）。

## 0. 结论（三条，最短版）

1. **引擎真的发 N 次**，不存在「同帧两次 `etOn` 合并成一次」。一手：`etOn` handler
   （`0x433f34`）的两条写路径（绝对路 `0x433f9c jmp 0x433ffa`、相对路 `0x433ff4` 直落）在 `0x433ffa`
   汇聚，其后 `0x43401a call 0x427810`（真发弹）**无条件**执行；整段没有任何「上次发过就跳过」
   的分支。我们只有**在站点返回非 0 时**才置 `retaddr = 0x43402d` 跳过它（否决语义，未变）。
2. **实跑直证站点每帧被进两次**：log:158633 `youkai/emit SITE: enemy_id 158 slot 0 call#=2 frame=9537`
   （slot 1 同帧同款；全 log **64 行**、帧 9537→9939、**每 12 帧**一次 = `st05mbs MBoss1_at` 的 `wait(12)`）。
   ⇒ **「只建一侧」的根因在我们这侧**：缓存/表键只有 `(敌人,槽)`，同帧第二次 `etOn` 把第一次的
   `props+0x08` **后写覆盖**了。log:158642/158643 那两条 SPAWN 落在**同一坐标** `(-65.5,152.7)`
   就是现场（旧版把两次写成一条、两只壳叠在同一点）。
3. **修法 = 键加一维「同帧第几次真发」**（`key = 槽 + 16×ord`），位置/寿命/关火**逐出点**独立；
   **不需要改 ECL**（§6 给结论：上游 leveledit 无需动作）。9 处多原点 ECL 全部由这一套通用覆盖，
   逐处期望壳数见 §3。

## 1. 为什么「引擎发两次」（一手）

| # | 证据 | 出处 |
| --- | --- | --- |
| ① | `etOn` handler（`0x433f34`）的两条写路径（绝对/相对）都落到 `0x433ffa`，其后 `0x43401a call 0x427810`（真发）**无条件**执行 | 反汇编 `0x433f34`–`0x43402d` |
| ② | 两条路都**先写 `props+0x08/+0x0c/+0x10`**（`0x433f8a/0x433f92` 绝对、`0x433fec/0x433ff4` 相对），写点全在站点 `0x433ffa` **之前** ⇒ 站点读到的就是**这一发**的出点 | 同上；`HANDOFF-108-EMITTER-STATIC-VERIFY.md` §1.2 |
| ③ | `0x433ffa`–`0x43402d` 之间只有四步（置保护半径 `0x42cca0` / 取 `&props[i]` / `call 0x427810` / 复位），**无比较、无标志位、无「已发过」状态** | 同上（全 `.text` 扫 `props+0x08` 写点/读点，7 处已穷举） |
| ④ | 实跑 log：`SITE … call#=2` 共 64 行（enemy 158 slot 0/1 各 32 行、每 12 帧） | log:158633 起 |
| ⑤ | 反证（旧版现场）：同帧两次 fire 只建出**一条**壳，且两次 SPAWN 同坐标 ⇒ 是「我们少一维」，不是「引擎只发一次」 | log:158642 / 158643 |

判据 ① 的逐指令原文（供复核）：

```
433f9e  lea ecx,[edi+0x44] / call 0x413280     ; 相对路：取敌人位置
433fb4  addss xmm2,[edi+esi*4+0x4e58]          ; + offsets.x
433fec  movq  [esi+0x5a0],xmm2                 ; props+0x08/+0x0c = 出点
433ff4  mov   [esi+0x5a8],eax                  ; props+0x10
433ffa  movss xmm1,[edi+0x5144]                ; ← 我们的站点（8 字节窗）
434013  lea eax,[esi+0x598]
434019  push eax
43401a  call 0x427810                          ; ← 真发弹（无条件）
43402d  jmp 0x436417                           ; opcode 收尾（被否决时直接跳这里）
```

★ 判据 ④ 的 `SITE` 行是**站点最顶端无条件**打的（在任何否决/早退之前），所以它数的是
「引擎进了我们几次」，不受闸的影响。旧文档
[`INVESTIGATION-108-ONE-SIDED-SHELLS.md`](INVESTIGATION-108-ONE-SIDED-SHELLS.md) §6-F 里
「引擎是否真发两次」标的是 🟡、靠 ECL 一手推断 —— **本条把它落定：✅ 真发**。

## 2. 9 处「同帧同槽多次 `etOn`」逐条判定（一手 dump）

判据 = 按 `wait(` 分段 + 段内同槽 `etOn` 计数（委托人扫出）；「出点形态」= 段内该槽的
`etOffsetRad`/`etOffsetAbs`/`etDist` 与 `etOn` 的相对位置（我逐条读 dump 原文）。

| 文件 | sub（首次行） | 槽 | 帧内次数 | 出点形态（dump 原文） | 期望壳数 | 处置 |
| --- | --- | --- | --- | --- | --- | --- |
| `st03.txt` | `GirlD01_at_520`（794） | 0 | **5** | 5 个 `etOffsetRad(0, θ, 64.0f)`，θ 均匀 72° 分五边形 | **5** | 我们这侧 |
| `st03.txt` | `GirlD02_at_484`（865） | 0 | **5** | 同上（逐字相同） | **5** | 我们这侧 |
| `st05mbs.txt` | `MBoss1_at_736`（96） | 0 | 2 | `%B∓90°`、r=70 两侧 | 2（与 slot 1 共点） | 我们这侧 |
| `st05mbs.txt` | `MBoss1_at_736`（101） | 1 | 2 | 与 slot 0 **逐字相同**的 `%B∓90°`、r=70 | （同上，合计 **2**） | 我们这侧 |
| `st06bs.txt` | `Boss1_984`（148） | 1 | **4** | `etCopy(1,0)` 后只改 `etDist(1, −88/−104/−120/−136)` ⇒ **出点同一个** | **1** | 我们这侧 |
| `st06mbs.txt` | `MBoss1_at_776`（97/103） | 0,1 | 2 each | `%B∓90°`、r=70（两槽逐字相同） | **2** | 我们这侧 |
| `st07.txt` | `GirlC01_at_360`（669） | 0 | 2 | **段内没有任何 `etOffsetRad`**（`offsets` 还是 0）⇒ 两次都在**母机本体上** | **0** | 距离闸（48 px）自动拒 |
| `st07mbs.txt` | `MBoss1_at_696`（91） | 0 | 2 | `%C∓90°`、r=70 两侧（`%B`/`%C` 反相推进） | **2** | 我们这侧 |

要点：
- **`etDist` 不是出点**（一手：`0x4251f9` 读到 `props+0x24` 才把子弹沿角度再推 dist px，**不改 `props+0x08`**）
  ⇒ `st06bs Boss1_984` 的 4 次 `etOn` 共用**同一个发射口**，正确观感是**1 只**（不是 4 只）。
- **两槽共点**（`st05/06/07mbs`）⇒ 4 次 `etOn` 落在 **2 个点**上，正确观感是**每侧 1 只 = 共 2 只**。
- **`st07 GirlC01_at_360` 是「本体口」**（`offsets` 全 0）⇒ 本来就**不该有壳**；本改动在这处
  一只都不建（我加的第二趟循环同样吃 48 px 距离闸）。
- `st03` 两处是唯一「N 次 = N 个不同点」的形态 ⇒ 5 只五边形使魔。**这处是我们这次唯一会
  「多长出壳」的场景**（旧版 1 只、新版 5 只），owner 若要收，把 `YK_EMIT_ORIGIN_MAX` 调成 2 即可
  退回「只认前两次」（见 §8-A）。

## 3. 改了什么（最小集；**只动 108 的文件**）

| 落点 | 改了什么 | 行号 |
| --- | --- | --- |
| `native/cards/youkai_core.h` | 新增键域：`YK_EMIT_ORIGIN_MAX`(6) / `YK_EMIT_KEYS`(96) / `YK_EMIT_ORIGIN_KEY` / `YK_EMIT_REAL_SLOT` / `YK_EMIT_KEY_ORIGIN`；`yk_slot_t.slot` 注明是「键」 | 938–946、1150 附近 |
| `native/cards/youkai_core.c` | `yk_map_find` / `yk_note_slot` / `yk_shot_slot_suppressed` 的界 `YK_EMIT_SLOTS`→`YK_EMIT_KEYS` | 500、579、776 |
| 同上 | `yk_map_enemy_slots` 改数**真实槽**的个数（`YK_EMIT_REAL_SLOT` 去重）⇒ 死亡撒点颗数**不变** | 443–467 |
| 同上 | `yk_fam_broken` / `yk_fam_gone` / `yk_fam_wave_clear` 改**扫全表**（同点共壳时一只壳被多条键引用 ⇒ 死亡/静默/清场必须一起处理） | 714、740、876 |
| `native/cards/youkai.c` | 新增 `yk_emit_fire_ordinal()`：本帧该 `(敌人,槽)` 的第几次真发（表 32 格、按帧清空） | 7278–7297 |
| 同上 | 站点 `yk_on_shot_slot`：`real`（引擎字段）与 `key = 槽 + 16×ord`（缓存/表/闸）分离；latch/sticky/map/gate 全部改用 `key` | 7309–7316、7346、7424、7476、7480 |
| 同上 | `YK_EMIT_LATCH_N` 32 → **64**（键 ×档） | 588 |
| 同上 | tick 新增**第二趟**循环（第 2..N 次真发）：每档一条独立条目；三道门与第一趟同口径 | 3406–3501 |
| 同上 | `YK_EMIT_SHARE_SAME_POS` 0 → **1**（「一个出点 = 一只使魔」；理由见 §5） | 733–747 |
| 同上 | `yk_run_action` 读引擎 `props` 前先 `YK_EMIT_REAL_SLOT(act->slot)`（否则读到 `props[16..]`） | 2869 |
| 同上 | 诊断：`SITE` 行加 `key`、`FIRE`/`ORIGIN-GATE` 加 `key` 并放宽额度、新增 `MULTI` 行、`yk_fk_bump` 表满抢占旧格、SPACE 击杀去重 | 7402、7414、7450、3479/3492、795–800、8673 |
| `native/tests/test_youkai_core.c` | 33 条新断言：键换算 / 两键独立 / 逐出点关火 / 共壳全表 broken / gone / wave_clear / 真实槽计数 / 键域上界 | 1342 起 |

**身份语义（写清楚，便于后来者对账）**：键 = `(enemy_id, slot + 16×ord)`；`ord` = 该帧内该
`(敌人,槽)` 第几次真发（0 起，站点无条件计数）。`ord == 0` 时键**逐值等于旧键** ⇒ 单次发弹的
场景（绝大多数）逐帧行为不变；`ord ≥ YK_EMIT_ORIGIN_MAX` 溢出并进最后一档（不丢壳、不多壳）。
`broken`/`fam_seen`/`fired`/`eton_gap`/`no_fam_frames` 全部**按键**独立；四放行口
（ECL 重新武装 / 波次边沿 `enmKillAll` / 相位撤壳 / 整表清 / TTL 600 帧）语义不变，只是现在
逐键生效。

## 4. 怎么验证（owner 一局要看哪几行）

打一局到 **5 面道中 boss**（或 3 面 `GirlD01/D02`、6 面 `Boss1`），看这几行：

| 想确认的事 | 期望日志行（grep 关键字） |
| --- | --- |
| 引擎确实同帧发两次，且**两次拿到不同的键** | `youkai/emit SITE:` → `enemy_id 158 slot 0 call#=2 key=16 frame=…`（同帧 slot 1 也一行 `key=17`） |
| 两侧各建了一只壳（**坐标为镜像的两点**） | `youkai/emit SPAWN:` → 同帧两行、`slot 0 key 0` 与 `slot 0 key 16`，`pos=` **相差 ≈140 px**（旧版是两行同坐标） |
| 5/6/7 面中 boss：合计**两只**（不是四只叠两点） | 同帧 `MULTI:` 行 + `CREATE shell` 只出 2 条；`SPAWN` 只 2 条 |
| 3 面 `GirlD01/D02`：**5 只** | 一个 `SITE` 组里有 `call#=2..5`（key 16/32/48/64）+ 5 条 `SPAWN` |
| 打死**一侧**的使魔只关那一侧 | `youkai/emit: BROKEN set for (enemy_id … slot 0 key 0) … EVERY key bound to this shell goes silent`；接着同帧的 `youkai/emit: gate check (… key 0 ord 0) … => veto=1`，而 `key 16` 那行 `=> veto=0` |
| 另一侧仍照常发弹 | 同帧 `gate check (… key 16 …) => veto=0`；下一波 `SITE` 里 key 16 仍在 |
| 该有的仍有壳、跟着母机动 | `youkai/emit TRACK:` / `TRACK2:`（`pinned_this_frame=1`、`fam_cur` 与 `target` 重合） |

判据（失败即回归）：**5/6/7 面中 boss 同屏 2 只、左右关于母机对称、随 `%B` 旋转；
打死左边 ⇒ 左边 `veto=1`、右边 `veto=0`；一面关底前 0 壳；5 面道中小怪 0 壳。**

## 5. 「一个出点 = 一只使魔」（`YK_EMIT_SHARE_SAME_POS` 重新打开的理由）

旧口径（2026-10-01 关闭它的原文）是：owner 看到「两个槽 ⇒ 一只壳」就否掉了 —— 那时的现场是
**每槽只剩最后一次 `etOn` 的出点**（后写覆盖），两个槽的点恰好重合，于是共壳把本该有的两只
并成了一只。**现在键带 `ord`**，同一槽的两个出点各成一条键：

- `st05/06/07mbs`：4 次 `etOn` / **2 个点** ⇒ 共壳给出 **2 只**（每侧一只）——正是 owner 要的观感；
  若不共壳会是 4 只**两两重叠**（视觉 2 只、但打死一只另一只还在 ⇒ 「打死使魔该口停火」会失效）。
- `st06bs Boss1_984`：5 次 `etOn`（slot 0 ×1 绝对锚 + slot 1 ×4 同点）⇒ **1 只**。
- 关火随之**逐出点**：`yk_fam_broken` 现在把**所有引用该壳的键**一起标 `broken`
  （否则被绑的那条会走 `broken == 0` 自行重建 ⇒ 那一侧「打死了又复活」）。

## 6. ECL：**不需要改**（给上游 leveledit 的结论）

- 9 处多原点全部可以由我们这侧通用覆盖（§2 表「处置」列全是「我们这侧」或「距离闸自动拒」）。
  **请上游不要为了这条改 `st05mbs`/`st06mbs`/`st07mbs`/`st03`/`st06bs`/`st07`** ——
  改了会与我们这套键冲突（例如「slot 1 相位 +180°」会变成 4 个不同出点、长出 4 只壳）。
- 唯一「原作意图不明」的是 `st03 GirlD01_at/GirlD02_at` 的 5 个 72° 出点：按 1:1 映射我们给 5 只；
  若 owner 认为那处只该有 1 只，**旋钮在我们这侧**（`YK_EMIT_ORIGIN_MAX` 2 ⇒ 只认前两次），
  **不必动 ECL**。

## 7. 影响面 / 共享件 / 回归

- **共享件：0 改动**。`native/sites.py` / `patch/**` / `sdk.*` / `Makefile` **一个字节都没动**
  （站点数仍 53，未新增站点）⇒ 无需 `AUDIT.md §BE` 登记。
  ⚠ 工作区里 `native/sdk.h` / `sdk_core.h` / `tests/test_sdk_core.c` 有**别人的**未提交改动
  （「出框免删」，2026-10-11 另一条线）—— **不是本文的改动**，我未触碰。
- **闸（全过）**：`cd native && make -s test-host`（`youkai_core: 12770 checks, 0 failed`）·
  `make -s dllx87`（`x87 in our objects: 0`）· `python3 tools/capcheck.py`（全绿）·
  `python3 tooling/check-docs.py`（全通过）。未 `make release`、未 commit。
- **不回归清单（逐条对代码结构论证）**：

| owner 口径 | 为什么不受影响 |
| --- | --- |
| 一面关底前 0 壳 | 9 处多原点全在 stage 3/5/6/7；stage 1/2/4 **没有任何**多原点 ECL ⇒ 第二趟循环逐帧空转 |
| 5 面道中小怪 0 壳（48 px 闸） | 小怪单次发弹 ⇒ 键 `slot+16` 从未被写；且第二趟同样吃 48 px 距阵闸 |
| 4 面大妖精 / 4 面道中 boss / 6 面道中妖精 / 米斯蒂亚 该有的仍有壳、跟着母机动 | 单次发弹 ⇒ 键 = 裸槽号，第一趟**逐字节未改**；位置仍走 latch 重建（相对量 + 每帧敌人位置） |
| 打死壳 ⇒ 该发射口停火（站点否决 + 四放行口 + TTL） | 闸/放行口/`yk_shot_slot_gate` 逻辑未改，只是现在**逐键**裁决；`yk_fam_broken` 从「第一条」变「全部引用」 |
| 母机绝不被误杀（空格调试键 + 父子匹配） | 空格击杀路径只加了「同一 id 去重」，判定链（`own_parents` skip / 实体存在 / `yk_shell_parent_ok`）未动 |
| 无体术（`e+0x6334=2`）、空格、`I` 无敌 | 三处调试代码未动 |
| 死亡撒点颗数（`yk_map_enemy_slots`） | 已改成数**真实槽** ⇒ 与改动前逐值相同（单测钉住） |

## 8. 🟡 不确定项

| # | 项 | 现状 |
| --- | --- | --- |
| A | `st03 GirlD01/D02` 从 1 只变 **5 只**是否合 owner 意 | 按「一个出点 = 一只」的 1:1 规则应给 5；若否，把 `youkai_core.h` 的 `YK_EMIT_ORIGIN_MAX` 改 2 即可退回（其余 8 处只需 2）。**未实跑验证观感** |
| B | `ord` 溢出（同帧 ≥7 次） | 站点把 `ord ≥ 6` 并进最后一档（key `slot+80`）⇒ 那一档的出点取**最后一次**写值；观察到的最大是 5，未触到 |
| C | 帧内 `ord` 计数表 32 格满（同帧 >32 个 `(敌人,槽)` 开火） | 溢出的那些 `(敌人,槽)` 退化成 `ord=0`（= 旧行为，只一侧）；实跑未见（FIRESUM ≈ 0.33 次/帧） |
| D | `YK_FAM_MAX = 32` 在多原点下的余量 | 一只 `st03` 敌人占 5 条；表满仍是**静默放弃**（`fam_full` 有计数）。未实跑统计 |
| E | 键在「ECL 改发弹顺序」时会换位 | 键 = 顺序序号；9 处的顺序在一波内固定，波间也固定 ⇒ 稳。若某 ECL 交替顺序，壳的身份会换位（观感同色，风险低）。**未找到反例** |
| F | 站点的 `ord` 与 tick 的键是否始终一致 | 只有实跑能证（这次 `SITE` 行会打出 `key`，一局即可对账）。逻辑上两者同源（同一函数） |
| G | 第二趟的表格/日志额度 | `s_mo_logged` 16 行、`SITE` 256 行、`FIRE` 512 行 —— 够一局定位，不保证覆盖全部 9 处 |

---

## 9. 追加（owner 实测两条：血量 / 波次重建）

> 本节的 log/行号均为**本轮追加改动**（`TUNING §2.1ccc-89`）；§1–§8 的结论不变。

### 9.1 ①「使魔的血量有点太多了」⇒ 每槽预算按出点数均分

| 项 | 内容 |
| --- | --- |
| 现象 | 每只壳都拿 `yk_map_fam_hp()`（母机 HP ÷3 × 机体系数；5 面中 boss 13000/3 ≈ **4333**）⇒ 多原点之后同槽 2 只 ⇒ 该槽总血量**翻倍** |
| 口径（owner） | **每槽总预算不变**，按该槽**本帧活跃出点数**均分：2 个 ⇒ 每只 **2166**；`st03 GirlD01_at` 5 个 ⇒ 每只 **866** |
| 实现 | 站点每次 `etOn` 写 `yk_origin_count_note(enemy, real, ord)`（`youkai.c:654-700`，本帧最大 `ord`+1、带 3 帧时效、表满 LRU）⇒ `yk_run_action` 建壳时 `yk_slot_origin_count()` 当除数（`youkai.c:2925-2943`） |
| 旋钮 | `YK_EMIT_HP_SPLIT_MODE`（`youkai.c:748-762`）：**1 = 均分（默认）** / 0 = 旧口径（每只满血） |
| 不回归 | 单出点场景除数恒 1 ⇒ `life` **逐位不变**（判据：`HP-SPLIT` 行**只**在多原点时出现） |
| 日志 | `youkai/emit HP-SPLIT: enemy_id N slot i key k has 2 active origins => life 4333 / 2 = 2166`（前 8 次） |

### 9.2 ②「没有随弹幕的波次重建」⇒ 波次边沿倍率 4 → 3

| 项 | 内容 |
| --- | --- |
| 根因 | 第五个放行口（`yk_shot_slot_gate` 的自适应 `etOn` 间隔）**早就存在**，但倍率写死 `4×`：`st05/06mbs MBoss1` 波内 12 / 波间 **42**（= 波内最后一轮 `wait(12)` + 外层 `wait(30)`）= **3.5 倍** ⇒ `42 < 48` **永不触发** ⇒ 打死一侧只能等 TTL 600 帧（≈10 s） |
| 修法 | 倍率改成常量 `YK_EMIT_WAVE_RATIO = 3`（`youkai_core.h:1074-1087`，3.5 的下取整）；判据不变：`gap >= max(24, 3 × 该键自己的最小间隔)`。`st05/06mbs` 42/12 ✓、`st07mbs` 40/10 ✓ 都放行；缓发敌人（波内 30）要 90 才算新波 ⇒ 不误放 |
| 数据 | 新增 `youkai/emit GAPS: enemy N slot i key k ord o gaps(last n of m)=12,12,12,42,0,… wave_thresh=24 (new wave = gap >= max(24, 3x this key's own min gap))`（每键前 8 次必打、之后只打疑似波边界，上限 64 行） |
| 判据（owner） | 打死左侧 ⇒ `gate check … key 0 … veto=1`；下一波 ⇒ `released by WAVE-GAP(adaptive) after vetoing K etOn(s)` + 次帧 `SPAWN: slot 0 key 0` |
| TTL | **保持 600**（owner 2026-10-01 拍板的「相对长」兜底）。改成 240（≈4 s）是一行（`youkai_core.h:1046`）+ 一条单测同步；**此决定待 owner 拍** |

### 9.3 本节闸

`make -s test-host` → `youkai_core: 12777 checks, 0 failed`（+7 条倍率断言）·
`make -s dllx87` → `x87 in our objects: 0` · `capcheck` 全绿 · `check-docs` 全部通过。

⚠ **并发写者**：本轮期间 `native/cards/youkai.c` 被**另一个写者**覆写过一次（§9 的改动一度消失，
已按同一份补回 + 重建 + 重跑闸）。发布前请核对 `youkai.c` 里这三个标记都在：
`YK_EMIT_HP_SPLIT_MODE` / `yk_origin_count_note` / `youkai/emit GAPS:`
（本节收尾时 `youkai.c` 的 md5 = `280bc6740473cbfbb3c7c44952b35914`，仅作对账参考）。

---

**改动授权声明**：本次只改 `native/cards/youkai.c`、`native/cards/youkai_core.c`、
`native/cards/youkai_core.h`、`native/tests/test_youkai_core.c`（均为卡 108 自己的文件）；
增/改的旋钮（`YK_EMIT_ORIGIN_MAX`、`YK_EMIT_SHARE_SAME_POS`、`YK_EMIT_HP_SPLIT_MODE`、
`YK_EMIT_WAVE_RATIO`、`YK_EMIT_LATCH_N`）已在代码注释里写明理由，数值口径全部登记在
[`TUNING.md`](TUNING.md) §2.1ccc-88 / §2.1ccc-89。
未 `make release`、未 commit（由上层的发布流程处理）。
