# HANDOFF-108 · leveledit（第三方米斯蒂亚补丁）**同开**时的使魔归属

> **本文是什么**：owner 2026-10-15 定的口径 + 到本文为止**已核到什么程度** + 还缺什么 + 怎么补。
> **硬约束**：**不改 leveledit 任何文件** ✗；只在**我方 108**（[`native/cards/youkai.c`](native/cards/youkai.c)）里加门/接线 ✓。
> **不合并两个 mod** ✗（它们是两个各自独立的 thcrap 补丁 ✓；同开兼容的护栏见 [§BE-COEXIST-GUARD](AUDIT.md) 与
> [`native/tools/check_modkit_packages.py`](native/tools/check_modkit_packages.py) ✓）。

## 1. owner 口径（逐条，硬约束 ✓）

1. **那场 boss 战时，108 在"建立使魔"这块不应当使用** ✓ —— 即**不建我们自建的敌机壳** ✗
   （现状：`yk_orbit_tick` / `yk_map_*` 走 `ce_spawn_enemy` ✓）。
2. **把他们的使魔认为是我们样式的使魔** ✓ —— 接管进 108 的使魔语义 ✓，
   **包括转伤（1/2）与人妖切换** ✓（他们的使魔只有**人侧基础层** ✓，妖侧由我方 DLL 提供 ✓）。
3. **没装 108 时，他们的使魔也不应该做人妖形态切换** ✓ —— 因为此时「**只有人侧、没有妖侧**」✓。
   （这一点**天然满足** ✓：形态切换全部由我方 DLL 提供 ✓；108 不在 ⇒ 没有任何东西去驱动它们 ✓。
   反过来说：**将来任何"对敌机 anm 做形态切换"的新代码都必须挂在 108 上** ✗，不许做成全局 ✓。）
4. （更早的同日口径 ✓）两个 mod **必须各自独立** ✗不合并；**都开强强联合、只开一个也能用** ✓
   —— 原文见 [`assets/mystia/build_st02.py:5-11`](assets/mystia/build_st02.py) ✓。

## 2. 已核实的（一手 ✓）

| 事 | 结论 | 证据 |
| --- | --- | --- |
| leveledit 的家 | GitHub `Fantasyguide/th18-leveledit`（**私有**、SSH-only ✓）；本机 clone = `/home/nipporita/th18-leveledit`（**树外** ✓），分支 `mystia-familiar-trail` ✓ | clone 内 `README.md` / `git log` ✓ |
| 要同开的补丁 | `mods/stage02-mystia/` ⇒ `thcrap.json`：覆盖 `st02bs.ecl` / `st02enm.anm` / `front.anm` / `st02a–d.msg`，**纯资源替换、不需要 dll** ✓ | 该文件 ✓ |
| 与我方的同名碰撞 | **零** ✓（我装 `ability.anm`/`cards.js`/`default.ecl`/`pl0X.*`/`voice*` ✓）⇒ 他们**不**patch `default.ecl` ✓ | P112/AUDIT §BE-COEXIST-GUARD ✓ |
| 他们的使魔是什么 | 「**移植为 TH18 普通敌人**」✓（保留生成波次、图像、移动与发弹 ✓） | `mods/stage02-mystia/README.md:127` ✓ |
| 他们 boss 的 sub 谱 | `Sub26–28` 第一非符/使魔 ✓、`Sub33–43` 蛾符 ✓、`Sub29–31` 第二非符 ✓、`Sub44–50` ✓、`Sub52–57` 夜雀の歌 ✓、`Sub58–61` 追加阶段 ✓；notes：「**Sub51 +80 → Sub52**」（**他们插入的过场** ✓） | `…/README.md:101-106`、`…/notes.md:21-22` ✓ |

## 3. ✗ 已证伪的两条（别再照它们写代码 ✓）

1. **「用 anm 脚本 28/29 认他们的使魔」不成立** ✗
   `thanm -l 18` 实测：零售 `local/th18.v1.00a/dat/st02enm.anm` 与**他们改过的**
   `work/edit/stage02-mystia/input18/st02enm.anm` **脚本号都是 0–18** ✓（一模一样 ✓）。
   README 那句「`enemy.anm` scripts 57/58 → 追加 scripts 28/29」指的是 **TH08 源侧 `anm8/enemy.anm`** ✗，
   **不是** TH18 运行时脚本号 ✓。
2. **「从他们的编译产物读 ECL sub 索引」在本机走不通** ✗
   他们构建要的 `local/th08-origin-game/th08.dat`（manifest 声明的 sha256 `9d7edf43…`）**不在本机** ✓
   （本机那份 `local/th08.v1.00d/th08.dat` 哈希不符 ✗）⇒ 实测：在他们的**副本** `/tmp/le-build` 里跑
   `python scripts/thcrap.py package stage02-mystia` 被该哈希闸拦下 ✓（**他们仓库未改动** ✓）。
   ⇒ 产物 `build/stage02-mystia/files/st02bs.ecl` 因此拿不到 ✓。

