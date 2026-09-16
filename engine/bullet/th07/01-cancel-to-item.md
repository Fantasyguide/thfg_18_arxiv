# TH07 妖妖梦 消弹 → 出不出实体道具

> **版本**：TH07（`local/th07.v1.00b/th07.exe`，650,752 B，imagebase `0x400000`，md5 `d08774b90b821b6a2764280bb146a400`）。
> 本文裸地址默认属该版本；引用别作一律写 `th10:0x…`。
> 一手工具：`objdump -d -M intel …`（逐指令复核，含跳表按 PE 节表换算真读）；复现命令见 §4。
> 命名辅助（**非一手，只借名字**）：`local/vendor/th-re-data/data/th07.v1.00b/funcs.json`；素材：`local/_scratch/research-cancel-original-th06-th11.md` §TH07（第 453–706 行）。
> **本文与素材的分歧逐条列在 §1.2 / §2.2 / §3，一律不替素材圆场。**

## 1. 发现（一手事实）

### 1.1 池 / 结构 / 常量（本轮逐字节复核）

| 项 | 值 | 出处 |
| --- | --- | --- |
| BulletManager | **静态结构体** `0x62f958`（不是指针；调用方一律 `mov ecx,0x62f958`） | `0x4247c4`、`0x424cf1` |
| 管理器全长 | `0x37A164` B：`0x4232e0` 里 `mov ecx,0xde859` + `rep stos` | `0x4232e8`–`0x4232f2` |
| 弹池 | `0x400`(1024) 槽 × stride `0xd68`，基址 `0x63b218` | `0x42474a`、`0x424766`、`0x42476f` |
| 弹字段 | state @ `+0xbfc`（WORD）、pos @ `+0xb8c` | `0x42477f`、`0x4247be` |
| 激光池 | `0x40`(64) 槽 × stride `0x4ec`，基址 `[BM+0x366628]` | `0x42480a`、`0x424827`、`0x424830` |
| 激光字段 | in-use `+0x4d4`、flags `+0x4e4`(WORD)、state `+0x4e8`、pos `+0x498`/`+0x49c`、offset `+0x4a4`/`+0x4a8`/`+0x4ac` | `0x42483d`、`0x42484b`、`0x424864`、`0x424900`、`0x4248ed` |
| **ITEM_MANAGER** | 静态 `0x575c70`；`spawn_item` = `0x4326f0`（`ret 0xc`，3 栈参） | `0x4247c4`、`0x4247c9` |
| 道具池 | `0x44c`(1100) 槽 × stride `0x288`；环形游标 @ `[mgr+0xae2e8]` | `0x432749`、`0x4326fc`、`0x432702` |
| 道具字段 | pos @ `+0x24c`、**type @ `+0x27c`**、state @ `+0x27f`、占用 @ `+0x27d` | `0x4327d7`、`0x432819`、`0x432825`、`0x4327ca` |
| **type 分派跳表** | `0x433a5a`，10 项（`ee 0..9`）；越界 `ja` 走默认 | `0x432ebe`–`0x432ede` |
| 消弹用 type 旋钮 | `[BM+0x37a160]`（= 全局 `0x9a9ab8`），**初值 6** | `0x423314` |

跳表真读（按 PE 节表换算，非猜）：

```
0x433a5a: e52e4300 d2304300 ed334300 74354300 c4354300 b5354300 91364300 5f374300 42374300 e3384300
⇒ 0→0x432ee5  1→0x4330d2  2→0x4333ed  3→0x433574  4→0x4335c4  5→0x4335b5  6→0x433691  7→0x43375f  8→0x433742  9→0x4338e3
```

