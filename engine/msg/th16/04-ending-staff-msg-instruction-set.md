# 04 · TH16 结局/Staff-roll MSG 指令集(第二套,社区未公开 · 一手反编译)
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> 适用:**TH16 `th16.exe` v1.00a**。本表覆盖**结局 `eXX.msg` + staff-roll `staffN.msg`**(二者**共用**一套指令集,
> 与关卡内 stage-MSG 的 `GuiMsgVm`(见 `02`)**完全不同**)。
>
> ✅ **可信度声明(已升级)**:本表现已**三方交叉**——① 一手反编译 `Ending::on_tick_23__main`(`0x4199F0`);
> ② 真实 `e01.msg` 字节实测(§1.5);③ **thmsg `th10_msg_ed_fmts` 表交叉对名(§2.5,gate 4 此前以为缺失,实则存在!)**。
> thmsg 的 ending 表与本表**逐 opcode 实参签名吻合**,且**本表比 thmsg 更全**(多出 op4/op13 + 行为语义 + 结构字段)。
> ExpHP thpages 的 "ending MSG to be documented" 指的是**行为语义**未文档化;**格式/签名 thmsg 早有**(只是没人写成行为表)。
> 命名仍为我们自创(thmsg 只给签名不给名),但 behavior+signature 已三方钉死。仅未在 e01 出现的 0xd/0xf-0x11 的行为细节靠一手+盲验证(🟡 行为细节)。

## 1. 子系统与加载链(gate 2 已闭环 ✅)

```
Ending::initialize (0x4191F0):
  idx = (CHARACTER+SUBSHOT)*2  (+1 若用过 continue → good/bad 结局区分)
  文件名 = (&PTR_s_e01_msg_00491780)[idx]      ← eXX.msg 表 @0x491780(8 结局)
  → reads_file_into_new_allocation_402440(THA1 归档读) → Ending+0x10 缓冲
  → EndingChildF0__constructor(child, 缓冲 + *(缓冲+4))  ← 文件头同 stage-MSG:8 字节项,+4=脚本偏移
  → child 存 Ending+0x14 ; child 即本 VM

Ending::on_tick_23__main (0x4199F0)= VM 取指-派发循环(每帧;UpdateFunc 优先级 0x23):
  - 指令指针 = child+0x54 ; 指令格式同 zMsgRawInstr:
      +0 int16 time(时间门控:child+0x1c 计数 < instr.time 则等)
      +2 uint8 opcode(switch 对象)
      +3 uint8 args_size(推进:child+0x54 += args_size + 4)
      +4.. args
  - opcode 0 → return -1(结束)

case 0xc(load_staff_roll):用 FUN_00419170 换载 staffN.msg(按难度)→ memset(child,0,0xf0) 重置 → 同 VM 续跑。
  → **结局演完 → staff roll,同一套指令集。**
```

**VM 状态(`EndingChildF0`,0xf0 字节;ExpHP 仅注了 timers+anm_ids,余为我们一手推)**:
| off | 用途(一手) |
| --- | --- |
| 0x1c/0x18/0x20 | 指令时间计数 / 插值(同 GuiMsg 的 +0x1c 计时机制) |
| 0x2c–0x3c | 等待计时器(zTimer;case 5/6 用) |
| 0x40 | **文本行 anm 槽 [5]**(滚动 5 行;ExpHP `anm_ids[5]`) |
| 0x54 | **当前指令指针**(zMsgRawInstr*) |
| 0x70 | case 7 存 instr+8 指针 |
| 0x74 | 标志位(bit1=??、bit2(|4)=线程运行中→本帧提前 return) |
| 0x78 | **当前文本行号**(0..4,case 3 递增、满 5 归零) |
| 0x7c | **文本颜色**(COLORREF;case 9 设) |
| 0x90 | **图片 anm 槽**(case 8/0xf-0x11 写 +0x90+slot*4) |
| 0xd0 | 线程对象(case 7 Thread__restart) |
| 0xec | case 7 存 instr+4 |

