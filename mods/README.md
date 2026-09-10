# mods/ — 产物层

> **动手前先读 [`thcrap-platform.md`](thcrap-platform.md)**：改动住在 patch 还是 DLL、
> asm 怎么变成 codecave、怎么分发。选错路线的代价比写错代码大。
> 然后读 [`LESSONS.md`](LESSONS.md)：card-expand 一线踩过的坑（ABI、对象模型、时序、自检、崩溃日志怎么读）。

**版本为主，死绑 build。** 一个 mod 的每个写入点都是「某个 exe 的某个地址上的某几个字节」——
换一个 build 就全部作废。所以目录第一层是版本，不是 mod 名：

```
mods/<版本>/<mod名>/
  README.md      这是什么 / 怎么装 / 版本约束
  TARGET.md      ★ 死绑登记:exe 哈希 + 每个写入点的 addr / expected / 调用约定
  native/        cave 源码(.asm / .c)或插件 DLL 源码(构建产物不入库)
  assets/        成品脚本资产(.ecl / .anm / .sht) —— 用 THTK-Studio 编辑,这里存源
  patch/         thcrap patch 目录(patch.js / files.js / <版本>.js)
  AUDIT.md       对抗审计记录
```

## 为什么 `TARGET.md` 是一等公民

没有它的话，「这个 mod 绑死了什么」会散落在 patch 的 `expected` 字段和 AUDIT 的行文里。
提成一份显式登记后，**换版本时一眼知道要重取哪些量**。新建 mod 时从
[`_template/TARGET.md`](_template/TARGET.md) 拷一份填。

## 纪律

- **只分发 thcrap 元数据、源码/汇编、预期原字节和文档；不分发游戏字节。**
- 地址只写入该版本文件；`expected` 不匹配必须停止，不能强行应用。
- **不把某作的地址/ABI/机器码套到另一作。** 跨作只能借方法论。
- ★ 涉及手写机器码 / ABI 的产出，**必须过
  [`_template/AUDIT-checklist.md`](_template/AUDIT-checklist.md)**——
  这不是形式主义，见下。
- 加载层统一用 thcrap（它的运行时加载机制就是 DLL 注入），除非任务本身研究加载器。
- **默认把改动写进 patch，不写 DLL**——社区实证：ExpHP 的 17 个玩法补丁零 DLL。
  要 C 的可调试性时才上 DLL，代价是它走不了 thcrap 的包管理（只能手动放 `<thcrap>/bin`）——
  这一层由发布仓库 [`th18_modkit`](https://github.com/Renko6626/th18_modkit) 的启动器接管：
  `mods/*.dll` 勾选式装进 `bin/`。**开发在 renkolab，发布进 modkit**，见
  [`th18.v1.00a/card-expand/README.md`](th18.v1.00a/card-expand/README.md) 的「发布」。
- **DLL 数量不得随 mod 数量增长。** DLL 只提供**能力**（`BP_*`、工具函数），不承载某个
  具体 mod 的**身份与数据**——地址、`expected`、数值、开关一律住在 patch。
  一问就能验：新加一个 mod，`bin/` 里的文件变多了吗？判据与构建链路见
  [`thcrap-platform.md`](thcrap-platform.md) §3、§5。

## ★ 为什么强制对抗审计

`tracking-laser` 的静态审计抓到过一个**肉眼和单人复核极易漏**的 BLOCKER：
引擎内部大量函数是 **stdcall（callee 清栈，`RET imm`）**，按 cdecl 写了 `add esp` 会抬高 ESP，
`pop esi/edi` 读到栈帧里的局部值 → esi/edi 被破坏 → `tick_bullets` 崩。
逻辑全对、只错栈平衡，靠「默认自己写错、去二进制证伪」的独立 agent 才抓出来。

同一轮还抓到第二个真 BUG：thcrap 的 `[expr]` 是**相对**地址、`<expr>` 才是**绝对**地址，
重指数据指针表槽写成 `[codecave:…]` 会把相对偏移当指针用 → 跳错地址崩。

## 现有 mod

| mod | 目标 | 状态 |
| --- | --- | --- |
| [`th18.v1.00a/mouse-control`](th18.v1.00a/mouse-control/README.md) | 鼠标控制自机 + 左/右/中键映射 | ✅ **实跑通过**(2026-09-01) |
| [`th18.v1.00a/runtime-probe`](th18.v1.00a/runtime-probe/README.md) | 只读探针:汇报玩家坐标与状态位 | 已编译 + 静态审计通过，**未实跑** |
| [`th06nc/mouse-control`](th06nc/mouse-control/README.md) | 鼠标控制自机 + 左/右/中键映射(单 DLL 代理,不走 thcrap) | ✅ **实跑通过**(2026-09-10,VCRUNTIME140_1 代理);坐标需两点标定 |
| [`th18.v1.00a/card-expand`](th18.v1.00a/card-expand/README.md) | 卡表搬迁 / 扩容（战线 A–C） | ✅ A、B **实跑通过**（2026-09-02）；C 已发布待实跑；下一步 D 见 [`NEXT.md`](th18.v1.00a/card-expand/NEXT.md) |
| [`th18.v1.00a/card-rework`](th18.v1.00a/card-rework/ROADMAP.md) | 卡牌系统改造 | 📋 调研与方案，见 [`PLAN-255-ids.md`](th18.v1.00a/card-rework/PLAN-255-ids.md) |
| [`th16.v1.00a/tracking-laser`](th16.v1.00a/tracking-laser/README.md) | 重指 SHT tick 槽注入追踪激光 | 静态审计通过，**未实跑** |
