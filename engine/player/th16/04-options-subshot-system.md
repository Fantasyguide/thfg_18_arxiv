# player 逆向 04:option / 子机系统(本体子机 + ★季节子机)
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> 方法:Ghidra(ghidra-re MCP)一手反编译 `PlayerInner__repopulate_options` `0x4440e0`(th16.exe 用户自有,
> ExpHP 符号已套)。日期 2026-06-12。分级 ✅高 / 🟡中 / ❓未解。**仅 TH16 v1.00a**。
> 回答用户点名的"季节子机":TH16 自机的两组 option(子机)各从哪来、几个、摆在哪。

## 0. 一句话结论

TH16 自机有**两组 option(子机)**,由 `PlayerInner__repopulate_options` 在 power/季节槽变化时重建:
1. **本体子机(main options)**:数量 = **火力档** `CURRENT_POWER/POWER_PER_LEVEL`(≤4),位置取自**主 .sht**
   的 option_pos 表,精灵按 `CHARACTER`。
2. **★季节子机(season options)**:数量 = **季节槽档位**(`get_season_gauge_fill_ratio`,0..5,≤8),位置取自
   **副 .sht**(`PLAYER+0x2c78c`)的 option_pos 表,精灵按 `SUBSEASON`(`DAT_00492be0[SUBSEASON]`)。

两组都是 stride **0xe4** 的运行时槽(与 `engine/sht/th16/04` 的自机弹槽同 stride);**子机既是"显示的小球"也是
"副火力发射点"**——shooterset 的 `opt` 字段(`../sht/05` §2、`07` §2)就是选第几个子机当发射点。

## 1. 触发与流程:`PlayerInner__repopulate_options` `0x4440e0` ✅

**何时调**(一手):
- `player_update_perframe` 状态2 帧==3(死亡掉 power 后,`engine/player/th16/01` §4);
- `Bomb__activate_bomb`(炸/季节释放后,`engine/player/th16/02` §3);
- 自机 init。
即**火力档或季节槽档发生变化**时重建子机布局。函数内分两段(本体 / 季节),各自比对"上次档位"
(本体 `param+0x165f4`、季节 `param+0x1608c`),不变则跳过。`param = PLAYER+0x610`。

## 2. 本体子机(main options)✅

```c
power_lvl = CURRENT_POWER / POWER_PER_LEVEL;         // 档数(0..)
// 数量 = power_lvl(>4 截到 4;火力降则中断清空 4 个 option anm 树)
for (i = 0; i < power_lvl; i++) {
   sht_base = PLAYER+0x2c788;                        // 主 .sht
   idx = *(int*)(0x4a5e4c + CHARACTER*0x20 + power_lvl*4) + i;   // 布局索引表
   slot.pos_unfocus = (option_pos[idx].x, .y) × 128; // sht_base+0x40 + idx*8
   slot.pos_focus   = (option_pos2[idx].x, .y) × 128;// sht_base+0xe8 + idx*8
   // 立即位置 = 自机坐标 + (聚焦?focus偏移:unfocus偏移);精灵 anm = AnmLoaded__copy_vm(DAT_00492c00[CHARACTER])
}
```

要点 ✅:
- **数量 = 火力档**(满 4 档 4 个);`CURRENT_POWER` 存 ×100,`POWER_PER_LEVEL=100`(`../sht/05` §2b)。
- **布局索引表 `0x4a5e4c`**:二维 `[CHARACTER][power_lvl]`(每角色 0x20 字节=8 个 int),给出该(角色,火力档)
  下子机在 option_pos 表里的**起始下标**,再 + 子机序号 `i`。
- **位置 = 主 .sht 的 option_pos 表**:`sht_base+0x40 + idx*8`(x,y float,×128 转定点)。
- **聚焦/非聚焦两套位置**:`+0x40`(非聚焦)与 **`+0xe8`(聚焦)** 是**两张并列的 option_pos 子表**——
  `+0x165c8`(聚焦,`engine/player/th16/03`)选用哪套 → 聚焦时子机收拢、非聚焦时散开(经典表现)。
  这**细化了 `engine/sht/th16/05/07` 的 option_pos**:它不是一张表,而是 unfocus(@+0x40)/ focus(@+0xe8)两段。