**这正好钉死「哪个字段是 item type」**：跳表只对 `+0x27c` 分派，而 `+0x27c` 收的是
`spawn_item` 的**第 2 个栈参**（`0x432816 mov cl,[ebp+0xc]` → `0x432819 mov [eax+0x27c],cl`）。
旁证：type 1 分支 `0x4330d2` 里就是蓝点计分（`0x43310a mov [ebp-0xdc],0xc350` = **50000**，
`0x433143` 起入账），type 8 分支 `0x433742` 就是 `push 0x1e`(30) → `0x42f5a2` + `push 0x46`(70) → `0x42f69f`。

### 1.2 消弹入口全集（三个函数；**激光不是第四个**）

| # | 入口 | 签名 | 覆盖 | 撒道具 |
| --- | --- | --- | --- | --- |
| ① | `0x424740` | `thiscall(mode)`，`ret 4` | **弹池 1024 + 激光池 64**（同一函数两半段） | ✅ 看 mode |
| ② | `0x424c00` | `(pos*, r)`，`ret 8` | 弹池 1024；三轴球半径 | ✅ 恒撒 `type 6` |
| ③ | `0x4249a0` | `(mode, cap)`，`ret 8` | 弹池 1024（**含 state 5**） | ✅ 撒 + 冒数字 + 入账 |
| ④ | 擦弹 | — | ❌ **TH07 没有「擦弹消去」**（素材 T7.5 的否定证据） | — |
| ⑤ | `0x424e00` | `BulletManager::shoot_laser`（vendor 名） | **发射**激光，与消弹无关 | — |

**① 的后半段就是激光消弹**：`0x42476f cmp [ebp-0x10],0x400 / jge 0x424807` —— `0x424807` 起
改成遍历 64 根激光（`0x424830 cmp [ebp-0x10],0x40`），末尾 `0x424995 ret 4` 与前半段共用一个 `ret`。
⇒ 素材 §T7.6 的 ❓「激光侧到底有没有独立入口」**答：没有独立入口，就是 ① 的第二半**；
形状与 TH06 同构 —— 沿轴**每 `32.0f`**（`0x498a84` 真值 `32.0f`，`0x424964 fadd`）一颗。

**① 的 mode 分派（`0x42479b`–`0x4247e8`，弹半段）**：

```
42479b: cmp [ebp+0x8],0x0      ; mode 0 ⇒ 0x4247f6（只置 state=5）
4247a1: cmp [ebp+0x8],0x9      ; mode ≥9 ⇒ 0x4247f6（只置 state=5）
4247a7: cmp [ebp+0x8],0x3      ; mode 3..8 ⇒ 0x4247d0
4247d0: push 0x1 / push 0x8    ;   (pos, type=8, state=1)   ← 6a 08 是字面量
4247ad: mov eax,[ebp+0x8]      ; mode 1..2 ⇒
4247b0: push eax               ;   第 3 栈参 = mode（= state）
4247b4: mov edx,[ecx+0x37a160] ;   第 2 栈参 = [BM+0x37a160]（= type，初值 6）
4247c9: call 0x4326f0          ; ItemManager::spawn_item
```

激光半段同一张表（`0x4248b8 cmp [ebp+0x8],0x0` / `0x4248c2 cmp [ebp+0x8],0x9` /
`0x42492b cmp [ebp+0x8],0x3`），多一条**保护位**：`0x424852 and edx,0x4` + `0x424859 cmp [ebp+0x8],0xa`
⇒ **只有 `mode == 0xa`(10) 才连带 flags bit2 的激光一起收**，其它 mode 跳过它。

### 1.3 「出的那几条 / 不出的那几条」

**出实体（要走过去收）**

| 入口 / mode | 出的东西 | 每颗几个 | 值（一手落点） |
| --- | --- | --- | --- |
| ① mode `1`/`2`（弹 + 激光） | `type = [BM+0x37a160]`，**初值 6**；state = mode | 每颗弹 1 颗；激光沿轴每 `32.0f` 1 颗 | type 6 handler `0x433691`（§1.4 表） |
| ① mode `3..8`（弹 + 激光） | **固定 `type 8`**（`push 0x8`，与 mode 无关）；state 1 | 同上 | `0x433742`：`+30` → `0x42f5a2`、`+70` → `0x42f69f` |
| ② `0x424c00` | `push 0x6` ⇒ **`type 6`**，state 1 | 圆内每颗 1 颗 | 同 type 6 |
| ③ `0x4249a0` | `type = [BM+0x37a160]`（初值 6），state 1 | 每颗 1 颗（**含原本已在 state 5 的**） | 见下 |

