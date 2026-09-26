# TH18 `parent_enemy_id`（父子敌机）与「子敌人」：谁是谁的零件、以及**关卡时间轴本身是一只敌人**

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本，跨版本引用要写前缀。
> **方法**：一手反汇编 `EnemyManager::allocate_new_enemy` `0x42d7d0`、`EnemyData::ecl_run_over_300` `0x430d40`
> 的 `enmCreate` case、`GameThread::thread_start` `0x4424e0` / `GameThread::on_tick` `0x443d70` 的关卡启动段；
> 加上 `thecl` 反编译全 22 个 `.ecl` 后**跨文件调用图**静态分析（复现命令见 §5）。
> **可信度**：机制 ✅（逐指令）；§4 的表是**静态分析**结论（🟡 只有实跑才能确认某只敌机在同一帧里是否真的活着）。
> 起因：卡 108（TH08 永夜抄）要照抄 `Enemy::sub_42adb0` 的「母机带子敌人一起死 ⇒ 每个子敌人一盘时点」，
> 而 TH18 没有 TH08 那种「弹幕子敌人」链表 ⇒ 需要先把 TH18 的对位物搞清楚。

## 0. 一句话结论

TH18 的 `zEnemy.parent_enemy_id`（`enemy+0x6834`）= **「谁跑的 ECL 把我建出来的」**；
而**关卡脚本 `main` 本身就是一只敌人**（`GameThread::on_tick` `th18:0x444228` 用 ECL 名 `"main"` 建）
⇒ **时间轴刷出来的每一只敌机，父字段都等于那个「时间轴载体」的 `enemy_id`（非 0）**，
只有「载体自己」与「本作自带壳」的父字段是 0。
所以「父字段非 0」在 TH18 里**不能**当成「这只敌机是别人的零件」的判据 —— 它几乎恒真。

## 1. 字段与写点（一手）

| 对象 | 字段 | 语义 |
| --- | --- | --- |
| `zEnemy` | `+0x6830` | `enemy_id`：**单调递增**，由 `EnemyManager+0x90 (next_enemy_id)` 分配 |
| `zEnemy` | `+0x6834` | `parent_enemy_id` = 创建时 `spawn_params+0x50` |
| `EnemyManager` | `+0x90` / `+0x94` | `next_enemy_id` / `last_enemy_id` |

**分配（一手 `0x42e3f0`–`0x42e40b`）**：
```asm
42e3f0  mov eax,[ecx+0x90]        ; next_enemy_id
42e3f6  mov [esi+0x6830],eax      ; enemy_id = next
42e3fc  mov eax,[ecx+0x90]
42e402  mov [ecx+0x94],eax        ; last_enemy_id = next
42e408  add eax,0x1
42e40b  mov [ecx+0x90],eax        ; next = next + 1
```
**初值 = 1**（不是 0）：`EnemyManager::operator new` `0x42dc20` 先 `memset(this, 0, 0x1098)`，
随后 `0x42dc59`–`0x42dc77` 把 `last` 写 0、`next` 写 **1**（`mov eax,[edi+0x90]; add eax,1`）
⇒ **ID 从 1 开始**（这一点很要紧：见 §3）。
`EnemyManager::destroy_all` `0x42ab40` **不清** `+0x90/+0x94`（它只清 `+0x48..+0x88`）
⇒ **ID 在整局里单调增长，换关不重置**。

**父字段（一手 `0x42d8e4`/`0x42d8e7`）**：
```asm
42d8e4  mov eax,[edx+0x50]        ; edx = spawn_params（`ce_spawn_enemy` 的第二个实参）
42d8e7  mov [esi+0x6834],eax      ; → zEnemy.parent_enemy_id
```
`spawn_params` 就是 `engine.h` 的 `CE_SPAWN_*` 那张表（`CE_SPAWN_SIZE` = **0x54**，
父在最后一个字段 `CE_SPAWN_PARENT` = **0x50** ⇒ 结构体跨度 `[base, base+0x54)` 与「父 = `base+0x50`」自洽）。

**谁写它（一手 `EnemyData::ecl_run_over_300` `0x436b64`–`0x436b9c`）**：
```asm
436b64  mov eax,[esi+0x5588]      ; esi = 正在跑 ECL 的那个 zEnemyData；+0x5588 = .full（所属 zEnemy）
436b76  mov eax,[eax+0x6830]      ; = **施放者自己的 enemy_id**
436b82  mov [ebp-0x4],eax         ; 存进 spawn_params 的最后一格（[ebp-0x54]+0x50 = [ebp-0x4]）
436b9c  call 0x42d7d0             ; EnemyManager::allocate_new_enemy(mgr, name, &params, 0)
```
⇒ **`enmCreate*` 一族（ECL 301/302/305/306/310–313）建出来的敌机，父 = 跑这条指令的那个敌人**。
（`enmCreate` 之外的创建点：`GameThread::on_tick` `0x444228`/`0x444471`（"main" 与另一只）、
`CardLilyWhite::c_press` `0x40ff46`、`Bullet::run_ex` `0x426fe4`、`EnemyData::ecl_func_call_*_ex`
`0x439168`/`0x4392e8`（`BossCard6_et`）—— 这些的 params 里父字段多半是 memset 出来的 0。）

## 2. ★★★ 关卡时间轴是一只敌人（这才是 TH18 的特殊之处）