## 3. ★季节子机(season options)✅

第二段结构同形,但全部换成"副 .sht + SUBSEASON + 季节槽档":
```c
season_lvl = 数 CURRENT_SEASON_POWER 越过 SEASON_POWER_LEVEL_REQUIREMENTS 几档;  // 0..5
// 数量 = season_lvl(>8 截到 8;不足补清空,共 8 槽 @ param+0x3e0)
for (i = 0; i < season_lvl; i++) {
   sht_base2 = PLAYER+0x2c78c;                       // ★ 副 .sht
   idx = *(int*)(0x4a5dac + SUBSEASON*0x20 + season_lvl*4) + i;   // 季节布局索引表
   slot.pos_unfocus = (sht_base2->option_pos[idx]) × 128;   // +0x40 + idx*8
   slot.pos_focus   = (sht_base2->option_pos2[idx]) × 128;  // +0xe8 + idx*8
   // 精灵 anm = AnmLoaded__copy_vm(DAT_00492be0[SUBSEASON])   ← ★ 按副季节
}
```

要点 ✅:
- **季节子机数量 = 季节槽档位**(捡季节道具充能 → 档涨 → 子机增多,见 `engine/player/th16/02` §2);最多 8 个。
- **位置来自副 .sht**(`PLAYER+0x2c78c`,= plXsub,按 `SUBSEASON` 选,见 `engine/sht/th16/03` §6.1)。
- **精灵按 `SUBSEASON`**(`DAT_00492be0[SUBSEASON]`)→ 不同副季节的子机长相不同(春/夏/秋/冬/土用)。
- 布局索引表 **`0x4a5dac`** = 二维 `[SUBSEASON][season_lvl]`。
- → **这就是"季节子机"的来历**:它们随**季节槽**长出来、跟随自机、按副季节火力开火,并在按 C 季节释放
  (`engine/player/th16/02`)时仍在场。

### 3b. 季节子机：数量 / 位置 / 外观 / 发弹（2026-09-09 一手复核）✅

用脱壳版 exe 重读了 `PlayerInner__repopulate_options` 的季节段（`th16:0x4445bf`..`0x44488f`），
订正并补全 §3：

- **数量 = 季节档位**（不是「每档新增 L 个」）：档位从 `CURRENT_SEASON_POWER` 数阈值得到（`0x4445bf`），
  循环 `for (i = 0; i < level; i++)`（`0x444878 cmp edx,ecx; jge`），档 1..6 → 1..6 个子机；
  槽数组 8 个（`+0x3e0`，stride `0xe4`），余下槽清空（`0x444898 mov edi,8; sub edi,edx`）。
- **位置**：`idx = 表[档位-1] + i`，表基址 = `th16:0x4a5db0 + SUBSEASON*0x20`（`0x4445fb shl edx,5`、`0x4445fe add edx,0x4a5db0`；即 6 月文档里那张 `0x4a5dac` 表的第 1 项起），值是三角数
  `{0,1,3,6,10,15,21,28}` → **每一档一套独立阵型**；坐标取副 sht 的 `option_pos`：
  非聚焦 `+0x40`、聚焦 `+0xe8`（`0x4446c6` / `0x4446fe`），×128 定点。
- **外观**：副 anm 文件按**季节**选（不是按角色）—— `th16:0x492cdc[SUBSEASON]` =
  `{pl00sub, pl02sub, pl03sub, pl01sub, pl04sub}`（春/夏/秋/冬/土用），
  脚本 = `th16:0x492be0[SUBSEASON]` = `{1, 1, 1, 18, 1}`（`0x44477b`）。
  ⚠️ **冬用的是 `pl01sub.anm` 的 script18**（那张文件有 22 个脚本，其余只有 5 个）—— 文件编号是
  **角色**序（pl00=灵梦/春、pl01=魔理沙/冬、pl02=琪露诺/夏、pl03=文/秋、pl04=土用），
  所以「冬」不在 pl03sub 里。副 sht 同理（`th16:0x492cb8`）。
