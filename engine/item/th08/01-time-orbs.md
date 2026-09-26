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
| **击破敌怪**（含它 ECL 连锁出来的子敌机） | **6..26**（某分支到 **40**），按「连锁段数」算；另有尾支 `2 × subEnemyCount` | `Enemy::sub_42adb0` `th08:0x42adb0` 的 `th08:0x42aec9`–`th08:0x42af45` 三条公式；落点见 **§4.1**（圆盘 `2N`）/ **§4.2**（128 环 × state 1） |

≥ **2026-09-27 订正**：`ItemManager` 的收集分派**只排除 state 3**
（`th08:0x4409f2`–`0x4409ff`：`cmp ecx,3` / `je 0x440c37`）⇒ **state 5（符卡螺旋 / 擦弹）在上抛期间就能被收**；
本卡曾把两者并成一档（`AUDIT.md` §BE #111 已修）。

**「一定会被自动收集」体现在 state 3**：`spawn_item` 对类型 7 强制 `state = 3`（`th08:0x440115`），
state 3 是**时点专属的应收分支**（`th08:0x4409f2` `cmp state,3` → `th08:0x440c37`），
与普通道具「接触自机抓取盒才收」（state 0/1，`ItemManager.cpp:288` 的盒相交）不是同一条路。
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

### 2.0 ★★★ 类型表的全部语义（2026-09-22 补齐；回答卡 108 的 owner #24「深蓝色的点」）

枚举名一手来自**社区反编译**（`local/vendor/th08_dec/src/ItemManager.hpp:8-20`，MIT；与我们样本的
类型号逐条对得上 —— 见下面「交叉印证」一栏）：

| type | 枚举名 | `etama.anm` 脚本 / sprite | 贴图形状与主色（逐像素量过）| 收下时发生什么 |
| --- | --- | --- | --- | --- |
| 0 | `ITEM_POWER_SMALL` | 61 / s172 (0,64,16,16) | 12×12 方块，红 (215,83,83) | 火力 +1 档（满 P 时**降到 type 8**）|
| 1 | `ITEM_POINT` | 62 / s173 (16,64,16,16) | **12×12 方块**，蓝 (51,51,204) | `CollectPoint()`：加**点**分（PoC 线以上 = `pointItemValue`）**并记 `pointItemsCollected`** ⇒ 推进 1UP 门槛；subrank +10/+3 |
| 2 | `ITEM_POWER_BIG` | 63 / s174 (32,64,16,16) | 大块，红 (218,96,97) | 火力大档（满 P 时也**降到 type 8**）|
| 3 | `ITEM_BOMB` | 64 / s175 | 绿 (127,191,137) | 加 bomb 碎片 |
| 4 | `ITEM_POWER_FULL` | 65 / s176 | 黄 (206,177,95) | 火力拉满 |
| 5 | `ITEM_EXTEND` | 66 / s177 | 紫 (201,84,219) | 加一命 |
| 6 | `ITEM_POINT_STAR` | 67 / s178 | 灰白 (165,175,166)，脚本带 `alpha(192)` | 星星点（分会掉？见注）|
| 7 | `ITEM_TIME` | 68 / s179 | 紫，带 `colorTime` 变色 | **时点**（刻符）＝本文件的主角 |
| **8** | **`ITEM_POINT_SMALL`** | **69 / s180 (0,80,16,16)** | **10×10 圆点，深蓝 (0,0,102)–(85,102,187)** | `CollectPointSmall()`：**只加分**（PoC 线下按距离衰减）—— 详见下面「type 8 的作用」|
| 9 | `ITEM_UNK9` | 70（同 s179 贴图，`color(192,192,192)` + `alpha(192)`）| 灰化的刻符 | 未钉（社区枚举名也没给出语义）|
| 10 | `ITEM_TIME2` | —（**被 `0x440124` 当场改写成 7 + state 5**）| — | 「时点的另一种生成方式」（擦弹）|

**⇒ 卡 108 的 owner #24「TH08 掉一个**深蓝色的点**，不知道干什么用的」= `type 8 / ITEM_POINT_SMALL`**：

- **判据（三条都硬）**：① 形状 —— 它是**圆点**（10×10 圆），而 type 1「点道具」是 12×12 **方块**；
  ② 颜色 —— 它的像素是**深蓝**（(0,0,102)/(0,0,119)/(85,102,187) 一档，最暗的一族），type 1 是亮蓝 (51,51,204)；
  ③ 来路 —— `ItemManager::SpawnItem` 开头那条改写（`ItemManager.cpp:33-36`，对应 `th08:0x4400f1`–`0x44010e`）：
  **`power >= 128` 且类型是 P 点（0/2）⇒ 当场改成 type 8**，另有 `CancelAutoCollect` 一族（`:618-636`）
  把已经掉在地上的 P 点也改成它（并把 VM 脚本换成 `0x45` = 69 —— 与我们读出的「type 8 → 脚本 69」完全对上）。
- **它的作用 = 满火力后的「空 P 点」**：满 P 时 P 点已经没有意义，游戏把它降格成**只给分的小点**；
  收下只加 `pointItemValue` 档的分（極端人間 ×2），**不推进 1UP、不加 subrank**。

**type 8 的作用（`Item::CollectPointSmall` `ItemManager.cpp:482-513`）**：值 = `pointItemValue`
（自机在 PoC 线以上），否则 `pointItemValue/2 − (y − pocY) × (pointItemValue/1000)`，再**十位取整**；
`isMaxValue` 过线后恒为满值；**極端人間（`GaugeIsExtremelyHuman`）⇒ 值 ×2**；弹分数、`AddScore`。
与 type 1 的关键差别：**它不记 `pointItemsCollected`**（不推进 1UP 门槛）、**不调 `IncreaseSubrank`**。