**不出实体（只杀）**：① `mode 0`、① `mode ≥ 9`（含 `0x420131` 传的 `0xa`）。
两条都只写 `state = 5`（`0x4247f6`），激光侧写 `state = 2` + 清 `+0x4d0`（`0x424877`、`0x424975`）。

**③ 的特殊性**：它**同时**撒实体**和**冒数字入账 —— `0x424a15 mov ecx,0x575c70` + `0x424a1a call 0x4326f0`（ITEM_MANAGER）
与 `0x424a43 mov ecx,0x134ce18` + `0x424a48 call 0x4024f0`（AsciiManager 数字槽）**在同一函数体里，两个都有**（每颗 1 spawn + 1 冒数字）。
累加与自增真值（`0x4249b0` 起）：起点 `0x7d0` 即 **2000**、每颗 `+0x14` 即 **+20**、
上限 = 第 1 个栈参（调用点 `0x4101d3 push 0x1f40` = **8000**）；函数返回累计和，
调用方 `0x4101e9`–`0x410227` 把它交给 `0x423090(sum, 0x1f40)`，**再加 `返回值 / 10` 到 `score`**
（`0x410210 idiv 10` → `0x410212 add [player+0x4]`）。

### 1.4 type 6 / type 8 的值（一手落点，语义标 🟡）

| type | handler | 冒数字 | 分 | 樱点侧 |
| --- | --- | --- | --- | --- |
| 6 | `0x433691`–`0x43373d` | `(player[+0x18]/0x28)*0xa + 0x12c`；`[0x4bfeb4]!=0` 时固定 **100** | `score += 值/10`（`0x4336f4 idiv 10`） | `[0x4d44f8]==0` ⇒ `push 0x14`(20) → `0x42f5a2`；否则交替 `push 0xa` → `0x42f5a2` / `0x42f69f` |
| 8 | `0x433742`–`0x43375a` | — | — | `push 0x1e`(30) → `0x42f5a2`；`push 0x46`(70) → `0x42f69f` |
| 1（对照） | `0x4330d2` | 值 = **50000**（线上）或 `50000 − 高度×100`（`0x43312d imul 0x64`） | `值/10` 入账 | — |

🟡 `0x42f5a2` / `0x42f69f` 的名字（「樱点 & +C 同加」/「只加樱点」）来自我们卡的注释（`senro_core.c`），
本轮**没有**一手复核这两个函数体；`[0x4bfeb4]` / `[0x4d44f8]` 是什么也 ❓ 未钉死。

### 1.5 关键常量真值

| 地址 | 真值 | 用在哪 |
| --- | --- | --- |
| `0x498a84` | `32.0f` | 激光沿轴撒点步长（`0x424964`） |
| `0x498c78`/`0x498b34`、`0x498c74`/`0x498b08` | `288.0f`/`48.0f`、`192.0f`/`64.0f` | state 2 随机速度 `x=rand*288+48`（`0x432864`）、`y=rand*192−64`（`0x432883`） |
| `0x498b28` | `4294967296.0f`（= 2³²） | RNG 归一化：`0x431900` = `整数 / 2³²` ⇒ `[0,1)`（`0x43191e`） |
| `0x498d20` | `-0.5f` | 道具速度钳位（`0x433b9c`） |
| `0x433a5a` | 跳表 10 项 | item type 分派（§1.1） |

### 1.6 RNG 结论：⚠ **素材把 `0x48b8a0` 当随机源是误判；真 RNG 只有 `state == 2` 那一条**