- **外观细节**：script1（春/夏/秋/土用）= `alpha(224)` + 加算 + 自转 0.5067 rad/帧 +
  4 帧一次的 scale 1.0↔0.5 脉动，贴图是**玩家贴图里裁出来的 16×16 小圆**
  （副 anm 的 entry 用 `xOffset/yOffset` 从 `player/pl0X/pl0X.png` 裁一块 64×16，再切成 3 个 16×16 帧）；
  script18（冬）= `type(2)` + 蓝色小圆（`(51,136,204)`），出场 scale 0→1→0.6，之后 15 帧一步的
  0.5↔0.6 呼吸。收集/释放时脚本的 `interruptLabel(1)` 放大消失。
- **行为**：纯跟随（聚焦/非聚焦两套坐标 + 插值），没有自主 AI。
- **发弹**：走副 sht 的 shooter 里 `opt=1..4` 的那批（从子机位置发；`engine/sht/th16/07` §
  「子机弹(副/option)」），各季节弹道不同（春 homing / 冬 laser / 夏 直线 / 秋 微曲 / 土用 直线，
  `engine/sht/th16/03` §6 的表）。

## 4. 与运行时槽 / shooterset 的关系 🟡

- 两组 option 槽:本体在 `param+0x104`(=PLAYER+0x714)起、季节在 `param+0x3e0`(=PLAYER+0x9f0)起,stride **0xe4**。
  **`PLAYER+0x9f0`(×8)与 `engine/sht/th16/04` 的"shot 组 B(option)"基址吻合** → 季节子机槽 = 那组 ×8 槽。
  本体子机槽与 `../sht/04` 的 `+0x660`(×4)组的精确字段对应**未逐偏移核实**(同一 0xe4 记录里"发射字段"
  与"option 位置/anm 字段"分处不同子偏移),标 🟡。
- **子机 = 发射点**:`engine/sht/th16/07` 里 shooter 的 `opt`(+0x20)字节选"从第几个子机发";本篇给出"这些
  子机实际站在哪、有几个"。两篇拼起来 = 完整的"火力随档增长 → 子机增多 → 每个子机按 opt 发它那簇弹"。

## 4b. 布局索引表数值(本会话 dump,静态 .data,✅值)

> 复核（2026-09-09）：四张表（季节/本体布局索引 + 两组 anm 脚本 id）由脚本按 RVA 重读 exe 文件字节，
> 与本节数值逐项一致，见 [`season/th16/01`](../../season/th16/01-season-data-and-tables.md) §5。

两张索引表都是**静态非零**(已 read_bytes 实证),且**所有角色/副季节行完全相同**:

- **本体 `0x4a5e4c`**(每角色 0x20=8 int,4 角色行一致):有效序列 `{0, 1, 3, 6, 10, 11, 13, 16}`。
- **季节 `0x4a5dac`**(每副季节 0x20=8 int,5 副季节行一致):有效序列 `{0, 1, 3, 6, 10, 15, 21, 28}`。

→ 季节表是干净的**三角数** `L(L−1)/2`(0,1,3,6,10,15,21,28)= **档位 L 时新增 L 个子机**,起始下标
  = 前面各档累计 → 各档的子机块在 option_pos 表里互不重叠。本体表前 5 项同为三角数(0,1,3,6,10),
  高档(11,13,16)增量收敛(火力上限 4 档,高项多半用不到)。
- **季节子机 anm 脚本 `DAT_00492be0[SUBSEASON]` = `{1, 1, 1, 0x12, 1}`**(春/夏/秋=1,冬=0x12,土用=1)✅。