**交叉印证**：`thanm -l 8 -m v2.anmm` 读出的「脚本号 = 61 + type」与社区枚举的**顺序完全一致**
（0..8 逐条）；`ItemManager.cpp:299-300` 的 `case ITEM_POINT_SMALL: item->CollectPointSmall()` 与
我们前面读出的「state 3 在收集分派里被排除」是同一条 `switch`（`th08:0x4409xx`）。

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
    item[+0x2d7] = 0;                      // state 从 3 降成 0（普通下落；收集仍是抓取盒相交，见 §7.2）
    item[+0x2b0] = 0.0f; item[+0x2b4] = -0.9f (0xbf666666); item[+0x2b8] = 0.0f;
}                                                                        ; th08:0x440301–0x44032e
```

### state 一览（读点全在这一段里，✅ 地址一手）

| state | 一手读点 | 行为（读出来的） |
| --- | --- | --- |
| **0** | `th08:0x44095e` | 普通下落（`vx=0`、`vy += 0.03` 上限 3.0）；**掉到 `448+16` 以下 ⇒ `Delete()` 且不给分**（`ItemManager.cpp:271`；`th08:0x440985` 那一段是它）。**收集**走的是另一条：抓取盒相交（§7.2）|
| **1** | `th08:0x4407f6` `cmp eax,1 / je 0x44085f` | ★ **归航（朝自机飞）**：每帧重算速度指向自机（细节见 §7.2）|
| **2** | `th08:0x4405ad` `cmp edx,2` | 「从上方撒入」的飞行段（用 `Timer(item+0x2c8)` 做插值，`th08:0x4405ca`–`0x44061d`） |
| **3** | `th08:0x4409f2` `cmp ecx,3` → `th08:0x440c37` | **时点专属**：先 `Timer(item+0x2c8)` 推进，若 `item+0x220 != 0` 则 `th08:0x45ea00` 注销附属对象，然后从活跃表摘除（收下 = `Item::collect_time_orb`） |
| **5** | `th08:0x440737` `cmp edx,5` | 类型 10 档的上浮/越界转 1 |

> ★ **2026-09-12 订正**：本表初版把 state 1 写成「被停下」、state 2 写成「飞向自机」，**方向反了**。
> 指令原文：`th08:0x4408ad` 与 `th08:0x4408f0` 是**两个不同**的写字 ——
> `th08:0x4408ad mov BYTE PTR [edx+0x2d7],0x1`（归航段末尾，置 **1**）与
> `th08:0x4408f0 mov BYTE PTR [eax+0x2d7],0x0`（玩家已死那条路，置 **0**）。
> 而 state 1 的分支头 `th08:0x4407f6 cmp eax,0x1 / je 0x44085f` 落到的正是「每帧把速度指向自机」那段。


## 2.9 ★★★ 夜点（= `pointItemValue`）—— 初值四档与「每 2 刻符 +10」（2026-09-22 夜，卡 108 的 P-A）

- **夜点就是 `zGlobals.pointItemValue`**（`globals+0x24`，`globals` = `[0x160f508+8]`）；屏幕中间下方那个数、
  以及「蓝点的最大分」都是它（`Item::CollectPoint` 的 `itemScore` 直接由它算，见 §2.0 那条 type 1 的说明）。
- **初值按难度四档**：`th08:0x43af1b mov eax,ds:0x160f538` → switch 0..4 ⇒ `[globals+0x24]` =
  **60000**（`0x43af49`，Easy）/ **100000**（`0x43af58`，Normal）/ **200000**（`0x43af67`，Hard）/
  **300000**（`0x43af76`，Lunatic）/ **300000**（`0x43af85`，Extra）。
- **每 2 个刻符 +10**（`GameManager::AddTimeOrbs th08:0x418220`）：
  `0x418294 and eax,1`（用**累计刻符数的奇偶**修正本批颗数）→ `0x41829d add` → `0x4182ab sar 1`
  → `0x4182b0 imul 0xa` → `0x4182bc` 写回；门 = `0x418227 orbs >= 0`、`0x418288 orbs > 0`。
  ⇒ 跨批也精确（不是每批各自取整）。
- 刻符被**收下**时才进这一笔：`Item::collect_time_orb th08:0x4412b0` → `0x44137a AddTimeOrbs(1)`。

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
| `th08:0x431650` `0x431668` | `BulletManager::on_tick` | `7` / `1` ×2 | ~~**擦弹**~~ ⚠ **2026-09-23 订正：`eax == 2` = 命中「炸弹消弹区」那一支**（订正块 ①）：`ds:0x18b8988 == 9` 时掉 2 个；否则掉 `ds:0x18b8988` 个类型 7 |
| `th08:0x43170a` `0x431722` `0x431749` | `BulletManager::on_tick` | `7` / `1` ×2 或动态 | ~~同上另一处~~ ⚠ **同族：同样是 `cmp eax,0x2`（炸弹消弹区）支**，不是擦弹 |
| `th08:0x431837` `0x43184e` `0x431876` | `BulletManager::on_tick` | `7` / `1` ×2 或动态 | ~~同上~~ ⚠ **同族：`call 0x449ff0` → `cmp eax,0x2` 门**，不是擦弹 |
| `th08:0x431948` `0x431960` `0x431987` | `BulletManager::on_tick` | `7` / `1` ×2 或动态 | ~~同上~~ ⚠ **同族：`call 0x449ff0` → `cmp eax,0x2` 门**，不是擦弹 |
| `th08:0x431a59` `0x431a71` `0x431a98` | `BulletManager::on_tick` | `7` / `1` ×2 或动态 | ~~同上~~ ⚠ **同族：`call 0x449ff0` → `cmp eax,0x2` 门**，不是擦弹 |
| `th08:0x44aac9` `0x44aaef` `0x44ab0f` | `Player::sub_44a6a0` | `0xa` / `1` | ★ **这才是擦弹那一路**（`0x44a930` 体内，每擦一颗弹 **1~3 颗**；三道门见订正块 ②） |
| `th08:0x451892` | `Player::compute_damage_to_enemy` | `7` / `3` | **自机弹打死敌机**时补一颗（`th08:0x451871` 读 `[enemy+0x480]+0x1e < 0` 判死） |

> 交叉印证：`th08:0x18b8988` 这个全局在 `th08:0x43163d` / `0x43166f` 被当**数量**用（`push ds:0x18b8988` 当 type），
> 同一地址在 `th08:0x4308cf` / `0x430b25` 也被当数量用 —— ~~🟡 是「擦弹连击档」一类的档位值~~ ⚠ **2026-09-23 订正：它只出现在「炸弹消弹区」那一支里当 `type` 用**（`0x43163d` / `0x43166f` / `0x4316ee`…），
> 与擦弹无关；「它怎么涨」仍未追（订正块 ①，🟡）。

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
        spawn_item(敌人位置 + 圆盘内随机一点, type=7, state=3)   ; th08:0x42b03b
```

### 4.1 ★★★ 落点公式的**订正**（2026-09-26，owner 报「从敌机铺出去的范围太大」时逐指令重读）

⚠ 本节旧版写的是「**半径 128 的环上**，角度 = `randf(2π)` 后 `+ π/2`」—— **两处都错**，
而且那个 128 其实属于**另一条支路**（§4.2）。逐条（`th08:0x42aff0`–`0x42b048`）：

```asm
42aff0  fild  [ebp-0x2c]      ; = N（本帧撒几颗）
42aff3  fadd  st(0),st        ; = 2·N
42affe  call  0x40d390        ; Rng::GetRandomF32InRange(rng, 2N)        → ∈ [0, 2N)
42b011  call  0x4143c0        ; Rng::GetRandomF32SignedInRange(rng, π)  → ∈ [−π, π)
42b01d  call  0x4286e0        ; Float3::FromAngleMagnitude(&v, angle, magnitude)
42b036  call  0x410a70        ; Float3::operator+=  → v += enemy->final_pos (+0x2d88)
42b048  call  0x4400a0        ; SpawnItem(&v, type 7, state 3)
```

