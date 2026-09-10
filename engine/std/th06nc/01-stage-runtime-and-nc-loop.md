# TH06NC 的关卡脚本运行时，以及 NC 独有的循环 opcode

> **版本**：TH06NC（`th06nc.exe`，MD5 `b0cc28ec904e9efce2439ad857c938b5`，
> imagebase `0x140000000`）。本文裸地址默认属该版本；引用原版一律写成 `th06:0x…`。

状态：解释器与六条 opcode ✅ 一手（NC 反汇编 + 跳表 + 样本数据三边闭合）；
opcode 3 的两个 NC 独有分支 ✅ 一手，并已在 **TH06 原版二进制上对抗证伪**；
§8 的本地化 key 那条 🟡。

一句话：**NC 的 STD 就是 TH06 那台机器换 x64 重编译**，指令集一条没加、
跳表顺序逐项一致——但 **opcode 3 多了两个魔数参数，构成一对「记点 / 跳回」**，
让背景可以无限循环。

---

## 1. 怎么找到的

不是靠猜，是顺着已命名的东西走：

1. `g_StageFiles` —— CC0 的 `Stage.cpp` 里那张 8 项的 `{anmFile, stdFile}` 表，
   第 0 项是 `{"dummy", "dummy"}`。在 exe 里搜 `data/stage1.std` 等 14 条字符串，
   再在 `.data` 里找连续存放它们的指针，落在 `0x1403de540`（Ghidra 自己都把它
   标成了 `PTR_s_dummy_1403de540`——第 0 项的 `"dummy"` 对上了）。
2. **它的唯一引用者是 `0x1400785c0`**，按 `0x1404f1e84` 这个关卡号 ×2 索引。
3. `0x1400785c0` 被 `0x1400789a0` **取址**并装进一个结构体——那是 `ChainElem`。
   于是 `0x1400789a0` = `Stage__RegisterChain`，它取址的五个函数就是 CC0 的五个
   chain 回调。
4. 其中 `0x140077bb0` 装在 calc 链上（优先级 6，与 CC0 的
   `TH_CHAIN_PRIO_CALC_STAGE 6` 一致）——**它就是解释器**。

## 2. 指令记录与分派

```
0x140077bb0  Stage__OnUpdate(Stage *stage)
  if (stage->stdData == NULL) return CONTINUE;          // +0xa8
  if (g_GameManager.isTimeStopped) { spellcardBg.color = 0x60803030; return; }
  for (;;) {
    insn = stage->beginningOfScript + index * 0x14;     // +0x1f0, +0x24
    if ((u16)insn->opcode >= 6) break;                  // CMP DL,0x5 / JA
    switch (insn->opcode) { ... }                       // 跳表 0x14007826c
  }
```

记录布局与 TH06 **逐字节相同**：

| 偏移 | 字段 | 证据 |
| --- | --- | --- |
| `+0x00` | `i32 frame` | `MOV EAX,dword ptr [RDI]` 后与 `scriptTime.current` 比较 |
| `+0x04` | `i16 opcode` | `*(short *)(script + idx*0x14 + 4)`，喂给跳表 |
| `+0x06` | `i16 size` | 未参与分派；opcode 0 的前向扫描按 `+0x14` 步进，不读它 |
| `+0x08` | `i32 args[3]` | 三条 opcode 各读 1–3 个 |

**步长 `0x14`、上界 6**。样本侧独立验证：七个 `stage*.std` 按这个读法解出
**298 条指令、零解析错误**，opcode 全落在 0–5。

跳表在 `0x14007826c`，6 个 RVA：

| opcode | handler | CC0 `StageOpcode` |
| ---: | --- | --- |
| 0 | `0x140077e0c` | `STDOP_CAMERA_POSITION_KEY` |
| 1 | `0x140077c4f` | `STDOP_FOG` |
| 2 | `0x140077d03` | `STDOP_CAMERA_FACING` |
| 3 | `0x140077d39` | `STDOP_CAMERA_FACING_INTERP_LINEAR` |
| 4 | `0x140077cc3` | `STDOP_FOG_INTERP` |
| 5 | `0x140077dd0` | `STDOP_PAUSE` |

**这个顺序是从 NC 的跳表读出来的，不是从 CC0 抄的**；测试
（`test_th06nc_std_spec.test_opcode_set_matches_the_cc0_enum`）拿 CC0 的枚举逐项比对，
对不上就失败。

