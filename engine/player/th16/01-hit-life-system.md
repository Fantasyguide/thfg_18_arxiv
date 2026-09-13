# player 逆向 01:TH16 中弹 / 生命 / 死亡 / 复活系统
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> 方法:Ghidra(ghidra-re MCP)一手反编译 th16.exe(用户自有,ExpHP th-re-data 符号已套)。
> 日期 2026-06-12。分级 ✅高 / 🟡中 / ❓未解。**仅 TH16 v1.00a**。
> 承接 `engine/sht/th16/05`(判定半径=hitbox header+0x04,擦弹)、`04`(player 状态机切口)。

## 0. 一句话结论

自机的"命"由 **player 对象的 5 态状态机(`+0x165a8`)** 驱动:被弹/敌人碰到 → 进 **状态 4(决死窗口,
约 8 帧)**;窗内按炸 → 取消死亡(deathbomb);窗外 → **commit 死亡**(扣 1 命 + miss+1 + 掉半档 power +
重置炸弹库存=3),进 **状态 2** 播死亡/复活动画,约 30 帧后**复活**(回状态 0,约 280 帧无敌),
命 < 0 则进 **Game Over**(续关菜单)。中弹与否的闸门是 **无敌帧 `+0x1663c`**(>0 免疫)。

## 1. 状态机:`player_update_perframe` `0x442560`(switch on `+0x165a8`)✅

每帧由 player 任务入口 `0x443720` 调用。`+0x165a8` 五个状态:

| 状态 | 名 | 行为(一手) |
| --- | --- | --- |
| **0** | 出场 / 复活上升 | 自机从屏幕下方升起:`y = 0xf000 − (帧×0x2800)/0x3c`(定点,约 60 帧到位)。期间在自机
  半径内**清弹**(`et_cancel_special_in_radius` / `BulletManager__cancel_radius_as_bomb`)=出场无敌清屏。帧>0x3b(59)→ 转状态 1 |
| **1** | 存活 | **吃炸弹输入**:`MAIN_BOMB_PTR`+`can_bomb`+`INPUT_RISING_EDGE&2` → 主炸;`SUBSEASON_BOMB_PTR`+
  `can_bomb`+`INPUT_RISING_EDGE&0x800` → 季节释放(见 `02`)。然后 `player_input_move`(移动/聚焦/开火,见 `03`) |
| **2** | 死亡结算 / 复活等待 | 帧==3:**掉 power**(见 §4)+ 喷 7 个掉落物 + `repopulate_options`。帧>0x1d(29):命<0 且帧==0x1e → `FUN_0043f350`(Game Over,§6);否则**复活**(回状态 0,§5) |
| **3** | (过场)| 帧==0xf 时 `et_clear_all_special(1)`;原版极少见,🟡 |
| **4** | **决死窗口(deathbomb)** | 帧>7 → `FUN_00443cd0`(commit 死亡,§4)然后落到状态 2 逻辑;否则按炸(`INPUT_RISING_EDGE&2`/`&0x800`)+`can_bomb` → `Bomb__activate_bomb` + `player_set_alive_after_bomb`(决死成功) |

> 状态切换后,函数下半段(与状态无关)每帧还做:256 个伤害源运动/寿命更新(见 `engine/sht/th16/08`)、
> **无敌帧渲染 + 倒计时**(`+0x1663c`,§3)、自机判定矩形世界角点重算(`+0x2c730..`)、聚焦缩放、
> 帧计数器自增、**开火大门**(满足条件才调 `Player__tick_shooting_state`,见 `03`)、`Player__tick_bullets`。

## 2. 中弹判定链:碰撞 → 死亡入口 ✅

```
敌弹每帧 EnemyData/BulletManager
  └─ bullet_vs_player_collide @0x4124b0          // 弹 flag+0x20 选判定形状
        ├─ player_collide_rect   @0x4438c0       // 矩形弹
        ├─ player_collide_circle @0x4439e0       // 点/圆弹(主路径)
        └─ player_collide_laser_obb @0x443af0    // 激光 OBB
              ↓ 返回 1=命中 / 2=擦弹 / 0=未中
        ret 1 → 弹标记消亡(+0xc72=3)+ 命中特效;判定内部已调 player_on_death
        ret 2 → player_graze(擦弹,见 engine/sht/th16/05 §4b:计数 +音效0x2a)+ 弹置"已擦"位(bit2)防重复
```