- **参数位次**由 `FromAngleMagnitude` `th08:0x4286e0` 的正文定死：`fld [ebp+8]; fsincos; fmul [ebp+0xc] → x;
  fmul [ebp+0xc] → y` ⇒ **`[ebp+8]` = 角度、`[ebp+0xc]` = 模长**。cdecl 下最后压栈的那个落在 `[ebp+8]`
  ⇒ 上面**先压的 `rand(2N)` 是模长、后压的 `rand_signed(π)` 是角度**（`0x40490fdb` 是 **π**、不是 π/2 —— 它只是
  `GetRandomF32SignedInRange` 的**参数**，不是角度偏移）。
- ⇒ **落点 = 半径上界 `2·N` 的圆盘内均匀撒**（`radius` 本身均匀随机 ⇒ 越靠圆心越密，与一手同构），
  **不是固定 128 的环**。N = 12 时上界 = 24。
- **同一函数的另一条支路（`0x42b15d`）才是 128**：见 §4.2。卡 108 旧实现把两处混成了
  「12 颗 + 固定 128 环 + state 3」⇒ 范围是正确值的 5.3 倍（owner 的「范围太大」）。

### 4.2 尾巴（`0x42b0e5`，**函数参数 `[ebp+8] != 0` 才有**）：`2 × enemy[0x3380]` 颗、**半径 128 环**、**state 1**

```
for (i = 0; i < enemy[0x3380] * 2; i++)      ; 0x42b156 shl eax,1 / cmp
    radius = 128 (0x43000000)                 ; 0x42b15d
    angle  = Rng::GetRandomF32SignedInRange(π) ; 0x42b17a
    SpawnItem(enemy->final_pos + FromAngleMagnitude(angle, 128), type 7, state 1)   ; 0x42b1b1
```
`enemy+0x3380` = `subEnemyCount`（`EnemyManager.hpp:214`）⇒ **这条是「有子部件的敌人」那一批**：
子部件数 ×2 颗、丢在**半径 128 的环**上、**state 1（生成即满速归航 = 自动吸收）**。
同一段还会 `CreatePopup(敌人位置, value = enemy[0x3380], 0xfff0f00f)`（`0x403460`）——
即 TH08 在「这个敌人有几个子部件」时会有一次弹字。

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

### 5.3 ★★★ **螺旋的全部参数**（2026-09-22 逐指令复核，订正 §5.2 的两处）

> owner 口径（原话）：「**击破符卡会有一个螺旋式的撒永点**，无敌一段时间后自动满速吸收」。
> 下面每个数都出自 `local/th08.v1.00d/th08.exe` 的指令（`Spellcard::sub_416b10` `th08:0x416b10` 内，
> 喷点主体在 `th08:0x417533`–`0x4176e1`）。

| 参数 | 一手值 | 一手出处（逐指令） |
| --- | --- | --- |
| **门** | ① `Timer(owner+0x338)` 命中 **8**（= 每 8 帧一批）；② `spellcard[+0x10] > 0` | `0x41751a push 8` → `0x41752e call 0x4066c0`；`0x417541 cmp [edx+0x10],0 / jle` |
| **角度** | `θ(t) = ((t − 10) × 2π / 40) − π/2`，`t` = 那个 Timer 的当前帧数 ⇒ **每帧 9°**（40 帧一圈）、`t = 10` 时正上方 | `0x417564 call 0x40b8c0`（Timer→float）→ `0x41756a fsub ds:0x4b4390`(**10.0**) → `0x417570 fmul ds:0x4b4460`(**2π**) → `0x417576 fdiv ds:0x4b432c`(**40.0**) → `0x41757c fsub ds:0x4b4468`(**π/2**) |
| **角规约** | `normalize_angle(θ, off)`：`θ += off`，然后 `while (θ > [0x4b4334]) θ −= 2π`（≤16 次） | `0x43edb0` 全文（`43edbb fld/fadd [ebp+0xc]`、`43edc7 fcomp ds:0x4b4334`、`43edd7 fsub ds:0x4b4460`）|
| **半径** | **128.0** | `0x417593 push 0x43000000`（= 128.0f）→ `0x41759f call 0x4286e0`（用 (角度, 模长) 造 Float3 —— 与道具归航同一个 helper）|
| **绕谁** | **绕符卡所有者（boss）的位置**：`vec += *(owner[+0xf8] + 0x2e0)` | `0x4175a4 mov eax,[ebp-0x94]` → `0x4175aa mov ecx,[eax+0xf8]` → `0x4175b0 add ecx,0x2e0` → `0x4175ba call 0x410a70`（向量加）|
| **每批颗数** | `k = min(spellcard[+0x10], **7**)`，然后**再固定 6 颗**（同一角度 +π）⇒ 一批 **13** 颗 | `0x4175cc cmp [edx+0x10],7 / jle` → `0x4175ed`；循环 `0x417608`–`0x417622`；第二段 `0x4176be cmp [ebp-8],6 / jge` ⇒ i=0..5 = **6 颗** |
| **第二段的角** | 第一段角 **+π**（`normalize_angle(θ, π)`）| `0x417639 push 0x40490fdb`（**π** = 3.1415927）→ 同 `0x43edb0` ⚠ §5.2 旧文写「+π/2」是**错的**，常量是 π |
| **额度扣减** | 第一段后 `spellcard[+0x10] -= k`（`0x41762d`）；第二段后 `-= 6` 并夹到 0（`0x4176e1`） | `0x41762d sub ecx,[ebp-0x4]` / `0x417636 mov [edx+0x10],ecx`；`0x4176e1` 一带 |
| **出生态** | `spawn_item(pos, type **0xa**, state 0)` → `spawn_item` 把 type 10 **改写成 type 7 + state 5** | `0x417610 push 0x0 / 0x417612 push 0xa / 0x41761d call 0x4400a0`；`0x440124 cmp [ebp+0xc],0xa / mov [ebp+0x10],5 / mov [ebp+0xc],7` |

**观感（实跑判据）**：额度 1000（人妖两侧都不是「不算击破」那一位时用时最短档）⇒ 每 8 帧一批 13 颗、
**约 77 批 / 约 616 帧（≈10.3 s）**；每一批的 7 颗在**半径 128** 的圆上同一个角、另外 6 颗在**对面**（+π），
而这个角**每帧转 9°**（40 帧一圈）⇒ 看到的是**一簇簇沿圆周转的永点**（「螺旋」）。700 颗 ≈ 7.2 s、100 颗 ≈ 1 s。

**收集相位（与 #25 的「无敌一段时间」对齐）**：state 5 ⇒ 先上抛（`vy = −2.0 − rand(0.2)`、每帧 `+0.05`），
`vy > 0` 才转 state 1；**上抛期间进不了收集判定**（卷首 §7 与 `0x4409f2`–`0x4409ff`）⇒
「无敌窗口」= 上抛帧数 ≈ 40–44 帧（`2.0/0.05 = 40`，加 rand 的 0–0.2 ⇒ 最迟约 44）—— 逐来源见 §7.3 的那张表。

### 5.4 ★ 三条来源 × state × 「何时可被收」对照表（2026-09-22，一手）

