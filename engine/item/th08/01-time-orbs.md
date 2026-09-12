# item 逆向 01：TH08 时点（刻符）道具 —— 类型、掉落、自动收集与中弹停住
> **版本**：TH08 v1.00d（`th08.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>
> **方法**：一手平坦反汇编（`objdump -d -M intel` 产出 `local/_scratch/th08_full.asm`）
> + ExpHP `local/vendor/th-re-data/data/th08.v1.00d/funcs.json` 命名 + ECL 操作码跳表直读
> + `local/th08.v1.00d/dat/`（TH08 的条目 THTK 解不开，用 `tooling/thtk/extract_th08.py` 自写提取）。日期 2026-09-12。
> **可信度**：§2–§6 的**数量、类型、条件、地址**全部 ✅（一手指令）；§7 的**运动学正负号约定**标 🟡（见 §7 注）。

## 0. 一句话结论

TH08 的「时点」在引擎里就是一个**普通道具**：**类型 7**（另有一个别名 **类型 10**，在 `ItemManager::spawn_item` 里
被改写成「类型 7 + state 5」）。它**不是敌人**，走的是和 P 点 / 蓝点**同一张道具池与同一套运动学**（`ItemManager`，
池 = 0x830 个 `Item`，每个 0x2e4 字节，`th08:0x4400ac` 的 `imul ecx,ecx,0x2e4`）。

「大量掉」的两个大头（与用户口径完全对上，见 §4/§5）：

| 触发 | 数量 | 证据 |
| --- | --- | --- |
| **击破 boss / 符卡结束** | **100 / 700 / 1000**（按用时比例，见 §5） | `Spellcard::ecl_spell_end` `th08:0x4161b0` 写 `spellcard+0x10`；`Spellcard::sub_416b10` `th08:0x416b10` 按 ≤7+≤6 每 8 帧喷出 |
| **击破敌怪**（含它 ECL 连锁出来的子敌机） | **6..26**（某分支到 **40**），按「连锁段数」算 | `Enemy::sub_42adb0` `th08:0x42adb0` 的 `th08:0x42aec9`–`th08:0x42af45` 三条公式 |

**「一定会被自动收集」体现在 state 3**：`spawn_item` 对类型 7 强制 `state = 3`（`th08:0x440115`），
state 3 是**时点专属的应收分支**（`th08:0x4409f2` `cmp state,3` → `th08:0x440c37`），
与普通道具「落到回收线才收」（state 0，`th08:0x44095e` 读 `player_y + off` 比较）不是同一条路。
**中弹会停住**这一条也有一手落点：`ItemManager::cancel_autocollect` `th08:0x4413e0` 把**整池**道具的 `state` 改成 1
并重设速度；再起时 `ItemManager::sub_441530` `th08:0x441530` 把 state 1 的改回 0（§7）。

---

## 1. 道具对象字段图（一手，`Item` = 0x2e4 B）

| 偏移 | 类型 | 含义 | 证据 |
| --- | --- | --- | --- |
| `+0x220` | `void*` | 某个已注册的附属对象（非 0 时 state 3 退场要先在 `th08:0x45ea00` 注销它） | `th08:0x440c4a` / `th08:0x440c5d` |
| `+0x2a4` | `Float3` | **位置**（`pos += vel*dt` 的落点） | `th08:0x44094d`–`0x440956`（`lea ecx,[item+0x2a4]; call Float3::operator+=`） |
| `+0x2b0` | `Float3` | **速度**（每帧积分：`th08:0x44093e` 求 `vel*dt`，`th08:0x44094d` 累加到 pos） | 同上 + `th08:0x4402b0`（spawn 时赋值） |
| `+0x2bc` | `Float3` | 第二组向量（只在 state 2 的「从上方撒入」初始化里写） | `th08:0x44026e` / `0x44028c` / `0x440295` |
| `+0x2c8` | `zTimer`（12 B） | 道具自己的计时器（spawn 时 `set(0)`） | `th08:0x44024b`（`push 0` + `call 0x4065f0`）、`th08:0x440c42`（`call 0x406640`） |
| `+0x2d4` | `uint8` | **道具类型** | `th08:0x44022e` `mov BYTE PTR [ecx+0x2d4],dl`；读点 `th08:0x440a43` `movsx edx,BYTE PTR [ecx+0x2d4]` |
| `+0x2d7` | `uint8` | **state**（★ 见 §2） | `th08:0x44023a` `mov BYTE PTR [eax+0x2d7],cl`（cl 来自 `spawn_item` 的 arg2 `[ebp+0x10]`） |
| `+0x2d8` | `uint8` | 另一枚标志字节（「已达上限」一族；`th08:0x4403f6` 清 0） | `th08:0x4403f6` 等 |
| `+0x2dc` | `Item*` | 池内链表 next（遍历用） | `th08:0x441432` / `th08:0x440c65` |

> ⚠️ 字段名是本文起的（ExpHP 只给了 `Item` 的构造函数/析构，没给字段），但**偏移与语义都有一手写读点**。

## 2. `ItemManager::spawn_item` `th08:0x4400a0` —— 类型与 state

签名（cdecl 从调用点读回，`th08:0x4400a0` 起）：

```
ItemManager::spawn_item(Float3 *pos /*[ebp+8]*/, int type /*[ebp+0xc]*/, int state /*[ebp+0x10]*/)
```

函数头的三条硬改写（**这是理解时点的关键**）：

| 地址 | 指令 | 语义 |
| --- | --- | --- |
| `th08:0x4400f1`–`0x44010e` | `get_current_power()`；`if (power >= 0x80 && (type==0 || type==2)) type = 8` | 火力满 ⇒ P 点降级成小点 |
| `th08:0x440115` | `cmp DWORD PTR [ebp+0xc],0x7` → `jne`；`mov DWORD PTR [ebp+0x10],0x3` | **类型 7 ⇒ state 强制 = 3**（时点专属应收态） |
| `th08:0x440124` | `cmp DWORD PTR [ebp+0xc],0xa` → `jne`；`mov DWORD PTR [ebp+0x10],0x5` + `mov DWORD PTR [ebp+0xc],0x7` | **类型 10 = 时点的另一种生成方式**（⇒ 类型 7 + state 5） |

写入 item 的初值（`th08:0x440201`–`0x44024b`）：

```
item[+0x2b0] = 0.0f          ; vx
item[+0x2b4] = 0xc00ccccd    ; vy = -2.2f   ← 「道具弹出来」的初速（§7 有正负号讨论）
item[+0x2b8] = 0.0f          ; vz
item[+0x2d4] = (uint8)type
item[+0x2d7] = (uint8)state
Timer(item+0x2c8).set(0)
```

按 state 分派（`th08:0x440250` 起）：

| state | 分支 | 一手内容 |
| --- | --- | --- |
| **2** | `th08:0x440256` | 「从上方撒入」：`item[+0x2bc] = randf(288.0f) + 400.0f`、`item[+0x2c0] = randf(192.0f) − 96.0f`、`item[+0x2c4] = 0`，并把 `item[+0x2a4..0x2ac]` 拷进速度 |
| **3** | `th08:0x4402cb` | **时点主角**（初速两条随机 + `shottype == 2` 时降级成 state 0，公式见下面 §2.1） |
| 5 | `th08:0x440340` 之后 | 类型 10 改写来的那一档（`th08:0x440730` 的 `cmp state,5` 分支：`vy += 0x4b5b38 * dt`，越界后转 state 1） |

### 2.1 state 3（时点）的初速

```
item[+0x2b4] (vy) = ds:0x4b5b28 − Rng::randf_0_to_x(0x164d520, 0.2f)     ; th08:0x4402cb–0x4402e3
item[+0x2b0] (vx) = Rng::randf_neg_x_to_x(0x164d520, 0.6f)               ; th08:0x4402e9（th08:0x4143c0）
if (*(uint8_t *)0x17d5ef8 == 2) {          // 0x17d5ef8 = PLAYER 基址，+0 = shottype 字节
    item[+0x2d7] = 0;                      // state 从 3 降成 0（走「落到回收线才收」那条）
    item[+0x2b0] = 0.0f; item[+0x2b4] = -0.9f (0xbf666666); item[+0x2b8] = 0.0f;
}                                                                        ; th08:0x440301–0x44032e
```

### state 一览（读点全在这一段里，✅ 地址一手）

| state | 一手读点 | 行为（读出来的） |
| --- | --- | --- |
| **0** | `th08:0x44095e` | 普通下落 + **落到回收线才收**：`if (item.pos.y <= zPlayer.y + ds:0x4b42d4)` ⇒ `th08:0x44098c` 结算 + `th08:0x440994` `Item::remove_from_active_list` |
| **1** | `th08:0x4407f6` `cmp eax,1 / je 0x44085f` | ★ **归航（朝自机飞）**：每帧重算速度指向自机（细节见 §7.2）|
| **2** | `th08:0x4405ad` `cmp edx,2` | 「从上方撒入」的飞行段（用 `Timer(item+0x2c8)` 做插值，`th08:0x4405ca`–`0x44061d`） |
| **3** | `th08:0x4409f2` `cmp ecx,3` → `th08:0x440c37` | **时点专属**：先 `Timer(item+0x2c8)` 推进，若 `item+0x220 != 0` 则 `th08:0x45ea00` 注销附属对象，然后从活跃表摘除（收下 = `Item::collect_time_orb`） |
| **5** | `th08:0x440737` `cmp edx,5` | 类型 10 档的上浮/越界转 1 |

> ★ **2026-09-12 订正**：本表初版把 state 1 写成「被停下」、state 2 写成「飞向自机」，**方向反了**。
> 指令原文：`th08:0x4408ad` 与 `th08:0x4408f0` 是**两个不同**的写字 ——
> `th08:0x4408ad mov BYTE PTR [edx+0x2d7],0x1`（归航段末尾，置 **1**）与
> `th08:0x4408f0 mov BYTE PTR [eax+0x2d7],0x0`（玩家已死那条路，置 **0**）。
> 而 state 1 的分支头 `th08:0x4407f6 cmp eax,0x1 / je 0x44085f` 落到的正是「每帧把速度指向自机」那段。


## 3. 谁在生时点 —— 全 exe 穷举

全 exe `call 0x4400a0`（`ItemManager::spawn_item`）共 **58 处**。按 `[ebp+0xc]`（type）参数筛，
**type = 7 或 10** 的调用点如下（其余全是 P/点/1UP/Bomb/小点）：

| 调用点 | 所在函数（`th-re-data` 名） | push 的 type/state | 触发条件（一手） |
| --- | --- | --- | --- |
| `th08:0x41761d` | `Spellcard::sub_416b10` | `0xa` / `0` | **符卡结束时喷时点**（§5） |
| `th08:0x4176d1` | `Spellcard::sub_416b10` | `0xa` / `0` | 同上，第二个角度各喷 6 个 |
| `th08:0x42b048` | `Enemy::sub_42adb0` | `7` / `3` | **敌机死亡**（§4） |
| `th08:0x42b1b1` | `Enemy::sub_42adb0` | `7` / `1` | 连锁子机尾巴（§4） |
| `th08:0x42b2ae` | `Enemy::sub_42adb0` | `7` / `1` | 同上，单发 1 个 |
| `th08:0x42da54` | `EnemyManager::on_tick` | `7` / `1` | 门 = `th08:0x406d10` / `th08:0x406d70` 两个 GameManager 查询同时为真 |
| `th08:0x4308e5` | `BulletManager::cancel_all` | 动态 / `1` | **消弹**：子弹被消时按 `[bullet+0x6ba570]` 的值掉 |
| `th08:0x430a63` | `BulletManager::cancel_all` | 动态 / `1` | 同上（另一条消弹路径） |
| `th08:0x430b3a` | `BulletManager::sub_430aa0` | 动态 / `1` | 局部消弹 |
| `th08:0x430c77` | `BulletManager::sub_430aa0` | 动态 / `1` | 同上 |
| `th08:0x430cf0` | `BulletManager::sub_430aa0` | 动态 / `1` | 同上 |
| `th08:0x430de9` | `BulletManager::ecl_161__cancel_radius?` | `6` / `1` | ECL 161 半径消弹（掉的是类型 6） |
| `th08:0x431650` `0x431668` | `BulletManager::on_tick` | `7` / `1` ×2 | **擦弹**：`ds:0x18b8988 == 9` 时掉 2 个；否则掉 `ds:0x18b8988` 个类型 7 |
| `th08:0x43170a` `0x431722` `0x431749` | `BulletManager::on_tick` | `7` / `1` ×2 或动态 | 同上另一处 |
| `th08:0x431837` `0x43184e` `0x431876` | `BulletManager::on_tick` | `7` / `1` ×2 或动态 | 同上 |
| `th08:0x431948` `0x431960` `0x431987` | `BulletManager::on_tick` | `7` / `1` ×2 或动态 | 同上 |
| `th08:0x431a59` `0x431a71` `0x431a98` | `BulletManager::on_tick` | `7` / `1` ×2 或动态 | 同上 |
| `th08:0x44aac9` `0x44aaef` `0x44ab0f` | `Player::sub_44a6a0` | `0xa` / `1` | 玩家相关（`0x44aaae` 起先查 GameManager 两个查询，再查符卡状态） |
| `th08:0x451892` | `Player::compute_damage_to_enemy` | `7` / `3` | **自机弹打死敌机**时补一颗（`th08:0x451871` 读 `[enemy+0x480]+0x1e < 0` 判死） |

> 交叉印证：`th08:0x18b8988` 这个全局在 `th08:0x43163d` / `0x43166f` 被当**数量**用（`push ds:0x18b8988` 当 type），
> 同一地址在 `th08:0x4308cf` / `0x430b25` 也被当数量用 —— 🟡 是「擦弹连击档」一类的档位值，本文未追它怎么涨。

## 4. 敌机死亡掉多少 —— `Enemy::sub_42adb0` `th08:0x42adb0`

结构（一手）：

```
[enemy] 起沿 enemy+0x8 连锁遍历（每个连环子机各掉一次）：
    n = Enemy::sub_41fd40(enemy)     ; th08:0x41fd40 = 沿 enemy+0x8 数连锁段数（th08:0x41f000 判「算不算」）
    if (n == 0) goto 尾巴(0x42b1e5)
    ... 主道具掉落 ...
    N = 时点数（下面三条公式）
    if (ds:0x17d6ed4 != 0) N = N / 3          ; th08:0x42af48 —— 某个「减半/练习」模式的开关
    for (i = 0; i < N; i++)
        spawn_item(敌人位置 + 半径 128 的环上, type=7, state=3)   ; th08:0x42b03b