## 3. 六条 opcode

| op | 干什么 | NC 侧写到哪 |
| ---: | --- | --- |
| 0 | 相机位置关键帧。`frame == -1` 时只播种；否则提交本帧，然后**向前扫到下一条 opcode 0**，取它的 frame 与位置当插值终点 | `+0xc0`（position）、`+0x1f8`（插值起点）、`+0x204`（起始帧）、`+0x30`/`+0x3c`（终点/终止帧） |
| 1 | 雾：`{color, near, far}` | `+0x40` / `+0x44` / `+0x48`；`+0x90 == 0` 时立即下发（见 §5） |
| 2 | 相机朝向：`initial <- final`，`final <- args` | `+0x04` / `+0x10` |
| 3 | 朝向插值时长 | `+0x78`，计时器 `+0x7c`/`+0x80` 置 `{-999, 0}` ——**外加两个魔数，见 §4** |
| 4 | 雾插值：`interpInitial <- skyFog`，时长 `args[0]` | `+0x4c`、`+0x90`，计时器 `+0x94`/`+0x98` 置 `{-999, 0}` |
| 5 | 暂停：等 `+0x68` 的 unpause 标志置起，然后清掉它 | `+0x68` |

opcode 0/1/2/4/5 与 CC0 的 `Stage::OnUpdate` 逐条同形，不再展开。

## 4. ★ NC 独有：opcode 3 的 `-1` / `-2`

`0x140077d39` 在读到 `args[0]` 之后先看两个魔数，都不是才走 TH06 的老路：

```asm
140077d4a  MOV EAX,dword ptr [RDI + 0x8]      ; args[0]
140077d4d  CMP EAX,-0x1
140077d50  JNZ 0x140077d86
;; ---- args[0] == -1：记回跳点 ----
140077d52  MOV RAX,qword ptr [RBX + 0xb0]     ; scriptTime {previous, current}
140077d59  INC EDX                            ; 下一条指令下标
140077d5b  MOVSD XMM0,qword ptr [RBX + 0xc0]  ; position.x, .y
140077d63  MOV qword ptr [RBX + 0x208],RAX
140077d6a  MOV EAX,dword ptr [RBX + 0xc8]     ; position.z
140077d70  MOVSD qword ptr [RBX + 0x214],XMM0
140077d78  MOV dword ptr [RBX + 0x21c],EAX
140077d7e  MOV dword ptr [RBX + 0x210],EDX
;; ---- args[0] == -2：跳回去 ----
140077d8b  MOV RAX,qword ptr [RBX + 0x208]
140077d92  MOVSD XMM0,qword ptr [RBX + 0x214]
140077d9a  MOV EDX,dword ptr [RBX + 0x210]    ; 指令下标回到记的那个
140077da0  MOV qword ptr [RBX + 0xb0],RAX
140077da7  MOV EAX,dword ptr [RBX + 0x21c]
140077dad  MOVSD qword ptr [RBX + 0xc0],XMM0
140077db5  MOV dword ptr [RBX + 0xc8],EAX
;; ---- 其余：TH06 的老行为 ----
140077dbd  MOV dword ptr [RBX + 0x78],EAX     ; facingDirInterpDuration
```

**语义**：`-1` 把 **{脚本计时器、相机位置、下一条指令下标}** 四样存进
`+0x208` / `+0x214` / `+0x21c` / `+0x210`；`-2` 原样还回去。
因为**连脚本计时器一起还原**，跳回去之后相机运动会一模一样地重演——
所以这一对不是单纯的跳转，而是一个**闭合的背景循环**。

### 对抗证伪：TH06 原版没有这两条

`th06:0x403810`（`Stage__on_tick_06`，ExpHP 与 th06-re 两个上游都落在这个地址）
的 case 3 一手反编译：

```c
case 3:
  if (*(int *)(param_1 + 0x24) < *local_c) goto default;
  *(int *)(param_1 + 0x2c4) = local_c[2];   // facingDirInterpDuration
  *(undefined4 *)(param_1 + 0x2d0) = 0;
  *(undefined4 *)(param_1 + 0x2cc) = 0;
  *(undefined4 *)(param_1 + 0x2c8) = 0xfffffc19;   // -999
  *(int *)(param_1 + 0x28) = *(int *)(param_1 + 0x28) + 1;
  break;
```

