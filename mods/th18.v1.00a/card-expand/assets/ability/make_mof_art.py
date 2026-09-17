#!/usr/bin/env python3
"""风神录（id 110）· 地灵殿（id 111）共用的**绿点信仰实体**贴图 —— 从 TH10 原版图切。

    python3 make_mof_art.py

=====================================================================================
★ 2026-09-12 订正块（做 110 的 HUD 原生切图时顺手查清的；**只改注释与认知，产物一个字节没动**）
=====================================================================================

本文件下面「脚本原文」一节把 TH10 的 raw opcode 猜成了 `ins_63 = delete` / `ins_66/ins_67 没对上`
/ `ins_53 = rotate` —— **三条都不对**。理由：TH10 `front.anm`/`bullet.anm` 的 entry 是
`version: 4`，而 `local/vendor/thpages/static/mapfile/v4.anmm` 的抬头**自己写着**
「for version 4: StB, Mof / and version 7: SA, UFO, DS, GFW」—— 也就是说 **TH10 有官方助记符表**，
本文件当初是**不带 `-m` 直接 dump**（所以才看到 `ins_N`）。带上它重读即可对出真名：

    local/vendor/thtk/build/thanm/thanm -l 10 local/th10.v1.00a/dat/bullet.anm \
        -m local/vendor/thpages/static/mapfile/v4.anmm

订正后的对应（v4 号 → 助记符）：

| v4 opcode | 真名 | 本文件原来的说法 |
| --- | --- | --- |
| 51 | `alpha` | ✅ 对 |
| 50 | `scale` | ✅ 对 |
| 60 | `scaleTime` | ✅ 对 |
| **53** | **`angleVel`**（角速度）| ❌ 原写 `rotate`；`rotate` 是 49 —— 所以 faith dot 那条
  脚本的真实语义是「**ωz = π/4 /帧 的自转**」，不是「定角 45°」 |
| **63** | **`stop`** | ❌ 原写 `delete`（`delete` 是 1）—— 且原文末尾那条 `ins_63()` 在 `+32:` 段，
  是「停止脚本」而不是「删除 VM」 |
| **66 / 67** | **`blendMode(1)` / `type(1)`** | ❌ 原写「没对上、不搬」—— 两条都有名字，可搬 |
| 89 | `resampleMode` | ✅（本文件没用它）|
| 85 / 86 | mapfile 里**无名**（`# 85: unknown flag`）| 本文件与 `209/210` 都**没搬**（v4→v8 号不通用）|

⇒ **`assets/ability/scripts/185_faith_dot.anm.txt` 的注释与内容需要一次订正**（不在本轮的改动
范围内，作者/主 agent 处理）：注释里的「ins_63 = delete」「ins_53 = rotate」「ins_66/67 没对上」
换成上表，并把丢掉的 `blendMode(1)`、`type(1)` 补回去（那两条影响出生动画的混合模式与类型）。
本轮新写的 `209_mof_faith_icon.anm.txt` / `210_mof_faith_digit.anm.txt` 已经是**带 v4 mapfile
逐条读出来的原文**（见那两个文件头的「一手原文」段）。

======================================================================================
一手考证：TH10 的信仰点（绿点）贴图在哪
======================================================================================

■ 结论（✅ 全部一手；2026-09-15 定案，取代下面 §候选 里的 🟡 推理）
  源图 = **`local/th10.v1.00a/anm/bullet/bullet/etama2.png`**（256×256，TH10 `bullet.anm`
  **entry2**，`format: 5` = RGBA）；y=208 那一行是 **16 个 16×16 的「道具球」单元**
  （sprite344..361，其中 [112,208] 与 [240,208] 各被两个 sprite 号共用）。
  **道具类型 → 该道具的 sprite 号 = 343 + 类型**（一手链见 §2026-09-15 定案）⇒

  | 档 | TH10 道具类型 | 一手地址 | sprite | 矩形 | 产物 |
  | --- | --- | --- | --- | --- | --- |
  | **大信仰点** | **3** | 分数表 `th10:0x41b57f`（+5000/8000/10000、弹窗色 `0xff00ff00`）| `sprite346` | `(32,208,16,16)` | `FAITH_DOT_BIG.png` |
  | **小信仰点** | **9** | 分数表 `th10:0x41b5fc`（+100、弹窗色 `0xff00ff00`）| `sprite352` | `(112,208,16,16)` | `FAITH_DOT.png` |
  | **微信仰点** | **8** | 分数表 `th10:0x41b5d6`（+10）| `sprite351` | `(112,208,16,16)` | `FAITH_DOT.png`（**与小同矩形**）|

  ⚠️ **小 / 微 在 TH10 里共用同一个矩形**（`sprite351` 与 `sprite352` 在 `bullet.anm` 里
  字面同值 `(112,208,16,16)`）—— 这是 ZUN 自己的重号，不是我们切的错。两者的差别只在
  **脚本**：type 9 的 `script383` 与 type 3 的 `script377` 逐条同形（`scale(0.5→1)`、
  `alpha` 默认 255），而 type 8 的 `script382` 是 `blendMode(1)` + `alpha(16)` + `scale(0→1)`
  的加算淡出球（见 `engine/anm/th18/07-…` 对 TH18 加算语义的说明）。
  产物**不缩放**：16×16 就是原生矩形尺寸 ⇒ 原样切、零补边。

======================================================================================
★★ 2026-09-15b 三档外观：**微档另切一格 `(240,208)`**（本轮新增 `FAITH_DOT_MICRO.png`）
======================================================================================

■ 第一问：原版到底有没有第三张**微档**图 —— **没有**，TH10 / TH11 都没有

  | 版本 | 大档 | 中/小档 | 微档 | 一手 |
  | --- | --- | --- | --- | --- |
  | TH10 | type 3 → sprite346 `(32,208)` | type 9 → sprite352 `(112,208)` | type 8 → sprite351 `(112,208)` | script377 / 383 / 382 正文 |
  | TH11 | type 3 → sprite347 `(32,208)` | type 9 → sprite353 `(112,208)` | type 8 → sprite352 `(112,208)` | script99 / 105 / 104 正文 |

  · 两作的**小档与微档都是同一个矩形**（`sprite351` 与 `sprite352` 字面同值 = ZUN 自己的重号）；
  · 复核命令（**TH11 也吃 v4 表**，用 `v8.anmm` 会把 `sprite()` 静默译成 `stop()`）：

        local/vendor/thtk/build/thanm/thanm -l 10 local/th10.v1.00a/dat/bullet.anm \
            -m local/vendor/thpages/static/mapfile/v4.anmm
        local/vendor/thtk/build/thanm/thanm -l 11 local/th11.v1.00a/dat/bullet.anm \
            -m local/vendor/thpages/static/mapfile/v4.anmm

  · 道具类型只有 1..11（分派表 `th10:0x41b3ce` 的 11 项跳转，长度已核）⇒
    `(160,208)` / `(240,208)` 那几格**不对应任何可达的道具类型**（要 type 12 以上才会走到）。
  ⇒ **原版没给微档单独画图**：ZUN 自己的做法就是「同一格贴图 + 另一条脚本」。

■ 第二问：那还有没有别的候选 —— **有**，`on_draw__body` 其实是**两族** sprite

  `th10:0x41b8e0` 逐条读（`objdump -d -M intel --start-address=0x41b8e0
  --stop-address=0x41ba00 local/th10.v1.00a/th10.exe`）：

      41b92b: fld   DWORD PTR [esi]          ; 画用 y = 道具 y + 16.0（常量 `0x470b48`）
      41b92d: fcomp DWORD PTR ds:0x470bd0    ; 与 **8.0** 比
      41b938: jp    0x41b99f                 ; y ≥ 8（或 NaN）→ 「家族 A」
      41b990: lea   edx,[eax+0x160]          ; 否则 sprite = **vtype + 352**（家族 B）
      41b9bf: lea   edx,[eax+0x157]          ; 家族 A：sprite = **vtype + 343**

  （`eax` = `[esi+0xac]` = 视觉类型 vtype；41b93a–41b96d 那支还把画用 y 夹到 `0x41c00000`=24.0f、
  按 `(y−8) × 0.03125 × 255` 算 alpha ⇒ y<8 那一支基本是「顶出屏幕外的不可见档」。）
  ⇒ 同一张 `etama2.png` 的 y=208 行被读成**两族**：`343+vtype`（x=0..112）与
  `352+vtype`（x=128..240），vtype 1..9 各占一个 sprite 号。

  | 族 | vtype 3（大信仰点）| vtype 8（微）| vtype 9（小）|
  | --- | --- | --- | --- |
  | A = `343+vtype` | sprite346 `(32,208)` 大绿花 | sprite351 `(112,208)` 12px 绿珠 | sprite352 `(112,208)` 同格 |
  | B = `352+vtype` | sprite355 `(160,208)` 10px 素绿点 | sprite360 `(240,208)` 同款 | sprite361 `(240,208)` 同款 |

  · `(160,208)` 与 `(240,208)` **逐像素只差 2 个像素**（全格 256 个像素里）⇒ 是同一张素色绿点；
  · `(112,208)` 与 `(240,208)` 差 133 个像素（不透明像素 76 vs 66）⇒ 一眼能分的两张图
    （`python3 make_mof_art.py --dump` 会把这三格并排算出来）。

■ 本轮决定：**微档 = `(240,208)`**（= 家族 B 的 sprite360/361）

  · **像素 100% 原版**：同一张 `etama2.png`、同一行 y=208、ZUN 画的素色绿点 —— **零自绘**；
  · **号也是一手的**：`sprite360/361` 正是 `on_draw__body` 在家族 B 里给 **vtype 8/9**
    （微/小信仰点的视觉类型）设的那个号；
  · ❓ **自定的只有「把它指给微档」这一步**：原版在**可见分支**里给 type 8 的仍是 `(112,208)`
    （与本仓小档同图）；原版既然没有第三张微档图，这一层映射只能我们自己定。
    用户 2026-09-15 已授权「图我不管，自造也行」，而这里连自绘都不必（有原版图可切，按仓规优先）。
    已登记 `TUNING.md` §2.6。
  · ⇒ 三档**三图各不相同**：16px 大绿花 / 12px 绿珠 / 10px 素绿点；再叠一层脚本差
    （微 = `blendMode(1)` + `alpha(16)` + `scale(0→1)`；大/小 = `alpha 255` + `scale(0.5→1)`）。
    微档脚本逐条见 `assets/ability/scripts/273_mof_faith_dot_micro.anm.txt`。
  · ⚠️ **暂时看不到**：本仓微档**不建实体**（`faith_spawn()` 对 `FAITH_TIER_MICRO` 直接 `return 0`，
    见 `faith.c` 文件头）⇒ 这一档今天只存在于代码与图集里，实跑要等微档实体的 TODO 落地。

■ 怎么找到的（可复核，别信我的话，跟着跑）

  1. 线索排除：`front.anm` 只有 1 个 16×16（`front00.png` sprite29 @(256,80)）—— 那是
     一个图标，不是道具；TH10 的道具贴图在 **`bullet.anm`**（4 张 etama 图）。
  2. **`ItemManager__spawn_item` `0x41bb00`**（th-re-data `local/vendor/th-re-data/data/th10.v1.00a/funcs.json`）
     里 **类型 → anm 脚本号** 的表就在这里（一手汇编，见下），**不是**在 `0x41beb0`：
     `0x41beb0` 是极坐标 → 速度的 `FSINCOS` 小函数（入参 `ecx` = 输出 float[2]、`[esp+8]` = 角度、
     `[esp+0xc]` = 速度，把 `cos*speed`/`sin*speed` 写进 `[0]`/`[4]`）—— **任务里说的
     「0x41beb0 是道具初始化」是误判**，附此更正。

        0x41bc47:  cmp   ebp, 0xa          ; ebp = 道具类型
        0x41bc4a:  mov   [esi+0x3e0], ebp   ; 存类型
        0x41bc56:  jne   0x41bc8c
        0x41bc6f:  mov   eax, 0x177         ; 类型 0xA → 脚本 375
        0x41bca1:  mov   eax, 0x17a         ; 类型 0xB → 脚本 378
        0x41bca8:  lea   eax, [ebp+0x176]   ; 其余    → 脚本 类型 + 374
        0x41bcb5:  call  0x404f30          ; AnmVm__run_script(VM@esi+0x3b8, 脚本号 eax, anm_id)

     ⚠️ **与任务给的地址有偏差，说明如下**（`0x41b8e0` 确实不在指令边界上 —— 那是**数据**：
     它是 `bullet.anm` 的 `id`/`nameoffset` 表，th-re-data 的地址在这一段可信但
     `on_draw__body` 的**入口**要从 `0x41b8e0` 前一条完整指令起读；我用 Capstone 从
     `0x41b8e0` 开始线性扫描得到的是 `push ebx; push ebp; push esi; push edi; mov edi,eax` ——
     即 **`0x41b8e0` 就是 `on_draw__body` 的入口**，能对上指令流，th-re-data 没错）。
     定位方法：`python3 -c "import capstone,…"` 线性扫 `.text`（**别用 `local/_scratch/th10_full.asm`**，
     那份平坦反汇编在 `0x41bc09`–`0x41bc10` 的 **byte 跳转表**处跑飞了，把数据当代码印，
     同一段在 `0x41bd00` 附近更是整个错位 —— 这就是「以能对上指令流的那份为准」的意思）。

  3. 拿脚本号去 `thanm -l 10` 的清单里读正文（下面「脚本原文」一节），**脚本正文里的
     `stop(spriteN)` 就是本地 sprite 号**，再用 `bullet.anm.txt` 的 sprite 矩形表换算成像素矩形。

  ⚠️ **TH10 的 mnemonic 与 TH13+ 不同**：`thanm -l 10` 打的是 `ins_50/ins_60/ins_53/...`
  （版本 10 没有 anmmap 别名），上面那份清单里是宏名。按**同一条指令在 TH13 `astral.anm` 里的
  已知语义**（`scripts/174..` 那批就是逐条搬的，见 `make_trance_art.py` 文件头）对应：
  `ins_51` = `alpha`、`ins_50` = `scale`、`ins_60` = `scaleTime`、`ins_53` = `rotate`、
  `ins_63` = `delete`、`ins_67`/`ins_66`/`ins_82`（🟡）= 混合/开关类，本次**不搬**（见报告）。

■■ 2026-09-15 定案：三档信仰点的**类型号 → sprite 矩形**（补上 §候选 缺的那一步）■■

  §候选 卡住的那一步（「哪一格是信仰点」）本轮**用三条独立的一手链钉死了**，全部可在
  `local/th10.v1.00a/th10.exe` 上复跑（Capstone，imagebase 0x400000）：

  **① 类型号 → 信仰值**（`ItemManager::on_tick__body` th10:0x41afd0 里的类型分派）
      `0x41b3c1: mov eax,[ebp+0x30]`（ebp = item+0x3b0 ⇒ 读的是 **item+0x3e0 = 道具类型**）
      `0x41b3c4: dec eax; cmp eax,0xa; ja skip`
      `0x41b3ce: jmp dword ptr [eax*4 + 0x41b8a0]`   ← 11 项跳转表（索引 = 类型−1）
      表内容（本轮 hexdump 读出）：idx2(type3)→`0x41b57f`、idx7(type8)→`0x41b5d6`、idx8(type9)→`0x41b5fc`
      · `0x41b57f`：难度表 E/N `0x1388`=5000 · H `0x1f40`=8000 · L/EX `0x2710`=10000，
        弹窗色 **`0xff00ff00`（纯绿）**，再 `call 0x41be80`（= 加信仰）
      · `0x41b5fc`：弹窗色 **`0xff00ff00`**、`add_faith(100)`
      · `0x41b5d6`：`add_faith(10)`（无数值弹窗色）
      ⇒ **type 3 = 大信仰点 · type 9 = 小信仰点 · type 8 = 微信仰点**（与本卡
        `faith.h` 的 BIG/SMALL/MICRO 三档、`TUNING.md` §2 的 5000/100/10 逐条对上）。

  **② 类型号 → 出生脚本号**（`ItemManager::spawn_item` th10:0x41bb00）
      `0x41bca8: lea eax,[ebp+0x176]` ⇒ **脚本 = 类型 + 374**（0xA→375、0xB→378 是两条特例）
      type 8 走 `0x41bccd` 的**轮转池**（2048 槽、state=5 + 倒计时），倒计时到点在
      `0x41b010-0x41b045` 补跑同一支 `类型+374` 脚本 —— 所以 type 8 一样有 VM、一样有贴图。

  **③ 脚本号 → sprite 号**（`thanm -l 10 bullet.anm -m v4.anmm` 的脚本正文）
      `script377 { sprite(sprite346); … }` / `script383 { sprite(sprite352); … }` /
      `script382 { sprite(sprite351); … }`，而 **sprite(343+类型)** 与 `on_draw__body`
      （th10:0x41b8e0，`lea edx,[eax+0x157]` / `lea edx,[eax+0x160]`，eax = `item+0x3e4` = 视觉类型）
      里那两处 `set_sprite` 逐条吻合。
      ⇒ **sprite 号 = 343 + 类型**，§候选 里那个「脚本号与 sprite 号错开 30」的疑惑到此消解
        （374+类型 ↔ 343+类型 本来就是两套编号：脚本号与 sprite 号各有一张表）。

  **矩形 → 像素**：`entry2` 的 sprite 表（`thanm -l 10` 原样）：
      `sprite346: {x:32, y:208, w:16, h:16}`（大）· `sprite351` **与** `sprite352` 都是
      `{x:112, y:208, w:16, h:16}`（小 = 微，ZUN 自己的重号）· `sprite360`/`sprite361` 同为 `(240,208)`
      （第二组「盘/箭头」那一族的同一重号现象）。

  **交叉校验（不用信上面任何一条）**：`front.anm` 的信仰 HUD 图标 `sprite29 (256,80,16,16)`
  = 本文件 §候选 里 x=16 那一格**逐像素相同**（同为「带纹样的珠子」），而 y=208 那一行里
  **只有 3 格是绿系**（x=32 / x=112 / x=240，平均色见 `--dump`），与①②③ 指到的三档完全一致。

■ 候选：为什么是 `(112,208)` 这一格（**🟡 已被上面 §2026-09-15 定案取代，留档**）

  这一节**没有排他的一手证据**，把推理链和反证都摆出来：

  - `etama2.png` y=208 那一行 16 格，按平均色分是（一手像素统计，见 `--dump`）：
    `(0,R) (16,B) (32,G) (48,R) (64,B灰) (80,Y) (96,M) (112,G) (128,R) (144,B) (160,G) (176,R) (192,B灰) (208,Y) (224,M) (240,G)`
    ⇒ **绿色只有 3 格：x=32 / x=112 / x=160，其中 x=240 与 x=112 逐像素相同**（同图别名）。
  - 16×16 单元里 x=0..15 是 **8 格一组的「一组颜色」**（红/蓝/绿/紫 ×2），
    x=112 起是**第二组**（绿/红/蓝/绿…），`(112,208)` 与 `(240,208)` 是**同一张贴图数据的两个
    sprite 编号**（ZUN 用同一块 UV 给两个 sprite 号是常规手法，`thanm` 也照录）。
  - 30×30 那一族（`sprite336..343`，y=145/177）是**大件**（道中 Boss 掉的 P / 点道具本体），
    不是 8×8 的绿点；绿点在本作里是**小单元**，所以取 16×16 那一族。
  - ❓ **没有做到**（🟡→ **2026-09-15 已解决**，见上一节 §定案）：我没能把「道具类型 8（= 信仰）」钉到某一格。卡住的两步：
    ① `ItemManager::spawn_item` 里脚本号 = **类型 + 374**，而四种小件脚本（381..384）的
       `stop(spriteN)` 只差 `N`，**脚本号与 sprite 号正好错开 30**（381↔351、382↔352…），
       这个错位我没有找到一手解释（可能是 `sprite` 号在 TH10 里还有一层入口内偏移）；
    ② `0x41bbf7` 的 `cmp WORD PTR ds:0x474c48, 0x64`（火力满）之后那张 **byte 跳转表
       `0x41be28`** 会把类型 1..7 改写成 5/9，而 `0x474c48` 我读出来全 0
       （疑似 HMODULE/表基址，不是「当前火力」）⇒ 无法用它反推类型号。
    **以 `sprite` 号为准**取 `sprite351`（`make_spirit_art` 一族里唯一带 `ins_51(16)` 的那条
    就是 script382 的）；若卡侧实跑发现应当取 `sprite352`（x=128 那格，红色），
    只需把本文件里 `GREEN_MAIN` 改成 `(128, 208, 16, 16)` 重跑一次 —— **一行的事**。
    可信度：源图 / 矩形 / 行的位置 **✅ 一手**；「这一格就是信仰点」**🟡 推理**。

■ 脚本原文（一手，`thanm -l 10 local/th10.v1.00a/dat/bullet.anm`，脚本 382；宏名在注释里给出）

      script script382 {
          stop(sprite351);          // ← 本文件切的就是它（= 绿点，16×16 @(112,208)）
          ins_66(1);
          ins_67(1);
          ins_51(16);               // alpha(16)
          ins_53(0, 0, 0.7853982f); // rotate(0, 0, π/4)  ← **定角 45°**，不是动画
          ins_50(0.0f, 0.0f);       // scale(0, 0)      ← 从 0 起
          ins_60(16, 0, 1.0f, 1.0f);// scaleTime(16, 0, 1, 1)
      +32:
          ins_53(0, 0, 0.0f);       // rotate(0, 0, 0)  ← 32 帧后**瞬间**转正
          ins_67(0);
          ins_63();                 // delete()
      }

  ⇒ **8×8（16 的 scale 0.5 × 16px）的小方块，出场 16 帧从 0 放大到 1、
     入场时带 45° 定角、第 32 帧转正并立刻 delete**（所以它不是「一直在场」的道具，
     而是**出生一瞬的闪光单元**；TH10 里真正持续显示的是同一格的另一份 sprite 号）。
  ⚠️ 与本仓 `SPIRIT`/`season` 两处不同：**这里没有 `layer()` 也没有 `alpha` 的淡出**，
     照抄即可，不要替它加。
"""
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC = REPO / "local/th10.v1.00a/anm/bullet/bullet/etama2.png"
OUT = HERE / "mof"

