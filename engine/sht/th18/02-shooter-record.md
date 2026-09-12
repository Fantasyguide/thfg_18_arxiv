# TH18 shooter 记录（`0x5c`）字段图 + 一条 shooter 怎么变成一发自机弹

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
>
> 一手：2026-09-05，Ghidra 反编译 `PlayerBullet__create` `0x45e320`（字段的**唯一消费者**）、
> `Player__tick_shooters_for_ability_card` `0x40a9c0`、`Player__tick_shooting_state` `0x45ea00`、
> `Player__shoot_one_bullet` `0x45e930`；数值面由四个 `pl0X.sht` 的 **268 条 shooter 全量统计**佐证。
> 文件层的位置见 [`01`](01-file-layout-and-shooterset-index.md)。

## 1. 字段图

stride `0x5c`。`th16:` 的 stride 是 `0x58`（[`../th16/05`](../th16/05-th16-flags-no-runtime-read.md)），**别套用偏移**。

| 偏移 | 类型 | 字段 | 消费者 |
| --- | --- | --- | --- |
| `+0x00` | i8 | **fire_rate**。**符号位 = shooterset 终止符**（`< 0` 停止遍历），所以有效值 1–127。**`0` 在装备卡路径上会除零**（§2）| 两处发射循环 |
| `+0x01` | i8 | start_delay（`timer % fire_rate == start_delay` 才发）| 同上 |
| `+0x02` | i16 | **damage** → `bullet+0x9c` | `0x45e320` |
| `+0x04` | f32 | 出膛偏移 x（加到自机 / 子机位上）| 同上 |
| `+0x08` | f32 | 出膛偏移 y | 同上 |
| `+0x0c` | f32 | → `bullet+0xa0` | 同上 |
| `+0x10` | f32 | → `bullet+0xa4` | 同上 |
| `+0x14` | f32 | **angle**（弧度，装载后归一到 `[-π, π]`，两端都闭——见表下补注）。三个分档见 §3 | 同上 |

> ★ **2026-09-10 一手更正（在 `th18.exe` 上自证，不是从 th16 外推）**：区间是 **`[-π, π]`，两端都闭**，不是 `(-π, π]`。th18 的归一例程 `th18:0x402890`（常量 π `th18:0x4b922c`、2π `th18:0x4b9260`、−π `th18:0x4b9430`，float32 值与 th16 逐字节相同）两段循环的出口判据分别是 `x <= π` 与 `-π <= x`；守卫 `cmp eax,0x20; jg` 比的是**自增前**的值 ⇒ 上限 **34** 步，泄漏阈值 `|θ| ≲ 216.77`（= 69π）。（本条只核了共用例程本身，未逐一追这个字段的每个写点走的是不是它。）

