# TH13 灵界 BGM / TH18 复刻可行性 —— 研究 + 方案（未实装）

> **版本**：跨版本。本文出现的真实地址一律带版本前缀（`th13:0x…` / `th18:0x…`）。
> **状态**：2026-09-13 **研究 + 方案** → **同日路线 A 已实装**（见文末 §8：构造器精确签名 / 内存流循环语义 / 补 buffer 归属，以及卡 113 的接线）。§1–§7 保留研究原样（它们是路线 A 的依据），**§8 是实装期新增的一手**。
> **样本**：TH13 v1.00c（`local/th13.v1.00c/th13.exe`，md5 `6f39c0708ee4daa94f5053526c4e10cc`）与 TH18 v1.00a（`local/th18.v1.00a/th18.exe`，imagebase `0x400000`）。
> **读法复述**：任务 = ① 先搞清 TH13 的“灵界期间换成灵界版 BGM”到底怎么实现；② 回答这套机制能不能在 **TH18** 里复刻。用户末句的 “th13” 我按 **TH18** 处理；若字面问“TH13 自身能不能做”，见 §1.4 —— TH13 已经做了，只是**不是双轨**。

---

## 0. 结论摘要（TL;DR）

| # | 问题 | 结论 | 可信度 |
| --- | --- | --- | --- |
| 1 | TH13 是“两条完整音轨同时播、只调音量”吗？ | **不是。** TH13 的 BGM 只有 **1 条流**；灵界进出是 `cmd 9`：**停旧 → 在新轨上按当前播放时间 seek → Play**。没有交叉淡化、没有 fade、没有 DSP/变调/pan | ✅ 一手 |
| 1b | 那“硬切”为什么听起来接得上？ | 两层原因叠加：① **素材**：13 对 `th13_NN`/`th13_NNb` 的 intro/loop **秒数逐一严格相等**（b 采样率 22050、字节数正好是正常轨一半）；② **代码**：`FUN_00476ea0` 把当前播放时间换算成新轨字节偏移并 seek 到同一时刻。不是 fade/DSP | ✅ fmt 一手 + 反汇编一手 |
| 2 | 那“灵界版”是什么？ | 是 `thbgm.fmt` 里**独立的一条 track**（文件名 `th13_NNb.wav`，自己的 `track_offset/intro_size/track_size`，22050 Hz 的另一段音频），不是同一首的两个 loop、也不是同轨中途换数据 | ✅ fmt 一手 / 🟡 缺实际 `thbgm.dat` 字节 |
| 3 | TH18 能复刻吗？ | **能**，但 TH13 那条 trance 专用切轨 wrapper 在 TH18 **没有调用者**（`cmd 9` 死代码），且 TH18 数据里**没有 b 变体**。路线见 §4；最省事的“真交叉淡化”是 §4-A（引擎 `CStreamingSound` 内存第二路流），最忠实的是 §4-B（补一条 track 后硬切） | ✅ 一手引擎 / 🟡 路线待实跑 |
| 4 | TH18 里有没有 TH13 那套“双轨/交叉淡化”遗留？ | **没有双轨/交叉淡化实现可遗留**（因为 TH13 本身就没有）；但 TH18 **保留了同源的单流淡入淡出状态机** `th18:0x453460` 和**未被调用的切轨 case 9** `th18:0x477276`。详见 §3.4 | ✅ 一手（含检索面） |
| 5 | 最快验证 | 先用 §5.1 验运行期音量；再验 §5.2 的 cmd 9 切现有 track；最后验 §5.3 的第二路 `CStreamingSound` | — |

一句话给 parent：**TH13 不是“双轨制”，是“单流 + 换一整首备用 track”；TH18 的 BGM 也只有一路，但引擎里保留了做交叉淡化所需的两块料 —— 单流音量斜坡 + 未被调用的切轨 case；真正的双轨要自己再开第二条 `CStreamingSound`。**

> **2026-09-13 第二轮订正/细化（用户问“真的硬切吗？”）**：初版写“硬切”方向正确，但要收窄——**音量/流上确实是硬切**（旧 buffer `Stop` 后新 buffer `Play`，无重叠、无 fade、无 DSP/变调）；但引擎在 `Play` 之前会把新轨 **seek 到与旧轨相同的播放时刻**（`th13:0x476ea0`），且 b 轨与正常轨的 intro/loop **时长严格相等**（§1.5 表）。所以“接得上”是**素材设计 + 同时间点 seek**，不是交叉淡化，也不是运行期效果。
>
> **明确回答“预置 b 轨 / 运行期计算或效果 / 两者都有？”**：**= 预置 b 轨播放。** 引擎只做两类调度计算：① 把当前曲名扩展名替换成 `b` 找 fmt 记录；② 用当前播放时间在新轨上 seek 到同一时刻。没有运行期音频 DSP/变调/交叉淡化。若有人只替换了正常轨而没替换 b 轨，听到的“变化”就是**原来预置的那条 b 轨**，不能证明是系统实时计算。

---

## 1. TH13 侧机制（一手）

### 1.1 播放链与相关全局

| 符号 / 地址 | 说明 | 证据 |
| --- | --- | --- |
| `th13:0x4df1a0` | `SoundManager` 基址 | `FUN_0043afc0` 里 `MOV EAX,0x4df1a0` 后调 `SoundManager__modify_bgm` |
| `th13:0x4e13c0` | **当前 BGM 文件名**（`SOUND_MANAGER.bgm_filename`，基址 `+0x2220`） | `th13:0x405ae0` / `th13:0x405ca1` 逐字节复制它；`th13:0x460e27` 起是 `SoundManager__update_sound_thread` 的处理区 |
| `th13:0x4e14c0` | **BGM 命令队列首项**（基址 `+0x2320`，stride `0x10c`，31 格） | `th13:0x461850` 起 `SoundManager__modify_bgm` 扫描并写队列 |
| `th13:0x4e4740` | **BGM 播放器对象指针**（`data_4e4740__seems_sound_related`） | `th13:0x43d0c0`、`th13:0x476ea0` 等都以它为 this |
| `th13:0x43afc0` | “把扩展名前补 `.wav` → 入队 `cmd 9`”的 wrapper | 见 §1.2；`find_imm_refs` 显示它**只被 `th13:0x405a20` / `th13:0x405b80` 调用** |
| `th13:0x461830` | `SoundManager__modify_bgm(this, cmd, arg)`：写命令队列 | `th13:0x46186b`–`th13:0x46189a` |
| `th13:0x460dd0` | `SoundManager__update_sound_thread`：处理队列（`cmd 1..9`），在声音线程/主线程推进 | 见 `th13:0x476d20` 的对照；返回值 `SOUND_MANAGER.__array_2320[0].__field_0` |
| `th13:0x476ea0` | **真正切 track / 重启流**：先停旧 buffer，再按新 fmt 记录重新装 buffer、Play | §1.3 |
| `th13:0x475840` | `CSound` 初始化；**`th13:0x47587f MOV dword ptr [EDI+0x10],0x1` = 只有 1 条 buffer** | 见 §1.3 |

> `th13:0x4e13c0` 与队列首址 `th13:0x4e14c0` 都落在 `SoundManager` 内部；前者是**当前曲名**，后者是**待处理请求**，不要混。

### 1.2 灵界进 / 出时到底做了哪几步（一手汇编）

`TranceManager::begin` `th13:0x405a20` 在设完灵界标志、背景 anm、玩家形态之后：

```
th13:0x405ae0  复制 [th13:0x4e13c0] 的当前 BGM 文件名到栈上 local_108
th13:0x405af2  读 CUR_STAGE_DATA(= [th13:0x4c22dc]) 第 0 个 dword
th13:0x405af7  CMP dword ptr [EAX],0x6
th13:0x405afc  CMP dword ptr [th13:0x4be824],0x6
th13:0x405b03  JGE 0x405b27                 ; 某个“第 6 关 / 高难度”分支：跳过换 b 变体
th13:0x405b05  LEA ECX,[EBP-0x104]         ; strchr(local_108,'.')
th13:0x405b0b  PUSH 0x2e
th13:0x405b0e  CALL 0x0047a680
th13:0x405b16  LEA EDX,[EBP-0x104]
th13:0x405b1c  PUSH EDX
th13:0x405b1d  MOV word ptr [EAX],0x62      ; ★ 把 '.' 覆盖成 'b' + NUL  ⇒  "th13_XX.wav" → "th13_XXb"
th13:0x405b22  CALL 0x0043afc0            ; ★ 走 BGM 切轨 wrapper
th13:0x405b27  CALL 0x00439280            ; 神灵 anm 切换（与 BGM 无关）
```

`TranceManager::end` `th13:0x405b80` 对称：

```
th13:0x405ca1  复制 [th13:0x4e13c0] 到 local_108
th13:0x405c8b  同一个 “stage==6 / field>=6” 判断，命中就跳过
th13:0x405cbc  CALL 0x0047a680             ; strchr(local_108,'.')
th13:0x405cca  PUSH EDX
th13:0x405ccb  MOV byte ptr [EAX],BL       ; ★ 把 '.' 处写成 NUL  ⇒ 回到 "th13_XX"（无 b）
th13:0x405ccd  CALL 0x0043afc0
th13:0x405cd2  CALL 0x004393a0             ; 神灵 anm 恢复
```

`FUN_0043afc0` `th13:0x43afc0` 本身：

```
th13:0x43b004  MOV dword ptr [EAX],0x7661772e   ; 在末尾补 ".wav"
th13:0x43b00f  PUSH 0x9
th13:0x43b011  LEA EDI,[EBP-0x104]
th13:0x43b017  MOV EAX,0x4df1a0
th13:0x43b01c  CALL 0x00461830                  ; SoundManager__modify_bgm(9,0)，EDI = 完成后的文件名
```

`SoundManager__update_sound_thread` `th13:0x460dd0` 的 `case 9` 再把名字换成 fmt 里的 track：

```
index = FUN_004603c0(name, 0x4df1a0)          ; 按 basename 在 thbgm.fmt 表里找 track 序号
fmt_entry = index * 0x34 + fmt_base            ; track 记录 0x34 字节
BGM[+0xc]->+0x90 = fmt_entry                  ; 把流的新 fmt 记录换进去
FUN_00476ea0(delta)                            ; ★ 停旧、重装、Play —— 硬切
```