| 来源 | 生成点（一手 push） | 出生态 | 无敌/上抛 | 何时可被收 | 之后 |
| --- | --- | --- | --- | --- | --- |
| **通用击破**（`Enemy::sub_42adb0`）| `0x42b03b push 3` / `0x42b03d push 7` | **state 3** | **上抛 ≈40–44 帧**（`vy = −2.0 − rand(0.2)`、`+0.05/帧`）| `vy > 0` 转 state 1 之后 | state 1：每帧重设满速（`[.sht+0x14]` = 10/12）归航 |
| **極端人/妖那一颗**（`EnemyManager::on_tick` 死亡块）| `0x42da41 push 1` / `0x42da43 push 7` | **state 1** | **0 帧** | **生成当帧**（若已在抓取盒内）| 同上（满速归航）|
| **击破符卡（螺旋）** | `0x417612 push 0xa`（→ type 7）| **state 5** | **上抛 ≈40–44 帧**（与 state 3 逐指令同形）| `vy > 0` 之后 | 同上 |
| （对照）**擦弹** | ★ `0x44aac9` / `0x44aaef` / `0x44ab0f`（`0x44a930` 体内，`push 1`(state) + `push 0xa`(type)）；⚠ 订正 2026-09-23（订正块 ②） | **state 5**（`type 0xa` ⇒ type 7 + state 5） | 同上 | 同上 | 同上 |
| （对照）**消弹 / `cancel_autocollect`** | `ItemManager::cancel_autocollect th08:0x4413e0`（整池改 state）| **state 1** | 0 帧 | 生成当帧 | 满速归航 |

⇒ **「自动吸收」两种含义分开写清楚**：① **state 1 = 当帧就满速归航**（不需要靠近它自己去够，但**仍要抓取盒相交**才入账）；
② 「上抛期收不到」= state 3/5 被 `0x4409f2`–`0x4409ff` 排除在收集判定之外 ⇒ **不是免疫伤害那类「无敌」**，只是「还没到可收相位」。

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
（**state 3 期间不可收**：`ItemManager.cpp:288` 的 `item->state != ITEM_STATE_UNK3 &&` 把 state 3 从**抓取盒相交**那条路里摘出去。）

### 7.2 归航（state 1）、**收集 = 接触抓取盒**、以及那条 `+16.0f` 到底是什么

> ★★ **2026-09-22 晚订正（owner 第 4 条「永点的吸收速度不对」）**：本节旧版把
> `ds:0x164d2e8 + 16.0f` 写成「落到回收线才收」，并据此认为归航速度是「未抽出的量」。
> 逐条重读后**两处都要改**：① 收集**与自机 y 无关**，是 `Player::CalcItemBoxCollision` 的**盒相交**；
> ② `+16` 那条线是**游玩区下沿之外**（`arcadeRegionSize.y + 16`）的**丢失线**（掉出去 = 那颗没了，**不给分**）。

- **state 1 = 归航，且当帧就是满速**（**没有加速段**）：每帧重算速度 —— `th08:0x4407f6 cmp eax,1 / je 0x44085f`
  → `th08:0x440886 call 0x44c1b0`（`Player::AngleToPlayer`，反编译 `Player.cpp:121` 的 `atan2f`，给方向）
  + `th08:0x440898 push [edx+0x14]` / `th08:0x4408a5 call 0x4286e0`（用 (角度, 模长) 造速度 Float3）
  写进 `item+0x2b0`。
  **模长 = `[ds:0x18b896c]+0x14`**，而 `ds:0x18b896c` = **当前装载的那份 `.sht` 头**（`PlayerRawShtFile*`）——
  同一指针在 `Player::on_registration`（`th08:0x44d7d1` 一族）里被读 `+0xc/+0x10/+0x18` 写进玩家的道具盒字段，
  在 `spawn_item`（`th08:0x440524`）里被读 `+0x4` 当炸弹出数 ⇒ 就是它。**本仓实测该字段**（`dat/ply0X{,as}.sht` `+0x14`）：
  **pl00 = 10.0 · pl01 = 12.0 · pl02 = 10.0 · pl03 = 10.0**（人/妖两份相同）。
- **收集 = 接触自机的抓取盒**（对 **state 0/1** 都成立；state 3 被显式排除）：
  `ItemManager.cpp:288` 的 `g_Player.CalcItemBoxCollision(&item->currentPosition, &size)`（`Player.cpp:100`）
  = 道具的 AABB（自身 `±size/2`）与 `grabItemTopLeft/BottomRight`（`Player.cpp:1059`：`positionCenter ± shotSpeed3ec`）
  相交。抓取盒半宽 = **`[.sht+0x18] / 2`**（`th08:0x44d83f`–`0x44d873`）= `itemCollectRadius/2`
  （pl00/02/03 = **12.0**、pl01 = **16.0**）。
- **`+16.0f` 那条线 = 丢失线**（`ItemManager.cpp:271-277`，逐行）：
  `if (item->state == ITEM_STATE_DEFAULT && g_GameManager.arcadeRegionSize.y + 16.0f <= item->currentPosition.y)
      { g_GameManager.DecreaseSubrank(3); item->Delete(); }`
  ⇒ 只有 **state 0** 走到；`ds:0x164d2e8` 在开局的初始化块里被写成 **448.0**（`th08:0x43a144` `fld ds:0x4b4940`
  → `fstp ds:0x164d2e8`，常量 `0x4b4940` = 448.0）⇒ 那条线 = `游玩区高 + 16`，**不是自机身上的线**。
  `th08:0x440962`–`0x44097e` 的比较方向也印证：`fld ds:0x164d2e8; fadd 16.0; fcomp item.y` + `test ah,0x41; jp`
  ⇒ 落进「结算」支的条件是 **`item.y > 448 + 16`**（掉到下沿之外）——旧版把不等号方向也写反了。
- **state 0 进归航的门**（`th08:0x4407fb`–`0x440859`，全部满足才进）：
  `ds:0x17d61b0`（自机的 POC 线）`>` `[ds:0x18b896c]+0x1c`（`pocY`：pl00/02/03 = 128.0、pl01 = 160.0），**且**下列任一：
  ① `get_current_power() >= 0x80`（128，`ds:0x4b5b30`）；② `ds:0x17d5efb`（`PLAYER+3` = `is_focused`）≠ 0；
  ③ `ds:0x164d0b1 == 1`；④ `ds:0x164d0b1 == 6`。
  进门后同样是 `th08:0x4408ad state = 1`。
