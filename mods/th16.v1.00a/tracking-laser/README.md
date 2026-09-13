# tracking-laser — 重指 SHT tick 槽注入追踪激光
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> **状态：静态审计通过，但尚未在游戏里跑过。** 它是**经过审计的流程参考**，
> 不是可照搬到别作的生产模板。

## 这是什么

TH16 的自机 shooter 通过 `func_on_tick` **索引**派发到 `.rdata` 里的一张函数指针表。
解析器 `sht_parse_resolve_funcptrs` `0x443790` 把索引解成指针时**没有边界检查**。
本 mod 用 thcrap 把 tick 表的 **idx4** 槽重指到一个 codecave，让选中该索引的 shooter
执行我们自己的行为——一发会追踪最近敌人的爆发激光。

原版流程用不到 idx4（只用 {0,1,2,3,5}），所以重指它**只牺牲 lock-dash 实验产物**，
不影响正常游戏。

## 怎么装

需要你自己合法持有的 `th16.exe` v1.00a。用 thcrap 加载 `patch/` 下的补丁；
thcrap 经 `base_tsa/versions.js` 的 exe 哈希匹配版本，`expected` 原字节不符会自动跳过。

配套的实验 `.sht`（把某个 shooter 的 `func_on_tick` 设为 4）在
`local/th16.v1.00a/pl02_tracklaser.sht`（本地，不入库）。

## 文件

| 路径 | 内容 |
| --- | --- |
| [`TARGET.md`](TARGET.md) | ★ 死绑登记：写入点、调用约定、结构偏移 |
| [`PLAN.md`](PLAN.md) | 设计与推导过程 |
| [`AUDIT.md`](AUDIT.md) | ★ 对抗审计记录（两个独立 agent，抓到 2 个真 BUG） |
| [`patch/thcrap_patch.md`](patch/thcrap_patch.md) | thcrap patch 结构、binhack/codecave 语法核实 |
| `native/tick_tracking_burst_starter.{asm,c}` | cave 起步版 |
| `native/tick_tracking_burst_laser.c` | 激光行为 |

## 依据

- 派发链与跳转表：[`engine/sht/th16/03`](../../../engine/sht/th16/03-th16-funcstar-jumptables.md)、
  [`04`](../../../engine/sht/th16/04-th16-shot-runtime-architecture.md)
- 伤害管线：[`engine/sht/th16/08`](../../../engine/sht/th16/08-th16-player-damage-pipeline.md)
- 引擎数学（atan2 / FPU 约定）：[`engine/_shared/math-and-prng.md`](../../../engine/_shared/math-and-prng.md)

## 下一步

**游戏内实跑**：汇编后先比对反汇编，再验证崩溃 / 坐标 / FPU 栈平衡。
实跑结果回写 [`AUDIT.md`](AUDIT.md)。
