# engine/anm/th16 — TH16 ANM(动画/精灵显示)系统逆向(新会话入口)
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> 本文件夹专做一件事:**逆向 TH16《鬼形兽》(`th16.exe`,imagebase `0x400000`)的 ANM 系统**——
> 即 `.anm` 资源里的精灵/动画**脚本 VM** 与运行时。东方里几乎所有"显示"都挂在 .anm 上:
> 创建一个显示对象 = 选一段 anm 脚本/模板,由 anm VM 每帧驱动其精灵、变换、特效。
> 你若是**新 Claude 会话**,读完这份 README 应知道目标、规范、环境、和**从哪一刀切**。
>
> ⚠️ **现状**:本文件夹**尚未系统开工**。下面的"起步锚点"是做弹幕引擎(`engine/bullet/th16/`)时
> **顺带掀开的口子**,多为 🟡(call-site/二手),**动手时务必自己反编译复核**,别当定论。

## 目标

1. **显示对象生命周期**:create / pool / 模板克隆 / 释放(已找到入口,见 §A)。
2. **ANM 脚本 VM**:谁每帧执行 anm 脚本?opcode 表(设精灵/UV/缩放/旋转/颜色/插值/跳转/等待)→ 行为。
   疑入口 `0x491b0c` 行为派发表 + 对象 `+0x5d0` 行为 idx(见 §B)。
3. **格式 ↔ 运行时**:把运行时 opcode 与 `thanm.exe`(thtk)反汇编出的 ANM 指令号/语义对应,
   做出"指令号→行为"表(社区有 thanm 可交叉印证,见 §社区)。
4. 回填THTK-Studio 仓库的 `docs/`(将来 IDE 的 ANM 支持)与可能的 THTK-Studio 仓库的 `src-tauri/src/modules/anm/`。

## 规范(与父工作区一致)

- **五段证据链**(发现→推测→验证→结论(可信度+版本)→证据(地址/出处)):`METHOD.md`。
- **可信度** ✅一手实证 / 🟡单源或推断 / ❓存疑;**结论必注版本(仅 TH16,勿外推)**。
- **防过拟合**:派子 agent 命名给中立判据、不喂标签;"反超社区"自创结论按四闸门复核;验证前往下取整标 🟡。
  (memory `re-overclaim-guard` / `re-agent-no-hypothesis-priming` / `re-evidence-chain-discipline` / `re-workflow-fanout-cost`)
- **主仓库不留版权字节**:游戏 exe / .anm 资产 / 大段反编译原文一律 gitignore;只提交脚本 + markdown。
- **★ 二手 label 不算数**:`DAT_004c0f48`="图形/特效管理器"、`0x491b0c`="特效行为表"这些都来自旧文档
  (`engine/sht/th16/04/06`),**做 ANM 时要亲自反到 draw/blit 与 anm 解释器坐实**,别沿用。

## 环境(已就绪,与父工作区共用)

- **ghidra-re MCP**,database `th16`(已分析,含 SHT/数学/弹幕模块命名)。MCP `rename_function`+`set_comment`+`save_database` 跨会话可靠落盘;数据符号真改名仍走 headless `apply_*.py`。
- 常量/字节可直读 PE:`local/th16.v1.00a/th16.exe`(解析节表算 VA→文件偏移)。
- 落盘:命名脚本 `tooling/ghidra/scripts/apply_th16_anm_names.py`(建议,待建);文档 → 本文件夹。

## 起步锚点(starting points)— 均来自 `engine/bullet/th16/01-core-engine.md` §1.3 † 脚注(2026-06-09 一手反编译)

### A. ★ 显示对象创建链(✅ 已一手反到三级)
- `FUN_0040e5c0(mgr, &out_handle, id, &xyz, p4, layer)`:创建一个显示实体并返回**句柄**。
  **50 处跨子系统调用**(player/敌人/道具/特效/弹幕)= 通用 helper。
- → 分配器 `FUN_0046f600()`:entry **取自 `DAT_004c0f48` 对象池**(空闲链 `DAT_004c0f48+0x184f4e0`),
  空则 `operator_new(0x5fc)`;entry 回指 `DAT_004c0f48`(`+0x568/+0x578/+0x588`)。
- → 初始化 `FUN_00407b20(mgr, entry, id)`:`id` **索引** `mgr+0x10c` 处**步长 `0x5fc` 的模板数组**,
  把 `模板[id]`(0x14e dword=0x538 字节)**整块拷进新 entry** = "按 id 克隆一个预加载显示模板(.anm 槽)"。
- **第一刀**:反 `mgr+0x10c` 模板数组的**装载**端(谁把 .anm 解析进这个数组?stride 0x5fc 模板的字段布局?),
  和 `FUN_0046e7d0`(注册/句柄分配)。这能解出"显示对象/anm 槽"的对象模型。