| `+0x18` | f32 | **speed** → `bullet+0x60` | 同上 |
| `+0x1c`–`+0x1f` | — | 🟡 268 条全 0 | — |
| `+0x20` | i8 | **低 4 位 = 子机槽号**（`0` = 自机本体）；`mode == 2` 时另取高 4 位 | 同上 |
| `+0x21` | i8 | **mode**：`2` = 走子机火力分配（读写 `player+0x476fc+slot*4`）；`4`/`5`/`6` = 出膛不做 PosVel 修正减法。其余取值 🟡（零售出现 0/1/2/4/5/6/7）| 同上 |
| `+0x22` | i16 | **ANM 脚本号** —— 主炮取 `pl0X.anm`，**装备卡子机取 `ability.anm`**（§4）| 同上 |
| `+0x24` | i16 | **音效 id**（`< 0` = 不响）。零售取值 `-1` / `0` / `0x40`（`se_msl`）| 同上 |
| `+0x26` | i16 | **激光命中特效的 anm 脚本号**（hit 表 idx2 `0x460320` 读；魔理沙 = 8）。零售取值 `{0, 5, 8}` | `playershot_hit_laser_idx2`（[`03`](03-func-tables-timers-and-dead-fields.md) §5.1）|
| `+0x27`–`+0x29` | — | 🟡 268 条全 0 | — |
| `+0x2a` | i8 | **fire_rate2**：非 0 时改用 long_timer（`long % fire_rate2 == start_delay2`）| 两处发射循环 |
| `+0x2b` | i8 | start_delay2（零售 268 条全 0）| 同上 |
| `+0x2c` | i32 | **func_on_init** —— 装载时经 `0x4b4230` 解成指针 | `0x45e320` 末尾 |
| `+0x30` | i32 | func_on_tick —— 表 `0x4b4210` | 弹的每帧 |
| `+0x34` | i32 | func_on_draw —— 表 `0x4cf414` | 弹的绘制 |
| `+0x38` | i32 | func_on_hit —— 表 `0x4b41f0` | 弹的命中 |
| `+0x3c`–`+0x4b` | — | 🟡 268 条全 0 | — |
| `+0x4c` | f32 | **横移倾斜系数**：`func_on_init = 3`（`0x460b60`）算 `角 = norm(PLAYER+0x4790c + angle) + PLAYER+0x47910 × 此值`；零售取值 `{0, ±3°…±20°, 1.3, 2.0}` | `playershot_init_tilt_by_move_idx3`（[`03`](03-func-tables-timers-and-dead-fields.md) §5.2）|
| `+0x50`–`+0x5b` | — | 🟡 268 条全 0 | — |

> `+0x3c` 起那 `0x20` 字节正是 [`../th16/05`](../th16/05-th16-flags-no-runtime-read.md) 里「运行时不读的 flags 段」
> 所在的量级。**本文只断言「零售数据里除 `+0x4c` 外全 0」，不断言语义**——要下「不被读」的结论得走那篇的对抗证伪流程。

## 2. 发射判定（两条路径，行为不同）

```c
/* 主炮：Player__tick_shooting_state 0x45ea00 */
if (fire_rate == 0)                     fire = true;              // ★ 显式的 0 分支
else if (fire_rate2 == 0)               fire = (short % fire_rate  == start_delay);
else                                    fire = (long  % fire_rate2 == start_delay2);

/* 装备卡子机：Player__tick_shooters_for_ability_card 0x40a9c0 */
if (fire_rate2 == 0)                    fire = (short % fire_rate  == start_delay);   // ★ 没有 0 分支
else                                    fire = (long  % fire_rate2 == start_delay2);
```

**装备卡路径上 `fire_rate == 0` 会整数除零**。索引 `0x16` 那组正好是 `fire_rate = 0`
（[`01`](01-file-layout-and-shooterset-index.md) §6），所以它一定不走这条路。

命中的那一发调 `Player__shoot_one_bullet(player, (set << 8) | 组内行号, timer, &pos, option)`；
这个 **packed id** 一路存进 `bullet+0xac`，弹自己要回查 shooter 行时再拆开
（`row = (id & 0xff) * 0x5c + sht[0xe0 + (id >> 8) * 4]`）。

## 3. 角度的三个分档（`0x45e320` 一手，常量已读出）

```c
a = shooter[0x14];
if (a < 1000.0f) {
    if (a < 995.0f)              use(a);                    // ← 零售 268 条全走这里
    else if (opt_slot == 0)      use(a);
    else                         use(option[0xa8]);         // 子机自己的角度，无散布
} else {
    if (opt_slot == 0)           use(a);
    else { use(option[0xa8] + randf(-1,1) * π/12);          // ±15° 散布（REPLAY_SAFE_RNG）
           bullet.speed = shooter.speed + 2*randf(-1,1); }  // 速度也抖
}
```

常量：`0x4b93cc` = `995.0`、`0x4b922c` = `π`、`0x4b9260` = `2π`、`0x4b9430` = `−π`、`0x4b927c` = `12.0`。

⚠️ **零售 268 条 shooter 没有一条的 angle ≥ 900**——`995 / 1000` 这两个哨兵在本 build 里是
**死代码**。要用它得自己承担「没被 ZUN 跑过」的风险。想瞄准，用 §5 那条零售天天在跑的路。