```

`N` 的三条公式（**按 `shottype_is_*` 分支**，`ecx = 0x160f508` = GameManager）：

| 分支 | 判定 | `n >= 阈值` | 否则 |
| --- | --- | --- | --- |
| 妖単独 | `GameManager::shottype_is_solo_youkai` `th08:0x42f270` 为真 | `n >= 0xa` ⇒ **N = 26** | `N = n*2 + 6`（`th08:0x42aee9` `lea ecx,[eax+eax*1+0x6]`） |
| 人単独 | `GameManager::shottype_is_solo_human` `th08:0x42f230` 为真 | `n >= 4` ⇒ **N = 40** | `N = n*6 + 16`（`th08:0x42af18` `imul eax,eax,0x6` / `add eax,0x10`） |
| 双人组 | 都不真 | `n >= 8` ⇒ **N = 26** | `N = n*2 + 0xa`（`th08:0x42af3b` `lea eax,[edx+edx*1+0xa]`） |

⇒ **击破一个带 8 段以上连锁的敌机（boss 部件、大妖精群）会一次喷 26 个时点**；人単独时阈值只要 4 段就满 40。

环上喷射的角度：`Rng::randf_0_to_x(2π)`（`th08:0x40d390` 传 `0x164d520` 这个 RNG）后 `+ π/2`（`th08:0x40490fdb`），
半径 `0x43000000` = **128.0f**（`th08:0x4286e0` 造 Float3 → `th08:0x410a70` 加到敌人位置）。

尾巴（`n == 0` 或连锁走完后 `[ebp+8] != 0`，`th08:0x42b0e5`）：用 `enemy+0x3380` 弹一个
`AsciiManager::sub_403460_makes_familiar_popup`（`th08:0x403460`），再按 `i < enemy[0x3380] * 2` 的循环各喷 1 个 type 7 / state 1
（`th08:0x42b1a4`），最后再补 1 个（`th08:0x42b29b`）。🟡 `enemy+0x3380` 的语义未定名（在 `zEnemyData` 里落在
`+0x3380−0x122c = 0x2154`，超出 ExpHP 现有字段图）。

## 5. 击破 boss / 符卡结束掉多少 —— `th08:0x416b10` + `th08:0x4161b0`

### 5.1 数量：`Spellcard::ecl_spell_end` `th08:0x4161b0` 写 `spellcard+0x10`

```
spellcard[+0x104] = spellcard[+0xfc]                  ; 记下符卡分终值
if (boss_enemy[+0x3324] >> 0x1b & 1)                  ; th08:0x416295 —— 某个「不算击破」位
        spellcard[+0x10] = 0x2bc  (700)               ; th08:0x4162a2
