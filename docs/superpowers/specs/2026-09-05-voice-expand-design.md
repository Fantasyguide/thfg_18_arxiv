# 音效表扩容（card-expand 语音）—— 设计

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 日期：2026-09-05。状态：待用户审阅。归属：`mods/th18.v1.00a/card-expand/`。

## 0. 一句话

把引擎那套写死 84 槽的音效表整体搬到 thcrap codecave 并加长到 116 槽，让 card-expand 能注册
**32 个自定义音效 id（`0x54`–`0x73`）**，第一批装 10–30 条角色语音。

用户已定的四个选择（2026-09-05）：**走扩表、不另开音频通道** / **语音就当普通 SE，可叠加、不做独占通道** /
**方案 B（四张表全搬，新音走引擎原生链路）** / **N = 32**。

## 1. 一手事实

本节全部来自 2026-09-05 的 headless pyghidra + 直读 exe（`tooling/ghidra/scripts/dump_func_asm.py`、
`find_imm_refs.py`、`sound_table.py`）。**一律 🟡，进 AUDIT 前不算成立。**

### 1.1 SoundManager 是静态全局，不是堆对象

对象基址 **`0x56ad7c`**。此前被当成独立全局的几个「数组」其实都是它的字段：

| 字段 | 绝对地址 | 偏移 | 形状 |
| --- | --- | --- | --- |
| `IDirectSound*` | `0x56ad80` | `+0x04` | 设备 |
| 本帧播放队列 | `0x56ad9c` | `+0x20` | 12 槽；计数上限 `0x3c`；声像存 `+0x80` 起 |
| **slot 数组** | `0x56c804` | `+0x1a88` | **84 × 0x18** |
| **blob 指针数组** | `0x56cfe4` | `+0x2268` | **72 × 4** |
| SE / BGM 音量 | `0x5704ac` / `0x5704b0` | `+0x5730` / `+0x5734` | 初值 `0x64` |

`0x1a88 + 84×0x18 = 0x2268` —— **slot 数组与 blob 数组首尾相接，零 slack**；`+0x2388`（blob 数组之后）
起是别的字段（6 处引用，`0x43e487` 等）。**对象内部无法原地扩容。**

### 1.2 五张表

| 表 | 地址 | 形状 | 用途 |
| --- | --- | --- | --- |
| cfg 配置表 | `0x4c9b80` | **0x54 = 84 行 × 0x14** | `+0` 槽号（= 音效 id）、`+4` wav 名下标、`+8` 低 word 声像 / 高 word 音量、`+0xc` 🟡 循环标志、`+0x10` 启用 |
| wav 名表 | `0x4b47a0` | 72 × `char*` + NULL | `se_*.wav` 文件名 |
| blob 指针 | `0x56cfe4` | 72 × 4 | 预加载好的 RIFF 字节 |
| slot 数组 | `0x56c804` | 84 × 0x18 | DirectSound buffer 与播放态 |
| 播放队列 | `0x56ad9c` | 12 槽 | 本帧待播 id |

**cfg 表原地扩不了**：表尾地址 `0x4ca214` 在 `0x401e0e`、`0x474b45` 被当**普通全局变量读写**
（`MOV [0x4ca214],ECX` / `MOV [0x4ca214],EAX`）。`.data` 里那 `0x23a` 字节零是别人的地盘，不是 slack。

### 1.3 slot 元素字段图（0x18 字节，`0x56c804 + k*0x18`）

| 偏移 | 内容 | 证据 |
| --- | --- | --- |
| `+0x00` | `IDirectSoundBuffer*` | `0x4713a1` Release(vt`+8`)、`0x444db9` Stop(vt`+0x48`)、`0x45a4bf` Play(vt`+0x30`) |
| `+0x04` | 优先级/音量 word，**初值 `0xFFFFFFFF` = 空闲** | `0x401110` 写 −1；`play_sound` `0x476d06` 写 `cfg[id]+0xa` |
| `+0x08` | **&cfg 行** | `0x401130` |
| `+0x0c` | 槽号 | `0x401129` |
| `+0x10` | 播放态 | `0x4775dc` |
| `+0x14` | 保存的播放位 | `0x444d96`、`0x444db1` |