* **真 RNG = `0x431900`**（thiscall，`ecx` = RNG 静态对象 `0x49fe20`）：内部 `0x43190c call 0x4318d0` 取 32 位整数 → `fild` → 除 `[0x498b28]` = **`4294967296.0f`（2³²）** ⇒ 返回 `[0,1)` 的 f32（TH06 源码里是 `g_Rng.GetRandomF32ZeroToOne()`）。
* `spawn_item` 里**只有 `state == 2`** 调它，每颗 **2 次**（`0x432854 cmp [ebp+0x10],0x2` → `0x43285f` / `0x43287e`），用于随机速度（`rand*288+48`、`rand*192−64`）。
* 消弹传的 state：① `mode 1/2` ⇒ state = mode ⇒ **`mode 1` 零 RNG、`mode 2` 每颗 2 次**；② `0x424c00`、③ `0x4249a0` ⇒ state = 1 ⇒ **零 RNG**。
* ⚠ **订正（素材 §T7.7「每撒一颗取一次随机数」不成立）**：素材指的 `0x432718 call 0x48b8a0` **不是随机源** —— `0x48b8a0`–`0x48b914` 只碰 x87 栈与 `[esp]`，**零全局读写** ⇒ 是 **float→int64 截断辅助**（MSVC `_ftol` 家族）。它接的是 `fld [player+0x7c]`（`0x432715`）+ `cmp eax,0x80`（`0x43271d`）⇒ 那一步是 **满 P（≥128）时把 `type 0/2` 改写成 `7`**（`0x432730`；素材同段也写了「满 P ⇒ type → 7」，两句互相矛盾）。
* 同理订正：type 1（蓝点）计分里的 `0x433128 call 0x48b8a0` **也不是随机** —— 它把 `高度 − 收集线`（`0x43311f`–`0x433125`）截断成整数，再 `0x43312d imul eax,eax,0x64` ⇒ `50000 − 高度×100`，**确定性**（正是我们卡 `senro_core.c` 的读法）。
* ⇒ **改判**：**TH07 的消弹撒道具是确定性的**（除 `0x424740(2)` 那条「随机速度」支），与 TH10/TH11 同档；素材的「消弹消耗 RNG、影响后续随机弹幕」**不成立**。
* 🟡 `[player+0x7c]` 的身份（读作「火力」、阈值 `0x80` = 128 满 P）没有独立复核，但与 TH06 的满 P 值 128 一致。

### 1.7 boss 击破 / 符卡结束 / 关卡切换属哪一档

* **引擎侧没有「击破清弹」专用函数**（与素材同口径：TH07 的弹幕消失来自 ECL / 敌机死亡处理器）。
  能看到的清弹**全部**是 §1.2 那三个入口的调用点（穷举见 §2.1），所以「哪一档」只能按调用点判：
  * `0x420131`（在 `run_timeline` `0x41f6f0` 内）传 `mode 0xa`（`0x42012a push 0xa`）⇒
    **全屏强清（连 flags bit2 的保护激光也收）+ 一颗道具都不出**。这是本作唯一一条「强清且不撒」的一手点。
  * `0x4101dd`（`0x4101a0` 内，前置门 `0x4101c4 cmp [0x12fe0c8],0x1`）与 `0x421bbe` 走 ③ ⇒
    **撒实体 + 冒数字 + 当场入账**。
  * `0x416349`（ECL 解析器）传 `mode 0` ⇒ **只杀不撒**（对应 ECL 的「推不出就写 0」）。
* ❓ **逐条归属没钉死**：9 个 ① 调用点里哪个是 Bomb、哪个是结界、哪个是道具超时/回收链，
  素材标 ❓，本轮也没能靠调用者语义全部定死（我能给出的是每处的 `push` 真值，见 §2.1）。

## 2. 验证（怎么排除掉别的路）

### 2.1 调用点穷举（可复现）

