# engine/bullet/th16 — TH16 弹幕(敌弹)引擎逆向(新会话入口)
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> 本文件夹专做一件事:**逆向 TH16《鬼形兽》(th16.exe,imagebase `0x400000`)的敌弹/弹幕引擎**。
> 你若是**新 Claude 会话**、没有上文,读完这份 README 应知道目标、规范、环境、和**从哪一刀切**。
> 做法与已完成的「数学模块」(`engine/_shared/math-and-prng.md`)一样:先 inline 钉锚点 → 必要时 Workflow 扇出 → 主控一手复核 → 落文档 + 命名脚本。

## 目标(两部分)

1. **核心引擎** → `01-core-engine.md`(✅ **首轮已成,2026-06-09**)
   - 身份坐实=**敌弹池**(接触伤自机 `0x4438c0`/`0x4439e0`/`0x443f10` 一手验);管理器 `DAT_004a6dac`(≠敌人 `DAT_004a6dc0`),2×2001 槽 stride 0x1478 内嵌池。
   - 每颗弹自带**运动字节码 VM**(`0x413860`,程序 `+0xc88` stride 0x2c,opcode 全表已解)+ `+0xc68` 行为位场(10 个 handler:加速/转向/反弹/趋点/夹取…)。
   - spawn `0x412cb0`、字段图、字节码源(疑 ECL)均已录;详见该文档。
   - **`02-bullet-vm-model.md`**(✅ 已写):**机制/概念解释**——为什么一颗弹是携带"程序+多个并发 etEx 效果"的微型 VM 上下文,而非扁平 struct;附设计评价(§7)。
   - **`03-lasers.md`**(✅ 已写):**激光子系统(EX_LASER)**——5 个 RTTI 激光类、线/无限差异、**旋转 OBB 碰撞**(主控亲验),曲线/beam 激光待挖。
   - **覆盖现状(2026-06-09)**:核心弹运行时 + VM + 全 handler + etEx 参数(含阈值分支二次验证)+ 擦弹 + 激光线/无限 = 闭环。**剩**:曲线/beam 激光生成路径、EX_REACT 消费者(疑 ECL)、弹的 ANM 显示半(→`engine/anm/th16/`)、真机 ground-truth(留 Windows)。详见 `01` §7/§8.2、`03` §8。
2. **ECL 接口** → 已独立成 **`engine/ecl/th16/`**(文件夹 + README + 起步锚点,待系统开工)
   - 哪些 **ECL opcode** 调 spawn/控制;参数如何映射到弹字段。接缝已钉:`FUN_00412cb0`←`FUN_00414da0`←开火点 `FUN_0041dcb0`/`FUN_00431fe0`/`FUN_00438cb0`;ECL opcode 表 `0x4921b4`。

## 规范(与父工作区一致,务必遵守)

- **五段证据链**(发现→推测→验证→结论(可信度+版本)→证据(地址/出处)):见 `METHOD.md`。
- **可信度分级** ✅一手实证 / 🟡单源或推断 / ❓存疑;**结论必注版本(仅 TH16,勿外推)**。
- **防过拟合纪律**:派子 agent 命名时给中立判据、不喂标签;"反超社区"的自创结论按四闸门复核;验证前往下取整标 🟡。
  (memory `re-overclaim-guard` / `re-agent-no-hypothesis-priming` / `re-evidence-chain-discipline`)
- **主仓库不留版权字节**:游戏 exe / 资产 / 大段反编译原文一律 gitignore;只提交脚本 + markdown 结论。
- **落盘**:命名 → 版本化脚本 `tooling/ghidra/scripts/apply_th16_bullet_names.py`(可复现 source of truth);文档 → 本文件夹。

## 环境(已就绪)

- **ghidra-re MCP**,database `th16`(已分析,**已含 SHT + 数学模块的命名**)。
- **★ MCP save 已修复**(txfix 补丁):`rename_function` + `set_comment` + `save_database` 跨 close/重开**可靠落盘**(本人实测 PASS,见 memory `ghidra-mcp-save-broken`)。
  - ⚠️ **但无数据符号重命名工具**:全局变量(`DAT_xxxx`)只能 `set_comment` 加注释,**真改名仍须 headless 跑 `apply_*.py` 脚本**。
- **常量/字节可直读 PE**:`local/th16.v1.00a/th16.exe`,解析节表算 VA→文件偏移(坑:节头字段从 `o+8` 读,见 memory `re-read-consts-from-pe`)。
- **数学模块已完成** → `engine/_shared/math-and-prng.md`,提供**子弹移动的底层原语**,直接复用:
  - 运动积分器 `motion_update_mode 0x402ff0` / `motion_update_mode_full 0x403110`(按 `obj+0x40&0xf` 模式每帧推进位置/旋转)。
  - 极坐标→速度 `math_set_velocity_polar 0x430df0`(角+速→vx/vy)。
  - PRNG `prng_gameplay_draw 0x402be0` + float 包装(角抖/散布);角度归一化族。