**只有设时长 + 起计时器，没有任何 `-1` / `-2` 分支。** 这是 NC 加的。

> 顺带一条跨版本常量对上了：时间停止时写进 spellcard 背景色的值，
> NC 是 `param_1[0x6f] = 0x60803030`，TH06 是 `+0x104..0x107` 四个字节
> `{0x30, 0x30, 0x80, 0x60}`——同一个小端 dword。

### 样本侧：只有 Extra 关在用

按 §2 的读法把七个 `stage*.std` 全扫一遍，opcode 3 带魔数的**只有两条**，
都在 `stage7.std`（Extra）：

| 指令下标 | frame | `args[0]` |
| ---: | ---: | ---: |
| 32 | 9345 | `-1` |
| 40 | 10945 | `-2` |

循环体是 #33–#40 共 8 条指令，周期 1600 帧（60 fps 下约 26.7 秒）。
Extra 关没有固定时长——背景必须能一直放下去，这条增量正好解释了它。
有测试（`test_the_nc_only_loop_opcode_shows_up_in_the_shipped_scripts`）钉着这两条，
样本一换就会失败。

> 🟡 **未实机验证**：语义是从控制流推出来的，没在游戏里看过 Extra 背景真的循环。
> 推理链完整（存什么、还原什么、下标回到哪，逐条指令可查），但没数过帧。

## 5. 雾：`SetRenderState` 换成了带脏检查的缓存写入器

TH06 的 opcode 1 直接连打三发 `SetRenderState(D3DRS_FOGCOLOR / FOGSTART / FOGEND)`。
NC 换成两个函数：

```asm
140077c8d  CALL 0x1400969c0        ; (r, g, b)，由 args[0] 拆出来
140077c92  MOVSS XMM1,dword ptr [RBX + 0x48]   ; farPlane
140077c97  MOVSS XMM0,dword ptr [RBX + 0x44]   ; nearPlane
140077c9c  CALL 0x140096b00        ; (near, far)
```

两个都是**先比缓存，变了才下发**：

- `0x1400969c0`：`0xff000000 | rgb` 存进 `0x14050a488`，变了才走 `0x14023cd40`。
- `0x140096b00`：两个 float 存进 `0x14050a48c` / `0x14050a490`，变了才走 `0x14023cdc0`。

两者都还要过 `0x14050a060 != 0 && 0x14050a1bc == 0` 两道闸。
这不是 STD 的语义变化——**脚本行为完全一样**，只是渲染层换了。

## 6. chain 注册与优先级

`Stage__RegisterChain` `0x1400789a0` 建三个 `ChainElem`，`arg` 全是 `g_Stage`（`0x140509b60`）：

| 链 | 结构体 | callback | 优先级 TH06 → NC |
| --- | --- | --- | --- |
| calc | `0x140509b20` | `Stage__OnUpdate`；`addedCallback` = `Stage__AddedCallback`，`deletedCallback` = `0x140078c30` | 6 → **6**（没变） |
| draw | `0x140509ae0` | `Stage__OnDrawHighPrio` `0x140078290` | 3 → **5** |
| draw | `0x140509aa0` | `Stage__OnDrawLowPrio` `0x140078390` | 4 → **6** |

两个绘制回调靠**调用集合**区分，不靠猜：`0x140078290` 先打两发雾（§5）再画
z 层 0/1；`0x140078390` 画 z 层 2/3，之后还有遮罩方块与符卡背景 vm
（`CALL 0x1400069b0` = 已命名的 `AnmManager__ExecuteScript`）。
与 CC0 的 `OnDrawHighPrio` / `OnDrawLowPrio` 逐项对得上。

> ⚠️ **绘制优先级的数值变了**（3/4 → 5/6），相对次序没变。
> 说明 NC 很可能整体重排过 `ChainPriorities`——**别把 TH06 的优先级表搬过来**。

## 7. `Stage` 结构体：重排过

本轮确证的字段（字节偏移，全部来自 `Stage__OnUpdate` / `Stage__AddedCallback`
自己的访问）：