**关键结论：灵界进/出没有任何 `cmd 5`（fade）/ `cmd 3/4`（fade in/out）调用，也没有同时动两条流；它就是“换一个 track 名，然后 `case 9` 停旧启新”。** 淡入淡出机器见 §1.3，但 trance 路径不经过它。

> 🟡 上面 `th13:0x405af7` / `th13:0x405afc` 那个“第 6 关 / field≥6 就跳过换 b”的分支，我只确认了比较与跳过行为；`[th13:0x4c22dc]` 第 0 dword 与 `[th13:0x4be824]` 的精确语义没有继续追（推测与关卡/难度或当前曲号有关）。**证伪方法**：在 TH13 里把 trance 触发点分别在 stage 1 与 stage 6 跑一次，看是否都切 b；或反读这两个全局的写点。

### 1.3 TH13 的“淡入淡出”是什么（单流，不是交叉淡化）

TH13 确实有淡入淡出，但对象**只有那 1 条 BGM 流**：

- 状态机：`SoundManager__sub_43d0c0` `th13:0x43d0c0`，由 `Supervisor__on_tick` 调 （`th13:0x439f0d CALL 0x0043d0c0`）。
- 字段：`BGM+0x1c` = 状态（1/2/3/4），`BGM+0x14` = 剩余帧，`BGM+0x18` = 总帧。
- 曲线/常量（一手汇编）：
  - state 1：`vol = (remaining*5000)/total - 5000`，到点 Stop；即从 0 dB 线性淡到 -50 dB 后停。
  - state 2：`vol = (remaining*-5000)/total`，从 -50 dB 线性淡到 0 dB。
  - state 4：`vol = (remaining*1000)/total - 1000`；state 3：`vol = (remaining*-1000)/total`（±10 dB 的小斜坡）。
  - 这些值经 `th13:0x476230`（SetVolume wrapper）写到流。
- 时长由入队参数给：`FUN_0043af70` `th13:0x43af70` 把参数经 `FUN_004971f0` 转成帧后翻成 `cmd 5`；TH13 的调用者（`GuiMsgVm__run`、结局/菜单等）给的是**秒级×60 帧**。

**但这套 fade 只有一条流**；它能让一首曲子淡出、再淡入，**不能同时把两首曲子交叉淡化**。

### 1.4 如果字面问“TH13 自身能不能做”？

能 —— 它已经做了。只是它不是靠“双轨”，而是靠 **`thbgm.fmt` 里另存了一整首 b 变体 + 运行期换 track**。所以“这套机制能不能在 TH13 做到”这个字面问题的答案是“TH13 本来就是这么做上游的”；我们真正要评估的是它在 **TH18** 的可复刻性（§3/§4）。

### 1.5 复核“真的硬切吗？”—— 逐调用点、DSP/变调检索、b 轨时长对齐

#### A. cmd9 全路径逐调用点（TH13，一手汇编）

| 步骤 | 地址 / 调用 | 做了什么 |
| --- | --- | --- |
| trance begin 入队 | `th13:0x405b22` `CALL th13:0x43afc0` | 只做“补 `.wav` → `modify_bgm(9,0)`”；begin 其余调用是 anm/spirit/player，不碰音频 |
| 命令线程取当前时间 | `th13:0x461122` `CALL th13:0x476e00` | `FUN_00476e00` 返回当前播放时间（秒） |
| 命令线程切流 | `th13:0x46113c` `CALL th13:0x476ea0` | 把新 fmt 记录写进流后调用切轨 |
| 停旧 | `th13:0x476ece`：`MOV EDX,[ECX+0x48]; CALL EDX` | DirectSound `Stop`（旧 buffer） |
| 按时间算新位置 | `th13:0x476ed9` 读 `fmt+0x24`（nAvgBytesPerSec）→ `th13:0x476ee9 FMUL [EBP+0x8]`（×当前时间）→ `th13:0x476f09 SUB ESI,ECX`（− block align） | 得到新轨的字节偏移 |
| 重装/seek | `th13:0x476f0b CALL 0x475a60`（重装 buffer）；`th13:0x476f13 CALL 0x4768c0`（内部 `FUN_00476bf0` seek） | `FUN_00476bf0` 用 `fmt+0x10`(track_offset)、`th13:0x4e4750`(base)、`fmt+0x18`(intro_size) 把文件指针 seek 到该位置 |
| 填充 + 起播 | `th13:0x476f3a CALL 0x475d40`（按位置填 buffer）；`th13:0x476f4a CALL 0x476170` | `FUN_00476170`：清 fade 计数（`th13:0x4761d3`–`th13:0x4761d9`）、`th13:0x4761e0 CALL 0x476230`（**传 0 = 用户满音量，不是斜坡**）、`th13:0x47621f` `Play` |

⇒ 整条路径**没有任何 `cmd 5` / `SoundManager__sub_43d0c0` fade 状态调用**；唯一一次 `FUN_00476230` 是把音量设到正常满值，不是淡入。旧 buffer 先 `Stop`，新 buffer 后 `Play`，没有重叠。

#### B. DSP / 变调 / pan 检索（TH13）

- 在 BGM 流代码区 `th13:0x475000`–`th13:0x478000`，所有 DirectSound buffer vtable 调用只命中：`+0x2c` Lock、`+0x30` Play、`+0x34` SetCurrentPosition、`+0x3c` SetVolume、`+0x48` Stop、`+0x4c` Unlock。**没有 `+0x40` SetPan、没有 `+0x44` SetFrequency。**
- 按两个模式全 exe 扫 `MOV reg,[reg+0x44]; CALL reg` 与 `CALL [reg+0x44]`：**只有 `th13:0x45d213`（`FUN_0045d060`，D3D/非音频）一条**。
- `th13.exe` 里没有 DSFX/EAX/reverb/chorus/echo/distortion/compressor 等字符串，导入表只有 `DSOUND.dll` / `WINMM.dll` / `DINPUT8.dll` / `d3d9.dll` / `d3dx9_43.dll` 等。
- 全 exe 的 `GAME_FLAGS` `th13:0x4be830` 读点里**没有声音管理函数**；trance 专属全局 `th13:0x4be80c` / `th13:0x4be808` 也没有被音频代码读。⇒ 不存在“被 trance 标志门控的每帧音频 effect 更新函数”。

#### C. b 轨与正常轨的时长严格对齐（一手 fmt 实测）

对 `local/th13.v1.00c/dat/thbgm.fmt` 的 13 对 `th13_NN` / `th13_NNb` 逐条算“秒 = 字节数 / nAvgBytesPerSec”：

| base | 正常采样率 | b 采样率 | 正常 intro_s | 正常 loop_s | b intro_s | b loop_s | intro 差 | loop 差 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `th13_01` | 44100 | 22050 | 17.256 | 108.686 | 17.256 | 108.686 | 0.000 | 0.000 |
| `th13_03` | 44100 | 22050 | 4.059 | 84.534 | 4.059 | 84.534 | 0.000 | 0.000 |
| `th13_04` | 44100 | 22050 | 2.260 | 106.960 | 2.260 | 106.960 | 0.000 | 0.000 |
| `th13_06` | 44100 | 22050 | 1.946 | 72.079 | 1.946 | 72.079 | 0.000 | 0.000 |
| `th13_07` | 44100 | 22050 | 15.411 | 139.363 | 15.411 | 139.363 | 0.000 | 0.000 |
| `th13_08` | 44100 | 22050 | 4.013 | 77.384 | 4.013 | 77.384 | 0.000 | 0.000 |
| `th13_10` | 44100 | 22050 | 9.105 | 156.185 | 9.105 | 156.185 | 0.000 | 0.000 |
| `th13_11` | 44100 | 22050 | 8.617 | 103.781 | 8.617 | 103.781 | 0.000 | 0.000 |
| `th13_12` | 44100 | 22050 | 1.562 | 116.877 | 1.562 | 116.877 | 0.000 | 0.000 |
| `th13_15` | 44100 | 22050 | 3.750 | 151.859 | 3.750 | 151.859 | 0.000 | 0.000 |
| `th13_16` | 44100 | 22050 | 16.066 | 112.497 | 16.066 | 112.497 | 0.000 | 0.000 |
| `th13_20` | 44100 | 22050 | 8.261 | 136.166 | 8.261 | 136.166 | 0.000 | 0.000 |
| `th13_22` | 44100 | 22050 | 13.599 | 144.781 | 13.599 | 144.781 | 0.000 | 0.000 |

⇒ 13 对**全部逐列相等**；正常轨的 intro/loop 字节数正好是 b 轨的 **2 倍**（采样率 2 倍）。这不是巧合，是**素材层面的 drop-in 设计**：b 轨是预录的另一版（22050 Hz），结构与时长与正常轨完全相同，专门让引擎 `seek 到同一时刻` 时对得上。

#### D. 有没有“从同一播放位置继续”的字段？

没有单独的 `continue_from` 字段。是运行时算的：`FUN_00476e00`（当前播放时间）→ 用 `fmt+0x24`(nAvgBytesPerSec)、`fmt+0x2e`(bits)、`fmt+0x22`(channels)、`fmt+0x2c`(block align) 换成新轨字节偏移 → `FUN_00476bf0` 用 `fmt+0x10`(track_offset)+`th13:0x4e4750`(base)+`fmt+0x18`(intro_size) seek。因为 intro/loop 秒数相等，seek 落点就是同一音乐时刻。

#### E. `th13:0x405af7` / `th13:0x405afc` 两个全局的语义（顺便钉）

- `th13:0x4c22dc` = **`CUR_STAGE_DATA` 指针**；`*(ptr+0)` = **stage number**。
  - 证据（一手数据）：stage 1 的表 `th13:0x4bb4f0` 第 0 dword = `1`，`+0xc/+0x10` 指向 BGM 名 `th13_01`/`th13_03`；Extra/第 7 关的表 `th13:0x4bb854` 第 0 dword = `7`，BGM 名 `th13_20`/`th13_22`。所以 `CMP [EAX],0x6` 就是“stage 6（主线最后一关）”。