```
0x424740 ×9（直接 call）
  0x40fcfd push 0x1  Enemy::ecl_spell(0x40fc90，vendor 名)   ★ 素材标「Bomb 区 / 灵击」
  0x415c17 push 0x1  EclManager::ecl_parser
  0x416349 push 0x0  EclManager::ecl_parser                  ⇒ mode 0 = 只杀不撒
  0x420131 push 0xa  run_timeline                            ⇒ mode 10 = 强清 + 不撒
  0x429b1f push 0x1  （0x429935 段，vendor 名不可信）
  0x433001 push 0x1  ItemManager::on_tick__…（前置 [0x12fe0c8]==0）
  0x4334ad push 0x1  同上
  0x4335e0 push 0x1  同上（前置 RNG < 0x80）
  0x441361 push 0x0  Player::sub_441330（0x441330）           ⇒ 只杀不撒
0x4249a0 ×2 : 0x4101dd（push 0x1 / push 0x1f40）、0x421bbe
0x424c00 ×1 : 0x41627a（ECL 解析器的 opcode 分支）
```

* **素材写「`0x424740` ×12」是计数错**：全 `.text` 线性扫，函数入口 `0x424740` 在整份 asm 里
  只出现 10 次 = 1 次定义 + 9 次 `call`；**没有** `jmp`、没有把该地址当 dword 存进表/vtable 的用法
  ⇒ 无间接调用，9 处就是全集（`METHOD.md` 第 4 条：字节穷举只当候选发现器，这里用作**证否**）。

### 2.2 「③ 只是冒数字、不撒道具」这条被指令否掉

`0x4249a0` 体内**两个东西都在**：`0x424a15 mov ecx,0x575c70` + `0x424a1a call 0x4326f0`（撒），
`0x424a43 mov ecx,0x134ce18` + `0x424a48 call 0x4024f0`（冒数字）。
⇒ 素材 §T7.2 表头 / §T7.3 正文的「不出实体」**与字节不符**；
素材自己的 §T7.8 ④「也撒」才对，但它把撒出的 type 记成「type 1」——按 §1.1 的字段落点，
那个 `push 0x1` 是 **state**，type 是 `[BM+0x37a160]`（初值 6）。

### 2.3 type 落点不是猜的

1. `0x432816`–`0x432819`：第 2 栈参（`[ebp+0xc]`）写进 `+0x27c`；`0x43281f`–`0x432825`：第 3 栈参写进 `+0x27f`。
2. `0x432ebe`：`movsx edx,BYTE [item+0x27c]` → `cmp edx,0x9` → `jmp [eax*4+0x433a5a]`（10 项跳表）。
3. 跳表 type 1 → `0x4330d2`（50000 蓝点计分）、type 8 → `0x433742`（30/70 樱点），与我们卡注释吻合。
4. 与 TH06 源码同构：`ItemManager::SpawnItem(D3DXVECTOR3 *position, ItemType itemType, int state)`（`ItemManager.cpp:21`）⇒ `(pos, type, state)` 参数序一致，第 3 个参数才是 state。
⇒ 素材的「`mode 1/2 ⇒ type = mode`」不成立：`mode` 是 **state**（所以 `mode 2` 才会走随机速度支）。

### 2.4 函数边界与「两个入口共用一个 ret」

`0x424740 … 0x424995 ret 4`（弹 + 激光）、`0x4249a0 … 0x424bf5 ret 8`、`0x424c00 … 0x424d10 ret 8`。
激光半段（`0x424807` 起）**没有自己的 `push ebp`**，且 ① 的前半段用 `jge 0x424807` 直接跳进去
⇒ 它属于同一个函数，**不是**独立入口。

## 3. 结论（可信度 + 适用范围）

