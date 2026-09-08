# engine/msg/th16 — TH16 MSG(对话/文本)系统逆向
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> **逆向 TH16《鬼形兽》(`th16.exe`,imagebase `0x400000`)的 MSG 系统**:`.msg` 里的对话/文本脚本 VM 与运行时
> ——关卡前后对话、立绘/说话人、逐行文本、等待输入、表情、结局/staff 文本等。东方的"剧情/对话"层挂在 MSG 上。
>
> ## ✅ 状态:本模块已告一段落(2026-06-11)
>
> 关卡内对话 + 结局/staff 两套指令集均已反完并交叉验证。新会话按 `01→02→03→04` 顺序读即可全貌掌握。
> 剩余只有少量低价值收尾项,见下「遗留小遗憾」。

## 成果(读这四篇 + 一个工具)

| 文件 | 内容 | 可信度 |
| --- | --- | --- |
| `01-architecture-overview.md` | 整体架构/数据流/子系统边界/`zGuiMsgVm` 字段图(**先读**) | ✅ |
| `02-msg-vm-opcodes.md` | **stage-MSG opcode 0..35 → 行为表**(一手 `GuiMsgVm::run` × ExpHP truth 签名 × thpages 名/行为 三方交叉)+ 文本 Shift-JIS/加速 XOR 编码 | ✅(余 op27 见下) |
| `03-dialogue-lifecycle.md` | 生命周期 + **ECL↔MSG 协程握手逐指令锁定**(`518 dialogRead` / `519 dialogWait` / `12 ecl-resume` 经 `vm[0x18c]` 脉冲) | ✅ |
| `04-ending-staff-msg-instruction-set.md` | **结局/staff 第二套指令集**(一手 `Ending::on_tick_23__main` `0x4199F0`)。三方交叉:一手 × 真实 7 文件实测(e01/e02/e08 + staff1-4)× **thmsg `th10_msg_ed_fmts`**。社区行为表此前空白,我们补全(并多出 op4/op13) | ✅(余 op4/0xd/0xf-0x11 仅代码语义,见下) |
| `tools/parse_th16_msg.py` | 结局/staff `.msg` 解析+文本解密器(`python3 msg/tools/parse_th16_msg.py files/e01.msg`) | — |

**一句话结论**:`.msg` 一个格式、**两套消费者**——① 关卡内对话 `GuiMsgVm`(由 `Gui` 每帧驱动,ECL `ins 518/519` 触发握手);
② 结局+staff `EndingChildF0`(由 `Ending` 场景驱动,独立 opcode 集)。文本走 **ANM 渲染**(非 GDI),编码 Shift-JIS + 自 TH09 的加速 XOR。

## 遗留小遗憾(收尾未做项,按价值排;均不影响主结论)

> 这些是"知道但没做完/做满"的点,留给将来或 IDE 实现期顺手解。**都不影响上面四篇的主链路结论。**
>
> **2026-09-06 更新**:第 1/2/3/6 项已在 **th18** 上对死(op27 读 float、`|x,字距,文本` 注音语法、op0xd = 淡入、
> 头部 `@8` = 入口 flags 且引擎不读),第 4 项 th18 的 16 个文件同样未用,第 7 项解析器已补 stage 表。
> 见 [`../th18/README.md`](../th18/README.md)。**TH16 本身未重验**,下列条目按 TH16 口径保留。