- `th13:0x4be824` = 一个未命名 dword，紧跟在 `CUR_STAGE_NUM`（`th13:0x4be81c`）与 `__CUR_STAGE_NUM__COPY`（`th13:0x4be820`）之后。
  - trance 里 `CMP [th13:0x4be824],0x6`；`FUN_00413270` 用它 `< 6` 选择一套 enemy anm。语义像“当前stage/area 选择位”。
  - **目前只找到一处直接写点**：`GameThread__thread_start` `th13:0x42bf47` 写 `0`。没有其它直接写者；全 exe 里对 `[0x4be824]` 的引用只有 `th13:0x4019d0`/`th13:0x4028e0`/`th13:0x405afc`/`th13:0x405c96`/`th13:0x41381e`/`th13:0x429a94`/`th13:0x42b206`/`th13:0x42bf34`，其中 `th13:0x4028e0` 只是一个 `MOV EAX,[0x4be824]; RET` 的 getter 且**零调用者**。若它真恒为 0，则 trance 里 `(*stage_data != 6) || (0x4be824 < 6)` 的第二个条件恒真，**stage 6 的跳过分支在这个 build 里实际是死的**（永远会切 b）。
  - 🟡 语义和是否有间接写者（块拷贝/`rep movs`）没有完全钉死。证伪方法：在 TH13 的 stage 1 与 stage 6/Extra 各开一次灵界，看 BGM 是否都切到 b；或对 `th13:0x4be81c`–`th13:0x4be828` 这 12 字节下写断点，看谁写 `th13:0x4be824`。
  - **这个条件本该干什么（结合 stage 数据看）**：stage data 表给出各关 BGM：stage1=`01/03`、stage2=`04/06`、stage3=`07/08`、stage4=`10/11`、stage5=`12/15`、stage6=`16/17`、Extra=`20/22`。其中**只有 `th13_17` 没有 b 变体**（fmt 里没有 `th13_17b`）。所以 `(*stage_data == 6) && (0x4be824 >= 6) ⇒ 跳过换 b` 从设计上像是**专门保护 stage 6（避免去找不存在的 th13_17b）**。如果 `th13:0x4be824` 在本 build 真恒 0，这个保护就是死的——stage 6 在 `th13_17` 段开灵界会去查 `th13_17b`（查不到：`FUN_004603c0` 返回终止记录）。这可能是 v1.00c 的边角残留/bug，或说明 `th13:0x4be824` 有我没找到的间接写者。🟡 实战验证：stage 6 放 `th13_17` 时开灵界，看 BGM 是否静音/异常。

---

## 2. TH13 到底是不是“双音轨”？—— 数据格式实测

### 2.1 `thbgm.fmt` 记录格式

`local/vendor/thtk/extlib/thtypes/bgm_types.h` 的 `bgm_fmt_t`：`fn[16]`、`track_offset`、`preload_size`、`intro_size`、`track_size`、`WAVEFORMATEX`（18 B）、`pad`，共 **0x34 = 52 B**。`fn[0]==0` 结束。TH13 / TH18 的 fmt 都按这个步长解通。

### 2.2 TH13：31 条记录，b 变体是独立 track

实测 `local/th13.v1.00c/dat/thbgm.fmt`（1629 B，来自 `th13.dat`；`thdat -l 13` 显示 `thbgm.fmt 1629`）：

| 类别 | 名字 |
| --- | --- |
| 通常版 | `th13_00/01/03/04/06/07/08/10/11/12/15/16/17/18/19/20/22` |
| **灵界版 b** | **`th13_01b/03b/04b/06b/07b/08b/10b/11b/12b/15b/16b/20b/22b`** |
| 额外 | `th128_08` |

b 变体在 fmt 里拥有**自己的 `track_offset / preload_size / intro_size / track_size`**；正常轨多为 `44100 Hz`，b 变体为 **`22050 Hz`**（见 `local/th13.v1.00c/dat/thbgm.fmt`，如 `th13_01` vs `th13_01b` 的 fmt 字段）。所以：

- **不是**“同一首曲子的两个 loop 段”（两段的 offset/size 完全独立，不是同一条 track 内切 loop）；
- **不是**“同一轨中途换数据/位置”；
- 是**另一段完整音频数据**（track 级独立，只是命名带 b）；
- 且两轨**时间结构严格对齐**：13 对全部 `intro/loop` 秒数相等、正常轨字节数正好是 b 轨的 2 倍（§1.5-C 全表）。

> 🟡 **本机没有 `local/th13.v1.00c/thbgm.dat`**（只有 fmt；`th13.dat` 是 thdat 容器，raw 搜不到 fmt 字节）。因此“b 变体在音频内容上到底是不是另一段编曲”目前只能由 **fmt 记录级证据** 推断为“另一条 track”，没有做字节级/听感复核。
> **证伪方法**：把 TH13 的 `thbgm.dat` 放到 `local/th13.v1.00c/`（或告诉我在哪），按 §2.1 解出 `th13_01` / `th13_01b` 的 PCM，直接比对长度/波形/是否同一段 remix；或跑 TH13 在 stage 1 进/出灵界，录下两版音频。

---

## 3. TH18 侧能力评估（一手）

### 3.1 播放链与单/多路

| 环节 | TH18 地址 / 结论 | 证据 |
| --- | --- | --- |
| fmt 加载 | `th18:0x452d4e`：`FUN_00402060("../../bgm/thbgm.fmt",…)` → `DAT_0056c700` | `FUN_00452cb0` 反编译 |
| 数据文件 | `th18:0x452d5e` 附近：`FUN_00402220("thbgm.dat")`；`thbgm.dat` 是**独立的大文件**（430,093,548 B） | `find_imm_refs` + 文件实测 |
| BGM 播放器全局 | `th18:0x570484`（`DAT_00570484`），**只有一个** | Ghidra xrefs：写点只有 `th18:0x471444`（WinMain 清零）与 `th18:0x477487`（case 2 清/重建） |
| 文件流创建 | `FUN_004895a0` `th18:0x4895a0`，**只被 `th18:0x452ee4` 调一次** | Ghidra xrefs |
| 内存流创建 | `FUN_004898f0` `th18:0x4898f0`，只被 `th18:0x476f40`（case 2 preload 路径）调 | Ghidra xrefs |
| CSound 初始化 | `FUN_00489c00` `th18:0x489c00` 里 **`th18:0x489c3a MOV dword ptr [EBX+0x14],0x1` = 1 条 buffer** | 反汇编 |
| 命令队列 | `FUN_00477a50` `th18:0x477a50`（入队，基址 `th18:0x56d204` = `[th18:0x56ad80]+0x2484`）；处理 `FUN_00476d20` `th18:0x476d20`（跳表 `th18:0x4776cc`） | 反汇编 |
| 当前 BGM 名 | `th18:0x56d104`（写点：`th18:0x477415`（case 2）、`th18:0x443762`、`th18:0x4650ca`；**没有 b 改写**） | Ghidra xrefs |

**结论：TH18 的 BGM 也是单路。** 没有第二个 `DAT_00570484`、没有第二个 `FUN_004895a0` 调用、没有第二条 BGM buffer；CSound 的 buffer 数明确是 1。

### 3.2 TH18 现成的音量/淡入淡出 API

| TH18 符号 / 地址 | 用途 | 备注 |
| --- | --- | --- |
| `FUN_0048a5a0` `th18:0x48a5a0` | **SetVolume(this, centibel)**：`vol = (1-(1-user/100)^2)*(param+5000)-5000`；`param=0` → 用户设定的满音量，`param=-10000` → DirectSound 静音 | 一手反编译；这是运行期改 BGM 音量的关键 |
| `FUN_0048a440` `th18:0x48a440` | Play/reset（引擎自己用 `(this,0,1,0)` 播 loop） | 一手 |
| `FUN_0048a620` `th18:0x48a620` | Stop/pause | 一手 |
| `FUN_0048af10` `th18:0x48af10` | 以当前流位置切到**新 fmt 记录**并重启（case 9 的落点） | 一手 |
| `FUN_00453460` `th18:0x453460` | **单流 fade 状态机**：状态在 `BGM+0x20`，剩余/总帧在 `+0x18`/`+0x1c`；state 1/2/3/4 的公式与 TH13 `th13:0x43d0c0` **逐常量和曲线一致**（5000/-5000/1000/-1000；state1 淡到 -50 dB 后 Stop） | 反汇编见 §3.4 |
| `FUN_00454720` `th18:0x454720` | 入队 `cmd 5` = 启动 fade（被角色选择/关卡选择/成就等菜单调，**活代码**） | xrefs |
| `FUN_00477a50` `th18:0x477a50` | 入队任意 `cmd 1..9` | 一手 |

> 注意：`FUN_00453460` 是**单流淡入淡出**；它每条分支都调同一个 `FUN_0048a5a0`，没有第二条流的声音。

### 3.3 灵界进出时的挂点（从 mod 侧看）

- 主动进入：`mods/th18.v1.00a/card-expand/native/cards/trance.c` 的 `on_activate()`（`trance_begin` 后）。
- 中弹救援进入：`on_death_after_deathbomb()` —— **死亡栈**，不建议在这里起音频/建 VM；可以在随后的 `on_item_frame()` 里检测 `phase != IDLE` 后起。
- 退出：`on_item_frame()` 里的 `if (t.ended)` / `trance_should_redie(&t)` 分支。
- 删卡：`on_delete`→`trance_clear()`；如果做 BGM 副作用，必须在这里收（METHOD 铁律二）。

### 3.4 ★ TH18 里有没有 TH13 那套“双轨/交叉淡入淡出”遗留？（单独回答 parent）

**逐层说：**

