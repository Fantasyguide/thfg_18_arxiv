# TARGET —— tracking-laser 死绑登记
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。


**换一个 exe build，下面每一项都要重取。**

## 目标二进制

| 项 | 值 |
| --- | --- |
| 游戏 / 版本 | TH16《東方鬼形獣》v1.00a |
| exe md5 | `cb9caf54ce5738f70086e783ec88fd2a` |
| 大小 | 683,520 B |
| imagebase | `0x400000` |
| thcrap 版本匹配 | 经 `base_tsa/versions.js` 的 exe 哈希匹配 |

## 写入点

| # | 地址 | expected（原字节） | 写入内容 | 依据 |
| --- | --- | --- | --- | --- |
| 1 | `0x4919b0` | `f0704400` | `<codecave:tracking_burst_laser>` | SHT tick 函数指针表 `sht_func_tick_table` @`0x4919a0` 的 **idx4** 槽，原指 `0x4470f0` |

⚠️ 这是**数据指针表槽**，必须写**绝对**地址 → `<codecave:…>`。
写成 `[codecave:…]`（相对 rel32）会把偏移当指针用 → 跳错地址崩。
出处：`thcrap/src/binhack.cpp` L869–879。**这是审计抓到的第 2 个真 BUG。**

## 依赖的引擎入口

| 项 | 地址 | 说明 |
| --- | --- | --- |
| SHT 解析器 | `0x443790` | 把 `func_*` 索引解成函数指针，**无边界检查**（这是本 mod 成立的前提） |
| tick 派发 | 经 `shooter+0x2c` | 见 [`engine/sht/th16/04 §1`](../../../engine/sht/th16/04-th16-shot-runtime-architecture.md) |
| 命中派发 | `0x445d40` | `playershot_hit_dispatch`：`if (*(shooter+0x34)) call; else playershot_launch_shared` → **`func_on_hit=0` 安全，不会 `call 0`** |
| 参考实现（寻的 tick） | `0x445ee0` | 引擎自己的寻的行为，cave 的 ABI 判据取自这里 |

## codecave 调用的引擎函数 —— ★ 全是 stdcall

判据：**引擎自己在 `0x445ee0` 调这三个都没有 `add esp`**（一手铁证）。

| 函数 | 约定 | 尾指令 |
| --- | --- | --- |
| `find_nearest_enemy` | **stdcall** | `RET 8` |
| `is_enemy_alive` | **stdcall** | `RET 4` |
| `anm_unload` | **stdcall** | `RET 4` |

> **历史 BLOCKER（已修）**：原 `.asm` 按 cdecl 在调用后加了 3 条 `add esp` →
> ESP 抬高 → `pop esi/edi` 读到栈帧局部 → esi/edi 被破坏 →
> `tick_bullets` 崩（它跨调用持有 ESI=弹槽、EDI=PLAYER_PTR）。
> 修复：删掉那 3 条 `add esp`，`.c` 的 extern 标 `__stdcall`。
> cave 自身是 `__fastcall`，用普通 `ret`。

## 依赖的结构偏移

| 项 | 偏移 / 算式 | 依据 |
| --- | --- | --- |
| 伤害源对象 | `PLAYER + 0xd080 + link * 0x94`（**无 +1**） | [`engine/sht/th16/08 §2`](../../../engine/sht/th16/08-th16-player-damage-pipeline.md) |
| 伤害上限 | SHT header `+0x28` = 60（本 mod 用 dmg=30，多源叠加单敌每帧 ≤60） | [`engine/sht/th16/05`](../../../engine/sht/th16/05-th16-flags-no-runtime-read.md) |
| tick 前置 | `func_on_init` = 3（`0x4470e0`），清 `+0x90` 目标槽 + `flag & 0x3c` | [`engine/sht/th16/03`](../../../engine/sht/th16/03-th16-funcstar-jumptables.md) |

## 占用的索引

`func_on_tick` **idx4**。原版 tick 只用 {0,1,2,3,5}；重指 idx4 仅牺牲 lock-dash 实验产物。

> 注：早期 PLAN 写「tick ∈ {0,1,2,5}」漏了 idx3（加速），与 idx4 的安全性无关，但记在这里防再错。

## 换版本时必须重取

- [ ] tick 表基址与 idx4 槽地址、原字节
- [ ] 解析器地址与「无边界检查」是否仍成立
- [ ] 三个被调函数的地址**与调用约定**（不要假设不变）
- [ ] 伤害源池基址 / stride / header 偏移
- [ ] 原版实际用到哪些 tick 索引（决定哪个槽可占）
