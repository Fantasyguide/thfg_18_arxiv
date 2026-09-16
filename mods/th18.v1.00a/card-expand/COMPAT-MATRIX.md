# COMPAT-MATRIX —— 106–117 全装时的兼容矩阵与判据

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`，md5 `9969cac756098c1da05a81de45437a70`）。
> 本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 目的（用户 2026-10-01 口径）：**装上 106–117 全部系统卡时，八个原语与它们互不打架** —— 给出矩阵 + 判据。
> 原语本身的「一手定位 / 预算 / 契约 / 验证批次」在
> [`engine/_shared/site-budget-and-primitives.md`](../../../engine/_shared/site-budget-and-primitives.md)；
> 本文件只管**全装时的占用与让位**。判读工具：`native/tools/verify.py`。

## 1. 站点账本（全进程共享，实测）

| 项 | 数 | 实据 |
| --- | --- | --- |
| 正式 patch 声明的断点 | **20** | `patch/th18.v1.00a.js`（名单见下方）|
| 测试 patch 追加 | **2** | `ce_test_deck` / `ce_trace_alloc`（`patch-test/th18.v1.00a.js`）|
| **合计（= 预算的度量）** | **22 / 24** | `native/sites.py check`（余量 **2**）|

**结论（写死进流程）**：预算按**全进程**算、与 patch 归档数量无关（分片实验已证 per-patch 是错的）⇒
**测试归档也吃这 2 条余量**。任何新原语**先算这 2 条**；能用函数调用 / 声明层 / 数据层解决的，一律不要站点。

正式 patch 的 20 个断点（`patch/th18.v1.00a.js` 一手）：

```
ce_card_bind          ce_enemy_damage     ce_enemy_death      ce_enemy_drop
ce_enemy_shot_slot    ce_gate             ce_item_collect     ce_item_commit
ce_item_frame         ce_item_money       ce_save_loaded      ce_shop_bought
ce_shop_reopen        ce_shot_damage      ce_snd_gate         ce_text_desc
ce_text_name          ce_text_notify      ce_unlock_all       ce_unlock_write
```

## 2. 共享回调热点（一条站点、多卡消费）

从 `native/cards/*.c` 的 `CE_CARD(...)` 注册块**自动抽取**（2026-09-17，35 张卡）：

| 回调 | 张数 | 用户（卡 id）|
| --- | --- | --- |
| `on_tick_2` | 25 | 106–117 全体 + 58/59/60/61/62/68/70/71/76/77/78/79/80 |
| `on_delete` | 24 | 同上（少 68 / 多 67/72/75）|
| `on_run_reset` | 22 | 106/107/108/109/110/111/112/113/114/115/116/117 + 67/68/71/72/75/76/77/78/79/80 |
| `on_stage_start` | 16 | 106/107/110/111/112/113/114/115/117 + 67/68/70/72/75/76/79 |
| `on_load` | 15 | 106/107/108/110/111/112/113/116/117 + 60/68/69/71/77/79 |
| `on_draw` | 11 | 106/107/108/110/111/112/113/115/116/117/80 |
| `on_enemy_death` | 6 | **108 / 109 / 110 / 112 / 113 / 117** |
| `on_bomb_spent` | 6 | 70/75/79/106/107/111 |
| `on_item_collect` | 5 | 106/107/110/111/114 |
| `on_item_frame` | 4 | **112**/113/114/106 |
| `on_shot_set_index` | 3 | 111 / 113 / 117 |
| `on_native_shot_suppress` | 2 | **108 / 109** |
| `on_option_shot_gate` | 2 | 113 / 117 |

**读法**：这条表就是"让位"发生的地方 —— 同一条站点把事件广播给所有注册者，
所以**新增原语优先并进这些热点**（`on_tick_2` / `on_enemy_death` / `on_item_frame`），
而不是新开站点。

## 3. 八原语 × 站点 / 让位