> ⚠️ 🟡:`SUBSEASON*0x20 + lvl*4` 的索引使每行的 **lvl0 项 = 上一行末项**(存在一个 lvl0 哨兵/错位),
> 即"档 L→起始下标 L(L−1)/2"对 L≥1 成立、lvl0 项不被用(数量循环 `for i<lvl` 在 L=0 不跑)。
> 这层对齐细节按读到的字节给出,**未再用动态断点验**,故标 🟡;数值本身 = ✅(静态实证)。

## 4c. option 槽 0xe4 记录字段图(2026-06-12 补,交叉 `repopulate_options` + `playershot_tick_dispatch`)✅

把 `PlayerInner__repopulate_options`(写)与 `playershot_tick_dispatch`(读,`player_input_move` 每帧调)的
偏移对齐(tick 游标起于 `record+0x60`,repopulate 写游标 `piVar9` 同样起于 `record+0x60`,`local_38` 起于
`record+0x00`)。**记录相对字节偏移**:

| record 偏移 | 字段 | 读/写点 | 可信 |
| --- | --- | --- | --- |
| `+0x00` | active/状态(0=空,2=激活 option)| tick 读 / repopulate 写 `*local_38` | ✅ |
| `+0x54/+0x58` | 目标坐标暂存(x/y,定点)| tick 写 `piVar4[-3/-2]`、repopulate 写 `local_30[-6/-5]` | ✅ |
| `+0x5c/+0x60` | **当前显示坐标 lerp 累加**(x/y,定点)| tick 读写 `piVar4[-1/0]`、repopulate 写自机坐标 `PLAYER+0x61c/620` | ✅ |
| `+0x64/+0x68` | **非聚焦位置偏移**(x/y,源自主/副 .sht option_pos `+0x40`)| repopulate 写 `piVar9[1/2]`、tick 读(focus=0)| ✅ |
| `+0x6c/+0x70` | **聚焦位置偏移**(x/y,源自 option_pos `+0xe8`)| repopulate 写 `piVar9[3/4]`、tick 读(focus=1)| ✅ |
| `+0xb0` | option anm vm id #1 | repopulate 写 `piVar9[0x14]`、tick 读 `piVar4[0x14]` | ✅ |
| `+0xb4` | option anm vm id #2 | repopulate 写(`param+0x104`=group A `record0+0xb4`)、tick 读 `piVar4[0x15]`;`FUN_00440dc0` 也按 `param+0x714` 中断它 | ✅ |
| `+0xd0` | 组内 slot 序号 | repopulate 写 `piVar9[0x1c]` | ✅ |
| `+0xd4` | 瞬移/吸附标志(非 0 → 跳过 lerp 直接到位,用后清 0)| tick 读写 `piVar4[0x1d]` | ✅ |
| `+0xdc` | **func_on_tick** 指针(`(*piVar4[0x1f])()`)| tick 调 | ✅(与 `../sht/04` 一致)|

> 组基址:A(本体)`PLAYER+0x660`×4、B(季节)`PLAYER+0x9f0`×8,stride 0xe4(均来自 `player_input_move`
> 显式调用 `playershot_tick_dispatch(player, +0x660, 4)` / `(+0x9f0, 8)`)。
>
> ⚠️ **与 `engine/sht/th16/04` 的偏移冲突(诚实留白,❓)**:sht/04 把 **`+0x60`=速度、`+0x64`=角度、`+0xb0`=
> 子弹链接**(自机弹运动学字段),而本组(option 路径)里 `+0x60`=显示 y、`+0x64`=非聚焦 x 偏移、`+0xb0`=anm
> vm id。同字节、两套语义。**两种可能**:(a)记录**双用**——作"已发射子弹"时走运动学字段、作"option"时走位置/anm
> 字段,由 active 值(1 vs 2)区分;(b)sht/04 当时看的是**子弹池对象**(`+0xd080`)而非 option 记录,偏移恰好撞。
> **未用开火/spawn 函数最终判定 → 标 ❓**,两边结论都先保留,不互相推翻。`+0xdc`=func_on_tick 两边一致(可锚)。
>
> ✅ **已判定(2026-06-12,经 ExpHP `zPlayerOption` struct)**:`zPlayerOption` 在 `+0x5c`=`scaled_cur_pos`、
> `+0x64`=`scaled_preferred_pos_rel_to_player`,**没有 speed/angle 字段** → `../sht/04` 的 `+0x60`=speed/`+0x64`=angle
> 是**子弹对象**(`zPlayerBullet`/`zBullet`)的字段,**与 option 记录是两个不同结构,偏移恰好撞**。冲突收敛 = 取 (a) 双用
> 的反面:不是同一记录双用,而是两类对象。详见 `05` §0.5。

