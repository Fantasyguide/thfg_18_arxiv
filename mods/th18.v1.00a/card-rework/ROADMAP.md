# TH18 卡牌改造协作指南

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 目标：让协作者以**合法自有的 TH18 v1.00a**为样本，通过 thcrap 的运行时加载/内存补丁能力，做可复现、可审计的卡牌系统实验。
> 本文不是可直接分发的 TH18 DLL 或补丁。仓库不含游戏二进制、Ghidra 项目或可复用的 TH18 注入地址。

## 现有材料能支持什么

卡牌系统的**架构、注册表、主要 vtable 接缝、商店/资源路径和 58 张卡的效果目录**已足以支持“从已有卡切入”的定点研究与设计讨论；它们均限定于 **TH18 v1.00a**，并标注一手/社区证据与未决项。

**运行时底座已不再是未知数**（2026-09-01）：[`mods/th18.v1.00a/mouse-control`](../mouse-control/README.md)
是首个在游戏内实跑通过的 TH18 注入产物（thcrap 断点 + 自建 DLL），它验通了 Linux 交叉编译 →
Windows 的交付链路、断点 ABI、以及版本守卫。平台侧的通用结论见
[`mods/thcrap-platform.md`](../../thcrap-platform.md)。

仍**没有**的是针对卡牌系统的注入补丁。完整卡牌重做要在这个底座上逐阶段建。

加载层优先使用 thcrap；“DLL 注入”在这里是 thcrap 所采用的运行时加载机制。除非任务本身研究加载器，协作者无需另写裸 loader。

## 版本与边界

- 仅以 **TH18 v1.00a** 为事实目标。地址、`expected` 字节和结构偏移均绑定该版本；不匹配时必须停止，不能强行应用。
- 仅使用协作者自己合法持有的 `th18.exe`。二进制、解包资产、Ghidra 数据库、反编译全文不得提交。
- 补丁只分发 thcrap 元数据、源码/汇编、预期原字节和文档；不分发游戏字节。
- 不把 TH16 地址、ABI 或机器码套到 TH18。TH16 材料只证明方法和审计流程可行。

## 最终目标与实施路线

目标是同时做到：替换原生卡的效果、定义新的卡牌运行时行为，并让新卡进入获取、显示、存档和回放链路。建议按下列阶段推进；每一阶段都产出可独立验证的补丁，避免把新的注册表、UI、序列化和玩法逻辑一次性混在同一个 codecave 里。

| 阶段 | 目标 | 已有依据 | 尚需确认/实现 |
| --- | --- | --- |
| 0. 运行时底座 | 确认 thcrap 能对 TH18 v1.00a 安全加载、写入并卸载最小观测逻辑。 | TH16 thcrap 文档/审计。 | TH18 目标点、原字节、ABI 与游戏内实跑。 |
| 1. 替换原生卡 | 将一个既有卡的效果转交给我们的逻辑；先主动卡，再被动/装备卡。 | `engine/card/th18/03-hooks.md` 的 vtable 调用点全表、`engine/card/th18/08-catalog.md` 的逐卡目录。 | 每个目标函数的 TH18 机器码级复核和稳定的自定义状态管理。 |
| 2. 自定义卡运行时 | 用自定义对象/vtable 或统一 dispatcher 承载新行为，并与 AbilityManager 卡链表兼容。 | 基类 `zCardBaseClass`、22 槽 `zVTableCard`、链表和分类 flags 已定位。 | 自定义对象内存布局/析构、完整必需 vtable 槽、与 HUD/option 的生命周期。 |
| 3. 注册与获得 | 让新 ID 被查找、分配、商店/掉落逻辑选择，并配置价格、权重和关卡可用性。 | `zTableCardData[]`、`allocate_new_card`、商店筛选/购买路径已定位。 | 静态表的迭代边界、未知 ID 的 allocator fallback、所有 ID 范围检查与掉落/图鉴入口。 |
| 4. 呈现与持久化 | 显示名称/图标/说明，支持初始卡组、解锁、存档与 replay。 | 表中 sprite 字段；存档和 replay 都保存 card-id 字节数组。 | 文本/ANM 资源来源与加载路径、存档/解锁数组的可扩范围和完整 UI 消费点。 |

**关键判断：**新增卡不能只“往 `zTableCardData[]` 后面写一项”。原生表是静态的，现有 `allocate_new_card` 的 ID→类分派、商店的表遍历、UI、解锁位、存档/replay 都可能各自假设原生卡集合。阶段 2–4 必须以同一套新 ID/元数据模型一起设计和验证。

阶段 1 仍是必要起点：它用于验证 TH18 的加载方式、调用约定和回滚机制，不是最终功能的限制。最适合作为第一个垂直切片的是“替换一张主动卡的效果，同时保留原生 ID、图标和商店入口”；成功后再把同一套 dispatcher 扩展到新 ID。

## 新增卡前必须补齐的研究