### B. ANM 事件回调表族(2026-06-09;th-re-data + dump + 反编译)

> ⚠️ **更正(2026-09-09,[`02-vm-core.md`](02-vm-core.md) §3.5)**:下面那条"`0x491b0c` = `ANM_ON_SWITCH_FUNCS`"
> **由 ✅ 下调为 🟡** —— `on_switch` 这个名字在全 exe 找不到调用点支撑。
> 一手看全部 **32 个引用该表的函数**,抽查 `0x46f0c8`/`0x46f14e`(`AnmManager::interrupt_tree(_and_run)`)与
> `0x44b843`–`0x44b863`(`MainMenu::do_title_screen`),形态**完全一致**:
> `ECX = vm; EDX = <中断号>; CALL [0x491b0c + vm[+0x5d0]*4]`,**紧接着** `vm[+0x490] = 同一个中断号`
> (例:`0x44b849` 写 `EDX = 0x1f`,`0x44b863` 写 `[EAX+0x490] = 0x1f`,再 `0x44b86d` 调 `AnmVm::run`)。
> 而 `vm+0x490` 已一手证实是 **pending_interrupt**。**所以在这些调用点它是 `ANM_ON_INTERRUPT_FUNCS`** ✅。
> 表项本身(`0x407900`/`0x405f20`/`0x406920`)干了什么仍如下述,只是**它们是被 interrupt 触发的**。

- 🟡 **`0x491b0c` 旧名 `ANM_ON_SWITCH_FUNCS`**(`void*[4]`={null,`0x407900`,`0x405f20`,`0x406920`}=`AnmVm::on_switch__1/2/3`)。反 `0x407900`:操作 AnmVm 顶点数组 `+0x5b8`(4 顶点)、设渲染/混合模式(`+0x534`、`+0x18`=0x17/0x24/0x1e)、写颜色/UV 常量 = **ANM 精灵渲染状态切换**。索引 = ANM 对象 `+0x5d0`(switch 类型,0=无→对应调用方 `if(idx!=0)` 守卫)。
- **整族 ANM 事件回调表**(ExpHP th-re-data,相邻):`0x491b0c` on_switch[4] · `0x491b1c` on_sprite_set[4](`AnmVm::on_sprite_set__*`)· `0x491b2c` on_draw[7](`AnmVm::on_draw__*`)· `0x491b48`/`0x491b50` on_copy[2]×2 · `0x491b58` on_delete[4]。→ **这是 ANM VM 的事件分派骨架,逐表反 entry 即得各事件语义。**
- 消费者 `FUN_0044f810`/`FUN_0044c8c0` = ANM 管理器(`DAT_004c0f48`)每帧渲染更新器;`0x46efa0`=`AnmManager::get_vm_with_id`(句柄→AnmVm)。
- ⚠️ **更正**:`0x491b0c` **不是** SHT 特效/敌弹派发(旧 `engine/sht/th16/04`/`06` 误标,已修);也不是 anm**脚本步进**(那是另找 anm bytecode VM,见 §C),而是**事件回调**。
- **要做的**:逐表反 entry 命名各 ANM 事件 handler;另找 ANM **脚本字节码** VM(精灵动画指令流)入口,与本事件表区分。

### C. 其他线索
- 句柄→对象 `FUN_0046efa0(mgr, handle)`、释放 `FUN_0046f0b0(handle, mode)`(旧文档,🟡)。
- entry 里 `0xfff0bdc1`/`0xffffffff` 等被反复初始化的块 = **插值器状态**(颜色/位置/缩放的 lerp,anm 典型),`FUN_00407a50` 清。

## 社区对照(关键:有 thanm 可交叉印证)

- **`thanm.exe`**(thtk,`thpatch/thtk`):ANM 的反汇编/重编译工具,**有完整 opcode 表与格式定义**(C 源)。
  这是把"运行时 opcode"对到"格式指令号"的**权威外部锚点**——逆向时**先看 thanm 的 ins_ 表**,再去 exe 核对实现。
- 速查见 `engine/_shared/community-sources.md`;克隆参考仓库走 `vendor/`(gitignore)。
- ⚠️ thanm 给的是**格式**语义;**运行时**实现(具体哪个 float 常量、哪条 SSE 指令)仍须在 th16.exe 自证。

## 计划产出

- ✅ **已出**:[`01-format.md`](01-format.md) —— `.anm` v8 文件格式（entry/sprite/script/指令头/THTX/format 表），
  一手加载路径 `0x46cdd0` → `0x46d1c0` → `0x46d3b0`；配套命名单 [`naming-batch.md`](naming-batch.md)。