**`player_collide_circle` `0x4439e0`(一手,补 `../sht/05` §4b)**:
- 距离² = (player `+0x610/+0x614` − 弹坐标)²;判定半径 `r = *(player+0x2c788 + 4)` = 主 .sht header `+0x04`
  **hitbox**(init 被按角色硬表覆写为 3.0,见 `../sht/05`);弹半径经 XMM2(=弹 `+0xc40`)。
- **聚焦缩放**:`if (player+0x1664c & 0x10) r *= player+0x2c7c8 × 3.6`(🟡 见 §7 注:`0x1664c&0x10` 语义待定,
  非聚焦本体——真正的聚焦标志是 `+0x165c8`)。
- 内环命中(dist²<r²)→ **`player_on_death`**(仅当 `+0x1663c < 1`,即**非无敌**;且状态不在 2/3/4)→ ret 1;
  内外环之间 → ret 2 擦弹;环外 → ret 0。
- **★ 无敌闸门坐实**:`if (*(int*)(PLAYER_PTR+0x1663c) < 1) player_on_death(...)` —— `+0x1663c` 就是无敌帧,
  >0 时碰撞不致死(仍 return 1 但不调 death)。

### 2b. 另两种判定形状(本会话补全,一手)✅

三个判定函数**共享同一套闸门**(GUI 不在对话、`param_3==0` 非"仅擦弹探测"、状态 ∉ {2,3,4}、`+0x1663c<1`
非无敌 → `player_on_death`;ret 1=命中 / 2=擦弹 / 0=未中)。差别只在几何:

| 函数 | 形状 | 判定几何(一手) |
| --- | --- | --- |
| `player_collide_circle` `0x4439e0` | 点/圆 | 距离² vs (hitbox+弹半径)²;聚焦缩放 hitbox(`../sht/05`,§2 上) |
| `player_collide_rect` `0x4438c0` | 矩形 | 弹 AABB(中心 `+0xc20` ± `+0xc40`×0.5)vs 自机矩形 `PLAYER+0x2c730..0x2c740`;擦弹环 = 自机矩形外扩 **24** |
| `player_collide_laser_obb` `0x443af0` | 激光(旋转 OBB)| 把(自机−激光起点)用 `crt_sinf/crt_cosf(−激光角)` **旋进激光本地系**,测盒 `[0,半长]×[−宽,+宽]`,再按自机 hitbox(`+0x2c748/+0x2c74c`×16)膨胀;擦弹=膨胀环 |

> `player_collide_rect/laser_obb` 与 circle **完全同构的致死逻辑**(状态/无敌闸门一致)——交叉印证 §2 的判定模型。
> 自机判定矩形角点 `PLAYER+0x2c730..` 每帧由半径 `+0x2c748..` 在 `player_update_perframe` 末尾重算(`engine/player/th16/01` §1)。
> (laser_obb 在 2026-06-09 `engine/bullet/th16/` 工作中已反并加注释,本篇仅纳入生命系统视角,结论一致。)

### 2c. 擦弹 `player_graze` `0x444cf0`(本会话亲验,补 `../sht/05`)✅

`bullet_vs_player_collide` 判 ret 2 且弹未擦过(`+0x20 & 4`==0)→ `player_graze(弹坐标)` + 置弹"已擦"位
(`+0x20 |= 4`,防同弹重复计)。`player_graze`:
- `GRAZE @0x4a57c0 += 1`、`GRAZE_IN_CHAPTER @0x4a57c4 += 1`(各封顶 99999999)→ 印证 `../sht/05` §4b 的
  `DAT_004a57c0/c4` = 擦弹计数(HUD / 分数弹出计数)。