else:
        t     = Timer(spellcard+0x114).current
        limit = t - t/7                               ; th08:0x4162c7–0x4162d4（idiv 7 后相减）
        e     = Timer(spellcard+0x108).current
        if (e >= limit)  spellcard[+0x10] = 0x3e8 (1000)                    ; th08:0x4162ef
        elif (e >= 0xb4 (180))
                spellcard[+0x10] = (e - 180) * 900 / (limit - 180) + 100     ; th08:0x41631b–0x416338
        else             spellcard[+0x10] = 0x64 (100)                      ; th08:0x416340
```

⇒ **一张符卡结束时挂上 100 / 700 / 1000 个时点的「待生成额度」**；用时越短（`e` 越接近 `limit`）额度越高。
`Spellcard::get_time_orbs_not_yet_spawned` `th08:0x453cc0` 就是读 `[ecx+0x10]`，HUD 用它显示「还剩几颗没吐」。

### 5.2 节奏：`Spellcard::sub_416b10` `th08:0x416b10` 分批喷出

```
每帧（th08:0x41751a push 8 → th08:0x4066c0(Timer(spellcard+0x338), 8) 命中）且 spellcard[+0x10] > 0：
    角度 = Timer(spellcard+0x338).f 经过 th08:0x40b8c0/fsub/fmul/fdiv/fsub 折成一个角，再 th08:0x43edb0 规约
    位置 = 敌人位置 + (半径 128.0f，该角度)
    k = min(spellcard[+0x10], 7)                       ; th08:0x4175cc → 0x4175f3
    for (i = 0; i < k; i++)  spawn_item(位置, type=0xa, state=0)   ; th08:0x41761d
    spellcard[+0x10] -= k
    角度再 +π/2（th08:0x417639 push 0x40490fdb）
    位置 = 同上重算
    for (i = 0; i < 6; i++)  spawn_item(位置, type=0xa, state=0)   ; th08:0x4176d1（固定再喷 6 个）
    spellcard[+0x10] -= 6（th08:0x4176e1，且夹到 0）