## 4. ★ 装备卡子机的弹，贴图来自 `ability.anm`

`Player__tick_shooters_for_ability_card` `0x40a9c0` 在开火前后各做一次替换：

```c
player[+0x10] = ABILITY_MANAGER->ability_anm;   // 开火期间
...  /* 逐 shooter 发弹；PlayerBullet__create 用 player[+0x10] 起 shooter[0x22] 号脚本 */
player[+0x10] = player[+0xc];                   // 还原成 pl0X.anm
```

数值面印证：主炮那 10 组的 `+0x22` 取值 `{5,6,7,8,9}`（`pl0X.anm` 的自机弹脚本），
13 组卡子机取值 `{0,3,8,10,15,17,21,25,53,54,56}`——全部 `< 68` = 零售 `ability.anm` 的脚本数。

> **对 mod 的意义**：给新卡的子机加自定义弹幕贴图，**不用碰四个 `pl0X.anm`**，
> 只要往我们本来就在重建的 `ability.anm` 里加脚本。

## 5. 瞄准：`player+0x479cc` + `func_on_init = 5`（零售天天在跑）

`CardAlice__on_shoot` `0x40b4e0` 的完整链路：

```asm
0x40b74c  call 0x438cb0            ; 最近敌人搜索（见下），半径 0x4b93b0 = 512.0
                                   ; → option+0xe4 = 目标 enemy_id
0x40b520  ...                      ; 按 id 在 EnemyManager+0x18c 链表里找回敌人
0x40b531  test [ecx+0x635c], 0xc000021   ; 不可锁定就放弃并清 option+0xe4
0x40b5b5  movss xmm0, [0x4b9310]   ; 128.0 —— 开火距离上限（比搜索半径小得多）
0x40b5e8  call 0x4a94a0            ; atan2(dy, dx)
0x40b5fa  fstp dword [eax+0x479cc] ; ★ 角度写进 player+0x479cc
0x40b601  call 0x40a9c0            ; tick_shooters(option, short, long, 0xf)
```

而 Alice 那组（`0x0f`）的 `func_on_init = 5` → `0x4612d0`：

```c
int __thiscall func_on_init_5(zPlayerBullet *b) {
    float a = *(float *)(PLAYER_PTR + 0x479cc);   // 卡刚写进去的瞄准角
    a = normalize_to_neg_pi_pi(a);                // 与装载时同一套归一循环
    *(float *)(b + 0x64) = a;                     // 覆写 bullet.angle（+0x64 就是 create 写的那个）
    return 0;                                     // 非 0 会当场废掉这发弹
}
```

**结论**：想让某组子机弹「朝某个方向发」，正路是
**① 把角度写进 `player+0x479cc` ② 该组 shooter 的 `func_on_init` 填 `5`**。
这是**瞄准（发射瞬间定向），不是追踪**——出膛后弹不再改向。

### 附：最近敌人搜索 `0x438cb0` ✅

```c
int *__stdcall find_nearest_enemy_id(int *out_id, const float pos[2]);   // + XMM3 = 搜索半径
// 遍历 EnemyManager+0x18c 链表；跳过 enemy+0x635c & 0xc000021 的；
// 取 (pos - enemy+0x1270/0x1274) 距离平方最小者；写 *out_id = enemy+0x6830；没有则写 0。返回 out_id。
```

用到的敌人字段（本会话一手，此前是 🟡 推断）：
`enemy+0x1270`/`+0x1274` = 世界坐标 x/y（= `zEnemy.data(+0x122c).final_pos(+0x44).pos`）、
`enemy+0x635c` = 状态位（`& 0xc000021` ≠ 0 → 不可锁定）、`enemy+0x6830` = `enemy_id`。
子机自己的坐标在 `option+0x5c`/`+0x60`，**定点数**，乘 `0x4b908c` = `1/128` 变像素。