# 一手：TH10 `bullet.anm` entry2（bullet/etama2.png）的 sprite 矩形。
# ★ 2026-09-15 定案（三条一手链见文件头 §定案）：**sprite 号 = 343 + 道具类型**。
SHEET      = (256, 256)
BIG_MAIN   = (32, 208, 16, 16)    # sprite346 —— 道具类型 3 = **大信仰点**（script377 的 sprite）
GREEN_MAIN = (112, 208, 16, 16)   # sprite352/sprite351 —— 类型 9 = **小信仰点**（script383）
                                  #   与类型 8 = **微信仰点**（script382）**共用同一矩形**（ZUN 自己的重号）
# ★ 2026-09-15b：**微档的独立贴图** —— 同一行里 ZUN 画的**第二套绿点**（素色圆点，见文件头 §三档外观）。
MICRO_PLAIN = (240, 208, 16, 16)  # sprite360/361 —— `352+vtype` 家族里 vtype 8/9 的那一格
# ★★ 2026-09-18（B12）：**满 P 转换特效**的贴图 —— TH10 `bullet.anm` **script393** 的 `sprite332`
#   = `(64,224,32,32)`（32×32）。一手：转换循环 `th10:0x41bad2` 对**每一颗**被转换的 P 道具调
#   `0x448db0(anm_obj, &item.pos, 0x189)`（该函数用 `ANM_MANAGER_PTR`、参 3 查**脚本表**
#   ⇒ 0x189 是 anm **脚本号** = `bullet.anm` script393；`0x43dc90`（SE 播放器）在转换里零调用
#   ⇒ **原版转换静音**，只有这台特效）。脚本正文见 `scripts/295_mof_conv_burst.anm.txt`。
CONV_BURST = (64, 224, 32, 32)    # ★★ 2026-09-18（B12）sprite332 —— 满 P 转换特效（script393）
                                  #   （32×32；本仓的信仰点切自同一张 etama2.png 的 y=208 行）