1. **“双轨/交叉淡化实现”没有。** 因为 TH13 本身也没有双轨；TH18 的 BGM 仍是 1 路（`th18:0x489c3a [EBX+0x14]=1`），只有单个 `DAT_00570484`，CSound 也只有一个。
2. **TH13 同源的“单流 fade 状态机”保留了，而且是活代码。** `th18:0x453460`（由 `Supervisor__initialize` 在 `th18:0x453db0` 注册为每帧 update）四个状态的公式与 TH13 `th13:0x43d0c0` 一模一样；但**只对一条流**做音量斜坡。`cmd 5` 的 wrapper `th18:0x454720` 仍被菜单函数调用。
3. **最接近“TH13 trance 专属遗留”的是 `cmd 9` 切轨 case。**
   - TH13 侧：`cmd 9` 的唯一 wrapper 是 `th13:0x43afc0`；`find_imm_refs` 显示它**只被 trance begin/end（`th13:0x405a20` / `th13:0x405b80`）调用** —— 也就是说 TH13 的 `case 9` 本来就是“trance 专用切轨”。
   - TH18 侧：`FUN_00476d20` 的 switch 表 `th18:0x4776cc` 第 9 项（cmd 9）目标 **`th18:0x477276`**，代码仍在（`FUN_00476890` 查 fmt + `FUN_0048af10` 重启，`th18:0x47728c` / `th18:0x477359`）。但是全部 35 个 `FUN_00477a50` 直接调用点里，长这样一组 `push cmd; push arg2; push name; call` 的 cmd 取值是 **1/2/3/4/5/6/7/8，没有 9**。⇒ **TH18 当前 exe 没有任何地方会入队 `cmd 9`，所以 switch 表第 9 项那一整块不可达（死代码）**。🟡（证据=直接调用点全扫；证伪法=找到一个 `push 9`/寄存器=9 的入队点）。
   - 注意区分：`case 9` 内部调用的 `FUN_0048af10` `th18:0x48af10` **本身不是死代码** —— 它还被 `GameThread__on_tick`（`th18:0x443cd1`）、`ReplayManager__on_tick__speedup_replay`（`th18:0x461df9`）、`FUN_00458e40`（`th18:0x45a3dc`）调用；死的是“按名字入队 cmd 9”这条入口。
   - TH13 那层 `th13:0x43afc0`（把当前曲名扩展名前一位改成 `'b'` 再入 cmd 9）在 TH18 **没有对应实现**：raw pattern 搜 `mov word/byte [reg],0x62`、`mov dword [reg],0x62` 均为 0 命中；`th18:0x56d104` 所有读点（`th18:0x43e486` / `th18:0x43e4a0` / `th18:0x458b11` / `th18:0x477406` / `th18:0x4713d8`）都没有写 `'b'`。
4. **数据侧也没有。** TH18 fmt 实测 18 条：`th18_01/03/04/05/06/07/08/09/10/11/12/13/14/15/16/17/18` + `th128_08`，**没有 `NNb`，没有成对 track，没有第二条 loop 段**。
5. **TH18 仍然有“造第二路/内存流”的能力**：`FUN_004898f0` `th18:0x4898f0` 能**从任意内存 PCM buffer + 内存 fmt 记录**创建第二个 `CStreamingSound`（见 §4-A）。这不是 TH13 遗留，而是同源引擎本来就有的构造器。

> 检索面（回答“怎么确认没有”）：① Ghidra 全 exe xrefs 到 `th18:0x477a50` / `th18:0x476d20` / `th18:0x570484` / `th18:0x56d104`；② 反汇编全 `.text` 扫 `push 0x9` + `call th18:0x477a50`；③ 反汇编全 `.text` 扫对文件名写 `0x62` 的指令模式（`66 c7 ?? 62 00`、`c6 ?? 62`、`c7 ?? 62 00 00 00`）；④ 直读 `local/th18.v1.00a/dat/thbgm.fmt` 全量 0x34 step 解析；⑤ `thdat -l 18` 确认 fmt 在 `th18.dat` 里的条目。这五条都没命中“b 变体/双流/交叉淡化实现”。

### 3.5 跨作同形函数对照（同源引擎血脉）

parent 提示的“拿 TH13 的常量/结构到 TH18 找同形函数”这条检索方向是有效的。下表是本次实际配上的对照（都能在两边读出同样的常量/字段/调用形状）：

| 环节 | TH13 | TH18 | 同形判据 |
| --- | --- | --- | --- |
| 命令入队 | `SoundManager__modify_bgm` `th13:0x461830` | `FUN_00477a50` `th18:0x477a50` | 都在 `SoundManager` 内扫描 31 格队列；格式 cmd/arg/name/state；stride `0x10c`；只有字段基址不同（`+0x2320` vs `+0x2484`） |
| 命令处理 | `SoundManager__update_sound_thread` `th13:0x460dd0` | `FUN_00476d20` `th18:0x476d20` | 都是 `case 1..9` 的 switch；都有 cmd 1 load、2 play、3/4 stop、5 fade、6/7 pause/resume、8 volume、9 switch |
| 单流 fade 状态机 | `SoundManager__sub_43d0c0` `th13:0x43d0c0` | `FUN_00453460` `th18:0x453460` | 状态 1/2/3/4、常量 `0x1388`(`5000`) / `-0x1388` / `0x3e8`(`1000`) / `-0x3e8` 逐条相同；只是字段偏移 +4（TH13 `+0x14/0x18/0x1c` vs TH18 `+0x18/0x1c/0x20`） |
| SetVolume | `FUN_00476230` `th13:0x476230` | `FUN_0048a5a0` `th18:0x48a5a0` | 都读用户音量、做 `(1-(1-v/100)^2)` 曲线 |
| 流对象初始化 | `FUN_00475840` `th13:0x475840` | `FUN_00489c00` `th18:0x489c00` | 都设 “buffer 数 = 1”；`th13:0x47587f` vs `th18:0x489c3a` |
| 按名字找 track | `FUN_004603c0` `th13:0x4603c0` | `FUN_00476890` `th18:0x476890` | 都比较 basename、按 `0x34` step 扫 fmt，直到 `fn[0]==0` |
| 切 fmt 记录并重启 | `FUN_00476ea0` `th13:0x476ea0` | `FUN_0048af10` `th18:0x48af10` | 都先停旧 buffer、按新 fmt reload、再 Play |
| 从内存建流（第二路的钥匙） | 同类 `FUN_004755c0` `th13:0x4755c0` | `FUN_004898f0` `th18:0x4898f0` | 都把 buffer/fmt/长度塞进 preload 版 `CSoundFile`，再建 `CStreamingSound` |

> 这张表说明：**TH18 不是缺“交叉淡化”能用的料，而是缺“第二条流对象”与“TH13 的 b 变体数据”**。淡出/淡入、切轨、SetVolume 都在；把第二路流建起来即可（§4-A）。

### 3.6 “换上别的 BGM，开灵界仍会变，所以显然是系统计算”怎么判

用户这条观察需要拆成 (a) 预置 b 轨机制 与 (b) 运行期效果/计算 两条；一手证据支持 (a)，且 (b) 在 TH13 音频路径上找不到挂点。

**(a) b 轨机制（预期就会“变”）**
- `TranceManager::begin` `th13:0x405a20` 读的是**当前曲名** `th13:0x4e13c0`，然后把扩展名的 `.` 覆盖成 `'b'`（`th13:0x405b1d`）→ 引擎按 basename 在 fmt 里找 **`th13_NNb.wav`**（`FUN_004603c0` `th13:0x4603c0`）。
- 如果只替换了 `th13_NN.wav` 的音频数据、没动 `th13_NNb.wav`，那么灵界时听到的“变化”就是**原来预置的 b 轨**。这**不能**证明是系统实时计算；相反，它恰好证明引擎在按名字找一条**已存在**的 track。
- 如果替换的是整个 `thbgm.dat`（别的游戏文件），引擎仍按 `th13_NNb.wav` 的 fmt 记录去**另一个文件的对应偏移/长度**取数据，听起来当然也会“变”（甚至可能是错位数据）——这仍是数据查找，不是音频计算。

**(b) 运行期效果/计算（TH13 音频路径没有挂点）**
- §1.5-B 已逐项排查：BGM 区只用了 DirectSound 的 Lock/Unlock/Play/Stop/SetCurrentPosition/SetVolume；**没有 SetFrequency、SetPan、没有 DSFX/EAX/reverb/chorus**；没有 trance 标志门控的每帧音频更新函数。
- 引擎唯一的“计算”是：**字符串换名 + 按当前播放时间在新轨上 seek 到同一时刻**（`th13:0x476ea0`）。这不是合成/效果，只是调度。
- 所以除非做控制实验时观察到 (b) 类证据（音高/音量/音色/速率变化），否则应按 (a) 解释。

**★ 一个必须注意的陷阱（会让实验假阳性）**
b 轨的 fmt 是 **22050 Hz**、正常轨是 **44100 Hz**。如果你把两段音频都替换成同一段 44100 Hz 的 PCM，但**没有同时把 b 轨的 fmt 记录改成 44100/相同 intro/track**，引擎会按 22050 Hz 去解释这段 44100 数据 ⇒ 播放速率/音高/loop 都会变，仍然听得到“变化”。**所以判别实验必须同时控制 fmt，而不只是音频字节。**

**判别实验（用户可做）**
1. **准备**：把同一关的 `th13_NN.wav` 和 `th13_NNb.wav` **同时**替换成同一段别的游戏 BGM（建议 44100 Hz），并且把 b 轨的 fmt 记录改成与正常轨**一致的 WAVEFORMATEX + intro_size/track_size**（或让两条 fmt 记录指向同一段数据；不同工具行为不同，务必核 b 轨采样率）。
2. **触发灵界**，录音/听：
   - **不再有变化** ⇒ (a) 预置 b 轨机制成立，§1/§2 结论不变；
   - **仍然有变化**（换了同字节数据、fmt 也一致，仍能听出 pitch/音量/音色/速率差异） ⇒ (b) 运行期效果/计算存在，应升级为新的待核项。
3. **阴性对照**：只替换正常轨、保留原 b 轨 ⇒ 预期灵界时听到“变化”（原 b 轨），这正好复现用户描述的现象。
4. **观察点**：对比灵界前后录音的**音高（采样率）**、**起始位置/节拍相位**、**音量包络**。若差异只在“换了一首/换了一个位置”，是 (a)；若同一段素材上出现 pitch/音量/音色变化，是 (b)。

**如果 (b) 成立，TH18 的同类挂点在哪？**
- TH18 的 BGM 流同样只用到 Play/Stop/SetCurrentPosition/SetVolume/Lock/Unlock/切轨 seek；BGM 区没有 SetFrequency/SetPan/DSFX（可用与 §1.5-B 相同的方法扫 `th18:0x489000`–`th18:0x48b500`）。
- 真要加运行期效果，能碰的只有：`th18:0x48a440`(Play)、`th18:0x48a5a0`(SetVolume)、`th18:0x48a620`(Stop)、`th18:0x48af10`(切轨/seek)、`th18:0x48a1a0`(buffer 填充)。没有现成的 DSP/变调遗留可直接复活。

---

## 4. TH18 实现方案（2 条主路线 + 1 条妥协）