### 1.4 id → wav 的映射，与现有文档的两处订正

`play_sound(id)` 取的是 **`+0` 等于 `id` 的那一行**（`+0` 是 0..0x53 的置换），再取 `ptr[该行 +4]`。
用 `engine/_shared/th18-sound-table.md` 的 82 行逐条比对，置换解释 **82/82 命中**，直取解释只 11/82。

现有文档需订正两处：

1. **表是 0x54 = 84 行，不是 0x52。** 界在 `0x476472` `CMP EDX,0x54` 与 `0x4766ef` `CMP ESI,0x4ca214`
   （`(0x4ca214 − 0x4c9b80) / 0x14 = 84`）。
2. 因此漏了两个 id：**`0x52` → `se_trophy.wav`**（声像 `0xfe0c`，与 `0x4f` 同音不同声像）、
   **`0x53` → `se_notice.wav`**。两者都有静态调用点（`0x4192c7`、`0x45653f`），**不是空闲槽**。

补充：`0..0x53` 里没有任何空闲 id —— 静态调用点只覆盖 43 个，其余由 ECL / ANM 动态传参使用。

### 1.5 播放链路

```
ce_play_sound(id, x)
  → 0x476c70 SoundManager__play_sound_at_position (stdcall(id) + xmm2 声像)
      EDI ← movsx word [id*0x14 + 0x4c9b8a]          // cfg[id] 的音量/优先级
      在 0x56ad9c 的 12 槽队列里找 id 或占一个空槽
      slot[id].+4 ← EDI                               // 0x476d06
  → 0x476d20 队列消费者：读 slot[i].+0 的 buffer，Play / SetPan / SetVolume
```

`0x476be0` 是两参版（`ret 8`），同样的表访问在 `0x476bea` / `0x476c60`。
**两个版本都没有 id 边界检查** —— 越界 id 会拿越界行的字节当音量、并往越界 slot 写，直接烂内存。

### 1.6 预加载链路

```
0x4767b0（独立线程）   for i in 0..0x48: blob[i] = read_file(wavname[i])     // 0x402060，走 dat
0x476410 SoundManager::init
  循环 1（0x476450，界 CMP EDX,0x54）  slot[k] = { +0:−1, +8:&行(+0==k), +0xc:k }
  循环 2（0x4766c2，界 CMP ESI,0x4ca214）for 行 j: this=&slot[j]; 0x4776f0(wavname[行j.+4])
0x4776f0  从 blob[行.+4] 的内存 parse RIFF（检 "RIFF"/"fmt "/"data"），
          先在别的 slot 里找同一 wav 下标 → 命中则 IDirectSound::DuplicateSoundBuffer（vt+0x14）
```

**`0x4776f0` 只从内存 blob parse，不碰文件系统。** 这是本设计绕开 dat 打包 / thcrap 文件替换的依据。

`0x401100 life_before_main__sub_401100` 是**循环 1 的 pre-main 副本**（CRT 静态初始化期运行），
界写死成 `0x401139` `CMP EDX,0x7e0`（= 84 × 0x18 字节）。

`0x45a4a0` 遍历 slot 数组调 Play（vt`+0x30`）—— **设备丢失 / 暂停后的统一重播**。
`0x444d80` 遍历 slot 数组调 Stop —— stop-all。新槽住在同一数组里，这两条路自动照顾到。

原生 71 个 wav 全是 PCM（`fmt` tag 1，chunk 16 字节），44.1k/22.05k × 8/16 bit × 单/双声道混用，
共 5.8 MB —— **引擎的 RIFF 解析不挑格式**。

## 2. 方案选择

| | 搬哪些表 | 站点 | DLL 要写什么 |
| --- | --- | --- | --- |
| A | cfg + slot | ~37 | 自己调 `CreateSoundBuffer` + Lock/memcpy/Unlock，自己管设备丢失 |
| **B ★ 选定** | cfg + slot + blob + wav 名（**四张全搬**） | **52** | 只把语音 wav 的字节读进 blob 槽 |
| C | 只搬 blob（slot 数组原地后长） | ~16 | 同 B |