MICRO_ALIAS = (160, 208, 16, 16)  # sprite355 —— 同家族 vtype 3 的那一格；与上一格**逐像素只差 2 个像素**
                                  #   （= ZUN 把同一个绿点画了两处），仅留证，不做成贴图


def _load():
    if not SRC.is_file():
        raise SystemExit(
            f"缺原版贴图 {SRC}\n（先 `python3 tooling/thtk/unpack.py th10.v1.00a`）"
        )
    im = Image.open(SRC).convert("RGBA")
    if im.size != SHEET:
        raise SystemExit(f"{SRC} 是 {im.size}，预期 {SHEET}")
    return im


def _crop(src, box):
    """按一手矩形切下来 —— **不缩放、不重拼、不补边**。

    `build_ability.py` 允许源图**小于** ORDER 里声明的尺寸（会自动居中补透明；
    见 `build_ability.py:106-110`），而 16×16 本身就是 2 的幂 ⇒ 这里保持原生原像素最好：
    放大到 32×32 会让脚本里的 `scale()` 语义跟着变（TH10 那条脚本没有 scale，靠原始 16px）。
    ⚠️ 用**不带 mask** 的 paste：带 mask 时 PIL 会做 alpha 合成，把「alpha 小、RGB 有值」的
    边缘像素预乘成暗色（实测原生图 (255,255,255,68) → (68,68,68,18)），就不是原样切图了。
    """
    x, y, w, h = box
    return src.crop((x, y, x + w, y + h)).copy()