**一句话**：TH07 的消弹撒**实体道具**，每颗弹 1 颗，掉在弹自己的位置；
`type` 由 `mode` 分档（`0/≥9` ⇒ 只杀不撒；`1/2` ⇒ `[BM+0x37a160]`，**初值 6**，`mode` 落 state；
`3..8` ⇒ 固定 `type 8`）；半径入口（ECL 161）另撒 `type 6`；
第三个入口 `0x4249a0` **既撒实体又冒数字并当场入账**；三个入口**都没有场地门**；
`spawn_item` 的随机**只在 `state == 2`（= `mode 2`）那一条速度支上取**，`mode 1` / ② / ③ **零 RNG**；激光消弹不是独立入口，
而是 `0x424740` 的后半段（64 根，沿轴每 `32.0f` 一颗）。

* ✅ **一手（本轮逐字节复核）**：池/字段/常量（§1.1 表）；三个入口的签名与 `ret`；mode 分派四条分支与其落点；
  激光半段的存在与 `32.0f` 步长；跳表 `0x433a5a` 的真值；`type → +0x27c`、`state → +0x27f` 的写入指令；
  `spawn_item` 的真 RNG 调用点（`0x431900`，仅 `state == 2`）；`0x48b8a0` 是 float→int 辅助的**证否**
  （函数体零全局读写）；9+2+1 处调用点穷举与每处的 `push` 值。
* 🟡 **中**：参数语义（`arg1=type` 这一层是**推断**，证据链见 §2.3，指令落点本身是一手）；
  opcode 号 161/162（**无 th07 eclmap**，素材按 th08 同位置对照推）；`0x423090` 的语义（入账比例只能给到「返回值/10」）；
  `0x42f5a2`/`0x42f69f` 的名字；`[player+0x7c]` 的身份（满 P 阈值 128）；
  「没有场地门」的**否定范围**只覆盖三个函数体 + `spawn_item` 我读到的部分。
* ❓ **未钉死**：`0x424740` 9 处调用点的逐条归属；`0x429bc9`/`0x429bf3`/`0x429c13` 把 `[BM+0x37a160]` 改成 **9** 的那条路
  是什么（同一函数里有 8 项跳表 + `call 0x427d92`，像是某种符卡/敌机专用清屏）；`[0x4bfeb4]` / `[0x4d44f8]` 的身份。

**适用范围**：只对 `th07.v1.00b` 这个 build 的字节负责；**mode 号 / type 号都不可跨作搬**
（TH10/TH11 根本没有 `mode` 参数，TH16 的 `mode 1` = 一颗不撒，TH18 的 `mode 1` = 撒 `type 9`）。

## 4. 证据（地址 / 常量 / 复现命令）