- **C 出局**：`+0x2388` 起被占，slot 数组原地最多只能长 `0x120 / 0x18 = 12` 槽，顶不住 10–30 条语音。
- **A 出局**：省下的 15 个站点要拿一坨自维护的 DirectSound COM 代码 + 设备丢失处理来换。
- **B 选定**：多出的 15 个站点全是同一种机械改写（一个 dword 立即数），可被 `make check` 的站点扫描覆盖；
  换来的是**新语音与原生 SE 在引擎眼里完全同构** —— RIFF 解析、buffer 去重、stop-all、设备丢失重播、
  音量滑条，一行都不用自己写。

## 3. 设计

### 3.1 新表形状（N = 32）

| 表 | 原生 | 新 | codecave 名 | 字节 |
| --- | --- | --- | --- | --- |
| cfg | 84 行 × 0x14 | **116 行** | `ce_snd_cfg` | `0x910` |
| slot | 84 × 0x18 | **116** | `ce_snd_slots` | `0xae0` |
| wav 名 | 72 + NULL | **104 + NULL** | `ce_snd_names` | `0x1a4` |
| blob | 72 × 4 | **104 × 4** | `ce_snd_blobs` | `0x1a0` |

新 id **`0x54`–`0x73`**，新 wav 下标 **72–103**，两者一一对应（`新行.+0 = 0x54 + k`，`新行.+4 = 72 + k`）。

### 3.2 codecave 承接

四块内存全部用 thcrap 的**具名 RW codecave**（`size` 有、`code` 无；一手 `binhack.cpp:1451`，
见 `mods/thcrap-platform.md` §2）。binhack 里一律写绝对表达式 `<codecave:ce_snd_cfg>` 等 ——
**地址由 thcrap 在 apply 时渲染，DLL 不参与重定位**。

⚠ `mods/thcrap-platform.md` §3.4：写绝对地址必须用 `<…>`，写成 `[…]` 会变成相对偏移。

### 3.3 站点全清单（52 处，其中 51 处改写）

**A. cfg 表基址（6）**

| 地址 | 指令 | 换成 |
| --- | --- | --- |
| `0x40111a` | `MOV EAX,0x4c9b80` | `<ce_snd_cfg>` |
| `0x476457` | `MOV EAX,0x4c9b80` | `<ce_snd_cfg>` |
| `0x4766bd` | `MOV ESI,0x4c9b84` | `<ce_snd_cfg>+4` |
| `0x476716` | `MOV EAX,[EAX*4+0x4c9b84]` | `<ce_snd_cfg>+4` |
| `0x476bea` | `MOVSX ESI,word [EAX*4+0x4c9b8a]` | `<ce_snd_cfg>+0xa` |
| `0x476c8d` | `MOVSX EDI,word [EAX*4+0x4c9b8a]` | `<ce_snd_cfg>+0xa` |

**B. cfg 表尾界（1）**：`0x4766ef` `CMP ESI,0x4ca214` → `<ce_snd_cfg>+116*0x14`

**C. slot 数组基址 / 元素字段（25）**

| 地址 | 元素偏移 | 出处 |
| --- | --- | --- |
| `0x401110` | `+4` | pre-main 写 −1 |
| `0x401129` | `+0xc` | pre-main 写槽号 |
| `0x401130` | `+8` | pre-main 写 &cfg 行 |
| `0x444d8f` | `+0` | stop-all 起点 |
| `0x45a4a1` | `+8` | 重播循环起点 |
| `0x45ff38` | `+0` | **硬编码槽 20**（`0x56c9e4`），= id `0x14` `se_lazer02` 的常驻激光音 |
| `0x471393` | `+0` | WinMain 释放起点 |
| `0x4766b8` | `+0` | init 循环 2 起点 |
| `0x476c60` `0x476d06` | `+4` | 两版 play_sound 写音量 |
| `0x477533` `0x477562` `0x4775aa` `0x4775bf` `0x4775ce` `0x4775fa` `0x477652` `0x47766b` | `+0` | 消费者读 buffer（8 处）|
| `0x4775f3` `0x477664` | `+8` | 消费者读 &cfg 行 |
| `0x4775dc` | `+0x10` | 消费者写播放态 |
| `0x47753a` `0x47755b` | `+0x14` | 消费者写播放位 |
| `0x477736` | `+8` | `0x4776f0` 去重扫描起点 |
| `0x4777c0` | `+0` | DuplicateSoundBuffer 源 |