- 生成擦弹粒子(anm 0x18)+ `PopupManager__generate_small_score_popup`(小分数弹出)+ **音效 0x2a(=42)**。
- 末尾 `ItemManager__spawn_item(0x10, …, atan2(...))` 在擦弹处按角度喷一个 type 0x10 道具/效果(🟡 用途未深究)。

## 3. 无敌帧 `+0x1663c` ✅

一个 Timer(cur=`+0x1663c` int、`+0x16640` float、prev=`+0x16638`)。在 `player_update_perframe` 下半段
每帧 −1(走 `__ptr_GAME_SPEED_MULT_FROM_ECL` 调速)。`<1` 时玩家可中弹;`>0` 时**判定免疫**(§2)且
自机精灵**闪烁**(`+0x538` 颜色 + `+0x544` 标志位,按 `+0x628 % 3` 红/透明交替)。被各事件设值:

| 设值处 | 值(帧) | 场景 |
| --- | --- | --- |
| `player_on_death` `0x443f10` | **6** | 刚中弹进决死窗口(短暂) |
| `FUN_00443cd0`(commit 死亡)| **0xb4=180** | 死亡黑屏/复活等待期 |
| 复活(状态2→0)| **0x118=280** | 复活后约 280 帧无敌 |
| `BombSub*::begin`(季节释放)| **10** | 释放瞬间短无敌(见 `02`)|
| 主炸 `BombMain*` | (炸发期更长,见 `02`)| |

## 4. 死亡 commit:`FUN_00443cd0`(= player_commit_death / on_miss)✅

决死窗口超时(状态4 帧>7)调用。建议 DB 名 `player_commit_death_on_miss`。

```c
EffectManager 生成死亡特效 anm 0x1c
CURRENT_LIVES = CURRENT_LIVES − 1;           // ★ 扣 1 命
CURRENT_BOMBS = 3;                            // ★ 下条命的炸弹库存重置为 3
Gui__update_bombs(...); if (LIVES>=0) Gui__update_lives(...);
player+0x165a8 = 2;                           // → 状态 2(死亡结算)
player+0x628 = 0;                             // 状态帧计数清零
player+0x1663c = 0xb4(180);  +0x16640 = 180.0f // 死亡黑屏/等待 180 帧
中断 4 个 option 的 anm 树;
SPELLCARD 失败处理(见 §4b);
ENEMY_MANAGER+0x3c += 1;  ENEMY_MANAGER+0x44 = 0;
if (GLOBAL_MISS_COUNT < 999999) GLOBAL_MISS_COUNT += 1;   // ★ miss 计数(死亡次数)
```

**掉 power(在状态 2、帧==3 时,`player_update_perframe` case 2)**✅:
```c
CURRENT_POWER -= POWER_PER_LEVEL/2;  if (CURRENT_POWER < POWER_PER_LEVEL) CURRENT_POWER = POWER_PER_LEVEL;
// 在自机位置扇形(7 发,±)喷出掉落物 ItemManager__spawn_item(...×7...) —— 死亡掉的 power 道具
PlayerInner__repopulate_options(...);          // 重建子机(option)
```
→ 死亡掉**半档 power**(地板=1 档),并把掉的 power 散成 7 个道具喷出。

### 4b. 与符卡(spellcard)的交互 🟡
`player_on_death` 与 `FUN_00443cd0` 都查 `SPELLCARD_PTR+0x78`:符卡奖励位(bit0)开启且符卡已进行
`+0x24 < 0x3c`(60 帧)且 `MAIN_BOMB_PTR+0x30==1`(主炸正在发)→ 置 bit0x20(疑似"符卡因决死/炸取消");
`>=60` → 清符卡奖励(`+0x7c=0`,bit 清)= **符卡 capture 失败**。语义 🟡,够定性"死/炸会判符卡失败"。

## 5. 复活流程(状态 2,帧>0x1d)✅