**这一节的前四条已在 2026-09-01 调研完成**，结论见
[`engine/card/th18/10-extensibility-limits.md`](../../../engine/card/th18/10-extensibility-limits.md)
（12 处硬边界全表 + 三条路线判据）。要点：

- `allocate_new_card` **没有 default 分支**——`cmp ebx,0x38; ja` 在跳转表之前就把未知 id 挡掉，返回 -1。
- 表边界是**代码里的绝对地址立即数**（`0x4c5f8c` / `0x4c5f88`），不是读表算的。
- 卡牌文案来自归档里的 **`ability.txt`**（`0x4160b0` 解析，按内部名查表，每卡 7 行 × `0x40`）——
  文案侧是 thcrap 的主场，几乎没有障碍。
- 真正的拦路虎是**按 card_id 索引的数组几乎没有余量**，而其中两个在 `zScoreFile` 里
  → **动它就动存档格式**（还有一份 backup 副本）。

**id 56/57 哨兵能否让出，也已查完：不能**——见
[`engine/card/th18/11-sentinels-56-57.md`](../../../engine/card/th18/11-sentinels-56-57.md)。
id 56 是全部内联查表的回退行兼卡组编成的「空槽」伪卡，id 57 是图鉴的卡背图且分配器不放行。
具体到「把 id 上限抬到 255」怎么做，见 [`PLAN-255-ids.md`](PLAN-255-ids.md)
（五条战线 + 保证「干净」的九条手段 + 建议推进顺序）。
**进度（2026-09-02）**：战线 A、B 实跑通过，C 已发布待实跑，实现在
[`../card-expand/`](../card-expand/README.md)；下一步 D 的交接页是 [`../card-expand/NEXT.md`](../card-expand/NEXT.md)。

**路线 B 出局，只剩「换皮」和「整表搬迁」**；后者的机械工价 = 只搬表 99 处 / 9 个函数，
全部改完 132 处 / 15 个函数。而且 `id 56` 是**写进存档**的空槽字面值
（`memset(初始卡组, 56, 48)`），所以动它连带动所有现有存档。

仍待补：

- 三个卡牌 ANM 的 sprite 索引空间余量（图标资产管线）。
- 装备卡的 shooter 数据来源；若新卡要发射新弹型，
  [`engine/card/th18/OPEN-questions.md`](../../../engine/card/th18/OPEN-questions.md) §1 是硬性前置。

原始社区素材在 `engine/card/th18/_sources/`，是素材不是结论，别直接抄。标为 🟡/⏳ 的字段可成为专项研究任务，但不能作为未经验证的新卡框架前提。

## 开工顺序

1. 读 [README.md](../../../games/th18.v1.00a/INDEX.md) 和 [findings/README.md](../../../engine/card/th18/README.md)，确认样本和证据纪律。
2. 按任务阅读：钩子全表 [03-hooks.md](../../../engine/card/th18/03-hooks.md)，卡效果 [08-catalog.md](../../../engine/card/th18/08-catalog.md)，商店 [05-shop-and-money.md](../../../engine/card/th18/05-shop-and-money.md)。
3. 在本地、忽略的 `local/th18.v1.00a/` 中打开 TH18 数据库；以函数语义和控制流复核目标点，记录 EXE 的版本/哈希和原始字节。
4. 用 thcrap patch 组织实验：逐版本脚本、`expected` 原字节校验、最小 codecave/binhack；地址只写入该版本文件。
5. 先做无行为改动的加载验证，再一次只加入一个行为变化；保留可还原补丁和游戏内验证记录。
6. 把已验证的地址、原字节、调用约定、测试步骤和结果写回 `findings/`；猜测只能留在开放问题。

## 可复用资料（仅流程，不是 TH18 地址）

- [mods/th16.v1.00a/tracking-laser/thcrap_patch.md](../../th16.v1.00a/tracking-laser/patch/thcrap_patch.md)：thcrap patch 目录、`binhack`/`codecave`、`expected` 校验及相对 `[…]`、绝对 `<…>` 表达式。
- [mods/th16.v1.00a/tracking-laser/NOTES.md](../../th16.v1.00a/tracking-laser/AUDIT.md)：ABI/栈平衡审计；按目标函数 `RET` 和游戏调用点确认调用约定。
- [tooling/ghidra/README.md](../../../tooling/ghidra/README.md)：本地 Ghidra/PyGhidra 环境和样本边界。
- [METHOD.md](../../../METHOD.md)：可复查结论的证据标准。

TH16 `test-laser` 尚未完成游戏内实跑，因此它是**经过静态审计的流程参考**，不是可照搬到 TH18 的生产模板。

## 合作者交付清单

- 目标版本和 EXE 哈希/版本识别方式；
- 目标卡、文档依据和目标函数的本地复核证据；
- patch 源文件（不含游戏文件）及每个写入点的 `expected` 原字节；
- codecave 的调用约定、保存/恢复的寄存器和栈说明；
- 最小复现步骤、预期游戏表现、实际结果及失败/崩溃记录；
- 对 `engine/card/th18/` 的增补：结论、置信度、适用版本和未验证假设。