**D. slot 数组界（5）**

| 地址 | 指令 | 换成 |
| --- | --- | --- |
| `0x401139` | `CMP EDX,0x7e0` | `0xae0`（= 116 × 0x18 字节）|
| `0x476472` | `CMP EDX,0x54` | `0x74`（= 116）|
| `0x444dbf` | `CMP ESI,0x56cfe4` | `<ce_snd_slots>+116*0x18` |
| `0x4713ad` | `CMP ESI,0x56cfe4` | 同上 |
| `0x45a4c5` | `CMP ESI,0x56cfec` | `<ce_snd_slots>+116*0x18+8` |

**E. blob 数组（9）+ 尾界（1）**

`0x4713b5`（基址）、`0x4767cc`（预加载写）、`0x477758` `0x47777b` `0x477788` `0x477905` `0x47791f`
`0x477956` `0x477970`（`0x4776f0` 内）→ `<ce_snd_blobs>`。

尾界 `0x4713d8` `CMP ESI,0x56d104` → **`<ce_snd_blobs>+72*4`，刻意只扩基址不扩界**：
WinMain 的释放循环因此只回收原生那 72 个 blob，语音的 32 个谁都不释放。它们的字节来自
thcrap 的 `stack_game_file_resolve`（thcrap 的堆），而引擎用 `0x491a3f`（游戏的 free）——
**跨堆释放必崩**。进程此刻正在退出，这个「泄漏」到此为止，换掉了一整类崩溃。

**F. wav 名表（4）+ 预加载数界（1，刻意不改）**

`0x4766d3` `0x47671d` `0x4767bc` `0x476803` → `<ce_snd_names>`。

`0x4767d8` `CMP ESI,0x48` **保持 0x48 不变** —— 预加载线程仍只从 dat 读原生 72 个；
新音的 blob 由 DLL 填。改了它会让引擎去 dat 里找不存在的语音文件而走错误路径。

### 3.4 填表时序（★ 走 `*_patch_init`，不走 `*_mod_init`）

`sites.py` 的 `emit_codecaves` 已经把这条路走通并注释了一手：**`*_patch_init` 由 `patch_func_init`
调用，位置在 `codecaves_apply` 的末尾（`binhack.cpp:1724`），早于 `binhacks_apply`
（`runconfig.cpp:655-656`）** —— 「先把表填好、再让改过的代码去读它」正好落在这个缝里。
card-expand 的卡表搬迁（`th18_card_table_patch_init`）就是这么做的。

**版权红线（仓库既定规矩）**：cfg 表的 84 行内容与 72 个 wav 名字符串都是 ZUN 的数据，
**patch 里一个字节都不留**。一律在 `ce_snd_patch_init` 里从**用户自己那份 exe** `rep movsd` 拷过来。

| 时点 | 谁 | 做什么 |
| --- | --- | --- |
| `codecaves_apply` 末尾 | **patch asm** `ce_snd_patch_init` | ① 原生 84 行 cfg → `ce_snd_cfg`（`rep movsd`，从模块基址 + RVA 取）；② 原生 72 个 wav 名指针 + NULL → `ce_snd_names`；③ 32 个新行写**结构骨架**：`+0 = 84+k`、`+4 = 0`、`+8 = 0`、`+0xc = 0`、`+0x10 = 0` |
| `binhacks_apply` | thcrap | 打 51 处 binhack |
| 断点 `ce_snd_gate` @ `0x476410` 入口 | **DLL** | 按 `voice.js`：每条已登记语音 k → `blob[72+k] = 读进内存的 RIFF 字节`、`cfg[84+k].+4 = 72+k`、`.+8 = 声像 \| 音量<<16`、`.+0x10 = 1`；`names[72+k] = "ce_voice_<NAME>"`；自检后放行 |
| 紧接着（同一次调用） | 引擎 | `0x476410` 循环 1 初始化 116 个 slot、循环 2 逐行建 buffer |