```

⇒ 1000 颗时点按 **每 8 帧 13 颗** 喷，约 **77 批 / 约 616 帧（≈10 s）** 喷完；700 颗约 7 s；100 颗约 1 s。
（`th08:0x4066c0` 判的是「计时器命中 8」⇒ 8 帧一批 —— 🟡 该 Timer 方法的精确语义未逐指令核实，只核到
「push 8 + 每帧调用 + 返回真假」。）

## 6. 收集时给什么 —— `Item::collect_time_orb` `th08:0x4412b0`

（这一节与既有研究稿一致，复述以便单文件自足；细账见 `local/_scratch/research-th08-night-points.md` §2.2。）

| 项 | 一手 |
| --- | --- |
| 分派入口 | `ItemManager::on_tick__` `th08:0x440500` 内跳表 `th08:0x440cc5`（9 项），**type 7 → `th08:0x440bf9` → `call 0x4412b0`** |
| 分数 | `if (stage 蓝点数 >= 2000) 10000 else max(100, (本局蓝点数/2)*10)`（`th08:0x4412c8`/`0x4412e8`/`0x4412ee`）；`ds:0x18b8974 != 0` 时固定 100（`th08:0x4412ba`） |
| 计数 | `GameManager::sub_418220(1)`（`th08:0x44137a`）⇒ 刻符 +1、**夜点 += 5/颗（每 2 颗 +10）** |
| 符卡加成 | `Spellcard::sub_416b10(0x1f40 = 8000)`（`th08:0x441389`） |
| 人妖槽 | 同帧 tick 归零时 `add_to_youkai_gauge(±111)`（`th08:0x4413cb`，聚焦 = +111） |

### 6.1 时点的贴图与它自己的 anm 脚本（一手）

- 贴图 = `data/etama.anm` **entry1**（贴图文件 `data/etama/etama2.png`，256×256）的 **sprite179 = { x: 112, y: 64, w: 16, h: 16 }**。
- 它自己的 anm 脚本 = 同一个 entry 的 **script68**：

```
script script68 {
    sprite(sprite179);
+30: // 30
    colorTime(20, 0, 128, 128, 128);
    alphaTime(20, 0, 192);
+20: // 50
    static();
}
```

- 脚本号 `= 61 + type`：`th08:0x4403b9 mov edx,DWORD PTR [ebp+0xc]` / `th08:0x4403bc add edx,0x3d` /
  `th08:0x4403ca call 0x4069f0`（把脚本装进 VM）。
- 旁证（另一条独立链，**同为 179**）：`ItemManager::on_draw__` `th08:0x4415a0` 用 `type + 0xac` 取 sprite ——
  `th08:0x4416f4 movsx ecx,BYTE PTR [eax+0x2d4]` / `th08:0x4416fb add ecx,0xac` / `th08:0x441706 call 0x45e430`。
- `on_draw__` 的另一支 `th08:0x44164b add edx,0xb6`（= `182 + type`，受「`item.pos.y < −8.0f`」门控的贴边指示）
  type 7 → sprite189，**实测整块全透明** ⇒ 时点**没有**贴边指示图标。

## 7. 「一定会被自动收集」与「中弹停住」

### 7.1 「一定会被自动收集」—— 时点的 state 3 归宿

类型 7 一生成就是 **state 3**（§2.1），而 **state 3 是一条「必然走到归航」的通道**：

```
state 3（th08:0x44068d 那一段）：
    vel.y += ds:0x4b5b38 (0.05f) * dt          ; th08:0x4406ac（重力往上加，见 §7.0 的正负号）
    pos   += vel * dt                          ; th08:0x440747
    if (vel.y > 0x4b4384 (0.0f))               ; th08:0x4406be / th08:0x4406c4
        state = 1                              ; th08:0x4406e4  ← ★ 转「归航」
