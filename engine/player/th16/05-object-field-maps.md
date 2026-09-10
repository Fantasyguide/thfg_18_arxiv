# player 逆向 05:对象字段图(player 对象 / Bomb 对象 / 伤害源)汇总
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> 方法:Ghidra(ghidra-re MCP)一手反编译 th16.exe(用户自有,ExpHP 符号已套)。日期 2026-06-12。
> 分级 ✅一手读写点 / 🟡观测但写点或全语义未追 / ❓未解。**仅 TH16 v1.00a**。
> 本篇是 player 子系统的**字段总账**:把 01–04 散落的偏移集中、补齐、标注读写点与可信度;**已在
> `engine/sht/th16/04,05,08` 给过的(shooter/header/运行时槽/伤害源)只指引、不重复**。

## 0. 三个对象 / 池

| 基址 | 对象 | 大小 | 来源 |
| --- | --- | --- | --- |
| `PLAYER_PTR=DAT_004a6ef8` | **player 对象** | `operator_new(0x2c828)` | `player_ctor` `0x441c60` |
| `MAIN_BOMB_PTR` / `SUBSEASON_BOMB_PTR` | **Bomb 对象** ×2 | `operator_new(0x108)` | `Bomb__operator_new` `0x40d890` |
| `PLAYER+0xd114`(+link×0x94,×256)| **伤害源池** | 0x94/项 | 见 `../sht/08` + §3 |

---

## 0.5 ★ ExpHP th-re-data struct 交叉验证 + 已导入 Ghidra(2026-06-12)

> **重大**:ExpHP `th-re-data/data/th16.v1.00a/type-structs-own.json` 有**完整 TH16 结构体**:`zPlayer`(0x2c828)、
> `zPlayerInner`(0x16090,在 player+0x610)、`zBomb`(0x108)、`zPlayerOption`(0xe4)、`zPlayerDamageSource94`(0x94)、
> `zVTableBomb`、`zTimer`/`zFloat3`/`zInt2`/`zBoundingBox3`。**逐项核对:我们手追的字段与 ExpHP 完全一致,零冲突**——
> 这是对 `01–05` 全部结论的独立第三方背书。ExpHP 还命名了几个我们标 🟡 的字段。

### 已写进 Ghidra(DB `th16`,已落盘)✅
- **建了 9 个 struct 类型**(parse_type_declaration):`zFloat3/zTimer/zInt2/zBoundingBox3/zBomb/zPlayer/zPlayerInner/zPlayerOption/zPlayerDamageSource94`(大数组/未知区用 `char[]` 填充对齐,size 精确:zPlayer=0x2c828、zPlayerInner=0x16090、zBomb=0x108)。
- **套到全局**:`PLAYER_PTR@0x4a6ef8`→`zPlayer*`、`MAIN_BOMB_PTR@0x4a6da8`/`SUBSEASON_BOMB_PTR@0x4a6da4`→`zBomb*`。
- **套到函数签名**(set_function_type,param→具名结构指针):`player_update_perframe/on_death/input_move/commit_death_on_miss/set_alive_after_bomb/shot_init`(`zPlayer*`)、`tick_shooting_state`、`PlayerInner__repopulate_options`(`zPlayerInner*`)、`Bomb__activate_bomb/can_bomb/on_tick/on_draw/initiate_season_release_cooldown`、`BombSub{Doyou,Spring,Fall,Winter}` + `BombReimu`(`zBomb*`)。
- 效果:反编译从裸偏移变成具名字段。如 `player_on_death` 现读 `(player->inner).state=4`、`(player->inner).iframes.current=6`、`(player->inner).flags & 8`;`Bomb__can_bomb` 现读 `bomb->is_season`、`bomb->cooldown.current<0`、`MAIN_BOMB_PTR->bomb_is_in_use`。
- 复跑 `tooling/ghidra/import_th_re_data.py` 不会动这些(它只导 funcs/statics,**不导 struct**)。
- **★ 写了专门的 struct 导入器 `tooling/ghidra/import_th_re_data_structs.py`**(ExpHP 无此工具):从 `type-structs-*.json` 生成布局精确的 C(每字段具名或 `char[size]` 兜底,值内嵌拓扑排序),`--check` 自检 **288/288 size 全对**;**✅ 已用法 B(headless,programmatic build)把全部 158 个 z* 结构体导入并落盘**(zEnemy/zBullet/zItem/zGui/zLaser*/zStage/zSupervisor/zMainMenu/zEclVm… 全在)。⚠️ 坑:Ghidra `CParser.parse` 不可靠落盘,改用 `StructureDataType`+`dtm.addDataType(REPLACE)` 才成。本次还导入了 **SHT 格式结构体**(`zShtRawFile/zShtShooter/zShtRawFileHeader/zShtRawOptionPos/zFloat2`,size 实测 0x210/0x58/0x40/0xa8 ✓——**直接是 IDE 的 SHT 数据模型**)+ player 邻接(`zSpellcard/zEnemyLife/zEnemyDropSeason/zPlayerBullet/zPosVel/zScreenEffect/zUpdateFunc/zVTableBomb/zVTableLaser`)。详见 `tooling/ghidra/README.md`。
- 注:ExpHP `zShtShooter` 已有字段名(fire_rate/damage/angle/speed/option/func_on_*),但 **`+0x38` flags 段、`+0x21`、`+0x1c` 仍标 `__unknown`** → 我们的"flags 运行时不读"(`../sht/05`)、`+0x21` 按角色 {0,1,2,4}、func_* 索引→行为表(`../sht/03`)是 ExpHP 没有的语义增量。