`0x476410` 入口是 `55 8b ec 6a ff`（`PUSH EBP; MOV EBP,ESP; PUSH -1`）—— 5 字节、无相对寻址，
与现有 `ce_gate` 用的 `0x4637d0` 同款形状，可直接当断点。它**就是** SoundManager 的初始化本身，
所以「blob 必须在建 buffer 之前就位」天然成立。

#### 3.4.1 两条不能违反的行不变式

| # | 不变式 | 违反的后果 |
| --- | --- | --- |
| **I1** | 116 行的 `+0` 必须**两两不同**且恰好覆盖 `0..0x73` | 循环 1 的扫描 `CMP [EAX],EDX / ADD EAX,0x14 / JNZ` **没有上界** —— 找不到就一直往后扫，直接跑飞 |
| **I2** | 116 行的 `+4` 指向的 blob 槽**都不能是 NULL** | `0x4776f0` 在 blob 为 NULL 时进 `0x477768` 的 `Sleep(10)` 等待循环，只有 `[0x5704a4] == 2` 才脱身 —— 正常流程里等不到，**挂死** |

所以 `ce_snd_patch_init` 给新行写的骨架是 `+4 = 0`（指向原生 wav 0 `se_plst00`），
**没被 `voice.js` 登记的新 id 只是重复一个原生音，既不 NULL 也不越界**；DLL 只改登记过的那几行。

🟡 `+0x10` 的语义未验：原生里为 0 的六个 id 是 `0x00` `0x01` `0x07` `0x08` `0x09` `0x0a`
（`se_plst00`×2 / `se_ok00`×2 / `se_cancel00` / `se_select00`，全是菜单音），其余为 1。新行取 1（跟游戏内 SE 一致）。

🟡 `+0xc` 只有 id `0x14`（`se_lazer02`）与 `0x37`（`se_ch03`）为 1 —— 两个持续音，大概率是循环标志。
前者正是 `0x45ff38` 硬编码引用的槽 20。

语音 blob 的字节直接用 thcrap `stack_game_file_resolve` 返回的缓冲，不拷贝、不释放 —— 见 §3.3 E 的尾界处理。

### 3.5 资源登记（照 `assets/` 现成的形状）

```
assets/voice/
  ORDER.txt              一行一个 NAME，只追加；行号 k → id 0x54+k，wav 下标 72+k
  <NAME>.wav             16-bit PCM 单声道；建议 44100 Hz
  _src/                  原始素材 + 出处 README
```

`make voice` 校验：ORDER 行数 ≤ 32、每个文件存在且是 PCM、总字节数打印出来；
把索引表打印成可直接抄进 JSON 的形式（与 `make anm` 同款）。

`patch/th18/voice.js`：

```json
{ "SPADE_10_ACTIVATE": { "wav": "spade10_act", "volume": 100, "pan": 0 } }
```

`volume` 0–100、`pan` 为 DirectSound 单位（原生用到 `0xfe0c` = −500 一档）。

### 3.6 SDK 接口

```c
#define CE_VOICE(k)  (0x54 + (k))          /* k = ORDER.txt 行号 */
ce_play_sound(CE_VOICE_SPADE_10_ACTIVATE, player_x());
```

构建脚本从 ORDER.txt 生成 `native/voice_ids.h`（`CE_VOICE_<NAME>` 常量），写卡的人不碰数字。

## 4. 不做什么（YAGNI）

- **不做语音独占通道 / 打断 / 队列**（用户 2026-09-05 明确否决）—— 语音就是可叠加的 SE。
- **不做按需加载 / 卸载**：32 条 × 2 秒 × 44.1 kHz × 16 bit 单声道 ≈ 5.6 MB，与原生 5.8 MB 同量级，全量预加载。
- **不做独立语音音量**：跟随引擎 SE 音量（`+0x5730`）。
- **不改 `0x4767d8`**：语音不走 dat。
- **不动图鉴 / 存档 / replay 格式**：`play_sound` 不碰 RNG，replay 决定性不受影响。
- **patch 里不留任何 ZUN 字节**：cfg 表内容与 wav 名字符串一律运行时从用户的 exe 拷 —— 见 §3.4。

