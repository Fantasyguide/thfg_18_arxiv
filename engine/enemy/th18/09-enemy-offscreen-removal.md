# 09 · TH18 敌人「出框即删」——判据、链路与反制（`flags_lo` bit2/bit3）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **方法**：一手 —— `local/_scratch/thdis.py`（capstone 32 位）逐指令；全 `.text` 位移扫描（`0x5130`/`0x635c` 的
> 读/写点、`0x4fe0`/`0x4fe8` 的读/写点）；`.rdata` 浮点常量直读；22 份原生 ECL 机扫（`flagSet`/`flagClear` 实参）。
> **可信度**：判据与删除链 ✅（逐指令 + 常量直读）；「置 `0xC` 即免删」✅（两条分支各查一位，全 exe 无别的读者）；
> 原生 `flagSet(12)` 的用途 ✅（4 处，全在 `Phantom01/02`）。
> **起因**：108 卡（永夜抄使魔）owner 报「**用程序生成的所有敌人，只要出框了就会删除**」。

## 0. 一句话结论

引擎每帧对每只敌人做一次「**精灵盒 vs 场地矩形**」判定：盒 = `final_pos ± final_sprite_size × 0.5`，
场地 = `x ∈ [-192, 192]`、`y ∈ [0, 448]`。**盒完全出框**、且该敌**进过屏**（`flags_lo` bit16 `0x10000`）、
且**对应轴的免删位是清的** ⇒ 该敌被**摘链并释放**。
⇒ 反制 = 置 `flags_lo` 的 **bit2 `0x4`（横轴）| bit3 `0x8`（纵轴）**（= `0xC`）。ZUN 自己在原生 ECL 里
用的就是 **`flagSet(12)`**（4 处，全在会飞出画面的 `Phantom` 身上）。

## 1. 删除链（逐指令，✅）

```
EnemyManager__on_tick 0x42df50
  └ 遍历 active_enemy_list（mgr+0x18c 头、node+4 = next）
      0x42dfc5  test [e+0x635c], 0x2000000   ; flags_lo bit25 = 「正在死」⇒ 直接去删
      0x42dfd1  call 0x42fe80                ; 否则：本帧的敌人更新 + 出框判定
      0x42dfd6  test eax,eax
      0x42dfd8  jne  0x42dfe8                ; ★ 返回非 0 ⇒ 去删
      0x42dfdc  and  [e+0x635c], 0xfffbffff  ; 清 bit18（0x40000 = 「本帧已更新」守卫位）
      0x42dfee  mov  eax,[e]
      0x42dff0  push 1
      0x42dff2  call [eax+0x14]              ; = vtable[0x14] = 0x42d220
```

- `FUN_0042ff80`（`0x42ff80`）：每敌每帧的实际更新体。开头 `test [e+0x5130],0x40000` → 已置就**整个跳过**
  （所以每帧只会真正跑一次）；否则置 bit18，然后
  `0x42ffa7 call 0x42e5a0`（**出框判定**）、`0x42fff0 call 0x42ed40`（ECL 步进）、`0x42ffff call 0x42f890` …
- `FUN_0042e5a0`（`0x42e5a0`）：动画 VM 更新 + 出框判定；**返回 `-1` = 「这只敌人该被删了」**，
  返回 `0` = 正常。返回值沿 `0x42ff80` → `0x42fe80` 一路上传，最终由 `EnemyManager__on_tick` 处置。
- `vtable[0x14] = 0x42d220`：敌人析构 —— 从 `mgr+0x18c/0x190/0x194` 摘链、修 `+0x1518/+0x151c` 的
  前后向指针、`dec [mgr+0x198]`（`enemy_count_real`）、清 boss id 表、释放 16 个 anm VM 槽。
  ⇒ 这就是「**消失**」的落点（对象被释放，不是隐藏）。

## 2. 出框判据（`0x42eacf`–`0x42ebf8`，✅）

| 量 | 来源 | 一手 |
| --- | --- | --- |
| 盒半宽/半高 | `final_sprite_size × 0.5` | `0x42eacf movss xmm2,[edi+0x4fd8]`；`0x42ead7 movss xmm4,[0x4b9138]`（= **0.5**）|
| 场地左 / 右 | `-192` / `192` | `0x42eae4 [0x4b946c]`（= -192）、`0x42eb04 [0x4b933c]`（= 192）|
| 场地下沿 | `448` | `0x42eb39 [0x4b939c]`（= 448）；上沿是 **0**（`0x42eb2c comiss xmm3(0)`）|
| 进过屏 | `flags_lo` bit16 `0x10000` | 屏内时 `0x42eb46 or [edi+0x5130],0x10000` 置位 |
| 横轴免删 | `flags_lo` bit2 `0x4` | `0x42ebe1 and eax,0x10004` → `cmp eax,0x10000` → **不等（bit2 置）就返回 0** |
| 纵轴免删 | `flags_lo` bit3 `0x8` | `0x42ebd7 test al,8` → `jne` 返回 0 |

判定顺序（简化）：