## 1.5 真实数据验证(`local/th16.v1.00a/e01.msg` = 灵梦 no-continue 结局)✅

用户提供真实 `e01.msg`(2520 字节,已解归档)按本格式 + 文本编解码(`02` §1)解析,**完全跑通**:
- **文件头**:`@0 u32=1`(脚本数)、`@4 u32=0x0c`(脚本 0 偏移,构造器用此)、`@8 u32=0x100`(用途未明 🟡);脚本从 0x0c 起。
- **指令格式吻合**:106 条全部按 `{i16 time, u8 op, u8 args_size, args}` 解出,首尾对齐到 0x9d8(文件尾),末条 `op 0`。
- **文本编解码吻合**:`op 3` 的串经 Shift-JIS + 加速 XOR(init 0x77/vel 0x07/acc 0x10)解出**完整可读剧情**
  (例:`　　博麗神社。`、`あうん「霊夢さんが戻ってきたって事は`、`霊夢　「勿論よ。`、末尾 `Ending　No.01　　再戦の決意` /
  `ノーコンティニュークリアおめでとう！`)。→ 证实结局/staff **与 stage-MSG 共用同一文本编码**。
- **opcode 实测分布**:`3`×42(文本)、`9`×23(颜色)、`5`×18 + `6`×14(等待)、`8`×3(图片)、`7`/`10`/`11`/`12`/`14`/`0` 各 1。
  → 印证"图片+文字"为主体,余为胶水。
- **实参实证**(对照 §2 表):`op8 {slot,group,script}` = `{0,0,0}→{1,0,1}→{2,0,2}`(递进 CG)✅;
  `op9` = COLORREF `0x00d0c0c0/0xa0c0d0/0xf0a0a0`(按说话人换色)✅;`op10` = `"bgm/th16_14"`(结局曲)✅;
  `op7` args = `{u32 0, "e01.anm"}`(载结局 anm)✅;`op12` args = **字符串 `"staff.msg"`**(见 §2 注:与 case 0xc 代码按难度选 staffN.msg **不一致**,待解)。
- **流程实测**:`7 载anm → 10 BGM → (9 色 + 3 文本 + 5/6 等待) ×N → 8 切 CG → … → 11 淡乐 → 14 画面淡出(60) → 12 转 staff → 0 end`。

> 结论:**核心指令集已用真实数据闭环**(gate 2/3 通过);行为 ✅,命名仍为提案。下一步只需 staff/其它结局样本补全 0xd/0xf-0x11。

## 1.6 真实数据验证(`local/th16.v1.00a/staff1-4.msg` = staff roll)✅

用户又提供 4 个 staff roll 文件(各 252 字节),用本工具解析,全部跑通,补充验证:
- **4 文件几乎逐字节相同,只差一个值**:末条 `show_image @0xd0` 的 script = **12/13/14/15**(staff1/2/3/4)。
  结合 §1 的 case 0xc(DIFFICULTY 1→staff2 / 2→staff3 / 3→staff4 / else→staff1):
  **难度专属的结尾 CG 是用"4 个近乎相同的文件、只换最后一张图"实现的,不是靠 opcode 0xf/0x10/0x11**。
  这解释了为何有 4 个 staff 文件,也说明 0xf-0x11(指令级难度分支图)在 TH16 staff/此结局里**未被使用**(故仍 behavior-🟡)。
- **新增实证**:`op7 Sz`=`{0,"staff.anm"}`(载 staff 图集)、`op8 SSS` 连用 12 次(slot 0–5 循环,script=staff.anm 内精灵号)、
  `op10 z`=`"bgm/th16_15"`(staff 曲,区别于结局的 th16_14)、`op14`=60(淡出)、`op11`(淡乐)、`op0`(end)。