## 5. 风险与对抗证伪

| # | 风险 | 证伪手段 |
| --- | --- | --- |
| R1 | `ce_snd_patch_init` 的时点（早于 binhack、早于游戏入口） | 已有一手（`binhack.cpp:1724` / `runconfig.cpp:655-656`）+ card-expand 卡表现网验证；再在 `ce_snd_gate` 里回读 cave 首行确认拷贝生效 |
| R2 | codecave 内存是否可写、地址是否在 apply 时已定 | `ce_snd_gate` 打印四块 cave 的地址与首尾字节；DLL 用 `func_get("codecave:ce_snd_cfg")` 取址 |
| R3 | 52 处站点是否漏 / 是否有假阳性 | `sites.py check` 加审计：全 `.text` 重扫 cfg / wav 名 / slot / blob 四个原生地址区间，未被覆盖的必须逐条列出并人工确认 |
| **R4** | **I1 被违反**（新行 `+0` 重号或缺号） | `ce_snd_gate` 里遍历 116 行，断言 `+0` 是 `0..0x73` 的置换；不满足直接 `FAIL:` 并还原 |
| **R5** | **I2 被违反**（某行 blob 为 NULL） | 同上，断言 116 行的 `+4` 对应的 `blob[]` 全非 NULL |
| R6 | 语音 wav 格式是否被 `0x4776f0` 接受 | 先拿一个原生 wav 的字节当语音跑通，再换真语音 |
| R7 | 硬编码槽 20（`0x45ff38`）改写后是否仍指 `se_lazer02` | 日志打印 `slot[20].+8` 指向的行的 `+4`，应为 `0x26` |
| R8 | 新 slot 的 `+4` 未被初始化成 −1（`0x401139` 界没改对） | `ce_snd_gate` 之后的检查点里断言 slot[84..115] 的 `+4` 全为 `0xFFFFFFFF` |
| R9 | 12 槽播放队列溢出 | 同帧最多 12 个**不同** id；语音叠加数远小于 12 |
| R10 | 设备丢失后新 buffer 是否被 `0x45a4a0` 的重播循环覆盖 | Alt-Tab 切出切回后放语音 |

**每一处写入点必须过 `mods/_template/AUDIT-checklist.md`，追加到 `AUDIT.md` 新的一节。**

## 6. 验收

`ce_gate` 日志应有：

```
snd: caves cfg=<addr> slots=<addr> names=<addr> blobs=<addr>
snd: copied 84 cfg rows + 72 names; appended N rows (id 0x54..0x??, wav 72..??)
snd: N voice blobs loaded, <bytes> bytes total
snd: 51/51 sites verified; slot[84..115].+4 == -1; slot[20] -> wav 0x26
```

进游戏：触发一张带语音的卡 → 听得到；两张同帧触发 → **两条同时响**（可叠加，符合设计）；
Alt-Tab 切出切回 → 语音仍能放；退出游戏无崩溃、无泄漏告警。
任何 `FAIL:` / 崩溃都是回归，优先怀疑 R1（时序）、R3（漏站点）、R7（界没改对）。

## 7. 追溯

| 产出 | 位置 |
| --- | --- |
| 引擎一手（订正 + 字段图 + 链路） | `engine/_shared/th18-sound-table.md` 重写 |
| 站点清单与审计 | `mods/th18.v1.00a/card-expand/AUDIT.md` 新节 |
| binhack 源 | `mods/th18.v1.00a/card-expand/patch/` |
| DLL | `mods/th18.v1.00a/card-expand/native/sound.c` |
| 资源 | `mods/th18.v1.00a/card-expand/assets/voice/` |
| 追溯表 | `mods/th18.v1.00a/card-expand/MAP.md` 加第 11 段 |