### ExpHP 名 ↔ 我们偏移(关键字段,全部 ✅ 对上)

> player 绝对偏移 = `0x610 + zPlayerInner 偏移`。

| 我们的偏移 | ExpHP 字段(zPlayerInner / zPlayer / zBomb)| 我们 01–05 的叫法 |
| --- | --- | --- |
| `+0x165a8` | `inner.state` | 主状态机 ✅ |
| `+0x1663c` | `inner.iframes.current` | 无敌帧 ✅ |
| `+0x1664c` | `inner.flags` | 标志位 ✅ |
| `+0x165c8` | `inner.is_focused` | 聚焦 ✅ |
| `+0x624/638/64c` | `inner.time_in_state / time_in_stage / __copy_3c` | 状态帧 / 进关帧(门控 focus≥4、shoot>19)✅ |
| `+0x165cc/165e0` | `inner.shoot_key_short_timer / shoot_key_long_timer` | 短/长射击计时 ✅ |
| `+0x16650/54/58/5c` | `inner.regular_speed / focused_speed / regular_over_sqrt2 / focus_over_sqrt2` | 4 档移速(**√2=对角线归一**)✅ |
| `+0x16684` | `inner.frames_after_stage_end` | bit0x2(转场)时自增的计数 ✅(原 🟡 命名)|
| `+0x16688` | `inner.speed_multiplier__used_by_fall` | 秋季节释放每帧写 ✅(原 🟡)|
| `+0x165f4 / +0x1669c / +0x16698` | `inner.num_main_options / num_season_options / __equals_power_level` | option 重建用档缓存 ✅ |
| `+0x2c748` | `hurtbox_halfsize` | 死亡 hitbox 盒 ✅ |
| `+0x2c7c8` | `player_scale__requires_flag_0x10` | 聚焦缩放(**ExpHP 名直接点明它依赖 flag bit0x10**,印证我们 §1b)✅ |
| `+0x2c7cc` | `damage_multiplier__used_by_winter` | 冬季节释放每帧写 ✅(印证 `02` §5e)|
| `+0x2c788/78c` | `sht_file / sht_file_subseason` | 主/副 .sht 基址 ✅ |
| Bomb `+0x30/34/d4/d8/e4` | `bomb_is_in_use / cooldown / is_season / season_level / release_piv_gain_display_timer` | 发动标志/冷却/类型/释放档/"+N"弹出 ✅✅ |