## 4. ✅ 现在的做法：**先实测，再钉常量**（本仓「先加 log 再定论」的老办法 ✓）

探针已在 **P113**（PR #104 → `3849a0cf` ✓）落地 ✓：`cards/youkai.c` 里 `YK_LE_PROBE`（可一键关 ✓）
挂在 `yk_side_lock_tick()` 最前面 ✓（**不挂 `on_tick_2`** ✗ —— 那里内联着 `ce_damage_rect` 的内联汇编，
再加活值会撞 i686 寄存器上限 ✗）。行为：**只在有 boss 在场**时、每 **30 帧**一行 ✓，字段**全部只读** ✓：

```
le-probe(boss0): id=<id> fl=<hex> anmMain=<n> slot0=<n> sub274=<n> own=<n> tin=<n> iv=<a>,<b>,<c>,<d> fv=<x>,<y>,<z>
le-probe(enemy): id=<id> anmMain=<n> slot0=<n> sub274=<n> tin=<n>      # 最多 8 只（他们的使魔会出现在这里 ✓）
le-probe: live=<n>
```

字段偏移出处 = vendor `type-structs-own.json` 的 `zEnemyData` ✓：`anm_slot_0_script` +0x26c ✓ /
`anm_main` +0x270 ✓ / `__probably_cur_subscript_for_anm_main` +0x274 ✓ / `ecl_int_vars` +0x28c ✓ /
`ecl_float_vars` +0x29c ✓ / `time_in_ecl` +0x2bc（`zTimer`：cur = +4）✓ / `own_boss_id` +0x5140 ✓。

**日志**：`D:\项目\幻想指南\18\th18_modkit\game\th18_card_expand.log` ✓
（WSL：`/mnt/d/项目/幻想指南/18/th18_modkit/game/th18_card_expand.log` ✓）。

### 要跑的两局（装备 108 ✓，都打到二面 boss 十几秒 ✓）

| 局 | leveledit | 目的 |
| --- | --- | --- |
| A | **不勾** ✓ | 拿到**原版**二面 boss 的指纹 ✓ |
| B | **勾上** ✓ | 拿到**他们那版**的指纹 + **他们的使魔**那批 `enemy` 行 ✓ |

⇒ 两局一 diff ✓：**哪个字段/哪组值能区分**就定了 ✓。

## 5. 钉死常量之后要做的事（接线，尚未开工 ✓）

1. **纯逻辑进 `youkai_core.c/.h`** ✓，例如：
   - `yk_le_is_levelledit_fight(int32_t boss_sub274, int32_t iv0, float fv0, …)` ✓（判据常数 = 第 4 节实测 ✓）；
   - `yk_le_is_their_familiar(int32_t enemy_id, int32_t anm_main, int32_t slot0)` ✓（同上 ✓）；
   - 主机单测 `tests/test_youkai_core.c` ✓：喂 `(boss 字段组, 敌机字段组, has_108)` ✓ 断言四类行为 ✓
     （①不建自建壳 ②可接管/转伤 1/2 ③切侧 ④**没 108 时一律不碰** ✓）；
2. `youkai.c` **接线** ✓：那场里跳过自建壳 ✓ + 把匹配的外部敌机 id 喂进 `yk_map_*` 的台账 ✓
   （转伤 1/2 与人妖切换沿用现成路径 ✓）；
3. **回读产物再落代码** ✓ 的纪律保持 ✓（P113 的教训：**一律在从 `origin/main` 开的工作区里改** ✗ ——
   主检出可能落后很多 ✓，第一版探针就因此编译失败过 ✓）；
4. 完工后**把 `YK_LE_PROBE` 改 0** ✓（并在此处记一笔 ✓）。

## 6. 影响面（写清 ✓）

- 只动**本卡自己的**文件 ✓（`youkai.c` / `youkai_core.*` / `tests/test_youkai_core.c` / 本文 ✓）；
  **不改** `sdk.*`/`engine.h`/其它卡 ✗；
- **没装/没启用 108 的玩家**：他们的使魔**保持 now 的样子** ✓（我方不做任何形态切换 ✓）；
- **没装 leveledit 的玩家**：判据不命中 ⇒ **行为与今天完全一致** ✓（108 照旧建自己的使魔 ✓）；
- 探针对**性能**的影响：只有 boss 在场时每 30 帧一次读几个字段 + 一行 `ce_log` ✓（可忽略 ✓），
  且**完结即关** ✓。