- ✅ **已出**:[`02-vm-core.md`](02-vm-core.md) —— VM 骨架(`AnmVm::run` `0x45f980` 主循环、`zAnmVm` 字段图、
  时间闸门与 `GAME_SPEED`、interrupt 查找、`delete`/`static`/`stop` 终止语义、三段式分发表)。
- ✅ **已出**:[`03-math-jumps.md`](03-math-jumps.md) —— opcode **100–131 / 200–213** 共 46 条
  (整/浮算术、随机、sin/cos/tan/acos/atan、角度归一、极坐标、14 条跳转)的签名与语义，
  含四个专项:变量槽编号与编码、除零/取模边界、跳转目标是字节偏移、跳转后 `time` 怎么设。
  **这一篇同时是后续 opcode 分组文档的格式范例**(表 + 专项 + 社区裁决 + 真实数据对账 + 播放器的坑)。
- ✅ **已出**:[`04-variables.md`](04-variables.md) —— 脚本变量号 **10000–10035** 共 36 条逐条定死
  (语义/类型/可写性/vm 字段落点/读侧 case 地址/随机族 PRNG 流)，可写 22 / 只读 14 的一手划界，
  含四个专项:可写性穷举、跨类型读非对称、随机族流划界、helper 变量(相机/有效旋转)。
- ✅ **已出**:[`05-system.md`](05-system.md) —— opcode **0–7** 共 8 条(nop/delete/static/stop/
  stopHide/case/wait/caseReturn),含终止语义与 interrupt 标签扫描。
- ✅ **已出**:[`06-settings.md`](06-settings.md) —— opcode **300–317** 共 18 条(sprite/blendMode/
  layer/renderMode/scrollMode/resampleMode…),含三条 `wip:2` 的裁决与 `flags_lo`/`flags_hi` 位归属。
- ✅ **已出**:[`07-general.md`](07-general.md) —— opcode **400–438** 共 39 条(变换与外观层),
  含 12 组帧尾插值器的结构图、32 条缓动曲线公式、求值次序表。
- ✅ **已出**:[`08-child.md`](08-child.md) —— opcode **500–509** 共 10 条(建子/建根/继承开关/
  拷父变量/effect 表),含父子坐标继承与 `create_managed_child` vs `create_managed_root` 的分野。
- ✅ **已出**:[`09-drawing.md`](09-drawing.md) —— opcode **600–613** 共 14 条 + **render state 一手扫描**
  (blend 10 模式 / alpha test / texture stage / FVF),含帧尾几何 `0x4632f0` 三族的完整公式
  (mode 9 整圆环、13/14 圆环弧、24/25 3D 圆柱与平面圆环)。
- ✅ **已出**:[`10-sprite-geometry.md`](10-sprite-geometry.md) —— **`draw_vm` 的 sprite 四边形路径**
  (mode 0/1/2/3):锚点两张表、位置公式(含旋转)、UV 的 `uv_scale` 子矩形、`423 colorMode` 的四种
  平涂/渐变。**这是播放器真正要画的那件事**——真实数据里只有 4.7% 的 script 用 Drawing 组。
- 待做:`draw_vm` 的 **mode 4 / 8 / 15**(走 `0x467410`,mode 8 真实用量 196 次)、
  rect 族(`0x468722`)与 poly 族(`0x4689b3`)的顶点计算;以及显示对象模型(创建/池/模板/释放)。

> ⚠️ **新增分组文档时,记得回来把它加进上面这份清单。**
> `tooling/check-docs.py --orphans` **查不出**这类遗漏——分组文档与 [`naming-batch.md`](naming-batch.md)
> 互相链接就已经"有入链"了(自满足的交叉链接),但从 README / OVERVIEW 仍然走不到它。
> 收口任务请**显式对照本清单**,不要依赖 `--orphans`。
- `tooling/ghidra/scripts/apply_th16_anm_names.py`(函数/数据符号/注释,headless 可复现)。
- 稳定后回填 THTK-Studio 仓库的 `docs/`。

## 关联

- ★ 锚点来源与证据链:`engine/bullet/th16/01-core-engine.md` §1.3 † 脚注(`0x40e5c0`→`0x46f600`→`0x407b20` 三级)。
- 弹幕引擎(gameplay ECL VM,与 anm 是**两套独立 VM**):`engine/bullet/th16/README.md`、`engine/bullet/th16/01-core-engine.md`。
- 旧切口(二手,待复核):`engine/sht/th16/06-th16-engine-incisions.md §6`(图形)、`engine/sht/th16/04 §2`(DAT_004c0f48)。
- 纪律:`METHOD.md`;memory `re-overclaim-guard` / `re-agent-no-hypothesis-priming` / `re-evidence-chain-discipline` / `re-workflow-fanout-cost`。
</content>