## 起步锚点(starting points)

### A. ★ 最佳入口:玩家碰撞 `0x4439e0`(本会话已确认 ✅)
- `FUN_004439e0(float* obj_xy, int mode)` = **玩家命中/擦弹判定**:算自机(`DAT_004a6ef8+0x610/+0x614`)到传入对象坐标的平方距离,比对半径(自机判定半径来自 `+0x2c788` 的 sht hitbox;**对象自身半径由 XMM2 传入**);返回 **1=死 / 2=擦弹 / 0=未中**;命中且无敌时调 `FUN_00443f10`(死亡)。
- **第一刀**:`get_xrefs_to 0x4439e0` → 调用方就是「拿每颗弹(及激光)去撞自机」的循环 = **敌弹池遍历器**。顺它即可定位:**敌弹池全局 + 弹结构(坐标/速度/半径/行为idx)+ 每帧更新器**。这是通往整个核心引擎最短的一根线。

### B. ⚠️ 不要直接把 `0x491b0c` 当弹幕入口(本会话已证伪一半)
- `effect_behavior_dispatch_table 0x491b0c`(115 xref)**至少有一部分是视觉特效**:其消费者 `FUN_0044f810` 是遍历**特效管理器 `DAT_004c0f48`**、更新一批固定布局特效对象(HUD/光环/option 视觉,`obj+0x5d0`=行为idx、`obj+0x490` 置 0x1e/0x1f)的**视觉更新器**,不是敌弹。
- 结论:`0x491b0c` 是**特效**行为表;敌弹是否也复用它**未证**。别据 04/06 旧标注("敌弹/特效")默认它=弹幕。以 §A 的碰撞锚点为准更稳。
- ✅ **补正(2026-09-09)**:这张表就是 **ANM VM 的 on_interrupt 回调表**,不是"特效行为表"。
  **本条自己记下的旁证正是铁证** —— 上面写的"`obj+0x5d0`=行为idx、`obj+0x490` 置 0x1e/0x1f":
  `obj+0x490` 已一手证实是 ANM VM 的 **pending_interrupt**,而 `0x1e`/`0x1f` 就是被投递的中断号
  (如 `th16:0x44b849` 置 `EDX = 0x1f` → 调表项 → `th16:0x44b863` 写 `[obj+0x490] = 0x1f`)。
  所以那批"固定布局特效对象"其实就是**受管的 ANM VM**。详见 `engine/anm/th16/02-vm-core.md` §3.5 / §5。

### C. 其他锚点 / 线索(未证,留查)
- **ECL 接口**:`FUN_00424110` 一带是 **ECL opcode 的 switch 派发区**(数学模块发现它派发 random opcode);找其中**弹幕 spawn opcode** → 核心 spawn API。`FUN_0044c8c0`(1369 指令,被 `FUN_0044c570` 调 3 次)是大候选,疑 ECL VM 或弹批量更新,待查。
- **同构模板**:自机弹系统(`engine/sht/th16/03`/`04`)已完整解出——slot(stride 0xe4)、池(player+0xd080 stride 0x94 ×256)、`func_on_tick` 派发 `0x442380`、命中派发 `0x445d40`。**敌弹大概率平行结构**,可逐项对照。
- **反向找移动器**:`get_xrefs_to 0x402ff0 / 0x403110`(运动积分器)的非自机调用方(如 `0x41bb50`/`0x410550`/`0x41c1f0`)是候选弹/对象更新器。
- 相关管理器全局:`DAT_004a6dc0`(敌人管理器,敌弹多由敌人/ECL 发出)、`DAT_004c0f48`(特效)、`DAT_004a6ef8`(player,内含自机弹池)。

## 计划产出

- `01-core-engine.md`、`02-ecl-interface.md`(本文件夹,五段链 + 可信度)。
- `tooling/ghidra/scripts/apply_th16_bullet_names.py`(函数名/数据符号名/注释,headless 可复现;函数名亦可经 MCP 直接 rename+save)。
- 稳定后回填THTK-Studio 仓库的 `docs/`(驱动 IDE 的弹幕/ECL 实现)。

## 关联

- `engine/sht/th16/04-th16-shot-runtime-architecture.md` — 自机弹运行时架构(敌弹的**同构模板**)。
- `engine/sht/th16/06-th16-engine-incisions.md` — 引擎切口地图(§2/§6 子弹·特效线索;注意 §中"敌弹/特效"标注已被本 README §B 修正)。
- `engine/_shared/math-and-prng.md` — 子弹移动/散布的数学原语。
- 纪律:`METHOD.md`;memory `re-overclaim-guard` / `re-agent-no-hypothesis-priming` / `ghidra-mcp-save-broken` / `re-read-consts-from-pe`。