### 路线 A（推荐：真·交叉淡化，**不改 thbgm.dat / th18.dat**）

**做法**

1. 灵界版音频做成**自己的 PCM**（WAV；可以嵌在 DLL 资源或散落资产）。
2. 在灵界进入时，用引擎现成的**内存流构造器** `FUN_004898f0` `th18:0x4898f0` 建第二路 `CStreamingSound`：
   - `this = DAT_0056ad8c`（引擎 SoundManager/DirectSound 对象）；
   - `param_1 = &my_bgm`（第二路输出指针）；
   - `param_2 = pcm_buf`、`param_3 = pcm_size`；
   - `param_4 = my_fmt`（0x34 B；`+0x20` 放 `WAVEFORMATEX`，`+0x18` = intro bytes，`+0x1c` = loop bytes）；
   - `param_11 = segment_size`（必须整除 pcm_size，引擎用它建 8 段循环 buffer，如 `pcm_size/8`）；
   - `param_12 = CreateEventA(...)`；其余参数按 `th18:0x476f0f` 的 `_DAT_004ad87c`（全 0）传 0。
   - 这个构造器是引擎 case 2 在运行期也在走的路径，**不是初始化期专用**。
3. 用 `FUN_0048a440(my_bgm, 0, 1, 0)` 起播（引擎自己播 loop 用同一形状）。
4. 每帧（`on_item_frame` 的灵界 tick）做交叉淡化：
   - 自己的流：`FUN_0048a5a0(my_bgm, cb)`（`cb` 从 -10000 → 0 fade in）。
   - 引擎主 BGM：先把引擎 fade 状态清 0（`*(int*)(BGM+0x20)=0`），再 `FUN_0048a5a0(BGM, cb2)`（`cb2` 从 0 → -10000 fade out）。
   - 两边的 `cb` 用同一个剩余帧数算，避免一个先没声。
5. 灵界退出：反向淡化；到 0 后 `FUN_0048a620(my_bgm,1)` 停自己的流，并按引擎 case 2 的做法 `(**(code**)(*my_bgm))(1)` 释放对象；主 BGM 调 `FUN_0048a5a0(BGM,0)` 还原。
6. 暂停/设备丢失：引擎暂停命令（`cmd 6/7`）只作用主 BGM；自己的流要在 `on_item_frame` 里检测 `GAME_SPEED==0` 或暂停菜单状态时自行 `FUN_0048a620`/恢复，或至少淡出。

**风险**

- 🟡 `FUN_004898f0` 从非初始化路径调用（卡 tick）虽然引擎自己在 case 2 也调，但**在卡 tick 里调可能需要自己保证不在死亡栈/暂停里**；建议只在 `on_item_frame` 正常游玩帧起/收。
- 设备丢失/窗口切出：第二路的 buffer 不会自动跟引擎的 restore 流程；需要在 pause/设备丢失时停掉，否则可能残留/杂音或崩（DirectSound 对象生命周期要自己管）。
- 资产必须 PCM；引擎 fmt 解析只认 PCM（TH18 原生全是 PCM）。
- 与用户音量设置：`FUN_0048a5a0` 会乘 `th18:0x5704b0`；用户改音量时我们每帧重算即可。但其他引擎逻辑若也调 `FUN_0048a5a0`，可能短暂压回满音量（可加 hook 统一乘一个全局系数来根治）。
- 工作量：**中高**（音频资产 + 第二路 ABI + 暂停/释放 + 实跑首日听感）。

**这条路的优点**：不改 430 MB 的 `thbgm.dat`、不改 117 MB 的 `th18.dat`；可以做真正的交叉淡化（TH13 都做不到的重叠过渡）。

### 路线 B（最忠实 TH13：补一条 track，然后硬切；**要改数据**）

**做法**

1. 在 `thbgm.dat` **末尾 append** 灵界版 PCM，注意文件开头的 16 B `"ZWAV"` 头 / `track_offset` 是文件内偏移（TH18 实测：最后一条 track 的 `offset+size` 恰好 = 文件大小；第一条 `track_offset=0x10`，正好跳过 16 B 头）。
2. 在 `thbgm.fmt` 末尾**追加一条 0x34 B 记录** `th18_XXb.wav`，字段照 `bgm_types.h`（注意 `fn` 要 NUL 终止、`+0x20` WAVEFORMATEX、`track_offset` 指向 append 的起点）。
3. 但 TH18 的 fmt 不是散文件：`FUN_00402060("../../bgm/thbgm.fmt",…)` 会**剥掉路径只留 basename，去 dat 归档索引查 `thbgm.fmt`**（`th18:0x402060` 反编译）；`thdat -l 18` 显示它就在 `th18.dat` 里（953 B）。所以 fmt 的修改要**重打包 `th18.dat`**，或找 thcrap/加载器层看能不能覆盖这一条目；`thbgm.dat` 则要**替换/扩展 430 MB 大文件**。
4. 灵界进/出：仿 TH13，把当前曲名 `th18:0x56d104` 拿出来，构造/直接使用新 track 名：
   - **最贴近 TH13**：入队 `FUN_00477a50(&DAT_0056ad80,9,0,"th18_XXb.wav")`，复活 §3.4 的 `case 9`；退出时 `cmd 9` 切回原曲名。注意 **TH18 的 case 9 不会更新 `th18:0x56d104`**，所以要自己保存/恢复，或改用 `cmd 1/2`。
   - 或走活的 `cmd 1/2`（`FUN_00454620` 先载入 slot、`FUN_004546a0` 播）；这条路引擎自己有 stage BGM 的先例（`GameThread__thread_start` `th18:0x443117`–`th18:0x443135`）。
5. 硬切就是 TH13 的原味；如果要“像 fade 一样”，可以先用 `cmd 5` 淡出、切轨、再 `cmd 3/4` 淡入（但这是**顺序淡出淡入，不是重叠交叉**）。

**风险**

- 数据体积/发布：fmt 在 `th18.dat`（117 MB）、音频在 `thbgm.dat`（430 MB）；对 mod 分发很不友好。
- `case 9` 是死代码，需要实测它是否仍能正确换 fmt 记录、循环点/采样率是否对；如果不行就用 live 的 `cmd 2`。
- 追加 track 后要保证 fmt 的终止记录仍在最后（`FUN_00476890` 扫到 `fn[0]==0`）；track 名不要与现有重名。
- `cmd 9` 不更新 `th18:0x56d104`，与 stage/对话 BGM 逻辑可能不同步，必须自己维护。
- 工作量：**高**（音频资产 + fmt/数据补丁 + 重打包 + 切轨实测）。

### 路线 C（最省事但妥协：不动数据，切现成曲目/压 BGM）

**做法**

- **C1 切现成曲目**：灵界进时用 `cmd 9`（复活死代码）或 `cmd 1/2` 切到现成的 `th128_08.wav`（menu/extra 曲，fmt 里已有），退出时切回保存的原曲名。零数据补丁。
- **C2 只压低主 BGM**：灵界进时 `FUN_0048a5a0(BGM,-10000)` 静音，退出还原/淡入；不播替代曲。
- **C3 自播一个极简提示音/短 loop**：用 Windows `PlaySound`/`waveOut` 播固定短音，不做真交叉，只求“灵界期间有变化”。

**风险 / 妥协点**

- C1 不是“该曲的灵界版”，只是换了一首现成曲；观感不对，且 `cmd 9` 死代码要实测。
- C2 灵界期间没音乐；C3 可能盖过 SE、无法随暂停停。
- 工作量：**低**（C1 需要 <100 行 hook；C2 <20 行）。适合拿来先验证“运行期能切曲/改音量”，再决定要不要上 A/B。

### 方案对照

| 路线 | 真·交叉淡化 | 需要改的数据 | 运行期风险 | 工作量 | 适合 |
| --- | --- | --- | --- | --- | --- |
| A 双流 + `FUN_004898f0` | ✅ | 无（只带自己的 PCM） | 中高（第二路生命周期/暂停） | 中高 | 想要最好听感、不想碰大文件 |
| B 补 track + 硬切/顺序淡入淡出 | ❌（可顺序） | `thbgm.dat` + `th18.dat`(fmt) | 中（数据补丁 + 死代码 case 9） | 高 | 想 1:1 复刻 TH13 |
| C1 切现成曲 / C2 压 BGM | ❌ | 无 | 低 | 低 | 最小验证 / 妥协 |

---

## 5. 最小验证实验（本轮不做，留给实装前）

### 5.0 实验 0：b 轨 vs 运行期效果（**最优先**，用户原话 2 的判别实验）

- **目的**：证明“换上别的 BGM 后开灵界仍会变”到底是 (a) 预置 b 轨还是 (b) 运行期效果/计算（详见 §3.6）。
- **控制组**：把同一关的 `th13_NN.wav` 与 `th13_NNb.wav` **同时**替换成同一段别的游戏 BGM（44100 Hz），并把 b 轨的 fmt 记录改成与正常轨一致的 WAVEFORMATEX + intro_size/track_size（或让两条 fmt 指向同一段数据）。**必须同时控制 fmt，不能只换音频字节**（b 轨原始 fmt 是 22050 Hz）。
- **触发灵界**：录音/听后判：
  - 不再有变化 ⇒ (a) 预置 b 轨机制，§1/§2 结论成立；
  - 仍有变化（且 fmt 已一致）⇒ (b) 存在运行期效果/计算，升级为新的待核项。
- **阴性对照**：只换正常轨、保留原 b 轨 ⇒ 预期会变（原 b 轨），复现用户现象。
- **观察判据**：灵界前后是否**换了曲子/位置**（(a)），还是同一素材上出现**音高/音量/音色/速率**变化（(b)）。

### 5.1 实验 1：运行期改 BGM 音量（最先做，判据最硬）

- **操作**：在某个安全游玩帧，从 debugger/临时 hook 调 `th18:0x48a5a0`，`this = DAT_00570484`（`[th18:0x570484]`）：
  - 先 `FUN_0048a5a0(BGM, -10000)` → 应完全静音；
  - 再 `FUN_0048a5a0(BGM, 0)` → 恢复用户设定音量。
  - 或等价地 `FUN_00477a50(&DAT_0056ad80, 8, 0, "SetVol")`（case 8 会读 `th18:0x5704b0`）。