```

出生初速（§2.1）`vel.y = ds:0x4b5b28 (−2.0f) − rand(0.2)` ⇒ 从 −2.0 起、**+0.05/帧**，
**约 40 帧后 vel.y 过 0 ⇒ 必然转 state 1（归航）**。这就是用户口径「时点是一定会被自动收集的」的一手落点：
它不需要玩家做什么，**自己飞 0.7 秒就进入归航**。
（**state 3 期间不可收**：`th08:0x4409f2 cmp ecx,3 / je 0x440c37` 把 state 3 从「落到回收线才收」那条路里摘出去。）

### 7.2 归航（state 1）与「落到回收线才收」（state 0）

- **state 1 = 归航**：每帧重算速度指向自机 —— `th08:0x4407f6 cmp eax,1 / je 0x44085f` →
  `th08:0x440886 call 0x44c1b0`（拿自机方向/距离）+ `th08:0x4408a5 call 0x4286e0`（造速度 Float3，模长取 `[ds:0x18b896c]+0x14`）
  写进 `item+0x2b0`。**state 1 的道具碰到自机就收**。
- **state 0 进归航的门**（`th08:0x4407fb`–`0x440859`，全部满足才进）：
  `ds:0x17d61b0`（收集线）`>` `[ds:0x18b896c]+0x1c`（道具自己那条线），**且**下列任一：
  ① `get_current_power() >= 0x80`（128，`ds:0x4b5b30`）；② `ds:0x17d5efb`（`PLAYER+3` = `is_focused`）≠ 0；
  ③ `ds:0x164d0b1 == 1`；④ `ds:0x164d0b1 == 6`。
  进门后同样是 `th08:0x4408ad state = 1`。
- **state 0 的收下条件**（`th08:0x44095e test edx,edx` 非 0 就跳过 → 只有 state 0 走到）：
  `if (item.pos.y <= ds:0x164d2e8 + ds:0x4b42d4 (+16.0f))` ⇒ `th08:0x44098c` 结算 + `th08:0x440994` `Item::remove_from_active_list`。

### 7.3 中弹 ⇒ 归航停住、按 P 点那样下落

**两块一手**，都要交代：

**① `ItemManager::sub_441530` `th08:0x441530`**（ExpHP 起名 `…makes_all_falling_items_move_up__lolwat`，名字**不可信**）：

```
for (item = IM[+0x17b088]; item; item = next(+0x2dc)):
    if (item[+0x2d7] == 1):                 ; th08:0x44154e movsx eax,BYTE PTR [edx+0x2d7]
        item[+0x2d7] = 0                    ; th08:0x44155d  ← state 1（归航）→ 0（下落）
        item[+0x2b0] = 0.0f                 ; th08:0x441567  vx
        item[+0x2b4] = 0xbf666666 (−0.9f)   ; th08:0x441574  vy
        item[+0x2b8] = 0.0f                 ; th08:0x441581  vz