```bash
# ①② #1 的两半段（弹 mode 分派 / 激光半段：64 槽 + 32.0f 步长）
objdump -d -M intel --start-address=0x424740 --stop-address=0x424810 local/th07.v1.00b/th07.exe
objdump -d -M intel --start-address=0x424807 --stop-address=0x424995 local/th07.v1.00b/th07.exe
# ③ 第三入口（撒 + 冒数字都在）
objdump -d -M intel --start-address=0x4249a0 --stop-address=0x424a90 local/th07.v1.00b/th07.exe
# ④ 半径入口（push 0x6 / 三轴球）
objdump -d -M intel --start-address=0x424c00 --stop-address=0x424d10 local/th07.v1.00b/th07.exe
# ⑤ spawn_item（arg1→+0x27c、arg2→+0x27f、真 RNG 0x431900 只在 state==2 支）
objdump -d -M intel --start-address=0x4326f0 --stop-address=0x4328a0 local/th07.v1.00b/th07.exe
# ⑤′ RNG 本体（0x431900 = 整数/2³²）与「不是 RNG」的 0x48b8a0（零全局读写）
objdump -d -M intel --start-address=0x431900 --stop-address=0x431928 local/th07.v1.00b/th07.exe
objdump -d -M intel --start-address=0x48b8a0 --stop-address=0x48b918 local/th07.v1.00b/th07.exe
# ⑥ type 分派函数头 + 跳表（真值 = 40 字节，按 PE 节表换算 RVA）
objdump -d -M intel --start-address=0x432ea0 --stop-address=0x432f00 local/th07.v1.00b/th07.exe
objdump -s -j .text --start-address=0x433a50 --stop-address=0x433a84 local/th07.v1.00b/th07.exe
# ⑦ type 6 / 8 / 1 的 handler 体
objdump -d -M intel --start-address=0x4330d2 --stop-address=0x433160 local/th07.v1.00b/th07.exe
objdump -d -M intel --start-address=0x433691 --stop-address=0x433760 local/th07.v1.00b/th07.exe
# ⑧ 调用点穷举（+ 证否间接引用：入口地址全 asm 只出现 10 次）
grep -nE 'call +0x424740|call +0x4249a0|call +0x424c00' local/_scratch/th07_full.asm
grep -c '424740' local/_scratch/th07_full.asm        # ⇒ 10 = 1 定义 + 9 调用
# ⑨ 常量真值（objdump -s 的 16 进制即小端 f32：0x498a84 = 00000042 = 32.0f）
objdump -s -j .rdata --start-address=0x498a80 --stop-address=0x498aa0 local/th07.v1.00b/th07.exe
#   同法：0x498c78=288.0f  0x498b34=48.0f  0x498c74=192.0f  0x498b08=64.0f  0x498d20=-0.5f
md5sum local/th07.v1.00b/th07.exe   # d08774b90b821b6a2764280bb146a400
```

## 5. 对卡的直接影响（只读观察，不改代码）

看的是 `mods/th18.v1.00a/card-expand/native/cards/senro.c`（`grep -n "ce_cancel\|cancel" senro.c`，
命中 20+ 行）与 `senro_core.c`。

| 项 | 我们卡现状 | 一手（TH07） | 判定 |
| --- | --- | --- | --- |
| 消弹调用 | `ce_cancel_radius(pos, 4000.0f, 9999, SENRO_CANCEL_MODE)`，`SENRO_CANCEL_MODE = 0`（`senro.c:69`、`:527`、`:569`） | `0x424740(0)` 也**只杀不撒** | ✅ 方向一致（都「零实体」；走的是 TH18 引擎的 mode 表） |
| 载体 | 引擎纯删除 + `senro_small_cherry(n)` 按**颗数**记账 | TH07 是**撒实体**（要走过去收） | ⚠ 已知载体差异（同素材 §1.1-U1，卡侧取舍） |
| 小樱点 | `senro_small_cherry`：每颗 `+30` / `+70`（`senro_core.c:265`） | type 8 handler `0x433742` = `push 0x1e`(30) + `push 0x46`(70) | ✅ 数值一致 |
| 蓝点分值 | `senro_point_item_value`：50000 起、`/5`、`round10`（`senro_core.c:249`） | `0x43310a` = 50000、`0x4331c8 idiv 5`、`0x4331db` 取整到 10 | ✅ 数值一致 |
| 型号码 | 注释写「th07 type 1（`0x4330db`）＝蓝点」「th07 **type 7**（`0x43370b`/`0x433712`）＝★」「th07 **type 2**（`0x433886`）＝1000+100×符卡奖励」 | 跳表 `0x433a5a`：`0x4330db`→type **1** ✅、`0x43370b`→type **6**、`0x433886`→type **7** | ❓ 两处型号码对不上（地址对、号错）；留给卡作者核 —— **本轮不改任何代码** |

⚠ 另注：卡里 `SENRO_CANCEL_MODE = 0` 的理由（「mode 1 会让引擎撒出 `type 9–0xe`」）说的是 **TH18** 的 mode 表
（见 `engine/bullet/th18/01-cancel-to-item.md` §1）：TH07 的 `mode 1` 是「撒 `[BM+0x37a160]`（初值 6）+ state 1」，**两类 mode 表不能互相印证**。