- **`op5 wait` 实参 = `0xFFFFFFFF`(−1)** → 命中我们从反编译读到的 **`<0 → 999 帧`** 分支(e01 用的是正超时 6000000;
  两条分支至此都见于真实数据)✅。
- staff roll = **纯图片幻灯 + 音乐,无 `op3` 文本**(staff 名字烤进 `staff.anm` 精灵)。故 `op4/0xd/0xf/0x10/0x11` 两份样本(e01+staff)都未触及。

**`local/th16.v1.00a/e02.msg`、`e08.msg`(追加样本)**:同 opcode 集(0/3/5/6/7/8/9/0xa/0xb/0xe/0x0),文本全部解出——
e02=灵梦败北结局(`Ending No.02 突然の敗北`),e08=魔理沙败北结局(`Ending No.08 不採用だったのかな?`,魔法森)。
→ 印证**结局索引方案**:`idx = 角色*2 + 是否用过 continue`(e01/e02=灵梦 通关/败北,e08=魔理沙 败北);
"败北/continue"结局尾部带 `次はノーコンティニューでクリアを目指そう`,与 `Ending::initialize` 的 continue 分支一致。

> 累计 **3 结局(e01/e02/e08)+ 4 staff = 7 个真实文件**:**op 0/3/5/6/7/8/9/0xa/0xb/0xc/0xe 已实测 ✅**(编解码全中)。
> `op4/0xd/0xf/0x10/0x11` 在**全部 7 个文件中均未出现** → 在 TH16 shipped 文件里很可能**罕用/未用**(难度图改走多文件方案,见 §1.6);
> 签名✅(thmsg)、行为仅一手+盲验证(🟡)。要再验只能寄望某个恰好用到它们的角色结局(如其它角色的"通关"结局 e03/e05/e07)。

## 2. opcode → 行为表(结局/staff,TH16)

> 名字为**我们自创**(🟡);行为为**一手**(✅,case `0x4199F0`)。签名记法同 `02`(S=int32, str=掩码字符串)。
> 缺失的 opcode 1、2 无 case → 落 default(仅推进指针),**疑为 nop/未用**(🟡)。