1. **stage `op27 music-fade-custom` 签名存疑** 🟡:ExpHP 给 `f`(float 时长),但盲读 `case 0x1b` 只见调 `FUN_0043c470`(同 op22)、未见读 float 实参。要么 ExpHP 的 `f` 不适用 TH16,要么 `FUN_0043c470` 内部读全局——需再反 `FUN_0043c470`。(`02` §2)
2. **stage `op17` 文本内 `|x,y,文本` 定位语法** 未精解 🟡:`FUN_0042bbe0`(取串)/`FUN_00476c60`(切逗号)的解析细节没展开;**对 thcrap 译文/IDE 文本编辑有用**。(`02`)
3. **ending `op0xd` 淡入方向** 🟡:已证 = ScreenEffect mode 0(op0xe 的孪生 mode 5),但"淡入 vs 淡出"靠对称推断;要钉死需反汇编裸 label `0x45c630`/`0x45c900` 比 alpha 斜率。(`04` §2.7)
4. **ending `op4 / 0xf-0x11` 无真实数据** 🟡:7 个样本都没用到(难度专属图 TH16 改走多文件方案)。`0xf-0x11` 已证字面共用 `op8` 代码(机制✅),`op4` 一手扎实;但终究无文件实测。要补只能寄望其它角色"通关"结局(e03/e05/e07)。(`04` §2.7)
5. **ending `case 7` 异步线程(`LAB_0041a310`)** 未细反 🟡:`op7 load_anm_present` 起的那个演出/滚动线程内部没展开。
6. **文件头未明字段** ❓:ending 头 `@8 = 0x100` 用途未知;stage/ending 头每条 8 字节项除 `+4=偏移` 外其余字段未验。(`04` §1.5)
7. **stage-MSG 解析器未实现**:`tools/parse_th16_msg.py` 只做 ending/staff;stage `.msg` 解析可照 `02` 表补上(IDE 需要时再做)。
8. **回填 THTK-Studio 仓库的 `docs/` 未做**:四篇结论尚未蒸馏进父仓库 `docs/`(IDE 的 MSG 支持:stage/ending 两套结构化 opcode 编辑 + 文本编解码 + 对 thmsg)。
9. **命名落盘脚本未建**:`apply_th16_msg_names.py`(建议但未做);因 th-re-data 已命名 GuiMsgVm/Ending 全簇,边际价值低。

## 规范(与父工作区一致)

- **五段证据链** + **可信度** ✅一手 / 🟡单源推断 / ❓存疑;**结论必注版本(仅 TH16,勿外推)**。见 `METHOD.md`。
- **防过拟合**:派子 agent 命名给中立判据、不喂标签;"反超社区"自创结论按四闸门复核(`04` 即此类,已守)。
  memory `re-overclaim-guard` / `re-agent-no-hypothesis-priming` / `re-evidence-chain-discipline` / `re-workflow-fanout-cost`。
- **主仓库不留版权字节**:游戏 exe / `.msg` 资产 / 大段反编译原文一律 gitignore;只提交脚本 + markdown。(`local/th16.v1.00a/*.msg` 已确认 gitignore。)

## 环境(与父工作区共用)

- **ghidra-re MCP**,database `th16`(已分析 + th-re-data 命名)。落盘 `save_database`;隐藏工具走 `batch`/`call`(`tooling/ghidra/mcp-tools.md`)。
- **符号金矿** `local/vendor/th-re-data`(ExpHP,gitignored):已命名 `GuiMsgVm`/`Ending` 全簇 + `zMsgRawInstr`/`zGuiMsgVm`/`zEndingChildF0` 结构。
- 样本:`local/th16.v1.00a/*.msg`(版权,gitignore,用户本地放)。

## 社区对照(交叉印证来源)

- **thtk `thmsg`**(`thpatch/thtk`,本地 `engine/ecl/th16/vendor/thtk/thmsg`):格式权威。`thmsg06.c` 的 `th06_msg_t` = 我们的 `zMsgRawInstr`;
  `util_xor(...,0x77,7,16)` = 文本解密;**`th10_msg_ed_fmts`(`-e` 选用)= ending 签名表**(`04` §2.5 已逐条对上)。解包:`thmsg -e -d 16 e01.msg`。
- **ExpHP truth / thpages**:stage opcode 签名(`core_mapfiles/msg.rs`)+ 名/行为(`tables/reference/msg.ts`)。thpages 把 ending 行为列为 "to be documented"(我们已补,见 `04` §2.6)。
- **ExpHP th-re-data**:符号/结构体(只给"叫什么/在哪",不给"干什么")。
- 速查 `engine/_shared/community-sources.md`。

## 关联

- 文本渲染端:文本经 `FUN_0046d990`(文本转 anm)走 **ANM**,**非** GDI `draw_text`(起步期锚点 B 已证否)。
- VM 范式参考:`engine/ecl/th16/07-vm-architecture.md`(ECL 取指-门控-派发,MSG VM 同构)。
- 主循环:`engine/_shared/frame-loop.md`(Gui::on_tick_20 优先级 0x20 / Ending::on_tick_23 优先级 0x23)。
- 纪律:`METHOD.md`;memory `msg-architecture-th16` 等。