def main():
    dump = "--dump" in sys.argv
    src = _load()
    OUT.mkdir(exist_ok=True)

    # 出**三**张 16×16（= 一手表里三档各自的矩形，一格不多切）：
    #   · FAITH_DOT_BIG.png   ← sprite346 `(32,208)`  —— 大信仰点（类型 3，script377）
    #   · FAITH_DOT.png       ← sprite352 `(112,208)` —— 小信仰点（类型 9，script383）
    #   · FAITH_DOT_MICRO.png ← sprite360/361 `(240,208)` —— 微信仰点（类型 8，script382）
    #                           ★ 原版在**可见分支**里给类型 8 的也是 `(112,208)`（与小档同格）；
    #                             这里用的是 ZUN 同一张图上的**第二套绿点**，理由见文件头 §三档外观。
    _crop(src, GREEN_MAIN).save(OUT / "FAITH_DOT.png")
    _crop(src, BIG_MAIN).save(OUT / "FAITH_DOT_BIG.png")
    _crop(src, MICRO_PLAIN).save(OUT / "FAITH_DOT_MICRO.png")
    _crop(src, CONV_BURST).save(OUT / "CONV_BURST.png")          # ★ 2026-09-18（B12）转换特效

    print("mof: FAITH_DOT.png 16x16（原始像素，未缩放）"
          " = TH10 道具类型 9（小信仰点）@ bullet.anm script383 的 sprite352"
          " → bullet/etama2.png (112,208)")
    print("mof: FAITH_DOT_BIG.png 16x16（原始像素，未缩放）"
          " = TH10 道具类型 3（大信仰点）@ bullet.anm script377 的 sprite346"
          " → bullet/etama2.png (32,208)")
    print("mof: FAITH_DOT_MICRO.png 16x16（原始像素，未缩放）"
          " = TH10 道具类型 8（微信仰点；script382）的**第二套**绿点 sprite360/361"
          " → bullet/etama2.png (240,208)")

    print("mof: CONV_BURST.png 32x32（原始像素，未缩放）"
          " = TH10 满 P 转换特效 @ bullet.anm script393 的 sprite332"
          " → bullet/etama2.png (64,224)")
    if dump:
        print("\n--- etama2.png y=208 一行 16 格的像素统计（核色用）---")
        for i in range(16):
            c = src.crop((i * 16, 208, i * 16 + 16, 224))
            px = [p for p in c.getdata() if p[3] > 40]
            if not px:
                print(f"  x={i * 16:3d} 空")
                continue
            n = len(px)
            avg = tuple(sum(p[k] for p in px) // n for k in range(3))
            print(f"  x={i * 16:3d} n={n:3d} avg={avg}")

        print("\n--- 三档取的三格：像素数 / 平均色 / 与 (240,208) 的逐像素差 ---")
        ref = _crop(src, MICRO_PLAIN).tobytes()
        for tag, box in (("大 BIG_MAIN  ", BIG_MAIN),
                         ("小 GREEN_MAIN", GREEN_MAIN),
                         ("微 MICRO_PLAIN", MICRO_PLAIN),
                         ("微 MICRO_ALIAS", MICRO_ALIAS)):
            c = _crop(src, box)
            px = [p for p in c.getdata() if p[3] > 40]
            n = len(px)
            avg = tuple(sum(p[k] for p in px) // n for k in range(3)) if n else None
            ndiff = sum(1 for a, b in zip(c.tobytes(), ref) if a != b) if c.tobytes() != ref else 0
            print(f"  {tag} {str(box):18s} 不透明像素 {n:3d}  avg={avg}  vs(240,208) 差 {ndiff} 字节")

    print("\n把下面几行追加到 entries/ORDER.txt **文件末尾**（已冻结段只能 append，别插进 ATLAS_MOF_FAITH 段里）：")
    print("FAITH_DOT       ability/mof/FAITH_DOT.png        16x16   # 已在 ORDER.txt:316（小信仰点）")
    print("FAITH_DOT_BIG   ability/mof/FAITH_DOT_BIG.png    16x16   # 2026-09-15 新增（大信仰点）")
    print("FAITH_DOT_MICRO ability/mof/FAITH_DOT_MICRO.png  16x16   # ★ 2026-09-15b 新增（微信仰点）")


if __name__ == "__main__":
    main()