| op | 我们的提案名 | 参数(读点) | 行为 | 验证 |
| --- | --- | --- | --- | --- |
| 0 | `end` | — | `return -1` → 结束 VM/场景 | ✅实测 |
| 3 | `text_line` | str@+4(加密 SJIS) | 在**滚动 5 行**文本窗写一行(槽 +0x40[行号 0x78],色 +0x7c);首次(行号 0)先清 5 槽;行号++ | ✅实测(剧情解出) |
| 4 | `text_clear` | — | interrupt 隐藏全部 5 个文本行槽 | ✅一手(e01 未用) |
| 5 | `wait` | S@+4(最大帧;`<0`→999) | 等最多 N 帧或玩家按**射击/Bomb**键推进(可快进);同页等待 | ✅实测(e01 用 6000000 大超时;staff 用 −1→999 哨兵——两分支皆见) |
| 6 | `wait_page` | S@+4 | 同 5,且**翻页/重置**:行号 +0x78=0、`DAT_004c0f40=0` | ✅实测 |
| 7 | `load_anm_present` | `{u32 idx@+4, str filename@+8}` | 置 +0x74\|4 + 起异步演出线程(`LAB_0041a310`,读 +0x70=instr+8 文件名载 anm)+ `FUN_0046d720(anm,idx+0x14)` | ✅实测(arg=`{0,"e01.anm"}`)— 修正:+8 是**文件名串**非 int |
| 8 | `show_image` | `{slot@+4, group@+8, script@+0xc}` | 卸旧 → create anm effect 进图片槽 +0x90[slot](group=anm 文件组,script=精灵/脚本号)= **显示 CG** | ✅实测(`{0,0,0}/{1,0,1}/{2,0,2}` 递进) |
| 9 | `set_text_color` | S@+4(COLORREF) | 设文本颜色 +0x7c | ✅实测(按说话人换色) |
| 0xa(10) | `music_by_name` | str@+4 | `FUN_0043c370(0,名)`(加 `.wav` 经 modify_bgm 载)+ 名与 **`DAT_00492158`=`"bgm/th16_14"`** 比 → 相等走 `FUN_0043c3f0(0,0xf)` 否则 `(0,0x10)`:按名播 BGM | ✅实测+盲验证 |
| 0xb(11) | `music_fade` | — | `SoundManager::modify_bgm(5,时长)` 淡出;清 +0x74&~1 | ✅实测(size 0,结尾淡乐) |
| 0xc(12) | `goto_staff_roll` | **file 中带 str@+4=`"staff.msg"`** | 卸 5 文本槽 → 换载 staff 文件 → memset 重置 VM → 跳新脚本。**⚠️代码(case 0xc)按 `DIFFICULTY` 硬选 staffN.msg(1→staff2/2→staff3/3→staff4/else staff1),不读该串** | 🟡 实测带 `"staff.msg"` 串,但代码忽略它按难度选——**待解** |
| 0xd(13) | `screen_effect_fadein` | S@+4(时长) | 构造 ScreenEffect **mode 0**(on_tick `0x45c630`+on_draw `____b`),= op0xe 的孪生 | 效果✅(同 op0xe 类,§2.7),方向(淡入)🟡按对称推断 |
| 0xe(14) | `screen_effect_fadeout` | S@+4(时长) | 同上(on_tick `LAB_0045c900`),type field3=5 = **淡出** | ✅实测(结尾 arg=60=淡出帧数)+盲验证 |
| 0xf(15) | `show_image_d1` | `{slot,group,script}` | **`if(DIFFICULTY==1)` 跳进 op8 函数体**(共用代码,§2.7) | 机制✅(=op8+难度门)+签名✅(thmsg SSS);仅"难度CG"用途无数据 |
| 0x10(16) | `show_image_d2` | 同上 | 仅 `DIFFICULTY==2`,同上共用 op8 | 机制✅ +签名✅ |
| 0x11(17) | `show_image_d3` | 同上 | 仅 `DIFFICULTY==3`,同上共用 op8 | 机制✅ +签名✅ |

> opcode **1、2 在 e01 未出现且 switch 无 case** → nop/未用(🟡)。

**难度映射**(🟡 待证):ZUN 惯例 `DIFFICULTY` 0=Easy/1=Normal/2=Hard/3=Lunatic/4=Extra。故 0xf/0x10/0x11 ≈
Normal/Hard/Lunatic 专属图;case 0xc 的 staff 选择:Easy/Extra→staff1、Normal→staff2、Hard→staff3、Lunatic→staff4。
**此映射未独立验证,勿当定论。**

## 2.5 thmsg 交叉验证(★ gate 4 满足)+ 能否用 thmsg 解包

**结论:能**。thtk 的 `thmsg` 用 **`-e` 标志**(`thmsg_opt_end`)解结局/staff,与正文 msg **同一套读写代码**
(`thmsg06.c` 的 `th06_read`/`th06_write`、同 `th06_msg_t = {u16 time; u8 type; u8 length; data}` 结构、同
`util_xor(...,0x77,7,16)` 文本解密、同 `entry_count@0 + 8 字节项`头)——**格式/编码/头与正文完全相同**(TH16;仅 TH19
正文比 ending 多一个 0x50 头块)。**差别只在 opcode 表**:`-e` 时 TH16 走 **`th10_msg_ed_fmts`**(TH10-19 ending 共用
一张**扁平**表),正文走 th16→…→th06 的累积链。即 ZUN 的 ending opcode 自 TH10 起基本不变。

解包命令(需先 build thtk):`thmsg -e -d 16 e01.msg e01.txt`(`-e` 选 ending 表,`-d` dump,`16`=TH16)。