| 偏移 | 字段 |
| --- | --- |
| `+0x024` | `instructionIndex` |
| `+0x040` / `+0x044` / `+0x048` | `skyFog.color` / `.nearPlane` / `.farPlane` |
| `+0x058` | `skyFogInterpFinal` |
| `+0x068` | `unpauseFlag`（u8） |
| `+0x078` / `+0x07c` / `+0x080` | `facingDirInterpDuration` / 计时器 previous / current |
| `+0x090` / `+0x094` / `+0x098` | `skyFogInterpDuration` / 计时器 previous / current |
| `+0x0a8` | `stdData` |
| `+0x0b0` / `+0x0b4` | `scriptTime.previous` / `.current` |
| `+0x0c0` | `position` |
| `+0x1f0` | `beginningOfScript` |
| `+0x208` / `+0x210` / `+0x214` | NC 循环点：存下的脚本计时器 / 指令下标 / 位置 |

**偏移与 CC0 `Stage.hpp` 的字段顺序对不上**（x64 重排 + 指针变宽），
和 [`../../anm/th06nc/02`](../../anm/th06nc/02-th06-baseline-comparison.md) §5、
[`../../ecl/th06nc/04`](../../ecl/th06nc/04-enemy-struct.md) 是同一个现象。
**照抄 TH06 的偏移一定错。**

## 8. 关卡名与曲名变成了本地化 key 🟡

解出来的 `stage1.std` 头部：

```
Stage: ST_STAGE1_STAGE_NAME
Song1: ST_MSG1_BGM_NAME_0      Path1: data/bgm/th06_02.opus
```

TH06 那边这几个槽装的是日文字面量。NC 换成了 key，配套的是样本里那批
12 语种 JSON（`local/th06nc/extracted/` 下 `*_zh-CN.json` / `*_ja.json` …）。

> 🟡 **本节只做到「看见」**：没有反出解析 key 的函数，也没验证 JSON 与 key 的对应关系。
> 这条大概率与 [`../../anm/th06nc/01`](../../anm/th06nc/01-vm-and-anchor-opcodes.md) §6
> 剩下的那个 ❓（锚点 opcode 对定宽文本槽起什么作用）是同一条线索。

## 9. 顺带更正：`0x1400785c0` 不是 `LoadStageData`

日志消息迁名那轮把它命名为 `Stage__LoadStageData`（它确实发着 CC0 里
`Stage::LoadStageData` 的那条错误消息），**非日志字符串那轮又独立地判了一遍，
还是同一个错名**。两轮都错。

它是 **`Stage::AddedCallback`**，`LoadStageData` 被内联了进去。三条独立证据：

1. `Stage__RegisterChain` 把它装进 calc 链的 `addedCallback` 槽（`0x140509b30`）；
2. 它是 `g_StageFiles` 的**唯一**引用者，而 CC0 的 `LoadStageData` 拿两个路径当参数，
   根本不碰那张表——索引它的是 `AddedCallback`；
3. TH06 原版两者是**两个分开的函数**（`th06:0x4044c0` 与 `th06:0x404690`）。

这是 [`../../_shared/th06nc-name-transfer.md`](../../_shared/th06nc-name-transfer.md)
§4.5 那个盲点的第三例：**内联者自己不贡献字面量时，判据看不见它。**
更正记在 `th06nc_logname_spec.CORRECTED` 与 `th06nc_std_spec.RENAMES` 里。

## 10. 下一步

1. 🟡 **实机验证 Extra 背景循环**：跑到 Extra、放着不打，看背景是否每 1600 帧重复。
   和别的几条 🟡 一起攒一次 Windows。
2. ❓ **静态几何那半边没碰**：`RawStageHeader` 的 objects / faces / quad 表本轮只在
   `Stage__AddedCallback` 里看到了指针赋值（`+0x28`、`+0xa0`、`+0xa4`），
   `Stage__RenderObjects` `0x140078cb0` 还没逐字段拆。
3. ❓ **本地化 key 的解析路径**（§8）。
4. ❓ NC 的 `ChainPriorities` 整表——本轮只拿到 stage 的三个（6/5/6）。

## 11. 复现

```sh
source tooling/env.sh
python3 tooling/ghidra/apply_th06nc_std.py --dry-run
python3 -m unittest discover -s tooling/ghidra/tests   # 含样本数据复核

# 反编译/反汇编（只读，不改库）
"$JAVA_HOME/bin/python" tooling/ghidra/scripts/decompile_funcs.py th06nc 0x140077bb0
"$JAVA_HOME/bin/python" tooling/ghidra/scripts/dump_func_asm.py  th06nc 0x140077bb0
"$JAVA_HOME/bin/python" tooling/ghidra/scripts/decompile_funcs.py th06.v1.02h 0x403810
```