```
半宽 = sprite_w * 0.5 ; half_h = sprite_h * 0.5
if (-192 > pos.x + half_w) goto X_OUT        ; 完全在左边界外
if ( pos.x - half_w >  192) goto X_OUT       ; 完在右边界外
if (0 > pos.y + half_h) goto Y_OUT           ; 完全在版顶之上
if (pos.y - half_h > 448) goto Y_OUT         ; 完全在版底之下
flags_lo |= 0x10000 ; return 0               ; 在框内（且记住「进过屏」）

X_OUT: f = flags_lo & 0x10004
       if (f != 0x10000) return 0            ; 没进过屏 或 bit2 置 ⇒ 不删
       return -1                             ; ← 删
Y_OUT: if (!(flags_lo & 0x10000)) return 0   ; 没进过屏 ⇒ 不删
       if (flags_lo & 0x8) return 0          ; bit3 置 ⇒ 不删
       return -1                             ; ← 删
```

★ 两个细节值得记：

1. **只有「进过屏」才会被删**（bit16）：从屏外刷出来、还没进框的敌人不会被误删；
   一旦进过框，之后再出框就被删 —— 这正是「**程序生成的敌人在场上待一会、飞出去就没了**」的形状。
2. `final_sprite_size` 参与判定 ⇒ **贴图越大，出框判定越晚**；反之若把尺寸写成 0，
   盒退化成一点，位置一出矩形就删（对自建壳尤其要注意）。

## 3. 反制（✅，且是 ZUN 自己的用法）

- **`flags_lo |= 0x4 | 0x8`（= `0xC`）** ⇒ `0x42e5a0` 在这两条出界分支上都返回 0 ⇒ 永远不因出框被删。
- 全 `.text` 扫描确认：**这两位没有别的读者**（没有副作用）。
- **原生 ECL 佐证**：22 份里 `flagSet(12)` 共 **4 处**，全在会飞出画面的幽灵敌上 ——
  `st01` 的 `Phantom01/02`、`st07` 的 `Phantom01/02`（`flagClear(12)` 4 处收回）；
  另外 `flagSet(3)`（21 处）与 `flagSet(32/16/64)` 是别的位，别混。
- **spawn 参数里没有这一项**：`EnemyManager__allocate_new_enemy`（`0x42d7d0`）只把
  `spawn+0x18` bit0 → `flags_lo ^= 0x80000`、`spawn+0x1c` bit0 → `flags_lo ^= 0x4000000`、
  `spawn+0x14 >= 1000` → `flags_lo |= 0x40000000`（见 `0x42d839`–`0x42d8fc`）
  ⇒ **必须在 `ce_spawn_enemy()` 返回之后自己写** `enemy+0x635c |= 0xC`。
- SDK 落笔（`sdk.h`）：
  · `ce_spawn_enemy(name, spawn)` —— ★ **2026-10-11 二轮起默认置 `0xC`**（owner 拍板：
    「我希望能用到，因为这样在动画上是好的」）⇒ 我们生成的敌人出框不再被引擎收走；
  · `ce_spawn_enemy_keep_offscreen(name, spawn)` —— 等价别名，**把意图写在调用点上**（108 使魔壳用它）；
  · `ce_spawn_enemy_cullable(name, spawn)` / `ce_enemy_cullable_offscreen(e)` —— **显式退回**引擎老行为；
  · 掩码常量 `CE_ENEMY_KEEP_OFFSCREEN_MASK` 与两个纯函数（`sdk_core.h`，主机单测覆盖），
    位名 `CE_ENEMY_FLAG_KEEP_OFFSCREEN_X/Y`（`engine.h`）。

### 3.1 为什么「默认打开」是安全的（本轮静态核过）

出框免删只取消「**引擎替我们按离场收尸**」这一条路；敌人该走的死法一条没少：
`life` 到 0、`enmKillAll`、段首清场、换关 `destroy_all` 都照旧。
而**我们所有 `ce_spawn_enemy` 调用点的 `life` 都是有限值**
（108 `YK_FAM_LIFE`、109 `POFV_SPIRIT_LIFE`、112 `UFO_ETBREAK2_LIFE`、灵梦信仰点 `FAITH_GF_LIFE`）
⇒ 不存在「永久孤儿」。
**代价**：留着的敌人仍占 `enemy_count_real`、一个 anm VM 槽与每帧一次 tick（自建壳的 ECL 是惰性 `wait(1000)`，
开销可忽略）。若某类召唤物**本来就该靠离场消失**，显式用 `ce_spawn_enemy_cullable()`。

## 4. 相关

- 108 侧落地与静态核实全文：[`HANDOFF-108-EMITTER-STATIC-VERIFY.md`](../../../mods/th18.v1.00a/card-expand/HANDOFF-108-EMITTER-STATIC-VERIFY.md)
- 自建敌人原语：[`sdk.h`](../../../mods/th18.v1.00a/card-expand/native/sdk.h) 的 `ce_spawn_enemy` / `ce_spawn_enemy_keep_offscreen`
- 字段与位名：[`engine.h`](../../../mods/th18.v1.00a/card-expand/native/engine.h) 的 `CE_ENEMY_FLAGS` 段
- 发射口槽与波次：[`07-emitter-slots-waves-and-phase-transitions.md`](07-emitter-slots-waves-and-phase-transitions.md)