```

调用者两处，**都在玩家死亡/罚刻符那段**：`th08:0x44aeae`（`Player::sub_44a6a0` 里的死亡块，紧邻
`th08:0x44ae92 mov DWORD PTR [edx+0xe2a68],0x2`）与 `th08:0x44ceaa`（`Player::sub_44cbf0` 内，同一段前面就是
`th08:0x44ce7c push 2 / push 3` 那颗 −15 刻符）。

**② `ItemManager::on_tick__` 里的同款分支**（玩家状态驱动的即时版）：归航段出口 `th08:0x44085f`
读 `ds:0x17d5ef8`（`PLAYER+0` = 玩家 state）—— 若 `== 2`（`th08:0x440866 je 0x4408e0`）或 `== 1`
（`th08:0x440872 je 0x4408e0`）则：

```
th08:0x4408e0 mov DWORD PTR [edx+0x2b4],0xbf333333   ; vy = −0.7f
th08:0x4408f0 mov BYTE PTR [eax+0x2d7],0x0           ; state = 0（回到下落）
```

⇒ **中弹那一刻，正在归航的道具立刻停下归航、恢复成普通下落道具**（与用户口径逐条吻合）。
两条路径**结果相同**，① 是死亡结算时的批量收尾、② 是每帧的即时判定。

### 7.4 `ItemManager::cancel_autocollect` `th08:0x4413e0` 不是「停住」，是「全屏收道具」

```
for (item = ...; item; item = next):
    item[+0x2d7] = 1                              ; th08:0x4413fe  ← state = 1（归航）
    item[+0x2b0..0x2b8] = Float3(0, −0.5f, 0)     ; th08:0x441407–0x44142c（0xbf000000 = −0.5f，归航初速）