- **state 0 自己的运动**（`ItemManager.cpp:268-276`）：`vx = 0`、`vy` 夹到 `>= ds:0x4b5b2c (−2.2)`、
  每帧 `vy += ds:0x4b44c0 (0.03)`、终速 `0x40400000 (3.0)`。

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
| **丢失线**（state 0，掉出去 = 没了） | `pos.y > ds:0x164d2e8 (448.0) + ds:0x4b42d4 (16.0f)` | `th08:0x440969`–`0x440978`（`fadd 16` / `fcomp item.y` / `test ah,0x41; jp`）|
| **收集**（state 0/1）= 抓取盒相交 | 盒半宽 = `[.sht+0x18]/2`（pl00 12.0 / pl01 16.0）| `ItemManager.cpp:288` + `Player.cpp:100` / `:1059`；`th08:0x44d83f` |
| **归航速度**（state 1，恒速、无加速）| `[.sht+0x14]` = pl00 **10.0** / pl01 **12.0** / pl02 10.0 / pl03 10.0 | `th08:0x440898`（`mov eax,[edx+0x14]`）/ `:0x4408a5` |
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
| 12 | **归航是恒速、无加速段**：模长 = `[ds:0x18b896c]+0x14`（= 当前 `.sht` 头的 `itemCollectSpeed`；pl00/02/03 = 10.0、pl01 = 12.0），方向每帧从 `Player::AngleToPlayer` 重算 | ✅ 一手（`th08:0x440886`/`:0x440898`/`:0x4408a5` + 8 份 `.sht` 直读，§7.2） | v1.00d |
| 13 | **擦弹** = `0x44aac9` 一族（`type 0xa` ⇒ `0x440124` 改写 **type 7 + state 5**，**先上浮**）；**消弹** = `push 1` ⇒ **state 1 满速归航**。⚠ 订正 2026-09-23：原文把两条写成一条、且引错了 `0x431650` 一族（订正块 ①②） | ✅ 一手（`th08:0x44aac9`/`0x44aaef`/`0x44ab0f` + `th08:0x440124`；`th08:0x4308e5` push 1） | v1.00d |
| 14 | 收集与自机 y 无关（**抓取盒相交**）；`arcadeRegionSize.y + 16` 是**丢失线**（掉出去那颗不给分）| ✅ 一手（`ItemManager.cpp:271-288`；`th08:0x43a144` 写 448.0）| v1.00d |

## 9. 未解 / 待办

- `❓ 未解 2`（已大幅收窄）：§7.0 的九个运动常量都已读出（`.rdata` 一手），正负号已定（+y = 下）；
  ✅ **2026-09-22 晚结案**：`ds:0x18b896c` = **当前装载的 `.sht` 头**（`PlayerRawShtFile*`；`+0x14` 归航速度、
  `+0x18` 抓取盒、`+0x1c` POC 线、`+0x4` 炸弹出数 —— 三处独立读点交叉印证，§7.2）；
  `ds:0x17d61b0` = 自机当前的 POC 线（与 `+0x1c` 比大小的那个量）。仍未钉死的只剩「谁写 `0x17d61b0`」。
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

## ★ 蓝点（type 1）分值链 + 本作的「机制项」（2026-09-18 补，一手 × 反编译源码互证）

> 本节的可靠性：`local/vendor/th08_dec`（社区 TH08 反编译，93% 函数已实现）的 `Item::CollectPoint()`
> （`src/ItemManager.cpp:430`）与 `GameManager::AddTimeOrbs()`（`src/GameManager.hpp:181`）
> **逐指令对上**了本仓自己的一手反汇编（`th08:0x440e40` / `th08:0x418220`，含常量直读）⇒ 本节按 ✅ 记。

**分派**：收集类型跳表 `th08:0x440cc5`（`th08:0x440a63 jmp [eax*4+0x440cc5]`，type 0..8）——
type 0 = `th08:0x440cf0`、**type 1 = 蓝点 `th08:0x440e40`**、type 2 = `th08:0x441170`、type 8 = `th08:0x441020`、
type 7 = 时点（`Item::CollectTimeOrb` `th08:0x4412b0`，本节不涉及）。

**蓝点分值（`th08:0x440e40`–`th08:0x440f1c`，逐指令）**：

```
PIV = [globals(0x160f510) + 0x24]                     ; pointItemValue
线上 (item->y < pocY，pocY = [0x18b896c]+0x1c)  ⇒ itemScore = PIV
线下                                        ⇒ itemScore = PIV/2 − ftol(item->y − pocY) × (PIV/1000)
if (item+0x2d8 == 1) itemScore = PIV                   ; 「按满值算」的例外位（同 th07 的 item+0x280）
itemScore -= itemScore % 10                            ; trunc10
if (call 0x406d10 == true)  itemScore += itemScore     ; ★ GaugeIsExtremelyHuman() ⇒ ×2
弹窗色 = (itemScore ≥ PIV) ? 黄 : 白
AddScore(itemScore)；pointItemsCollected(+InStage)++；GUI 刷新 2 帧；
IncreaseSubrank(itemScore ≥ PIV ? 10 : 3)；点道具 EXTEND 阈值推进
```

**本作的「机制项」= 时点（刻符）抬高 PIV 本身**（所以它**进**收分高度公式里面）：

```
GameManager::AddTimeOrbs(orbs)  th08:0x418220
418227  if (orbs < 0 && currentTimeOrbs < -orbs) currentTimeOrbs = 0     ; 越界清零
418241  currentTimeOrbs(+0x3c) += orbs
41825c  totalTimeOrbs(+0x44)   += orbs
41826e  hscr.numTimeOrbsCollected += orbs；call 0x406e50 (UpdateAntiTamper)
418288  if (orbs > 0):
418294      orbs += (totalTimeOrbs & 1)          ; ★ 奇偶补正（不丢半颗）
4182b0      PIV(+0x24) += (orbs/2) × 10          ; ★★ 每 2 颗时点 ⇒ 最大得点 +10
```

**PIV 初值**（`th08:0x43af49`–`0x43af85`，按难度 switch，直读原始字节）：
E `0xea60` = **60000** / N `0x186a0` = **100000** / H `0x30d40` = **200000** / L `0x493e0` = **300000** / EX **300000**。

**对「总最大得点公式」的两条结论**：
1. **机制项在收分高度里面**（机制改的是 PIV；线下折扣项 ∝ PIV）⇒ 与 TH07 相反，**不需要外挂 ΔG**。
2. 唯一的外挂是**乘法项 ×2**（`GaugeIsExtremelyHuman()`），它乘在 `trunc10` **之后** ⇒ 与高度系数**可交换**
   ⇒ 写成 `G = (…base…) × (1 + [极端人间])` 即可。另注意线下式**没有下限夹取**（大 `y` 时可为负）❓。

### 按 mod 的「额外机制」口径重述（2026-09-18，用户订正参照系后）

参照系：`G_0 = G_18`（TH18 原生身价，全卡共用）；各作机制 = `ΔG_i`，且逐项判断**进不进 `coeff_18`**。

| TH08 的机制 | 形状 | 进 `coeff_18`？ | 依据（一手） |
| --- | --- | --- | --- |
| **时点（刻符）⇒ 最大得点** | **加性**：`ΔG_08 = (时点/2) × 10`（每 2 颗时点 +10，奇偶补正不丢半颗） | ✅ **进**（原作里它改的是 **PIV 本身**，而 PIV 进高度公式 ⇒ 我们声明进 `ecx` 即可同义） | `th08:0x418220`（`PIV += (orbs+(totalOrbs&1))/2×10`）+ `Item::CollectPoint` 的线下式 `PIV/2 − d·(PIV/1000)` |
| **人妖槽「极端人间」** | **乘法**：`×2`（只作用蓝点/小点，时点不加倍） | 可交换（乘在 `trunc10` 之后）⇒ 写在系数内/外**等价** | `Item::CollectPoint` 的 `if (GaugeIsExtremelyHuman()) score += score`（`th08:0x440f03`；调用 `th08:0x406d10`） |
| 线下折扣 | `G_08 = PIV/2 − d×(PIV/1000)`（`d = y − pocY`，无下限夹取 ❓） | 由 TH18 的 `coeff_18` 代劳 ⇒ **我们不再自己乘** | 同上 |