## 5. 关键符号 / 数据(本篇新出)

| 符号 / 地址 | 含义 | 可信 |
| --- | --- | --- |
| `PlayerInner__repopulate_options` `0x4440e0` | 重建两组 option | ✅ |
| `0x4a5e4c` | 本体子机布局索引表 `[CHARACTER][power_lvl]`,值 `{0,1,3,6,10,11,13,16}` | ✅值(§4b)/ 🟡对齐 |
| `0x4a5dac` | 季节子机布局索引表 `[SUBSEASON][season_lvl]`,值 `{0,1,3,6,10,15,21,28}`(三角数)| ✅值(§4b)/ 🟡对齐 |
| `DAT_00492c00[CHARACTER]` | 本体子机 anm 脚本 id(疑 `{8,7,11,11}`,与邻表混淆风险,🟡)| 🟡 |
| `DAT_00492be0[SUBSEASON]` | 季节子机 anm 脚本 id = `{1,1,1,0x12,1}` | ✅ |
| `PLAYER_OPTION_ANM_SCRIPT_IDS_2[CHARACTER]` | 火力降档时的 option 退场 anm | 🟡 |
| .sht header `option_pos` 拆分 | **+0x40 非聚焦 / +0xe8 聚焦** 两段(各 stride 8 的 x,y)| ✅(读点);条目数 21/21 由 0x40..0x190 算得 🟡 |
| 槽:PLAYER+0x714(本体×4)/ +0x9f0(季节×8),stride 0xe4 | option 运行时槽 | ✅基址 / 🟡字段对应 |

## 6. 待办 / ❓

1. ✅ 布局索引表数值已 dump(§4b);🟡 剩 lvl0 哨兵/行对齐的动态确认。
2. ✅ option 槽 0xe4 字段图已补(§4c);🟡 剩与 `../sht/04` 的 `+0x60/64/b0` 语义冲突(需开火/spawn 函数判定,❓)。
3. 🟡 `option_pos` 两段(unfocus/focus)的条目数与各档用到的下标范围。
4. 🟡 `DAT_00492c00`(本体子机 anm)与邻接表(`0x492be0`+0x20 起)边界未确切切分,暂标 🟡。

## 7. 可信度 / 复核

- ✅ 一手:`PlayerInner__repopulate_options`(`0x4440e0`)本会话反编译;两组结构、数量来源(火力档/季节档)、
  位置表(主/副 .sht option_pos +0x40/+0xe8)、按 CHARACTER/SUBSEASON 选精灵——全来自代码读写点。
- 🟡 索引表数值、槽字段精确对应未取;option_pos 条目数为算术推断。
- 复核入口:Ghidra DB `th16`,地址见上。交叉:`engine/sht/th16/05`(option_pos)、`07`(opt/shooterset)、
  `04`(运行时槽 stride 0xe4)、`engine/player/th16/02`(季节槽档位)、`03`(聚焦 +0x165c8)。
- ❗ 纪律:本篇"option_pos 拆 unfocus/focus 两段""季节子机数=季节档"是**比社区/既有 sht 文档更细**的结论,
  已落到具体读点;表**数值未 dump**、槽字段对应**未逐偏移核**,相应标 🟡,未当定论。