| 原语 | 站点 | 落在哪条既有站点 | 与谁共享 | 让位规则 |
| --- | --- | --- | --- | --- |
| 拦截 Z (09) | **0** | `ce_shot_set_index`（`0x45ebfb`，与 `on_native_shot_suppress` 同点）| 108 / 109 注册 suppress；111 / 113 / 117 注册 set_index | 117 的咆哮**先**问 set_index（它覆写 ⇒ 本帧不抑制）；`0x45eca0` 起的卡广播照跑 |
| 蓄力 (09) | **0**（读 `on_tick_2`）| `on_tick_2` | 25 张 | 只读输入位（`0x4ca428` bit `0x001`），**绝不写输入** |
| 暴风连锁 (09) | **0** | `on_enemy_death` + `on_tick_2` | 108 / 110 / 112 / 113 / 117 同吃击破广播 | 回调里只**读** `ev`：不析构敌人、不改掉落表（`pofv.c` §AO6）|
| 创建灵魂 (09) | **0**（自建池）| 无新站点 | 与 110 / 113 的「我们的召唤物算不算击破」问题同源 | 只认 `KILLED` + `F_REAL`（见 §4）|
| 创建使魔 (08) | **0**（数据驱动）| 行表 + 六个回调（`row+0x28..0x34`）| 108 自己 | 不改引擎射手表；自己的行数据自己负责 |
| 关闭发弹口 (08) | **0**（数据层优先）| 无新站点 | 108 自己 | 只有要"随时按住/放开"才用 1 条每帧家务；发布版不用 |
| 创建敌人 (12) | **0**（调用 `0x412990` 路线）| 无新站点 | 112 载体 / 108 使魔同样用它 | 禁止在敌人死亡栈里调（DEATH §2.4 重入）|
| 收点 (12) | **1**（既有 `ce_item_frame`）| `ce_item_frame` | 106 / 112 / 113 / 114 同吃道具事件 | 声明式层（`ce_drop_*` / `ce_score_*`）先声明后落笔，按让位阶梯排 |

## 4. 原语 × 卡 的交叉影响（要点）

| 交叉 | 现象 | 判据 |
| --- | --- | --- |
| 暴风连锁(09) × 108 使魔 / 113 召唤物 | 爆风是**自机侧伤害源** ⇒ 我们的召唤物在 109 眼里就是"小怪"，会被连带引爆 | 装 108+109 → 召唤使魔 → 打爆一只小怪，看 `pofv: blast chain N` 是否把使魔算进去（审计登记：[`CARD-RESOURCE-INTERACTION-AUDIT.md`](CARD-RESOURCE-INTERACTION-AUDIT.md) §1.8）|
| 创建灵魂(09) × 110 / 113 的击破判据 | 三张卡都只查 `KILLED && F_REAL` ⇒ **互相把对方的召唤物算成击破** | 同上的场景看 110 / 113 的计数是否一起涨 |
| 拦截 Z(09) × 117 咆哮覆写 | 同一条 `0x45ebfb` 上两家都要说话 | 装 109+117：咆哮帧**不能**被 109 抑制（否则 117 静默失效）|
| 收点(12) × 108 / 110 / 114 的道具链 | 都在 `ce_item_frame` / `on_item_collect` 上改写道具去向 | 全装后收一次点：P/金钱/分数三本账各自对得上（`ufo: BREAK …` + 114 的 2 倍 + 110 的信仰）|
| 创建使魔(08) × 117 的原生子机 | 两条路都动 option 槽 | 108 声明层 `ce_native_option_hide_declare` 必须**粘性**并在收尾撤（`sdk.h` 已注明）|

## 5. 全装验收判据（可一键跑）

```bash
# ① 发布 + 构建门（唯一发布路径）
MODKIT_DIR=<modkit> python3 native/tools/deploy.py
# ② 构建期门（缺一不可）
cd native && make check && python3 sites.py check && python3 tools/gen_def.py --check
# ③ 游戏里：组一副**含 106–117 全部 12 张**的卡组，进一关
# ④ 判读（用户实跑之后）
python3 native/tools/verify.py all --check
```

必须同时成立：

1. **卡在场**（防假绿）：日志里 `integrity[stage N]: card[i] id=… ` 逐条列出 12 个 id（106…117），
   少一个 ⇒ 后面所有绿灯都不算数（少一张卡的判据全绿 = **假绿**）。
2. **站点账本**：`sites.py check` 报的声明数 ≤ 24（当前 22），且 `gen_def --check` 双向一致。
3. **八原语**：`verify.py all --check` 的 8 组必需判据**全中**，且反例（`blast queue full` 等）能被解释。
4. **无静默失效**：日志里没有 `youkai: … NOT installed`、`ufo: … could NOT be spawned`、
   `senro: 结界环 VM 起不来` 这类"机制没装上"的行（出现即记为待修，不是通过）。

## 6. 已知冲突 / 挂账

- 「我们的召唤物被别家算成击破」：三张卡（109 / 110 / 113）同源，已登记在资源交互审计 §1.8；
  修法是统一判据（例如引擎侧给自建载体打标记位），**属于跨卡契约改动**，要按 §BE 登记后统一改。
- 站点余量只有 **2**：任何"顺手加一条站点"的做法都要先在这里记账，否则可能直接进崩溃区（实测 ≥24 崩）。
- `dllverify` 的 ordinal 循环在 `Makefile:262` 报假阳性（手工复现通过：38 导出 = 38 def 符号、零不匹配），
  与兼容性无关，仅记录避免误判。