⇒ **TH08 卡的实现口径**：时点机制作为 `ΔG_08` **并进 `ecx` 声明**（随 `coeff_18` 一起被折扣）；
「极端人间 ×2」单独作为乘法项叠加。**不要**把 TH08 自己的 PIV 初值（60000/100000/200000/300000）
当成基线搬进来 —— 基线永远是 `G_18`。

### ★★ 「进不进 ΔG」的第三次复核（2026-09-22，为卡 108 工单 D 重跑一遍）

**结论不变：进**（本文 §按 mod 的「额外机制」口径重述 那一节的表就是这个判定）。这次是**另一个人、
从零再读一遍指令**（不引用上一轮的结论），三条都独立命中：

| 步骤 | 指令（一手，`local/_scratch/th08_full.asm`） |
| --- | --- |
| ① 时点加 PIV | `418294 mov eax,[edx+0x44]` / `418297 and eax,1` / `41829d add ecx,eax` / `4182ab cdq` / `4182ae sar eax,1` / `4182b0 imul eax,eax,0xa` / `4182bc mov [ecx+0x24],eax` |
| ② SC 捕获再加一笔 | `417329 mov edx,[ecx+0x10]` / `41732c imul edx,edx,0xa` / `417334 add edx,[globals+0x24]` / `41733d mov [globals+0x24],edx` |
| ③ PIV 就是点值基数 | `440e4b mov eax,ds:0x160f510` / `440e50 mov ecx,[eax+0x24]`（`Item::CollectPoint`）|

⇒ 机制改的是 **PIV 本身**，而 PIV 是点道具分值的基数 ⇒ 把 `ΔG_08` **并进 `ecx` 声明**与原作同义。

**卡 108 的落点（2026-09-22，P4 工单）**：判定为**进**，但**本轮不实装** —— 108 今天**没有刻符载体**
（没有「谁生时点」的生成点、没有 VM 池、没有收集路径、也没有人妖槽）。现在唯一能落笔的做法是
凭空造一个 PIV 涨源，那是 [`METHOD.md`](../../../METHOD.md) 禁止的「自定包装成复刻」。
⇒ 等 owner 定「刻符载体」那一轮（口径已由 owner 2026-09-18 给出：VM 池 + 默认自动吸收 + 中途暴毙变类 P 点），
按 `ce_piv_add(108, …)` + `FAMILY_08` 接上即可。登记见 `mods/th18.v1.00a/card-expand/AUDIT.md` §BE-FC。

### ★ PIV 初值的一手复核（2026-09-18，直读字节 + 平坦反汇编）

`th08:0x43af20` 起是一个按难度分派的 switch（分派变量 = **`ds:0x160f538`**，同文件多处拿它与 `0x4` 比 ⇒ 难度 0..4），
五个分支各写一次 `[[this]+8]+0x24`（= `[ds:0x160f510]+0x24` = **蓝点分值读的那个字段**）：

| 难度 | 分支 | 指令 | 值 |
| --- | --- | --- | --- |
| E（0） | `th08:0x43af43` | `th08:0x43af49` `mov dword [eax+0x24], 0xea60` | **60000** |
| N（1） | `th08:0x43af52` | `th08:0x43af58` `mov dword [eax+0x24], 0x186a0` | **100000** |
| H（2） | `th08:0x43af61` | `th08:0x43af67` `mov dword [eax+0x24], 0x30d40` | **200000** |
| L（3） | `th08:0x43af70` | `th08:0x43af76` `mov dword [eax+0x24], 0x493e0` | **300000** |
| EX（4） | `th08:0x43af7f` | `th08:0x43af85` `mov dword [eax+0x24], 0x493e0` | **300000**（与 L 同值） |

读侧确认：`th08:0x440e4b`–`th08:0x440e53`（蓝点处理体 `th08:0x440e40`）就是 `mov eax,ds:0x160f510; mov ecx,[eax+0x24]` ⇒ **同一字段**。
⇒ **单位是「点」1:1**（对比 TH12–TH17 的「存储 = 显示 ×100」），且**没有找到上限钳位**（init 分支与涨值站点都没有 clamp 常量）❓。

### ★★ PIV 的全部写点穷举（2026-09-18 补核，取代上一段的「❓ 未钉」）

**写 PIV（= `[[0x160f508]+8]+0x24`，即 `[ds:0x160f510]+0x24`）的站点全 exe 只有三处**（把 `mov reg,ds:0x160f510` 与 `mov [this+8]` 两种取法都扫了）：

| # | 站点 | 形状 | 语义 |
| --- | --- | --- | --- |
| 1 | `th08:0x43af49` / `:0x43af58` / `:0x43af67` / `:0x43af76` / `:0x43af85` | 按难度写初值 `60000/100000/200000/300000/300000` | 开局初始化（见上文表） |
| 2 | `th08:0x4182b0`–`th08:0x4182bc` | `PIV += ((orbs + (totalTimeOrbs & 1)) / 2) × 10` | ★ **`GameManager::sub_418220`**（本文称 `AddTimeOrbs`）—— **+10 / 2 颗**，**只在 `orbs > 0` 时进这段** ⇒ **负数不会掉 PIV** |
| 3 | `th08:0x41732f`–`th08:0x41733d` | `PIV += [zSpellcard+0x10] × 10` | ★ **`Spellcard::on_tick`**（`th08:0x416b90`）里的 **SC 结算段**：`AddTimeOrbs(count)` 之后**再额外**加 `count × 10` |

**站点 3 的来龙去脉（一手）**：
- `th08:0x417f60` = **`Spellcard::initialize`**：`UpdateFunc::operator new`（`th08:0x43ce60`）造两个回调（`on_tick` = `th08:0x418010` 的 thunk → `th08:0x416b90`；`on_cleanup`/draw = `th08:0x418030` / `th08:0x418050`），句柄存 `[zSpellcard+0x263c]` / `[zSpellcard+0x2640]`，再 `register__on_tick`（`th08:0x43c880`）/ `register__on_draw`（`th08:0x43c960`）挂上。
- 那段代码的触发：SC 结算分支（`th08:0x4172bf`–`th08:0x4172e8` 先 `add_score([zSpellcard+0x104])`，`0x4181f0` = `GameManager::add_score` = `SCORE += amount/10`）之后清 `flags` 的 `0x100` 位；**若 `[zSpellcard+0x10] > 0`** 才走这个 flush。
- `zSpellcard + 0x10` 的名字（ExpHP `type-structs-own.json`）= **`time_orbs_not_yet_spawned_during_capture`** ⇒ 这段就是**「SC 捕获时把 SC 期间攒下的时点一次性入账」**。
- ⇒ **与「面数/关底结算」无关**：关底那套（蓝点 5W / graze 500 / time 1000 那几行）**没有任何 PIV 写点**。