### 顺带解决的 ❓
- **`04` §4c 的 option vs 子弹运动学字段冲突**:`zPlayerOption` **没有** `+0x60`=speed/`+0x64`=angle——它在那两处是 `scaled_cur_pos`/`scaled_preferred_pos_rel`。故 `../sht/04` 的 +0x60/64=speed/angle 是**子弹对象**(`zPlayerBullet`/`zBullet`)的字段,不是 option 记录。**冲突判定:两者是不同对象,偏移恰好撞**——`04` §4c 的 ❓ 收敛为此结论。
- option `+0x6c`:ExpHP 标 `maybe_scaled_firing_angle`(它也不确定),与 agent D 的"聚焦偏移"分歧 → 保持 🟡。

---

## 1. player 对象字段图(本篇汇总,生命/输入/状态部分)

> 坐标/移速/sht 基址/判定矩形见 §1a;状态机/计时/标志见 §1b;射击/option 槽见 §1c。
> 子弹槽(+0x660/+0x9f0,stride 0xe4)与子弹池(+0xd080)字段已在 `../sht/04,08`,此处不复列。

### 1a. 坐标 / 速度 / .sht / 判定盒(`player_shot_init`/`player_input_move` 实证)✅

| 偏移 | 含义 | 读写点 | 可信 |
| --- | --- | --- | --- |
| `+0x0c` / `+0x10` | 主自机 anm / 副(季节)anm 加载句柄(`PLAYER_ANM_FILENAMES[CHAR]` / `..sub[SUBSEASON]`)| shot_init | ✅ |
| `+0x14`(16 字节)| 自机 AnmVm(精灵)| 多处 `AnmVm__run` | ✅ |
| `+0x610/614/618` | 自机 x/y/z(float)| 全程 | ✅ |
| `+0x61c/620` | 自机 x/y(定点 ×128)| `player_input_move` 积分 + 钳制 [-0x5c00,0x5c00]/[0x1000,0xd800] | ✅ |
| `+0x2c780` | 9 向移动方向(0..8)| `player_input_move` | ✅ |
| `+0x2c788/78c` | **主/副 .sht 解析后基址**(字段图见 `../sht/05`)| shot_init,全程 | ✅ |
| `+0x16650/54/58/5c` | 4 档移速 = 主 .sht header `+0x10/14/18/1c` × 128(直/聚直/斜/聚斜)| shot_init 装载、`player_input_move` 选 | ✅ |
| `+0x16660/64` | 当前帧速度向量(× `GAME_SPEED`)| `player_input_move` | ✅ |
| `+0x16688` | 速度/调速乘子(init=1.0)| `player_input_move` 乘、shot_init 置 1.0 | 🟡 |
| `+0x1668c/90/94` | 位置微调/反冲向量(每帧清)| `player_update_perframe` | 🟡 |
| `+0x2c748/4c`(z `+0x2c750=5.0`)| **死亡 hitbox 盒**半宽 = `CHARACTER_HITBOX_SIZE_TABLE[CHAR]`×0.5 | shot_init | ✅ |
| `+0x2c754/58`(z `+0x2c75c=5.0`)| **道具吸附盒(非聚焦)** = `CHARACTER_ATTRACTBOX_UNFOCUSED_TABLE[CHAR]`×0.5 | shot_init | ✅ |
| `+0x2c760/64`(z `+0x2c768=5.0`)| **道具吸附盒(聚焦)** = `CHARACTER_ATTRACTBOX_FOCUSED_TABLE[CHAR]`×0.5 | shot_init | ✅ |
| `+0x2c730..0x2c744` 等 | 上述盒的**世界角点**(每帧由半宽 + 自机坐标重算)| shot_init/`player_update_perframe` | ✅ |
| `+0x2c7c8` | **聚焦判定缩放**(init=1.0;collide 里 hitbox×它×3.6)| collide_circle、shot_init | ✅ |
| `+0x2c7c0` | 聚焦缩放插值开关(init=0)| `player_update_perframe` bullet_size_interp | 🟡 |