```

调用者**全是清屏/清敌路径**，不是中弹路径：
`th08:0x40bec6`（清掉 0x80 颗自机弹之后）、`th08:0x433da9`（`BulletManager` 清弹 + `th08:0x42efb0` 清敌之后）、
`th08:0x433e55`（ECL 侧，且 `th08:0x433e44` 先判 `PLAYER+0 != 2`）⇒ 语义是**「全屏道具收走」**。

> ⚠️ **2026-09-12 订正**：本文初版把这两条函数名按 ExpHP 的命名对号入座，**结论正好弄反了**
> （初版写「中弹 ⇒ `0x4413e0` 让整池停住」）。指令原文是 `0x4413fe` 写 **1**、`0x44155d` 写 **0**，
> 而 state 1 的分支头（`th08:0x4407f6`）落到的正是「每帧把速度指向自机」那段 ⇒ 1 = 归航、0 = 下落。
> 别再信 ExpHP 这两个名字（`cancel_autocollect__i_think` 与 `…move_up__lolwat` 都是猜的）。

### 7.0 正负号与运动常量（一手，`.rdata` 直读）

| 量 | 值 | 一手 |
| --- | --- | --- |
| `spawn_item` 给所有道具的初速 vy | `0xc00ccccd` = **−2.2f** | `th08:0x440211` |
| 重力（state 0） | `ds:0x4b44c0` = **0.03f**/帧 ×dt | `th08:0x4409c8` |
| 终速（state 0） | `0x40400000` = **+3.0f** | `th08:0x4409bb`（比 `ds:0x4b42dc` = 0.0）→ `th08:0x4409e8` |
| 上升速度下限（state 0） | `ds:0x4b5b2c` = **−2.2f** | `th08:0x44091c` → `th08:0x44092c` |
| state 3 初速 vy 的基数 | `ds:0x4b5b28` = **−2.0f** | `th08:0x4402da`（`fsubr`） |
| state 3 / 5 的每帧增量 | `ds:0x4b5b38` = **0.05f** | `th08:0x4406ac` / `th08:0x440746` |
| state 2 的生成矩形 | **288.0f × 192.0f**，中心偏移 (+48, −64) | `th08:0x440256`–`0x44028c` |
| 收下判据（state 0） | `pos.y <= ds:0x164d2e8 + ds:0x4b42d4 (16.0f)` | `th08:0x440969`–`0x440978` |
| 中弹停住后的 vy | **−0.7f**（代入式）/ **−0.9f**（批量式） | `th08:0x4408e0` / `th08:0x441574` |

⇒ **+y 是「下」**：重力为正、终速为正 = 下落；初速 −2.2 = 先往上弹。
（`0xbf333333` = −**0.7**，`0xbf666666` = −**0.9** —— 两个不同常量，别混。）


## 8. 结论表（可信度 + 版本）

| # | 断言 | 可信度 | 版本 |
| --- | --- | --- | --- |
| 1 | 时点 = 道具**类型 7**；类型 10 在 `spawn_item` 里被改写成「类型 7 + state 5」 | ✅ 一手（`th08:0x440115` / `0x440124`） | v1.00d |
| 2 | 类型 7 强制 `state = 3`；state 3 有专属应收分支（`th08:0x440c37`） | ✅ 一手 | v1.00d |
| 3 | 时点**有实体**（走 0x2e4 B 的 `Item` 池、有自己的 Timer 与速度积分） | ✅ 一手（§1） | v1.00d |
| 4 | **击破敌怪**掉 6..26（人単独分支到 40）颗，按连锁段数；某开关下 ÷3 | ✅ 一手（§4） | v1.00d |
| 5 | **符卡结束/击破 boss** 挂 100 / 700 / 1000 颗额度，按「每 8 帧 ≤7+6 颗」喷完 | ✅ 数量与批次公式一手；🟡「8 帧」那一步的 Timer 语义未逐指令核实 | v1.00d |
| 6 | 消弹、擦弹、自机弹打死敌机、玩家复活路径**都会**生时点 | ✅ 一手（§3 全表） | v1.00d |
| 7 | 中弹 ⇒ 归航停住、恢复下落：批量版 `ItemManager::sub_441530` `th08:0x441530`（state 1 → 0，vy = −0.9f），即时版 `th08:0x4408e0`（`PLAYER+0 ∈ {1,2}` ⇒ state 0，vy = −0.7f） | ✅ 一手（§7.3） | v1.00d |
| 8 | `ItemManager::cancel_autocollect` `th08:0x4413e0` **不是**「停住」，是**清屏后全屏收道具**（state = 1 + 归航初速 (0,−0.5,0)；调用者全在清弹/清敌路径） | ✅ 一手（§7.4） | v1.00d |
| 9 | 收集一颗时点：分数 100/10000 档 + 刻符 +1 + 夜点 +5 + 符卡 +8000 + 人妖槽 ±111 | ✅ 一手（§6，另有 `research-th08-night-points.md`） | v1.00d |
| 10 | 时点**必然进归航**：state 3 出生 vy = −2.0−rand(0.2)、+0.05f/帧，约 40 帧后 vy > 0 ⇒ `state = 1`（`th08:0x4406e4`） | ✅ 一手（§7.1） | v1.00d |
| 11 | 时点的贴图 = `data/etama.anm` entry1（`data/etama/etama2.png`）的 **sprite179** (112,64,16,16)，脚本 = 同 entry 的 **script68**（正文与两条独立证据见 §6.1） | ✅ 一手 | v1.00d |

## 9. 未解 / 待办

- `❓ 未解 2`（已大幅收窄）：§7.0 的九个运动常量都已读出（`.rdata` 一手），正负号已定（+y = 下）；
  仍未钉死的只有 `ds:0x17d61b0`（收集线）与 `ds:0x18b896c`（`+0x1c` 参与收集线比较、`+0x14` 归航速度模长）
  这两个全局的**语义与写入者**。
- `❓ 未解 3`：`ds:0x18b8988`（擦弹档？）与 `ds:0x17d6ed4`（÷3 开关）、`enemy+0x3380` 三者语义未定名。
- `❓ 未解 5`：`item+0x220` 那个附属对象是什么（state 3 退场时要在 `th08:0x45ea00` 注销它）。
- `❓ 未解 6`：`th08:0x4415a0` 的另一支（`type+0xb6`、受 `pos.y < −8.0f` 门控的「贴边指示」）
  对 type 7 指向 **sprite189**，但**实测整块全透明** ⇒ 时点没有贴边指示图标（这条由
  `assets/ability/make_tpoint_art.py` 每次生成时断言）。

## 附录 A：复现命令

```bash
# 1) TH08 的 .dat 条目 THTK 的 thdat 解不开（LZSS 后缺 "edz" 包装会报 incorrect entry magic）——
#    用本仓自写的提取器：
python3 tooling/thtk/extract_th08.py local/th08.v1.00d/th08.dat local/th08.v1.00d/dat
# 2) anm 文本 + 贴图（目录结构 anm/<anm 名>/<entry 内部路径>）：
for f in local/th08.v1.00d/dat/*.anm; do s=$(basename "$f" .anm); \
  mkdir -p local/th08.v1.00d/anm/$s; (cd local/th08.v1.00d/anm/$s && \
  ../../../../local/vendor/thtk/build/thanm/thanm -x 8 "../../dat/$s.anm" >/dev/null); done
# 3) 平坦反汇编：
objdump -d -M intel local/th08.v1.00d/th08.exe > local/_scratch/th08_full.asm
# 4) ECL 操作码跳表（statics.json 的 ecl_jump_table_41ecbe，184 项）；
#    dropPointItems = op 168 → th08:0x41e28f，dropItems = 142 → th08:0x41d183
```