**`AddTimeOrbs` 的 5 个调用点全列**（`grep 'call   0x418220'`）：

| 调用点 | 参数 | 归属 |
| --- | --- | --- |
| `th08:0x4155ae` | `[obj+0x10]` | `th08:0x4152a0` 里的**另一次** `+0x10` flush（**不带**额外 PIV 那一笔）⇒ 说明「攒着再入账」的形状在本作里有两处 |
| `th08:0x41731e` | `[zSpellcard+0x10]` | ★ SC 结算（带 `PIV += count×10`） |
| `th08:0x44137a` | `1` | `Item::CollectTimeOrb`（`th08:0x4412b0`，type 7）：先 `add_score([item])` 再 **+1 刻符** |
| `th08:0x44cc10` | `0xfffffff1` = **−15** | `th08:0x44cbf0`：**负**增量（miss / 决死那类）⇒ 只扣统计、**不进 PIV 分支** |
| `th08:0x44cd7c` | `−(x/10)` | 同上函数内的另一处负增量 |

⇒ **社区口径逐条对上（交叉验证 ✅）**：夜点初值 6W/10W/20W/30W/30W ✓、人逢魔 ×2 ✓、线下越靠版底越低 ✓、
**miss/决死只扣「右侧 time 统计」而不扣夜点** ✓（= 一手那条「负数不进 PIV 分支」）、SC 捕获按剩余时间给刻符奖励 ✓（= 站点 3）。

**⚠ 一条留给口径判的算术**：站点 3 对**同一批 `count`** 先调 `AddTimeOrbs(count)`（+10/2 颗）**再** `+10/颗`
⇒ 若两笔都算生效，SC 期间攒下的刻符是 **约 15/颗**（普通路是 5/颗）。这是 ZUN 有意还是重叠，静态**判不了**。
★ **用户口径（2026-09-18）：「有意无意就按照复刻即可」** ⇒ **两笔都写**：
`ΔG_08` 在 SC 捕获那批上 = `AddTimeOrbs` 那一份 **＋** `count × 10` 那一份（不要因为「看着像重复」而只取其一）。

**上限（用户口径 2026-09-18）**：本作 PIV **没有原生上限**（init 与两个写点都没有 clamp 常量）——
mod 侧**不必去找**，按 int32 自然上界设一个**安全上界**（`2147483647` 级，防溢出成负数）即可。

---

## ★★ 2026-09-23 订正块（清册普查查出：`th08:0x431650` 一族不是擦弹 + 本文内部两处自相矛盾统一到一手）

> 订正人 / 日期：**2026-09-23**（DSH 工单「按两份新清册订正既有文档误读」，本轮逐指令复核）。
> 原始清册：[`engine/_shared/rosters/th06-th09.md`](../../_shared/rosters/th06-th09.md) §3.2 的「⚠ 一条看着像擦弹、其实是炸弹裁剪的路」+ §5 第 5 / 第 6 条。
> 规矩：§3 表、§5.4、§8-13 的**旧字一字未删**（只加 ⚠ 与删除线），判据与正确说法写在这里。

### ① `th08:0x431650` / `0x431668`（以及同族四簇）= **命中「炸弹消弹区」支（`eax == 2`）**，不是擦弹

- **原文说了什么**：§3 表把 `0x431650` / `0x431668` 标成「**擦弹**：`ds:0x18b8988 == 9` 时掉 2 个…」，后面四簇写「同上」。§8-13 还用这一族当「擦弹那一路生成即 state 1」的证据。
- **为什么错（一手，逐指令）**：这一族的判定返回值在 `[ebp-0x4]`，三条出口**互斥**：

```
th08:0x4315f2  call 0x44a470               ; 弹 × 自机判定（0 = 没碰上 / 1 = 擦弹 / 2 = 命中炸弹消弹区）
th08:0x4315f7  mov  DWORD PTR [ebp-0x4],eax
th08:0x4315fa  cmp  DWORD PTR [ebp-0x4],0x1 ; jne 0x43160f     ; ★ eax == 1 = 擦弹
th08:0x431603  mov  BYTE PTR [eax+0xdbd],0x1 ; jmp 0x431699     ; ★★ 擦弹只置「已擦」闩，然后就走 ⇒ 不出货
th08:0x43160f  cmp  DWORD PTR [ebp-0x4],0x2 ; jne 0x431694     ; ★ eax == 2 = 炸弹消弹区
th08:0x431624  test edx,edx（[bullet+0xdb0] & 0x1000 门）
th08:0x43162b  mov  WORD PTR [eax+0xdb8],0x5                   ; state = 5
th08:0x431650  call 0x4400a0                                    ; ★ 撒 type 7（或 ds:0x18b8988）
```

  - `0x44a470` 的返回值 **2** 来自它开头的 `0x44a49e: call 0x449ff0`（`Player::check_bomb_cancel_areas`）：`0x44a4a6 test eax,eax` / `0x44a4a8 je 0x44a4b4` ⇒ 非 0 时 `0x44a4aa: mov eax,0x2` 直接返回；**返回 1 是它自己的擦弹出口**（`0x44a586`–`0x44a594`：`push 0x0` + `call 0x44a930` ⇒ `mov eax,0x1`）。⇒ **`eax == 2` 与擦弹互斥**。
  - 同族另四簇（`0x43170a` 族 / `0x431837` 族 / `0x431948` 族 / `0x431a59` 族）的门同样是 `call 0x449ff0` + `cmp eax,0x2`（`0x4317cf`/`0x4317d4`、`0x4318e1`/`0x4318e6`、`0x4319f2`/`0x4319f7`，`0x4316c3`/`0x4316e8` 是同一函数里另一处拷贝）⇒ **整族都是炸弹消弹区支**。
- **正确的说法**：`th08:0x431650` 一族 = **弹落在「炸弹消弹区」里时**的产出（判定函数 `th08:0x449ff0` = `Player::check_bomb_cancel_areas`：自机炸弹消弹区池 `player+0xbb834` 起、`0xc0` 槽 × `0x40` B，命中第一个区就返回 2 ⇒ `0x44a4aa: mov eax,0x2`）——
  调用侧压的是 `push 0x1`(state) + `push 0x7`(type)（或 `push ds:0x18b8988`），而 **`spawn_item` 对 `type == 7` 强制 `state = 3`**（`0x440115: cmp [ebp+0xc],0x7` / `jne` ⇒ `0x44011b: mov DWORD PTR [ebp+0x10],0x3`）⇒ 落到池里的是 **type 7 + state 3（先上抛）**；只有动态 type 那一支（`ds:0x18b8988 != 9`）才保持 `state = 1`。
  ⚠ 注意区分：`0x43162b: mov WORD PTR [eax+0xdb8],0x5` 写的是**弹对象自己的字段**（`bullet+0xdb8`），**不是道具 state**（原文把两者混着读）。**擦弹一颗道具都不出**（只置 `bullet+0xdbd = 1` 去重闩）。⇒ §3 表那五行的「擦弹」标注作废（❌）。