**thmsg `th10_msg_ed_fmts` 签名 vs 本表(逐条吻合)**:

| op | thmsg ED 签名 | 本表(我们一手+实测) | 一致? |
| --- | --- | --- | --- |
| 0 | `""` | end | ✓ |
| 3 | `m`(加密串) | text_line | ✓ |
| 5 | `S` | wait | ✓ |
| 6 | `S` | wait_page | ✓ |
| 7 | `Sz`(int+串) | load_anm_present `{idx, filename}` | ✓✓ |
| 8 | `SSS` | show_image `{slot, group, script}` | ✓✓ |
| 9 | `S` | set_text_color(COLORREF) | ✓ |
| 10 | `z`(串) | music_by_name | ✓ |
| 11 | `""` | music_fade | ✓ |
| 12 | `z`(串) | goto_staff_roll(串=`"staff.msg"`) | ✓✓ **解 ❓** |
| 14 | `S` | screen_effect_fadeout | ✓ |
| 15/16/17 | `SSS`×3 | 难度专属 show_image | ✓✓ |

- **op12 ❓ 彻底解决**:thmsg 把 op12 实参当合法 `z`(文件名串)。所以**文件里 `"staff.msg"` 是真·指令参数**(名义文件名),
  TH16 引擎运行时**另按难度覆盖**为 staffN.msg——文件格式与运行时行为各自自洽,不矛盾。
- **本表比 thmsg 更全**:thmsg ED 表**缺 op4(text_clear)和 op13(screen_effect_fadein)**——这两条我们从 exe switch 反出
  (e01 未用到故 thmsg 也没收录)。即我们补全了 thmsg 漏的两条 + 给出 thmsg 没有的**行为语义/结构字段**。
- thmsg ED 表只到 op17;op1/2 两表都无 → nop。

## 2.6 这是不是"第一张 ending opcode 表"?(诚实校准,防过拟合)

> **2026-09-06 追记**:ExpHP thpages 于 2026-03 合并了 `js/tables/reference/end.ts`(结局表,TH10–TH18 共用),
> 命名 delete / text-add / text-clear / wait / wait-clear / anm-source-load / anm-set-slot / text-color / music /
> music-fade / switch-to-staff / screen-effect-a / screen-effect-b / anm-set-slot-normal|hard|lunatic。
> 与本表逐条对应;其 13/14 仍标 "Does. Something.",行数(非 TH18)与淡出时长标 wip——这些本表与
> [`../th18/02`](../th18/02-ending-msg.md) 已给出。下文「thpages 列为 to be documented」为 2026-06 时的事实,现已过时。

- **签名层(opcode→实参类型):不是首创**。thmsg `th10_msg_ed_fmts` 早有(`-e` 选用),多年来 thmsg 就靠它反编译 ending。
- **行为/语义层(opcode→干什么 + 命名 + 结构字段 + 运行时逻辑如难度分支/staff 衔接/无 ECL 生命周期):
  据现有公开英文 RE 资料,我们应是第一份**。依据:① ExpHP thpages(最权威现代 hub)把 ending MSG 明列 "to be documented";
  ② thmsg 只给签名、**无命名**(反编译输出 `ins_3`/`ins_8`);③ thpatch wiki 只有 **TH06-09 旧文本式** ending(`end06`),非现代二进制式。
- **边界(必守)**:
  1. **仅 TH16 经我们验证**;ED opcode 在 thmsg 是 TH10-19 共用,但**别替别的作宣称行为**(各作 exe 未验)。
  2. `0xd/0xf/0x10/0x11` 行为仅一手+盲验证,**未见于真实数据**。
  3. 已查英文源(thpages/thmsg/thpatch/touhouwiki);**未扫日文社区/Discord/私人笔记** → "第一"= **"我们能查到的第一"**,非"史上第一"。