> 注:`CHARACTER_HITBOX_SIZE_TABLE@0x492c98` / `GRAZEBOX@0x492c78` / `ATTRACTBOX_FOCUSED@0x492c68` /
> `ATTRACTBOX_UNFOCUSED@0x492c88` —— 这几张表把 .sht header `+0x04/08/0c` **覆写**(`../sht/05` §2b 的死字段),
> 值实测(sht/05):hitbox 全角色 3.0。`+0x08`(grazebox)虽载入,实际擦弹用 hitbox+弹尺寸(`../sht/05` §4b)。

### 1b. 状态机 / 计时器 / 标志(`player_update_perframe`/`player_on_death`/`shot_init` 实证)

每个"计时器"是 ZUN Timer 三元组(prev-int / cur-int / cur-float),按 `__ptr_GAME_SPEED_MULT_FROM_ECL` 调速推进。

| 偏移 | 含义 | 读写点 | 可信 |
| --- | --- | --- | --- |
| `+0x165a8` | **主状态机**(0 出场/1 存活/2 死亡结算/3/4 决死)| `player_update_perframe` switch | ✅ |
| `+0x624/628/62c`(init flag `+0x634`)| **状态帧计时**(cur=`+0x628`;状态切换处归零)| 全状态逻辑 | ✅ |
| `+0x638/63c/640` | 计时器(`+0x63c≥4` 作**聚焦放行**)| `player_input_move`、`player_update_perframe` 推进 | 🟡 复位点未追 |
| `+0x64c/650/654`(init flag `+0x65c`)| 计时器(`+0x650>0x13` 作**开火放行**;init 归零,死亡/复活**不**复位)→ "开局/进关后帧数" | `player_update_perframe` 门控 | 🟡 |
| `+0x1663c/16640/16638`(init flag `+0x16648`)| **无敌帧**(cur=`+0x1663c`;>0 免疫,见 `01` §3)| collide_*、`player_update_perframe` 倒计时 | ✅ |
| `+0x16644` | 无敌期 GAME_SPEED_MULT 索引(`player_update_perframe`)| | 🟡 |
| `+0x16680` | init=0x1e(30);`player_input_move` 强制非聚焦时置 0x1e | shot_init/input_move | 🟡 |
| `+0x16684` | `+0x1664c & 2` 时每帧自增(疑聚焦切换/擦弹窗计数);`>0x1d`(29)清 `+0x165f4/+0x1669c` | `player_input_move` | 🟡 |
| `+0x165c8` | **聚焦标志** = `INPUT>>3 & 1`(`03`;订正 sht/07 🟡)| `player_input_move` 写、shooterset/collide/option 读 | ✅ |
| `+0x1664c` | **标志位**(见下表)| 多处 | 🟡 |
| `+0x165f4` | (开火/连射相关,死亡 commit 清 0)| `commit_death`、`input_move` | 🟡 |
| `+0x16088` / `+0x1608c` | option 重建用的"上次火力档 / 上次季节档"缓存 | `repopulate_options`(`04`)| ✅ |
| `+0x165f4`(注:与上同址,作 option 上次档)| 见 `04` | | 🟡 命名待并 |

**`+0x1664c` 标志位(2026-06-12 追写入点,大部分坐实)**:

| 位 | 含义 | 置位 / 清位(地址)| 可信 |
| --- | --- | --- | --- |
| 0x01 | (未知;`Player__destroy` 随 0x8 一起清)| set 未找到;clear `Player__destroy` `0x441912` | ❓ |
| 0x02 | **过场/对话转场进行中**(置位时 `+0x16684` 每帧自增)| set `FUN_0042ca80` `0x42ca92`;clear `FUN_00440dc0` `0x440dca`(同时中断全部 12 个 option anm 树 + `+0x16684=0`)| ✅ |
| 0x04 | **主炸进行中**(禁止开火)| set `BombMainMarisa__begin` `0x40faf2`(`OR [PLAYER+0x1664c],4`,实证;同时给 120 帧无敌);clear 主炸结束(`on_tick` 帧 300)/ `shot_init` `0x4416ef` / `Player__destroy` | ✅(Marisa;余角色主炸同 idiom 🟡)|
| 0x08 | **死亡音效抑制**(`player_on_death` 据此跳过 sound 2)| set 未找到(疑整字写);clear `Player__destroy` | 🟡 |
| 0x10 | 置位时**禁止开火** + 判定盒 `+0x2c7c8×3.6` 特殊缩放 + 特殊渲染 →**疑"剧情/无操控"态**(非普通聚焦)| **set 点未找到**(全 .text 无 `OR ...,0x10`,疑整字赋值/别名写);clear 未找到 | ❓ set 点 |
| 0x20 | **「加速/加伤态」标记**（`+0x2c7cc>1.01` 时置，否则清）——**唯一读者 = 残像/闪色分支**（`Player__on_tick` `0x442c32`）；冬释放每帧写 `+0x2c7cc=1.5` 触发（语义订正见下注）| set/clear `EnemyManager__on_tick_1a__body` `0x41b456`/`0x41b45f`（按 `+0x2c7cc`）| ✅机制 |

> ⚠️ **bit0x20 的语义订正（2026-09-10，一手）**：早前记成「速度缩放态」是**错的**。
> 真正被乘的是 `+0x2c7cc` 自己，乘在**对敌伤害**上 —— `EnemyData__step_logic` `0x41c5d6`
> 紧跟在 `enm_compute_damage_sources` `0x445a30` 的返回之后 `MULSS`；`BombSubWinter__on_tick`
> `0x410408` 每帧写 1.5 ⇒ **冬 = 1.5× 伤害**（秋的 1.5× 才是速度，见 [`02`](02-season-release-and-bombs.md) §5h）。

> ⚠️ 纪律:bit0x10 的**写入点全程序未找到 `OR ...,0x10`**(只剩整字写/别名写可能),保持 ❓;**绝不要**当聚焦
> ——聚焦是 `+0x165c8`(bit0x10 与禁射并存,而聚焦时可射)。bit0x4/0x2/0x20 已落到具体置/清位指令(✅)。

### 1c. 射击 cadence 计时 / option 缓存(`tick_shooting_state`/`shot_init` 实证)✅

| 偏移 | 含义 | 读写点 | 可信 |
| --- | --- | --- | --- |
| `+0x165cc/d0/d4`(init flag `+0x165dc`)| **短射击计时**(`set_shoot_key_short_timer` 起拍;`do_shooting` 用)| `tick_shooting_state` | ✅ |
| `+0x165e0/e4/e8`(init flag `+0x165f0`)| **长按射击计时**(封顶 0x76/0x77)| `tick_shooting_state` | ✅ |
| `+0x165ac` / `+0x165b0` | 聚焦判定点指示 anm(0x1a)/ 聚焦光环 anm(0x1b)句柄 | `player_input_move` | ✅ |
| `+0x660`(×4)/ `+0x9f0`(×8),stride 0xe4 | 主弹/本体子机槽 · 季节子机槽(字段见 `../sht/04`;option 位置由 `04` 写)| `playershot_tick_dispatch` | ✅基址 |
| `+0x6c0 + i*0xe4`(i=0..11)| 12 个 option 槽的某字段,init=`0xffff3800` | shot_init | 🟡 |
| `+0x1114 + i*0xc0`(i=0..0xff)| 256 项子弹/效果池(各项 `+0` init=序号)| shot_init,`Player__tick_bullets` | ✅ |
| `+0xd080`(stride 0x94,×256)| 子弹/伤害源池(见 §3 + `../sht/08`)| | ✅ |

---

## 2. Bomb 对象字段图(0x108 字节)✅

> 由 `Bomb__constructor`(`0x40d580`)、`Bomb__initialize`(`0x40d600`)、`Bomb__can_bomb`(`0x40dda0`)、
> `Bomb__activate_bomb`(`0x40db20`)、`Bomb__on_tick`(`0x40dd00`)、`Bomb__on_draw`(`0x40de30`)、
> `BombSub*::begin` 合并。**`param[N]`(反编译 dword 下标)= 字节 `4N`**。