命 ≥ 0 时(命 < 0 → Game Over,§6):
```c
player+0x165a8 = 0;                            // 回状态 0(出场上升动画,§1)
GAME_SPEED = 1.0;
Player__create_damage_source_4449b0(player, &pos, 0x1e, 0x96);  // 复活清屏式伤害源(半径式)
CURRENT_BOMBS = 3;  Gui__update_bombs(...);    // 炸弹库存=3
player 坐标复位到 +0x610.. → Player__set_position
player+0x1663c = 0x118(280);  +0x16640 = 280.0f // 复活无敌 280 帧
player+0x628 = 0;  +0x624 = -1;                // 状态帧计数复位
```
→ **复活点固定**(屏幕下方中央),**炸弹补满到 3**,**280 帧无敌**,自机走出场上升动画再转存活。

## 6. Game Over:`FUN_0043f350` ✅(建议 DB 名 `enter_gameover_continue_menu`)

状态 2、命 < 0、帧==0x1e 时调用(且 replay 模式 `REPLAY_MANAGER+0xc!=1` 才走)。拉起
`PAUSE_MENU_PTR`(续关/Game Over 菜单):`PauseMenu` 状态置 2、拍快照、播音效 0xe、BGM 切 "Pause"、
`GAME_SPEED=1.0`、写计分文件标志。= 标准"Continue?"界面。

## 7. player 对象生命相关字段图(本篇坐实)

| 偏移 | 含义 | 可信 |
| --- | --- | --- |
| `+0x165a8` | 主状态机(0/1/2/3/4) | ✅ |
| `+0x624/628/62c` | 当前状态帧计数 Timer(prev/cur-int/cur-float)| ✅ |
| `+0x1663c/16640/16638` | **无敌帧** Timer(cur-int/cur-float/prev)| ✅ |
| `+0x1664c` | 标志位:bit1(0x2)=擦弹累计触发、bit2(0x4)=禁射、**bit4(0x10)=禁射 + 判定半径特殊缩放**(语义 🟡,**非聚焦**) | 🟡 |
| `+0x610/614/618` · `+0x61c/620` | 自机 x/y/z(float)· x/y(定点)| ✅ |
| `+0x2c730..0x2c744` 等 | 判定/收集/吸附矩形的世界角点(每帧由半径 `+0x2c748..` 重算)| ✅ |
| `+0xd114`(+link×0x94)| 256 个伤害源池(见 `../sht/08`)| ✅ |

全局:`CURRENT_LIVES @0x4a57f4`、`CURRENT_LIFE_FRAGMENTS @0x4a57f8`、`CURRENT_BOMBS @0x4a5800`、
`CURRENT_BOMB_FRAGMENTS @0x4a5804`、`GLOBAL_MISS_COUNT @0x4a57cc`、`CURRENT_POWER @0x4a57e4`、
`POWER_PER_LEVEL @0x4a57ec`。

## 8. 关键数字一览(原版 TH16,一手)

- **决死(deathbomb)窗口 ≈ 8 帧**(状态4,`+0x628` 0→7;帧>7 即 commit)。
- **死亡惩罚**:命 −1、miss +1、power −半档(地板 1 档)、掉 7 个 power 道具、炸弹库存重置 3。
- **复活无敌 280 帧**(≈4.6s @60fps);死亡黑屏 180 帧;决死瞬间 6 帧;季节释放 10 帧。
- 命 < 0 → Game Over / 续关。

## 9. 可信度 / 复核

- ✅ 一手:`player_update_perframe`(`0x442560`)、`player_on_death`(`0x443f10`)、`FUN_00443cd0`(`0x443cd0`)、
  `player_set_alive_after_bomb`(`0x444070`)、`bullet_vs_player_collide`(`0x4124b0`)、`player_collide_circle`
  (`0x4439e0`)、`FUN_0043f350`(`0x43f350`)全部本会话反编译。
- 🟡 `+0x1664c` 各位语义、符卡交互细节、状态 3 用途未深挖。
- 复核入口:Ghidra DB `th16`,地址见上。
- ❗ 纪律:本篇是 player 运行时一手结论;数值(8 帧 / 180 / 280)来自 init 立即数,**未动态实测**——
  与游戏内体感若冲突先疑自己的帧换算(GAME_SPEED 调速会影响实际墙钟时长)。
