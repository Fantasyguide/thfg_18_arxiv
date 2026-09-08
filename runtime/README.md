# runtime — 引擎知识的可执行形式

`runtime/` 是本仓库的第四层：把 `engine/` 里已经验过的引擎语义，实现成一个真的能跑起来的
播放器/模拟器，用**行为对不对得上**去反向证伪那些语义。第一个入住的子项目是
[`anm/`](anm/OVERVIEW.md)（ANM v8 精灵动画播放器）。

## 四层分工

> `engine/` 引擎怎么工作 · `runtime/` 同一份知识的可执行形式 · `mods/` 我改了什么 ·
> `tooling/` 怎么再来一遍
>
> - `runtime/` 里每条实现必须能指回 `engine/` 的一条断言。指不回去 = 你在猜，代码里标 🟡。
> - `runtime/` 跑出来的差异回流 `engine/` 当新问题。播放器是证伪装置。
> - `runtime/` **不放引擎结论**。

也就是说：`runtime/` 里的代码注释可以写"实现了 `engine/anm/th16/xxx.md` §2 的这条断言"，
但不能反过来在 `runtime/` 里第一次断言"th16 的这个 opcode 是干什么的"——那种结论要先落在
`engine/anm/`，`runtime/` 只负责照着实现、再拿实际运行结果去戳它。

## 子项目

| 子项目 | 是什么 | 门面 |
| --- | --- | --- |
| [`anm/`](anm/OVERVIEW.md) | ANM v8 精灵动画播放器（Node，零依赖） | [`anm/OVERVIEW.md`](anm/OVERVIEW.md) |

## 只读的 th16 Ghidra 快照（波 D 并行用）

Ghidra 工程锁是工程级独占的（MCP 开库 / GUI 开工程 / driver 在跑，三者同一时刻只能有一个，
见 [`docs/SETUP.md`](../docs/SETUP.md) 的「锁」）。为了让后续多个 R 任务能并行只读地翻 th16 的
反编译/反汇编/交叉引用/调用图/结构体/字符串/函数表而不互相撞锁，T0 已经用
`tooling/ghidra/export_html.py` 做过一次性 HTML 导出（占锁几分钟，导出完之后随便翻都不占锁）：

- **导出目录**：`local/th16.v1.00a/state/`（gitignored，不入库；产物本身不含版权字节，
  只是反编译文本/函数表/结构体的静态快照）
- **模型冷启动读**：`local/th16.v1.00a/state/data/functions.jsonl`
- **人看**：`tooling/ghidra/serve.sh th16 <端口>` 起一个只绑 `127.0.0.1` 的 http.server
  （默认端口 6090 在这台机器上已被 th18 的导出占用，T0 起 th16 时用了 **6091**；
  下次起的时候先 `ss -tlnp | grep <端口>` 看看有没有别人占着）

**并行的 R 任务一律读这份 HTML 快照，不碰 MCP。** 这是硬规则。需要写库的动作（重命名函数、
建结构体、加 label）必须串行化：写一份 `naming-batch.md` 交回，由统一的任务用 MCP 落库，
再 `tooling/ghidra/symbols.py export th16` 导出——不导出就等于没有。