- 故对外措辞应为:**"现代(TH10 系)ending MSG opcode 的首份行为参考表(在 thmsg 既有签名表之上补全语义,并多出 op4/op13)"**,
  不要写成无条件 "first ever"。

## 2.7 未现身 opcode(op4 / 0xd / 0xf-0x11)的可信度来源与加固

7 个真实文件都没用到这 5 条,故无法靠数据验证。它们的判断**全部来自一手反编译 `Ending::on_tick_23__main`
(`0x4199F0`)的 switch case 体**(+ 独立盲验证 agent 复核,但属同类证据)。下面把"加固后"的依据写清,避免空口:

- **0xf / 0x10 / 0x11 —— 近乎确定(≈op8)**:反编译里这三条**直接 `goto LAB_00419d42`,即跳进 `op8` 的函数体本身**,
  外面只多包一层 `if (DIFFICULTY == 1/2/3)`。证据(一手):
  ```c
  case 8:   iVar12=*(instr+4); anm_unload_46f1c0(param_1[+0x90+iVar12*4]); puVar6=&local_18;
  LAB_00419d42:  param_1[+0x90+iVar12*4]=0;
                 puVar6=AnmLoaded__create_effect(param_1[+0x80 + instr+8 *4], puVar6, instr+0xc, -1, 0);
                 param_1[+0x90 + instr+4 *4]=*puVar6;  ...advance...
  case 0xf:  if (DIFFICULTY==1){ iVar12=*(instr+4); anm_unload_46f1c0(...+0x90); goto LAB_00419d42; } break;
  case 0x10: if (DIFFICULTY==2){ ... goto LAB_00419d42; } break;
  case 0x11: if (DIFFICULTY==3){ ... goto LAB_00419d42; } break;
  ```
  → 它们**不是"像 op8",而是字面共用 op8 的代码**。op8 已被 7 文件大量实测(show_image),故 0xf-0x11
  = op8 + 难度门 这一判断**与 op8 同级可信**(签名 thmsg `SSS` 亦印证)。唯"难度专属 CG"用途因 TH16 实际改走多文件(§1.6)而无数据,但**机制**确定。

- **op4 —— 一手扎实**:case 4 是个 5 次循环对 `+0x40..+0x50`(正是 op3 写文本行的那 5 个 anm 槽)发 `interrupt_tree(slot,3)`:
  ```c
  case 4: p=&param_1[+0x40]; i=5; do { AnmManager__interrupt_tree(*p,3); p++; i--; } while(i); advance;
  ```
  → 清空 op3 填的全部文本行 = text_clear。语义直接可读,无歧义。

- **op0xd —— 已加固到"同类效果,方向靠对称"**:case 0xd/0xe **都内联构造同一个 `ScreenEffect`**(operator_new 0x40 + 注册
  on_tick/on_draw),经 `ScreenEffect::initialize`(`0x45D1A0`)的 mode→回调表比对:**op0xd=mode 0**(on_tick `0x45c630`+on_draw `____b`)、
  **op0xe=mode 5**(on_tick `0x45c900`+on_draw `____b`),二者**仅差 alpha 推进的 on_tick 回调**。op0xe 已实测为场景末
  淡出(e01/staff 结尾 dur=60),故 op0xd(其孪生 mode 0)=**淡入**——**效果类已确定**,只有"淡入 vs 淡出"方向是**按对称
  推断**(未反编译 `0x45c630`/0x45c900 的 alpha 斜率,二者为 Ghidra 未定义的裸 label)。标 🟡(仅方向)。

**可信度天花板(诚实)**:0xf-0x11 与 op4 可视为 ✅(代码语义无歧义);op0xd 效果✅、方向🟡。
要把 op0xd 方向也钉死,需反汇编裸 label `0x45c630`/`0x45c900` 比较 alpha 斜率(未做,边际价值低)。
**这些都属"代码可读语义",不是真实数据验证**——区别保留,勿混。