- **判据**：BGM 立刻静音、再立刻恢复；不崩、不改变曲目/进度；日志能打出调用地址。
- **风险**：如果调用时正好有引擎 fade（`BGM+0x20 != 0`），可能被下一帧 fade 覆写；先把 `BGM+0x20` 清 0。

### 5.2 实验 2：验证 TH18 的 cmd 9 切轨（死代码复活测试）

- **操作**：在安全游玩帧入队 `FUN_00477a50(&DAT_0056ad80, 9, 0, "th128_08.wav")`（注意名字要带 `.wav`，符合 fmt 记录）；听 BGM 是否**在当前流位置切到 menu 曲**；再读 `th18:0x56d104` 保存原名，用 `cmd 9` 切回。
- **判据**：BGM 从当前曲切到 `th128_08`，不崩、不重启整个游戏；`th18:0x56d104` 字串不变（因为 case 9 不更新它）；暂停/恢复仍工作。
- **失败退路**：若 case 9 因年代久远不兼容，改用 live 的 `cmd 1/2`（`th18:0x454620` / `th18:0x4546a0`）。

### 5.3 实验 3：验证“第二路流能同时出声”（路线 A 的关键未知）

- **操作**：用小块 PCM（例如 1 s、44100 Hz、2 ch、16 bit 的正弦/静音）：
  - 分配 0x34 B 的 `my_fmt`：`+0x18 intro=0`、`+0x1c loop=size`、`+0x20` 填 `WAVEFORMATEX`（tag=1, ch=2, rate=44100, avg=176400, block=4, bits=16, cbSize=0）；`+0x2c` 处自然落成 block，`+0x2e` 处自然落成 bits。
  - `HANDLE ev = CreateEventA(0,0,0,0); intptr_t my_bgm=0;`
  - 调 `FUN_004898f0(DAT_0056ad8c, &my_bgm, pcm, size, my_fmt, 0,0,0,0,0,0, size/8, (int)ev)`。
  - `FUN_0048a440(my_bgm,0,1,0)` 起播；同时主 BGM 应仍在响。
  - 用 `FUN_0048a5a0(my_bgm, cb)` 单独改第二路音量，主 BGM 不应跟着变。
  - `FUN_0048a620(my_bgm,1)` + `(**(code**)(*my_bgm))(1)` 收尾。
- **判据**：**两路同时出声**（听/录音频谱看到两个音源），第二路音量只影响第二路；收尾后无残留声、不崩。
- **如果这条过不了**：退回路线 C，或改成自己用 DirectSound/XAudio2 在 DLL 侧起第二路（不靠引擎类）。

---

## 6. 还差哪些一手没核到（诚实登记）

| # | 事项 | 现状 | 证伪/补法 |
| --- | --- | --- | --- |
| 1 | TH13 `thbgm.dat` 的 b 变体字节内容 | 🟡 本机没有 `th13.thbgm.dat`；**已由 fmt 证明** 13 对 track 的 intro/loop 秒数严格相等、正常轨字节数正好 2×b 轨；但音频内容（同一演奏降采样 vs 独立 remix）未听/未解 | 放入 `th13.thbgm.dat` 后解 PCM 对比；或实跑录 normal/b 两版 |
| 2 | TH13 灵界切换条件里的 `CUR_STAGE_DATA` / `th13:0x4be824` | ✅ `[th13:0x4c22dc]`=`CUR_STAGE_DATA`、`*(ptr+0)`=stage number（stage1=1、Extra=7）；🟡 `th13:0x4be824` 只找到一处直接写 0（`th13:0x42bf47`），疑为 12 字节 stage 块的间接写入，stage 6 跳过分支在本 build 可能实际是死的 | 在 stage 1 / stage 6(Extra) 各开一次灵界看切不切 b；对 `th13:0x4be81c`–`th13:0x4be828` 下写断点找间接写者 |
| 8 | TH13 是否存在 trance 运行期音频 effect/计算 (b) | ❌ 代码层未找到：BGM 区无 SetFrequency/SetPan/DSFX，`GAME_FLAGS` 读点无音频函数；唯一“计算”是换文件名 + 同时间点 seek。🟡 要完全排除仍需 §5.0 的用户判别实验 | 做 §5.0；若控制组仍变，再查 `th13:0x476ea0`/`FUN_00476bf0` 的 seek 数学与任何 `FUN_00476230` 调用点 |
| 3 | TH18 `case 9`（`th18:0x477276`）死代码是否仍 100% 可用 | 🟡 静态看完整，无调用者 | 做 §5.2 实验；检查 `FUN_0048af10` 的 loop/位置计算是否依赖 case 2 才有的字段 |
| 4 | `FUN_004898f0` 从卡 tick 调用的安全性（锁/重入/死亡栈） | 🟡 引擎自己在 case 2 调，但不是在 card tick 里 | 做 §5.3 实验；在 `on_item_frame`（正常帧）而不是 `+0x0c` 死亡栈里起流 |
| 5 | TH18 第二路在暂停 / 设备丢失 / replay 时的行为 | ❌ 未核 | 加临时 hook 观察；必要时在 `on_item_frame` 里检测 pause 并停第二路 |
| 6 | TH18 `th18:0x402060` 是否可能优先读散文件（决定 fmt 补丁要不要重打包 th18.dat） | 🟡 当前反编译是“剥路径 → dat 索引查 basename”；未见散文件优先 | 放一个假 `bgm/thbgm.fmt` 看引擎是否读它；或读 `FUN_00402060` 的 dat 索引加载路径 |
| 7 | TH18 `FUN_0048a5a0` 被哪些引擎路径调用（会不会覆盖我们的音量斜坡） | 🟡 已知 fade 机器/菜单/设音量会调；未穷举 | xrefs 扫 `th18:0x48a5a0`；必要时 hook 它加全局系数 |

---

## 7. 复现入口

```bash
# TH13 侧
source tooling/env.sh
python3 tooling/ghidra/scripts/decompile_funcs.py th13.v1.00c 0x405a20 0x405b80 0x43afc0 0x461830 0x460dd0
python3 tooling/ghidra/scripts/dump_func_asm.py th13.v1.00c 0x405a20 0x405b80 0x43afc0 0x476ea0 0x475840 0x43d0c0

# TH18 侧
python3 tooling/ghidra/scripts/decompile_funcs.py th18.v1.00a 0x452cb0 0x476d20 0x453460 0x477a50 0x4898f0 0x48a5a0
python3 tooling/ghidra/scripts/dump_func_asm.py th18.v1.00a 0x476d20 0x453460 0x489c00 0x4898f0

# 数据侧（只读）
python3 - <<'PY'
import struct
for ver in ['th13.v1.00c','th18.v1.00a']:
    d=open(f'local/{ver}/dat/thbgm.fmt','rb').read()
    off=0; i=0
    while off < len(d) and d[off]:
        fn=d[off:off+16].split(b'\0')[0].decode('latin1')
        to,pre,intro,size=struct.unpack_from('<IIiI',d,off+16)
        wfx=struct.unpack_from('<HHIIHH',d,off+32)
        print(f'{ver} [{i:2d}] {fn:18s} off={to} intro={intro} size={size} wfx={wfx}')
        off+=52; i+=1
PY
```

**Doc 状态**：研究与方案，未实装；所有 `th18:` / `th13:` 地址为一手反汇编或直读数据。🟡 项按 §6 证伪。

---

## 8. 路线 A 实装期新增的一手（2026-09-13）

> 本节是**实装**（`mods/th18.v1.00a/card-expand/native/cards/trance_bgm.{c,h}` + `trance_bgm_core.{c,h}`）
> 期间为"把第二路流真正接起来"而**补做**的一手。§1–§7 的结论不变；本节只补它没写到的三件。

### 8.1 ★ 内存流构造成器 `th18:0x4898f0` 的**精确签名**（13 个栈参 + thiscall）

§4-A 给的是形状，实装时逐个参数从**参数基址**读了出来（`PUSH EBX; MOV EBX,ESP` ⇒ `arg n` 在 `[EBX + 4n + 4]`）：

| 参 | 读出位置 | 语义 | 引擎自己那处传什么 |
| --- | --- | --- | --- |
| `this`（ECX）| `MOV ECX,[0x56ad8c]` | DirectSound 侧对象的**值** | `[th18:0x56ad8c]` |
| 1 | `th18:0x489924` `[EBX+8]` → 局部 | **出参**：新的流对象 | `&[th18:0x570484]` |
| 2 | `th18:0x48998a` `[EBX+0xc]` → 块 `+0x80/+0x84` | **PCM 基址** | 预载缓冲 |
| 3 | `th18:0x48999a` `[EBX+0x10]` → 块 `+0x88` | **PCM 长度** | 预载长度 |
| 4 | `th18:0x48997f` `[EBX+0x14]` → 块 `+0x90` | **fmt 记录指针**（0x34 B） | `&fmt[idx]` |
| 6–9 | `th18:0x489990` `MOVUPS XMM0,[EBX+0x1c]` | `DSBUFFERDESC.guid3DAlgorithm`（16 B）| 全 0 |
| 11 | `th18:0x489934` `[EBX+0x30]` | **分段长度** ⇒ `dwBufferBytes = 它 × 8` | 见 §8.2 |
| 12 | `th18:0x489937` `[EBX+0x34]` | **事件句柄**（通知位在它上面） | `[th18:0x57048c]` |

★ **arg5 与 arg10 函数体一次都没读**（引擎自己那处传的分别是寄存器残值 `extraout_ECX` 与窗口句柄，
显然是编译器留下的空位）⇒ **传 0 即可**。返回值是 HRESULT（调用侧 `TEST EAX,EAX; JS`）。

### 8.2 分段长度的引擎公式（`th18:0x476ec3`–`th18:0x476ece`，逐指令）

```
MOVZX ESI, word [fmt+0x2c]      ; block align
MOV   EDI,       [fmt+0x24]     ; nAvgBytesPerSec
IMUL  EDI, ESI
SHL   EDI, 2 / SHR EDI, 3       ; = avg×block/2
DIV   ESI / SUB EDI, EDX        ; 向下取整到 block 的整数倍
```
⇒ `seg = floor(avg×block/2 / block) × block`；buffer 总长 = `seg × 8`。
本机实测（`th18_01.wav` 的 44100/2ch/16bit）：`seg = 352800 B = 2.0 s`、buffer `16 s`。