| 字节偏移 | dword | 含义 | 读写点 | 可信 |
| --- | --- | --- | --- | --- |
| `+0x00` | [0] | vftable(基类 `BombInf`;`operator_new` 按 CHAR/SUBSEASON 改成各变体)| ctor、activate `(**vtable[0])`=begin、on_tick `vtable[1]` | ✅ |
| `+0x04` | [1] | UpdateFunc 标志(ctor 置 bit1=2)| ctor、initialize | ✅ |
| `+0x08` | [2] | **on_tick** UpdateFunc 指针 | initialize 注册 | ✅ |
| `+0x0c` | [3] | **on_draw** UpdateFunc 指针 | initialize 注册 | ✅ |
| `+0x14/18/1c` | [5..7] | **释放原点 x/y/z**(begin 拍自机坐标)| `BombSub*::begin` | ✅ |
| `+0x2c` | [0xb] | 释放角(begin 置 `0xbfc90fdb`=−π/2)| `BombSubSpring/Doyou::begin` | 🟡 |
| `+0x30` | [0xc] | **发动中标志**(1=正在发;activate 入口防重入、can_bomb 查 `!=1`)| activate、can_bomb、on_tick | ✅ |
| `+0x34/38/3c` | [0xd/e/f] | **冷却计时**(cur=`+0x38`;<0=冷却中,can_bomb 拒;on_tick 推进回 0)| can_bomb、initiate_cooldown、on_tick | ✅ |
| `+0x44` | [0x11] | 冷却计时 init flag(bit0)| initialize/activate | ✅ |
| `+0x5c` | [0x17] | 释放特效 anm vm id #1(sprite 3)| begin、on_tick、ctor 清 0 | ✅ |
| `+0x64` | [0x19] | 释放特效 anm vm id #2(sprite 4)| begin | ✅ |
| `+0x68` | [0x1a] | 符卡失败标记(activate:符卡进行中开炸→1)| activate | 🟡 |
| `+0x70` | [0x1c] | **主炸工作缓冲**指针(`malloc(0x6c0)`;仅主炸 `BombMain*::begin`)| `BombReimu__begin` | ✅ |
| `+0xd4` | [0x35] | **类型**:0=主炸(耗 `CURRENT_BOMBS`)/ 1=季节释放(耗 `CURRENT_SEASON_POWER`)| initialize 置、can_bomb/activate 分流 | ✅ |
| `+0xd8` | [0x36] | 释放时的季节档位(begin 据此查 `RELEASE_LEVEL_*`)| activate、on_draw 着色 | ✅ |
| `+0xdc` | [0x37] | "+N" 弹出动画基值(activate 置 0)| activate、on_draw | 🟡 |
| `+0xe0` | [0x38] | "+N" 弹出当前值(idle=−1.0)| on_draw、initiate_cooldown | 🟡 |
| `+0xe4/e8/ec` | [0x39/3a/3b] | **"+N" 弹出计时**(cur=`+0xe8`;cooldown 置 180;on_draw 倒计时驱动显示)| on_draw、activate、initiate_cooldown | ✅ |
| `+0xf0` | [0x3c] | "+N" 弹出 GAME_SPEED_MULT 索引 | on_draw | 🟡 |
| `+0xf4` | [0x3d] | "+N" 弹出计时 init flag | initialize | ✅ |
| `+0xf8` | [0x3e] | "+N" 弹出着色相位(=释放档位)| on_draw、initiate_cooldown | ✅ |
| `+0xfc/100/104` | [0x3f/40/41] | "+N" 弹出位置(activate:自机坐标,y−32)| activate、on_draw | ✅ |