## 3. 与 stage-MSG(`GuiMsgVm`/`02`)的关系

- **同**:文件后缀 `.msg`、指令编码 `zMsgRawInstr`(time/opcode/args_size/args 8 字节头)、文件头 8 字节项(+4=偏移)、
  时间门控机制、文本经 `FUN_0046d990` 转 anm 渲染、字符串经 `FUN_0042bbe0`(故文本编码大概率同 `02` §1 的 Shift-JIS+加速XOR,🟡 待验)。
- **异**:**opcode 语义完全不同**(本表 vs `02`)、VM 是 `EndingChildF0` 非 `GuiMsgVm`、由 `Ending` 场景驱动非 `Gui`、
  无 ECL 握手(结局不在关卡内,无 dialogWait)。
- 印证 ExpHP "Ending/staff MSG use a **different set of instructions**" ✅,且 thmsg 代码结构亦如此(`-e` 切 `th10_msg_ed_fmts`,见 §2.5)。

## 4. 下一步 / 验证缺口

- ✅ **已闭环**:结局主链路指令(0/3/5/6/7/8/9/10/11/12/14)+ 文本编解码已用真实 `e01.msg` 验证(§1.5)。
  `wait` 超时实参恒为 **6,000,000**(= `0x5b8d80`,"等输入"哨兵值)。复现:`python3 msg/tools/parse_th16_msg.py files/e01.msg`。
- 🟡 **仍缺样本**:`0xd screen_effect_d`、`0xf/0x10/0x11 难度专属图` 在 e01 未出现 → 需别的结局(有难度分支 CG 的)或更多样本。
- ✅ **op12 vs 代码已确认**(独立盲验证):case 0xc **确实不读指令实参**,按 `DIFFICULTY` 硬选 `staffN.msg`(1→staff2/2→staff3/3→staff4/else staff1)。
  故文件里的 `"staff.msg"` 串是**占位/被引擎忽略**(可能是 thmsg 反编译的规范名)。非我们漏读。
- ✅ **整表已独立盲验证**(2026-06-11 子 agent 不喂标签盲读 `0x4199F0`):时间门控、0/3/4/5/6/7/8/9/0xa/0xb/0xc/0xd/0xe/0xf/0x10/0x11 行为与本表一致;
  1/2 无 case(nop);并解出 `DAT_00492158="bgm/th16_14"`、0xd=淡入 / 0xe=淡出。
- **细看**:case 7 线程(`LAB_0041a310`)是否=滚动演出;`0xa` 的 `DAT_00492158` 比较串含义。
- **回填**:本表 = IDE 对结局/staff `.msg` 的结构化编辑依据(与 stage-MSG `02` 分两套 opcode 表);文本编解码器见 `msg/tools/parse_th16_msg.py`。

## 证据指针

th16.exe v1.00a:`Ending::on_tick_23__main` `0x4199F0`(switch on `*(child+0x54)+2`,case 0/3/4/5/6/7/8/9/0xa/0xb/0xc/0xd/0xe/0xf/0x10/0x11);
`Ending::initialize` `0x4191F0`(eXX.msg 表 `0x491780`、THA1 读 `0x402440`、`EndingChildF0__constructor` `0x4197C0`);
`FUN_00419170` `0x419170`(staffN.msg 换载;staff 名表 `0x4917A0`);文本转 anm `FUN_0046D990`、取串 `FUN_0042BBE0`。
th-re-data:`zEnding`(0x24)、`zEndingChildF0`(0xf0,anm_ids[5]@0x40)。
社区/工具:**thmsg `thmsg/thmsg06.c`** 的 `th10_msg_ed_fmts`(ending 签名表,`-e` 选用;`th06_find_format`)+ `util_xor(...,0x77,7,16)`
文本解密——逐 opcode 签名与本表吻合(§2.5),是本表的外部佐证(gate 4)。ExpHP thpages 仅把 ending **行为语义**列为 "to be documented"。