### 8.3 ★ 内存流的**循环语义**（`th18:0x48acc0` 的 preload 分支，逐条）

```c
if (block->preload_flag(+0x7c) != 0) {          /* 构造成器会把它置 1 */
    block->cur(+0x84) = block->base(+0x80);
    if (fmt->track_size(+0x1c) > 0) block->size(+0x88) = fmt->track_size;
    if (rewind && fmt->intro_size(+0x18) > 0) block->cur = base + intro_size;   /* ★ 回卷点 */
}
```
⇒ **`intro_size = 0` + `track_size = PCM 全长` = 整段无缝循环**（这是实装里那两条字段的取值依据）。
顺带确认了三条**只能靠读代码知道**的：
1. 读取窗口是 `[base, base + track_size)`（`track_size` 不是"循环长度"而是"总长"）；
2. 回卷点 = `base + intro_size`（**不是** 0 也不是 track 末尾）；
3. 引擎自己那 18 条 fmt 的 `preload_size == track_size` 逐条相等 ⇒ 预载口径与"总长"一致。

### 8.4 ★ 补 buffer 归属：**第二路得自己喂**

- 引擎的补 buffer 线程 `th18:0x4779a0` 等的事件**只有一个全局** `th18:0x57048c`、被唤醒后调的也只有
  `th18:0x48a730([th18:0x570484])` ⇒ **它只服务引擎那一条 BGM**，我们新开的第二路它看不见。
- 通知位是**每段一个**（`th18:0x48a440` 建的 `segment*k - 1`，8 个）挂在**我们自己的事件**上；
  事件是 auto-reset（`CreateEventA(0,0,0,0)` 的第二个参数 0）⇒ **每帧 `WaitForSingleObject(ev, 0)`
  消费一个信号、有信号就 `0x48a730(our)`** 即可（环 16 s、60 fps 下绝不会问漏）。
- `th18:0x48a730` 内部进 `EnterCriticalSection(&th18:0x521780)` ⇒ 与引擎那条线程**天然互斥**，可以同时调。

### 8.5 释放与"引擎换曲"（实装期的防御）

- 释放照引擎自己的收尾形状：先 `CloseHandle(事件)`（`th18:0x477465`），再 `vtable[0](流, 1)`
  （deleting destructor，`th18:0x477481`）。
- `stop` `th18:0x48a620` 的 `param_1` **必须传 0**：传 1 时会走 `CloseHandle(block+0x8c)`，
  而内存流那个字段**构造成器根本没写**（它只写 `+0x80/+0x84/+0x88/+0x90/+0x7c/+0x2c/+0`）⇒ 关一个野句柄。
- 引擎换曲 / 重载关卡会**释放旧对象**并把 `th18:0x570484` 指向新的 ⇒ 第二路的持有者必须
  **每帧比对一次指针**，变了立刻撤（否则会去写一个已释放对象的音量）。
- 暂停：引擎暂停时会 `0x48a620(引擎BGM, 1)`，流对象的 `+0x58`（"在播"标志）随之变 0
  ⇒ 这是一个**现成的、一手的**暂停判据（`th18:0x48a440` 置 1、`th18:0x48a620` 置 0，各写一次）。

**Doc 状态（更新）**：§1–§7 研究 + 方案 · **§8 实装期一手**（均已落到
`mods/th18.v1.00a/card-expand/native/cards/trance_bgm.c`）。**仍未实跑**（本机无运行环境）
⇒ 每一条"能出声 / 暂停会停 / 收尾无残留"都还是 🟡，实跑清单见该文件头与 `TUNING.md` §2 的 113 行。

---

## 9. 灵界版 BGM **落地**（2026-09-13 第二轮）—— 补三条一手 + 一条口径订正

> 本轮把路线 A 推到"真素材"：生成器 `mods/th18.v1.00a/card-expand/tools/make_trance_bgm.py`、
> 接线 `native/cards/trance_bgm.{c,h}` + `trance_bgm_core.{c,h}`。实装与失败路径的完整记录在
> `mods/th18.v1.00a/card-expand/AUDIT.md` §CO（**那才是权威**）；本节只收**与本文 §1–§8 有关**的新一手。

### 9.1 ★ 口径订正：`track_size` **含** `intro_size`（不是"intro + loop"）

一手实测（TH18 fmt 全表逐条）：`offset[N+1] − offset[N] == track_size[N]` **处处成立**
（例：记录 0 `track_offset = 16`、`track_size = 19403296` ⇒ 记录 1 的 `track_offset = 19403312`）。
⇒ **循环段的字节数 = `track_size − intro_size`**；`intro_size` 是"循环起点在本轨内的字节偏移"，
与 §8.3 那条引擎语义（读窗口 `[base, base+track_size)`、回卷点 `base+intro_size`）**完全自洽**。
⚠ 本文 §1.5-C 表里的"秒数"是按 `字节 / nAvgBytesPerSec` 算的，那两列**不受影响**（都按同一口径缩放）。

### 9.2 ★ TH13 的 b 轨**不是**"正常轨的降采样/滤波版"（§6 未核项 #1 结案）

TH13 装机目录现在可读（`/mnt/d/117/桌面/迫真车万/[th13] 东方神灵庙 (汉化版+日文版)/`，
`thbgm.dat` = **492,019,608 B**、`thbgm.fmt` = 1629 B）。抽 3 对做 FFT 互相关（正常轨 2 帧一平均降到 22050 域）：

| 对 | 最佳对齐 | 单声道相关 | 逐声道相关（L/R）|
| --- | --- | --- | --- |
| `th13_20` / `th13_20b` | lag = 0 帧 | +0.80 | +0.94 / +0.94 |
| `th13_01` / `th13_01b` | lag = 0 帧 | +0.47 | +0.42 / +0.44 |
| `th13_03` / `th13_03b` | lag = 0 帧 | +0.26 | +0.17 / +0.15 |

- **时间对齐恒在 lag = 0**（同 tempo、同段落结构）；字节数比也**严格 2:1**（§1.5-C 复算成立）；
- 但波形相关只有 0.15–0.94 ⇒ **b 轨是预置的另一版编曲/缩混**，不是同素材降采样；
- 反证：把正常轨做各种强度的低通**不会**让相关升高（反而更低）⇒ 排除"b = 低通版"。
- 🟡 只抽 3 对、只做波形相关；要做听感/频谱复核才能完全钉死（这也解释了为什么 b 轨必须**预置**、
  不能运行期算 —— 它压根不是"同素材变换"）。

### 9.3 ★ stage → 曲名的**活代码**证据（TH18 侧的补全）

exe 的关卡表：基址 `th18:0x4c9410`、步长 `0xd4`、索引 = 关卡号，`CUR_STAGE_DATA` 在 `th18:0x4cf428`。
条目 `+0x0c` = 道中曲名、`+0x10` = boss 曲名，而 **`th18:0x443121`/`th18:0x443130` 就是关卡开始时把它们
分别送进 BGM 槽 0/1 的活代码**（`push [eax+0xc]` → `th18:0x454620`）。逐关结果（与 `musiccmt.txt` 的
作者注释「○面のテーマです」逐条相符）：

| 关卡 | 道中曲 | boss 曲 |
| --- | --- | --- |
| 1 / 2 / 3 / 4 / 5 / 6 / Extra | `th18_03` / `05` / `07` / `09` / `11` / `13` / `17` | `th18_04` / `06` / `08` / `10` / `12` / `14` / `18` |

（索引 0 是一条残留条目：`st01.std` + `st00.ecl`，boss 槽写着**前作 `th17_04`**、TH18 的 fmt 里没有这个
曲名 —— 没查到有谁用它，🟡 留作观察。）

★ **2026-09-15 补精确（字段层次 + 编号陷阱，两处都是订正）**：

1. 表里其实有**两组**曲名指针，别混：
   - `+0x3c` / `+0x40` = **静态**（.rdata 里 `th18_03`…`th18_18` 那批字面量），作者写死的"本关该用哪两首"；
   - `+0x0c` / `+0x10` = **运行时**（静态值为 0），关卡装载时填，`th18:0x443121`/`th18:0x443130`
     把 `+0xc → 槽 0`、`+0x10 → 槽 1` 经 `th18:0x454620(名, 槽)` 注册（该函数把名字接上 `.wav`
     再交给声音管理器 `th18:0x477a50`）；卸载是 `th18:0x4546a0(_, 槽)`。
     静态 dump 时若只看 `+0x0c/+0x10` 会读到 0，**必须看 `+0x3c/+0x40`**（或看运行时）。
     行布局其余已知位：`+0x30` = 关号、`+0x34`/`+0x38` = `stNN.std`/`stNN.ecl`（同表 `+0x3c`…`+0x54` 共 9 个名字指针）。
2. ★ **`th18_NN` 的编号有洞，不能拿编号当曲序**：`thbgm.fmt` 18 条记录的名字是
   `th18_01, th18_03 … th18_18, th128_08`（**没有 `th18_02`**），而音乐室/同人 MIDI 用的是**连续编号**
   `No.1 … No.17`，两者在 `th18_14` 之后**错开**：
   `th18_17`=No.14、`th18_18`=No.15、`th18_15`=No.16（结局）、`th18_16`=No.17（Staff）。
   **一手对照表 = 游戏自带的 `dat/musiccmt.txt`**（Shift-JIS，每块 `@bgm/th18_NN` 紧跟一行
   `No. N  标题`，注释里写明「○面のテーマです」/「エキストラステージのテーマです」/「エンディング」
   /「スタッフロール」）——要判"某个内部曲名是哪首曲子"，**只查这张表**，不要按数字推。
   （本 mod 的 `tools/make_trance_rearr.py` 就改成直接解析它；此前按"编号相等"取 MIDI 导致
   1 面道中错放成 1 面 boss 的编曲。）

### 9.4 状态更新

- §4-A 的"素材可以嵌在 DLL 资源或散落资产"**落地为**：素材 = **同曲 + naive 低音放大**的 WAV，
  放 **DLL 旁边的 `bgm/`**（本地派生品，**不入库、不进发布包**：见 `AUDIT` §CO-6）；
- §5.3 的"第二路能同时出声"**仍未实跑**（本轮只做到：真实素材跑通 DLL 侧同一条纯逻辑链 —— 载荷/格式/
  分段长度/循环字段全部符合引擎口径，7/7）；§6 表格里 3/4/5/7 各项的实跑验证**仍缺**；