- ⚠ 与清册的差别（照实记）：清册 §3.2 这道门写作「`ds:0x164d0b1 != 0xa`」——按指令是 `0x44aa94 movzx edx,BYTE PTR ds:0x164d0b1` / `0x44aa9b cmp edx,0xa` / `0x44aa9e jne 0x44ab14` ⇒ **必须 `== 0xa` 才继续**（写「!=」容易读反）；另 `0x42f1f0` 经清册 2026-09-22 夜复核 = `EnemyManager::is_boss_active`（扫 `ENEMY_MANAGER.bosses[0..7]`），即**有 boss 在场**是第 1 颗的前置。
- **清册的残留 ❓ 已收敛**：清册 §3.2 把 `ds:0x18b8988` 标成「🟡 静态 0、无字面写点」——本轮确认它**只在这一支当 `type` 用**（`0x43163d` / `0x43166f` / `0x4316ee` / `0x43181b` / `0x43192c` / `0x431a3e` 处的 `push ds:0x18b8988`），**写点仍未找到** ⇒ 「它怎么涨」保持 ❓（待核方式：算基址后扫写点，或运行期断点）。

### ② 擦弹的真实落点 = `0x44a930` 里的**三处** `spawn_item`（每擦一颗弹 1~3 颗），三道门逐条如下

- **原文说了什么**：§3 表把 `0x44aac9` / `0x44aaef` / `0x44ab0f` 记作「玩家相关（`0x44aaae` 起先查 GameManager 两个查询，再查符卡状态）」；§5.4 又把擦弹写成 `0x44aabe push 1`；§8-13 则写「擦弹/消弹那一路生成即 state 1」——**后两者与 §5.4 自相矛盾**（本文档内两处口径打架）。
- **正确的落点与门（一手，逐指令）**：三处都在 **`0x44a930`** 体内（= 擦弹奖励函数；它的两个调用者才是擦弹判定：`0x44a58f`（弹，`flag = 0`，来自 `0x44a470`）与 `0x44a8d1`（激光，`flag = 1`，来自 `Player::sub_44a6a0`））。
  ⚠ 命名坑：§3 表里写的宿主 `Player::sub_44a6a0` 是 vendor「取 ≤ 地址的最近名」的产物，**`0x44a930` 是它自己的函数**。

```
门①（第 1 颗，0x44aac9）  0x44aa94 movzx edx,BYTE PTR ds:0x164d0b1 ; 0x44aa9b cmp edx,0xa ; jne 0x44ab14   ; 必须 == 0xa
                          0x44aaa5 call 0x42f1f0(ecx=0x577f20)     ; 必须非 0
                          0x44aab3 call 0x406d70(ecx=0x160f508)    ; GaugeIsExtremelyYoukai 必须非 0
                          0x44aabc push 1(state) / 0x44aabe push 0xa(type) / 0x44aac0 位置 ⇒ 0x44aac9 call spawn_item
门②（第 2 颗，0x44aaef）  0x44aace cmp DWORD PTR [ebp+0xc],0x0 ; jne 0x44ab14                       ; flag == 0 才继续 ⇒ 激光路径（flag=1）只给第 1 颗
                          0x44aad9 call 0x4178a0(ecx=0x4ea670)     ; 必须非 0（清册 2026-09-22 夜复核 = SPELLCARD.flags & 1「符卡进行中」⇒ 不在符卡里连第 3 颗也没有）
门③（第 3 颗，0x44ab0f）  0x44aaf9 call 0x42f270(ecx=0x160f508)    ; GameManager::shottype_is_solo_youkai 必须 == 0（不是「妖単独」）
```

| 问题 | 答案 | 一手 |
| --- | --- | --- |
| 每擦一颗弹掉几颗 | **1～3 颗**（三道门全过 = 3；激光擦弹 = 1） | `0x44aac9` / `0x44aaef` / `0x44ab0f` |
| 走什么号 | **`type 0xa`**（三处都 `push 0xa`），在 `spawn_item` 里被 `0x440124` 改写成 **`type 7` + `state 5`** | `0x440124`：`cmp DWORD PTR [ebp+0xc],0xa` / `jne` ⇒ `mov DWORD PTR [ebp+0x10],0x5` + `mov DWORD PTR [ebp+0xc],0x7` |
| 出生态 | **state 5 = 先上抛**（≈40 帧，`vy = −2.0 − rand(0.2)`、`+0.05/帧`）⇒ **不是**「生成即满速归航」 | §5.3 / §7.1 同款 |

- ⇒ **§5.4 的 `push 1` 那格是笔误**（`0x44aabc` 才是 `push 0x1` = state；`0x44aabe` 是 `push 0xa` = type），**结论（state 5）本来就对**；
  而 **§8-13 的「擦弹生成即 state 1」作废**——state 1 只属于**消弹**那一支（`BulletManager::cancel_all` 一类：动态 type = `[bullet+0x6ba570]`（初值 6）+ `push 1`）。两处口径**已按本节统一到一手**。
- 🟡 **没复核到的一格（照实留）**：激光擦弹的判据「外扩 48.0f（`0x4b48c8`）+ 每个 `(timer % 20) == 0` 判一次」出自清册；
  本轮只复核了 **`0x4b48c8 = 48.0f`**（直读 `.rdata`）与 **`0x4b6e98 = 20.0f`**（弹擦弹外扩，`0x44a4c4 fsub ds:0x4b6e98` ✅），
  **没有逐指令复核 48.0f 在 `0x44a6a0` 里的用法**（该常量在 `0x42217c` 一族被读，属激光判定侧）⇒ 该句 🟡。

### 判据 / 复现

```bash
objdump -d -M intel --start-address=0x4315f0 --stop-address=0x4316a0 local/th08.v1.00d/th08.exe   # ① 三条互斥出口
objdump -d -M intel --start-address=0x44a470 --stop-address=0x44a4c0 local/th08.v1.00d/th08.exe   # ① 返回值 2 / 常量 0x4b6e98
objdump -d -M intel --start-address=0x44aa80 --stop-address=0x44ab30 local/th08.v1.00d/th08.exe   # ② 三道门 + 三处 spawn
objdump -d -M intel --start-address=0x4400f8 --stop-address=0x440140 local/th08.v1.00d/th08.exe   # ② type 0xa ⇒ 7 + state 5
grep -n 'call   0x44a930' /tmp/th08_full.asm                                                      # 只 2 个调用点（弹 flag=0 / 激光 flag=1）
```

- **可信度**：① = ✅ 一手（三条出口逐指令 + 同族四簇门穷举）；② = ✅ 一手（三道门逐指令 + `0x440124` 改写 + 两个调用点穷举）；
  `ds:0x18b8988` 的写点 = ❓（未找到）；激光擦弹的 48.0f 用法 = 🟡。