> ★ 关键修正:`Bomb__on_draw` 揭示 `+0xe0..+0x104` 是**释放后"+N"分数/反馈弹出**,**不是**冷却本身;
> 真正的"能不能再放"冷却闸门 = `+0x38`(`+0x34/38/3c` 计时,`initiate_season_release_cooldown` 置 −45)。
> `initiate_season_release_cooldown` 同时启动二者(弹出 180 帧 + 冷却 45 帧)。

---

## 3. 伤害源创建:`Player__create_damage_source_4449b0` ✅(交叉验 `../sht/08`)

`Player__create_damage_source_4449b0(this, pos, lifetime, dmg)`(`0x4449b0`)在 `PLAYER+0xd114` 池(stride 0x94,
×256,游标 `+0xd110`)找空槽,写入并返回 1-based 下标:

| 写入(相对 flag 基址 `+0xd114`)| 值 | = `../sht/08` 字段 |
| --- | --- | --- |
| `+0x00` flag | `\| 3`(bit0 active + bit1 线型)`& ~4` | obj+0x00 flag |
| `+0x1c/20`(=池 `+0xd130`)| `pos` | obj+0x1c/20 位置 |
| `+0x04/+0x08`(XMM2/XMM3)| 半径参数 | obj+0x04 线宽 / +0x0c 半径 |
| `+0x60/64/68`(Timer)| `lifetime`(param_2)| obj+0x60/64 寿命 |
| `+0x74` | `dmg`(param_3)| obj+0x74 每次伤害 |
| `+0x7c` | 9999999 | obj+0x7c 伤害上限 |
| `+0x80` | 1 | obj+0x80 命中间隔 |

→ **完全吻合 `../sht/08` 的伤害源字段图**(独立反推两次一致 = 强交叉验证)。调用方实测:
- **复活清屏**:`(player, pos, 0x1e=30, 0x96=150)` → 30 帧寿命、150 伤害(`engine/player/th16/01` §5)。
- **季节释放**:on_tick 每帧 `(player, pos, 1, 100)` → 1 帧寿命、100 伤害(`engine/player/th16/02` §5d)→ 连续造伤。
- 命中派生子弹(hit3/4):见 `../sht/03` §6.4 / `08`。

---

## 4. 仍未追透(诚实留白)

- ✅ 季节阈值已解出 = {100,230,390,590,840,1140},MAX=1140(`02` §2;含别名纠错)。
- ✅ `+0x1664c` bit 0x2/0x4/0x20 已落置/清位点(§1b);❓ **bit0x10 的 set 点全程序未找到** `OR ...,0x10`(疑整字/别名写),bit0x8/0x1 的 set 点同样未找到。
- 🟡 `+0x638/63c/640`、`+0x64c/650/654` 的复位点全链;`+0x16680/84/88`、`+0x1668c/90/94`、`+0x6c0+i*0xe4`(option 槽某字段)精确语义。
- 🟡 Bomb `+0x2c` 释放角、`+0x68` 符卡标记的下游消费;option 记录 `+0x60/64/b0` 与 `../sht/04` 运动学字段的语义冲突(`04` §4c,❓)。
- `__ptr_GAME_SPEED_MULT_FROM_ECL`:被所有 Timer 推进引用的调速乘子表(ECL 驱动的游戏速度);本篇按"调速"
  定性,未反其填充(归 `engine/ecl/th16/`)。

## 5. 可信度 / 复核

- ✅ 一手(本会话):`Bomb__constructor`(`0x40d580`)、`Bomb__on_draw`(`0x40de30`)、
  `Player__create_damage_source_4449b0`(`0x4449b0`)、`anm_40e490_compute_final_pos`(`0x40e490`)、
  `player_shot_init`(`0x440fb0`);其余偏移来自 01–04 已反函数。
- ✅ 季节 DELTAS seed `{0,100,130,160,200,250,300,0}`、box 表地址、伤害源参数 = 静态/反编译实证。
- 🟡 见 §4。复核入口:Ghidra DB `th16`,地址见各表。
- 交叉:`engine/sht/th16/04`(运行时槽)、`05`(shooter/header/box 覆写)、`08`(伤害源);`engine/player/th16/01–04`。