- §8 文末的"仍未实跑"**仍然有效**：本轮的产出是**素材 + 接线 + 离线自检**，不是实机证据。

---

### 8.6 ★★ 「同时间点 seek」**怎么才算落地**：顺序必须是 `play → poke 游标 → 整环重填`（2026-09-15，订正 §8.3 的用法）

用第二路放"另一份素材"时，想让**听到的起点**等于某个 PCM 偏移 `so`，光把 `desc+0x84`（`cur`）改掉是**不够**的，
而且**改的时机**很讲究。逐条一手：

1. **`play`（`th18:0x48a440`）自己会重置游标**：它遍历 buffer 后调
   `0x48a1a0(this, buf, 0, arg3)`（`push [ebp+0x10]` / `push eax(=0)` / `push esi`），
   而 `th18:0x48a1a0` 开头就是 `reset(desc, rewind=0, pos=arg3)`（`th18:0x48a22c`→`th18:0x48acc0`）。
   按 §8.3 的预载分支，`rewind=0` ⇒ **`cur = base`**（`pos` 参数在这条分支上不参与，`th18:0x48a23c` 起还会被覆盖）
   ⇒ **在 `play` 之前写的游标会被抹掉**，并且 `play` 会按这个 `cur` 再填一段环。
   ⇒ "先 poke 再 play"是**无效写法**（本 mod 的 113 就栽在这里：seek 只在日志里"算过"，从没落地）。
2. **`intro_size` 只在 `rewind != 0` 时才是位置来源**（`cur = base + intro_size`）——
   而带 `rewind=1` 的那次 `reset` 只出现在**构造成器**的预载路径里（`th18:0x48a320`，由 `[ebp+0xc] != 0` 进）。
   ⇒ 内存流"起播位置"唯一的**构造期**入口是 **我们交给构造成器的那份 fmt 的 `intro_size`**。
   证据：本 mod 的流在 `ctor 后` 探针里 `cur - base = 0x1580F0 = 1 411 200 B = 16.00 s` = 8 段 × 2 s
   （环被整环预填、`cur` 推满一圈），起点就是 `intro_size`（我们给 0）。
3. **环偏移 ↔ PCM 字节由"谁先被填"决定**（§8.4）：改 `cur` 只改"下一次读哪"，
   已经躺在环里的 8 段（= 16 s）不会变。⇒ 要在**运行期**把起点挪到 `so`，必须
   **poke `cur` 之后再连补整整一圈（8 段）**：写指针绕回环偏移 0，于是"从环偏移 0 播出"的就是 `so`。
   在 FADE_IN 起点（我方音量 0、主曲仍满音量）做这件事 ⇒ 听不到。
4. **DS buffer 的 vtable 槽位（一手，从引擎自己的 `stop`/`play` 里读出来的，别再猜）**：
   `+0x24` `GetStatus`、`+0x30` `Play`、`+0x34` `SetCurrentPosition`、`+0x48` `Stop`。
   `stop`（`th18:0x48a620`）对每个 buffer 先 `call [vtbl+0x48]` 再 `push 0; call [vtbl+0x34]`
   ⇒ **stop 会把播放位置归零**（所以暂停恢复必须自己重新对齐）；参数非 0 时还会把**文件**指针倒回
   （`[desc+0x78]==1` 的流，`th18:0x4ad08c`）。
   ⚠ 2026-09-14 那次闪退就是"猜"这个 vtable 猜错槽位（见 AUDIT §CR），**只有像上面这样从引擎自己的调用点读出来**才算一手。

⇒ 结论写法：`play(0)` → `poke cur = base + so` → `refill × 8` →（循环点 `intro_size` 收 0）。

## 6. ★ 原版 BGM 的播放与**循环点**（TH18 一手全链条，2026-09-14 补）

用户在实跑里问「原版音频播放是怎么做的，我记得有循环点的设置」——这一节把它钉成一手。

### 6.1 循环点**在数据里**：`thbgm.fmt` 的 `intro_size`

`thbgm.fmt` 每条记录 `0x34` 字节（TH18：18 条 `th18_01..18` + `th128_08`）：

| 偏移 | 字段 | 含义 |
| --- | --- | --- |
| `+0x10` | `track_offset` | 该轨 PCM 在 `thbgm.dat` 里的文件偏移 |
| `+0x14` | `preload_size` | 预载长度（TH18 实测逐条 = `track_size`）|
| **`+0x18`** | **`intro_size`** | **intro 段字节数 = 循环点（回绕点）** |
| `+0x1c` | `track_size` | **整条轨**长度（**含** intro）⇒ 循环体 = `[intro_size, track_size)` |

`thbgm.dat` 开头有 16 字节 `"ZWAV"` 头，其偏移存在全局 `th18:0x570494`（下面每次 seek 都加它）。

一手实测（TH18 自己的数，44100/2ch/16bit ⇒ 176400 B/s）：

| 轨 | intro | track | 循环体 |
| --- | --- | --- | --- |
| `th18_03`（1 面道中）| 24.61 s | 117.30 s | 92.69 s |
| `th18_04`（1 面 boss）| 2.28 s | 95.46 s | 93.18 s |
| `th18_09` | 30.00 s | 209.63 s | 179.63 s |

### 6.2 引擎侧是「文件流 + 8 段 DirectSound 环」，不是"整轨 buffer"

- 流对象 = `CStreamingSound`（构造器 `th18:0x4898f0`），它挂一个**缓冲描述符** `[流对象+0x10]`：

| 描述符偏移 | 字段 |
| --- | --- |
| `+0x08` | **剩余字节数**（起播 = `track_size − pos`；读盘后 `−= 实读`；回绕后重置为循环体长度）|
| `+0x7c` | preload 标志（1 = 内存流：数据已在内存；0 = 文件流）|
| `+0x80` / `+0x84` / `+0x88` | PCM 基址 / 当前读指针 / 读取窗口长度 |
| `+0x90` | fmt 记录（`0x34 B`）指针 |

- **引擎自己那路是 `preload = 0`**：`CreateFile` → `SetFilePointer`（`th18:0x4ad0a4`）→ `ReadFile`（`th18:0x4ad090`），
  一次读一段填进 DirectSound 的环（`dwBufferBytes = 段长 × 8`）。

### 6.3 「循环点」是怎么用的：`0x48acc0(this, rewind, pos)`（`ret 8`）

```c
if (描述符+0x7c /* preload */) {           // ---- 内存流（我们第二路走这支）----
    fmt = 描述符+0x90;  base = 描述符+0x80;
    描述符+0x84 /*cur*/ = base;                       // 从数据开头
    if (fmt->track_size > 0) 描述符+0x88 /*size*/ = fmt->track_size;
    if (rewind && fmt->intro_size > 0)
        描述符+0x84 = base + fmt->intro_size;         // ★ 从**循环点**起（跳过 intro）
} else {                                   // ---- 文件流（引擎自己的 BGM）----
    hFile = 描述符+0x8c;
    if (rewind && fmt->intro_size > 0) {
        SetFilePointer(hFile, fmt->track_offset + [0x570494] + fmt->intro_size, 0, 0);  // ★ 循环点
        描述符+0x08 = fmt->track_size - fmt->intro_size;
    } else {
        pos = 第二参数;
        if (pos >= fmt->track_size) pos += fmt->intro_size - fmt->track_size;   // ★ 越界按 intro 回绕
        SetFilePointer(hFile, fmt->track_offset + [0x570494] + pos, 0, 0);
        描述符+0x08 = fmt->track_size - pos;
    }
}
```

⇒ **`intro_size` 一个字段同时承担两件事**：① 它是 intro 的长度；② 它是**回绕点**。
"从哪开始"由 `rewind` / `pos` 决定：`rewind=1` = 从循环点起、`rewind=0` + `pos` = 从指定时刻起。

### 6.4 真正"loop"的那一刻

DirectSound 的 buffer 只是**环形搬运工**，它自己不会回到循环点。补 buffer（`th18:0x48a730`）只做
"把本段填到窗口末尾"（`cur + 本段 > base + size` 就截断到末尾，`th18:0x48a89a`–`th18:0x48a8c6`）；
到末尾之后由 §6.3 那支按 `intro_size` **重新定位**（文件流 = 重新 `SetFilePointer` 到循环点、
内存流 = `cur = base + intro_size`）⇒ **循环 = 引擎按 `intro_size` 重指位置**。

### 6.5 TH13 与 TH18 的差别（为什么 TH13 能"同时间点 seek"而 TH18 不能照抄）

- **TH13**：整条轨一次读进**一个** buffer（`th13:0x47587f MOV [EDI+0x10],1`）⇒ 当前播放位置
  **就是 DirectSound 的播放游标**，于是灵界换 `NNb` 轨时能按"同一时刻"直接 seek（`th13:0x476ea0`），
  加上 `NN`/`NNb` 的 intro/loop 时长逐一相等 ⇒ 硬切听起来接得上。
- **TH18**：改成文件流，位置不再等于 DS 游标，而记在**描述符的剩余字节数**（`+0x08`）里
  ⇒ "当前位置 = `track_size − remaining`"；回绕之后 `remaining` 本身就等于"离循环点还有多少"。

### 6.6 本 mod 怎么用这套（卡 113 的第二路）

- 第二路是**内存流**（`preload = 1`），fmt 由 `cards/trance_bgm_core.c` 自己造：
  **`intro_size = 0`、`track_size = PCM 全长`**，而素材本身就只装循环体
  ⇒ 引擎的回绕点 = 文件第 0 字节 = 音乐循环起点，**不需要去改引擎流对象的内部读指针**。
- 代价：素材**没有 intro**（TH13 的 b 轨是带 intro 的）⇒ 主曲还在 intro 段时我们只能从循环点起
  （`❓自定`，登记在 `TUNING.md`）。
- 进灵界时的"同时间点"用**引擎自己的 `remaining`** 换算（`AUDIT.md` §CS-4）：
  `se = 循环体长度 − remaining`（`remaining` 比循环体大 = 还在 intro 段 ⇒ 取 0），
  再按 `our_pcm / 循环体长度` 缩放到我们的素材，最后两条一起落地：
  内存游标 `cur = base + se` **和** `IDirectSoundBuffer::SetCurrentPosition(se mod 环大小)`。