`GameThread::on_tick`（`0x443d70`）的**关卡启动段**（一手，`0x4441ba`–`0x444228`）：
```asm
4441ba  call 0x423980     ; BulletManager::destroy_all
4441bf  call 0x45abb0     ; Player::reset
4441c4  call 0x42aa30     ; ItemManager::destroy_all
4441cf  call 0x42ab40     ; EnemyManager::destroy_all
4441da  call 0x442480     ; LaserManager::destroy_all
4441e5  …                 ; 三个全局清零
444208  push 0x54 / memset([ebp-0x70], 0, 0x54)     ; spawn_params 全 0 ⇒ **父 = 0**
44421c  mov ecx,[0x4cf2d0]                          ; EnemyManager
444223  push 0x4b6634                               ; ECL 子程序名 = "main"
444228  call 0x42d7d0                                ; allocate_new_enemy
```
⇒ **每关开头都新起一只「跑 `main` 的敌人」**。它没有 anm、没有血量、不上屏，
但它是一只**正常的 zEnemy**（有 `enemy_id`、进活动链表）⇒ 它 `enmCreate` 出来的所有道中敌机，
父字段 = 它。**TH08 里对应的一层是 ECL 管理器自己的时间轴（不是敌人）**，所以两作的
「普通敌机有没有 owner」语义在这里分叉 —— 拿 TH08 的 `HasOwnerEnemy` 判据直接套 TH18 会踩空。

## 3. 判据怎么用

| 想问的事 | TH18 里的判据 |
| --- | --- |
| 「这只敌机**是别人的零件/召唤物**」 | ❌ **不能**只看 `parent_enemy_id != 0`（时间轴载体让它几乎恒真）|
| 「**这只敌机有没有活着的零件**」（= 它是不是母机）| ✅ 扫活动链表：`other.parent_enemy_id == this.enemy_id`（本文 §4 的表）|
| 「这只敌机**是被谁召唤的**」 | ✅ `parent_enemy_id` 指向 `enemy_id`，但要把「= 时间轴载体」那一档剔掉（载体不可见、不可打、`life == 0`）|

⚠ ID 从 **1** 开始这一点是有用的：`parent_enemy_id == 0` 只可能是
「`main` 载体自己」或「创建点没写父字段的那几条硬编码路」（§1 末）。
卡 108（§BE #114）就踩在这上面：它用「父非 0」近似 TH08 的 `HasOwnerEnemy` ⇒
**普通妖精会掉 1 颗**（而不是 TH08 的 0 颗）—— 已登记 `TUNING.md`。

## 4. 「谁会造孩子」全表（静态分析：22 个 `.ecl` 的跨文件调用图）

判据：把 `enmCreate*("X")` 建出来的子程序名当作「由敌人跑的 sub」的根，
沿 `@Sub()` / `callSTD("Sub")` / `setInterrupt(..., "Sub")` 等**字符串子程序名**做传递闭包，
再看哪些**可达**的敌人 sub 里又出现了 `enmCreate*`。
（⚠ `GirlC02` / `ECir00*` 这类 sub 在全 22 个文件里**没有任何 `enmCreate` 建它们** ⇒ 是不可达的死代码，
不算。）

| 关卡 | 「母机」 | 它造什么（条数）| 触发时机 |
| --- | --- | --- | --- |
| st01–st07（每关都有）| `Boss` | `Ecl_EtBreak2` / `Ecl_EtBreak2_ni`（各 1）| `BossDead`（**死亡时**）|
| st01–st07（每关都有）| `MBoss` | `Ecl_EtBreak` / `Ecl_EtBreak_ni`（各 1）| 道中 boss 死亡 / `MBoss` 体内 |
| **st03** | `Boss`（经 `BossCard3_at2`）| `BossPhantom01` **×6**（life 300）| 符卡进行中 |
| **st05** | `Boss`（经 `BossCard1/2/4`）| `BossCard1/2/4_Et` 各 **×2**（life 100）| 符卡进行中 |
| **st07** | `Boss`（经 `BossCard2` / `BossCard8_at`）| `BossCard2_Et` ×3、`BossCard8_et` **×5** | 符卡进行中 |

⇒ **TH18 里「母机带着活着的零件死掉」是稀有事件**：主要是
① st03 那张会召 6 只幽灵的符卡；② st05/st07 的 `_Et` 还在场时把 boss 打死；
③ 道中 boss 的 `Ecl_EtBreak*`（那批是**死亡时**才建出来的 ⇒ 建它们的那只母机死时它自己还没有孩子）。
其余击破都是「单只、无零件」。

## 5. 怎么复现本文 §4 的表

```bash
# 反编译全部 22 个 .ecl（`-d 18` 必须相邻，其它 flag 放前面）
mkdir -p /tmp/th18ecl
for f in local/th18.v1.00a/dat/*.ecl; do
  b=$(basename "$f" .ecl)
  local/vendor/thtk/build/thecl/thecl -j -d 18 -m local/vendor/eclmap/eclmap/th18.eclm "$f" /tmp/th18ecl/$b.txt
done
# 再按「关卡组」把 st0X + st0Xbs + st0Xmbs 合起来做调用图（boss 的 sub 在 bs 文件里、按名字跨文件引用）
```
⚠ **跨文件**：`st0X.ecl` 里的 `enmCreate*("Boss", …)` 指向的子程序**定义在 `st0Xbs.ecl`**
⇒ 分析必须按关卡组并起来，否则 bosss 那一坨全成了「不可达」。

## 6. 与卡 108 的关系

`mods/th18.v1.00a/card-expand` 的 `youkai.c::yk_on_enemy_killed` 用本文 §3 的第二条判据当**门**
（TH08 `Enemy::sub_42adb0` 的 `GetSubEnemyChainCount() == 0 ⇒ 整段跳过`），
撒点在**每个孩子的位置**上；本表就是它「什么时候真的会撒」的预期。
一手与偏差登记见 `AUDIT.md` §BE #114、`TUNING.md` §2.0zy-bis。
