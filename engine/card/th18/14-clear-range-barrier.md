# 消弹范围的「可见化」 —— 给卡 111（芙兰蓄力清屏）找一圈像 TH09「固灵结界」那样的结界环

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）为默认；引用其他版本一律带前缀，如 `th09:0x…`、`th16:0x…`、`th17:0x…`。
> **方法**：TH18 侧 = 本仓既有成果（`engine/anm/th16/09-drawing.md` 的绘制 opcode 一手表）+ 原生 `ability.anm` / `effect.anm` 等 58 个 anm 文本 dump 普查 + mod 源码与构建管线盘点；
> TH09 侧 = 一手平坦反汇编（`objdump -d -M intel local/th09.v1.50a/th09.exe`）+ `thdat`/`thanm -m v2.anmm` 解包读 anm 脚本 + 自写 PNG 解码器量贴图 alpha 剖面；
> 半径口径 = `local/_scratch/th18.dis` 的擦弹圈两个函数与 `native/cards/signal.c` 的实现逐条对照。
> **可信度**：✅ 一手（§2.1–§2.6 TH09 全链：opcode 表 / 触发两条路 / 三级数字 / `etama.anm` 脚本原文 / 环带几何的 C 代码 / 贴图剖面；
> §3.1 环半径语义、§3.2 原生环族、§3.3–§3.5 盘点与先例、§4.1 三条式子）；
> 🟡 待验（§7 #2–#5；§3.1 注里那条 277 半径已由实跑证实，见 §1.2）；❓ 自定（§5 落地建议整节，除标「复刻」的行）。
> **⚠ 本文与 [`13-mallet-clear-and-th17-charge.md`](13-mallet-clear-and-th17-charge.md) 的分工**：13 号篇管「小槌挥击的配方 + TH17 蓄力的音效/光环」，
> 本文只管**「把消弹范围本身画出来」**这一件事（环的半径语义、TH09 的先例、三套半径的统一口径、落地编号）。

## 0. 一句话结论

- **能做到，而且不用自造**：TH18 原生 `ability.anm` 自己就有一族「画一圈环」的脚本（**script33/34/35**：
  `drawRing(80/96/112, …)` + `drawPolyBorder`，全部 **`sprite(-1)`**、`layer(15)`、加算、20 帧淡出），
  其中 **script34 是「半径可编程环」的最佳原型**（唯一一条 `drawRing` 且**没有任何 `scale` 插值** ⇒ 卡侧写 scale 不会被盖掉）。
- **环的半径语义是一手钉死的**：`drawPoly`/`drawPolyBorder`/`drawRing` 写的是 `sprite_size`，**最终尺寸 = `sprite_size × scale`**
  ⇒ `drawRing(1.0f, …)` + 卡侧 `ce_anm_set_scale(id, R, R)` 就是「半径 R px 的环」（§3.1；本仓 116 季节的 `+16.0f` 与原生
  `drawPolyBorder(62.0f, 48)` 是两条独立旁证）。
- ★★ **TH09 的固灵结界取到了，而且它教会我们一件反直觉的事**：那一圈**不是贴图、也不是 anm opcode**，
  而是**引擎 C 代码现场画的多边形环带**（`th09:0x40dc70`：64 段、`半径 ± 半宽`、`1/cos(π/n)` 内切修正、三角带）；
  参数靠「`posMode(1)` 让 `pos()` 改写成 `vm+0x288`，再由 C 代码把 `vm+0x288` 当半径读」这条隐式约定传进去。
  文件/脚本/sprite = **`etama.anm` script 91/92/93（0x5b/0x5c/0x5d）、sprite224**；半径 **16 → 192/256/448**（48/64/128 帧）。
- ★★ **TH09 自己也没做到「所见即所消」**：消弹圈是 `r(t) = dr × t`（从 **0** 长），环是 `16 → R`（从 **16** 长），
  **只在终点相等**（`4.0 × 48 = 192` = 脚本的 `posTime` 目标）—— ZUN 的做法是「两套式子、同一个终点数字」。
  ⇒ 我们推荐「环 = 红染 = 抖动 = 清屏 **共用一个 `R_clear`**」比 TH09 更严（§2.6 / §4.3）。
- **半径口径**：引擎擦弹圈是**逐弹**的（含弹自己的 `br` 项），清屏是**单值圆** `2 × (3.0 + 40.0)` = **86.00 px**
  ⇒ 推荐「**环 = 红染 = 抖动 = 清屏 = 86 px 那一个圆**」，引擎擦弹圈只留给真的在复刻擦弹的那一支（① 大小姐计数）（§4）。
- ★ **「环半径 = 参数 × scale」这条已被实机验证**：277 的上一版把 `scale` 当成半径写（`0.4→2.0`）⇒ 实跑真的看不见
  （用户原话「shift蓄力依然啥都没有」），**同一天**已改成 `scale = 真实半径 px`（§1.2）。
- ⚠ **还差最后一格**：环现在只在**蓄力期**存在，松手那一帧就被收（20 帧淡出），而消弹就在那一帧 ⇒
  「清屏那一刻的环」还没落地（§5.2 给了两条路：松手不收回 277 / 新增 279 做扩张环）。

## 1. 需求与现状

### 1.1 用户口径（2026-09-16，逐字）

> 「**哦不对，消弹范围应该是特效显示出来，类似于 th09 的固灵结界**」

指的是：mod `mods/th18.v1.00a/card-expand/` 的卡 **111**（地灵殿 / SA 复刻）里，二小姐（芙兰）的蓄力特技 ——
蓄满松开 Shift 后消除 `2 × 擦弹圈半径` 范围内的弹幕。现在这一下**只有一记爆闪 sprite**（脚本 278），
用户要的是把**「消弹范围」本身**画出来，像 TH09 的固灵结界那样一圈可见的结界/环，让人一眼看出消到哪。

### 1.2 现状（一手：读我们自己的源码）

> ⚠ **并发声明**：本文写作期间另一个 agent 正在改这张卡的同一块（`AUDIT.md` §BE `2026-09-16j`）。
> 下表读的是 **2026-09-16 15:18 的快照**：`277_flan_charge_ring.anm.txt` md5 `434d38ea5ed5ef9ca05f451c043f3398`、
> `native/cards/signal.c` md5 `1b6d94fdec3a6511a02b9d2cb4593407`。**落地前以当时的文件为准。**

| 元素 | 现状（快照） | 出处 |
| --- | --- | --- |
| 范围环（脚本 **277**，已从「蓄力光环」升级成「范围环」） | `sprite(-1); layer(13); blendMode(1); drawPolyBorder(1.0f, 64); color(120,220,255); alpha(64); stop();` + 两条 `interruptLabel` 各 20 帧淡出 | `assets/ability/scripts/277_flan_charge_ring.anm.txt` |
| 环的半径 | 卡侧每帧 `ce_anm_set_scale(id, ring_r, ring_r)`，**`ring_r` = `signal_flandre_cancel_radius_x100()` / 100 = 86.00 px**（与清屏判定同一个函数） | `native/cards/signal.c:1563`–`1575` |
| 环的生命周期 | **只在蓄力期**：`charge > 0` 起、每帧跟随自机；**松手那一帧就被 `interrupt`（20 帧淡出）**，紧接着只放爆闪 | `native/cards/signal.c:1576`–`1586` |
| 清屏那一下 | 脚本 **278**：原生卡 42 小槌挥击的四格 sprite 推进 + 8 帧淡出，VM 存活 15 帧，自删（层 13） | `assets/ability/scripts/278_flan_clear_burst.anm.txt` |
| 清屏判定 | `ce_cancel_radius(pos, r, 99999, mode 0)`，`r = 2 × (自机判定半径 + 40.0)` = **86.00 px** | `native/cards/signal.c:1618`–`1621`、`signal_core.c:709` |
| 蓄力帧数 | `SIGNAL_FLANDRE_CHARGE_FRAMES` = 300（5 s @60 fps） | `native/cards/signal_core.h:901` |
| ★ 还缺什么 | ① **清屏那一刻没有环**：环在松手那一帧开始淡出，而消弹就发生在那一刻 ⇒ 最关键的帧上仍只有爆闪；② 红染的判据与环/清屏不是同一条圆（§4） | 用户口径 + 上面两行 |

★ **一条已由实跑证实的旧账（本文的 🟡 变 ✅）**：277 上一版是 `drawPoly(1.0f, 32)` + 卡侧 `scale = 0.4 → 2.0`，
按 §3.1 的语义那是**半径 0.4 → 2.0 px**（肉眼不可见）。用户实跑反馈的原话（`native/cards/signal.c:1551` 的注释）：
**「shift蓄力依然啥都没有，只有消弹结果」** ⇒ 这条推断被实跑证实，并已由 `2026-09-16j` 那一轮改成
`drawPolyBorder(1.0f, 64)` + `scale = 真实半径`。**这也是 §3.1 那条「半径 = 参数 × scale」语义的第一次实机验证。**

### 1.3 要解决的三个问题（本文的三个交付面）

1. TH09 的固灵结界**到底怎么画的**（触发 / 半径字段 / anm 脚本 / sprite / 逐帧变化 / 时长）—— §2。
2. TH18 侧**复用**什么来画「半径可编程的环」（不自造）—— §3。
3. **半径口径统一**：消弹半径 / 红染半径 / 特效环半径三条式子摆在一起，指出差异项并给自洽口径 —— §4。

## 2. TH09 的「固灵结界」一手 —— ★ 取到了，而且它**不是**「贴图画的环」

> **本节可信度**：✅ 一手闭链（触发两条路 / 三级参数 / type→script 的表 / 脚本原文 / 环带几何的 C 代码 /
> 贴图逐像素剖面 / 时长三处自洽，全部我或复核者亲手读过）。**唯一 🟡**：sprite224 的 `h=768` 在 128 高贴图上的
> wrap 行为按「纵向重复」解释（无实机截图）；**未取到**两条列在 §2.7。
> 样本：`local/th09.v1.50a/th09.exe`（685,056 B，md5 `cf634df46e05552e104fa97a971aaac0`，imagebase `0x400000`）+ `th09.dat`（88,180,006 B）。
> 读 anm 必须用 **`-m v2.anmm`**（它的文件头写着「for version 2: PCB / and version 3: IN, **PoFV**」；用 `v8.anmm` 只会得到 `ins_15(…)` 这种未映射助记符）。

### 2.1 结论链（一句话走完）

**蓄力放炮 → 在自机位置起一个 `EffectSys` 特效 `type = 0xd/0xe/0xf` → 这三个 type 在特效表 `0x4a0b48` 里指向
`etama.anm` 的 script `0x5b/0x5c/0x5d`（= 91/92/93），sprite `sprite224`；
但「那一圈」不是贴图画出来的** —— 贴图只是一条 16 px 宽的红色**径向渐变剖面带**，
**环本体是引擎 C 代码 `0x40dc70` 用 anm VM 的字段现场生成的多边形环带**（64 段、`2×(count+1)` 个顶点、三角带）。
半径走的是「`posMode(1)` 让 `pos()`/`posTime()` 改写成 `vm+0x288`，再由 C 代码把 `vm+0x288` 当半径读」这条**隐式约定**。 ✅

### 2.2 触发：`Player::on_tick` 的两条路（✅）

| 项 | 内容 | 一手 |
| --- | --- | --- |
| 路径 A | `th09:0x41e9b9` → `th09:0x41ca00`：闸门 = `InputManager::is_active(input, **bit 2**)`（`imul ecx,ecx,0x8e; add ecx,0x4ace18; push 2; call 0x415cb0`，idx = `player+8`） | `th09:0x41ca26`–`0x41ca37` |
| 路径 B | `th09:0x41eabd` → `th09:0x41f810`：**逐帧充能**，`bit1` 按住即充、`bit4` 会**阻止**充能 | `th09:0x41f848` / `0x41f880` |
| 输入位语义 | `+0x2c`：`0x10/0x20/0x40/0x80` = 上下左右、低位 `1/2/4` = 三个动作键、`0x100` = Pause 系 | `th09:0x415cb0`（`mov ax,[ecx+0x2c]; and eax,bit`） |
| ★ **bit1 = 低速键 = 蓄力键（同一颗）** | 按住 bit1 时 `inc WORD [input+0x2a]`（封顶 8），人类玩家「低速」= 按住 ≥8 帧；菜单字符串 `0x48f988` 写死「低速、及び**チャージ**ボタンを設定します」 | `th09:0x420255` / `0x42054d` / `0x41c2a9` + `0x48f988` |
| 两路汇合 | 都进同一组「级别设置」`th09:0x403c30` / `0x403c90` / `0x403cf0` | `th09:0x403c30`–`0x403d53` |

### 2.3 三个级别的**全部数字**（✅ 一手；★ 三处自洽是本节最强的交叉验证）

| 级别 | 级别设置函数 | 消弹节点 `r₀ / dr / 存活` | `dr × 存活` | 环脚本的半径目标 `posTime` | `player+0x303c8` 计时器 | anm 总帧 |
| :-: | --- | --- | :-: | :-: | :-: | :-: |
| 1 | `th09:0x403c30` | `0 / 4.0f / 0x30`(48) | **192** | **192.0f** ✅ | 48 | 48 |
| 2 | `th09:0x403c90` | `0 / 4.0f / 0x40`(64) | **256** | **256.0f** ✅ | 64 | 64 |
| 3 | `th09:0x403cf0` | `0 / 3.5f / 0x80`(128) | **448** | **448.0f** ✅ | 128 | 128 |

每个级别函数都做同样五件事：① `th09:0x41cfe0(pos=&player+0x1b88, r₀, dr, 存活, 免疫)` 建**消弹节点**（kind 1）
② `th09:0x41d0d0(…)` 建第二个节点 ③ `th09:0x403c20(player, 3)`（player state = 3）
④ `th09:0x401500(&player+0x303c8, 存活)` 设计时器 ⑤ `th09:0x40cc00(side->effect_sys, id = 0xd/0xe/0xf, &player+0x1b88, 1, −1)` 起**环**。
**全 exe 只有 `0x403c83` / `0x403ce3` / `0x403d4c` 这三处**产生 id `0xd/0xe/0xf`。

充能量有**两个** float，别混：`player+0x30388` = 蓄力**上限/等级**（`th09:0x41bc90` 的 `add_charge`：`+= arg`，
低速态再 `+= arg × 0.15`（`0x48ef60`），`≥ 400.0f`（`0x48e3c4`）时钉到 `0x43c80000` = **400.0f**），
`player+0x30384` = **当前蓄力量**（`th09:0x41faa4`，夹在上限内）。门槛真值 = **400.0 / 300.0 / 200.0**（`0x48e3c4` / `0x48ef00` / `0x48eefc`）。

### 2.4 ★★ 那一圈环：文件 / 脚本 / sprite / 逐帧量（✅ 一手）

**钥匙**：`th09:0x40cc00`（特效池 0x101 × 0xd8）→ `th09:0x40ca60`：
`ebx = id × 24`（`0x40cae1` 的 `lea/sh…,3`）⇒ **特效表 `0x4a0b48`、步长 `0x18`**，字段
`+0x00` first_script / `+0x04` script 步长 / `+0x08` count / `+0x0c` spawn cb / `+0x10` tick cb / `+0x14` draw cb。
逐条读（我和复核者各读了一遍，值相同）：

| id | 表地址 | first_script | count | spawn cb | tick cb | draw cb |
| :-: | --- | :-: | :-: | --- | --- | --- |
| `0xd` | `0x4a0c80` | **`0x5b`** | 1 | `0x40dbb0` | `0x40d470` | **`0x40dc70`** |
| `0xe` | `0x4a0c98` | **`0x5c`** | 1 | 同上 | 同上 | 同上 |
| `0xf` | `0x4a0cb0` | **`0x5d`** | 1 | 同上 | 同上 | 同上 |

**anm 文件 = `etama.anm`**（槽 8）：`EffectSys+0x2d4` 由 `th09:0x40d1c0` 装载 = `AnmManager` 全局 `0x4dc550` 的槽 8，
空则用 `th09:0x43c7a0(0x48e488, 8)`；**`0x48e488` 这个字节串就是 `"etama.anm"`**（我实读）。
⇒ **文件 = `etama.anm`；script = 91 / 92 / 93（0x5b/0x5c/0x5d）；sprite = `sprite224`**（entry2 `data/etama/etama3.png`，128×128 的 16bpp）。

脚本原文（`thanm -l 9 etama.anm -m v2.anmm`，我独立复跑过）：

```
script91 {                                   # id 0xd / 级别1 / 半径 192 / 48 帧
    sprite(sprite224); blendAdditiveSet(1);  # 加算混合
    posMode(1);                              # ★ op24：把 arg bit0 写进 vm+0x1f8 的 bit 0x100
    alpha(0); alphaTime(20, 0, 255);         # 20 帧 0 → 255
    iset($I0, 64);                           # ★ 环的分段数 = 64
    iset($I1, 5);                            # 只有 pos2.y != 0 的分支用它
    scale(24.0f, 24.0f);                     # 环带【半宽】
    scaleTime(32, 0, 16.0f, 16.0f);          # 半宽 24 → 16（32 帧）
    pos(16.0f, 0.0f, 0.0f);                  # ★ posMode(1) ⇒ 写 vm+0x288，被 C 代码当【半径】
    rotate(0.0f, 0.0f, %RANDRAD);            # 随机初相
    posTime(48, 0, 192.0f, 0.0f, 0.0f);      # ★ 半径 16 → 192（48 帧线性）
+38: alphaTime(10, 0, 0);  +48: delete();
}
script92 { … scaleTime(48,0,16,16); posTime(64, 0, 256.0f, …); +54 淡出; +64: delete(); }
script93 { … scaleTime(48,0,16,16); posTime(128, 0, 448.0f, …); +118 淡出; +128: delete(); }
```

| 逐帧量 | 值 | 一手 |
| --- | --- | --- |
| 环带**中心半径** | `16 → 192 / 256 / 448` **线性**（`posTime`，时长 = 级别寿命） | 三段脚本的 `posTime` |
| 环带**半宽** | `24 → 16`（32 帧；级别 2/3 是 48 帧） | `scale` + `scaleTime` |
| 透明度 | `0 → 255`（20 帧 `alphaTime`），最后 10 帧淡出（`t−10` 起） | `alphaTime(20,0,255)` / `alphaTime(10,0,0)` |
| 分段数 / 初相 / 混合 | 64 段、随机初相 `%RANDRAD`、加算（`blendAdditiveSet(1)`） | `iset($I0,64)` / `rotate` / `blendAdditiveSet` |
| 总帧数 | **48 / 64 / 128**（`delete()`；= 节点寿命 = state 计时器） | 脚本 + `th09:0x403c30/90/f0` |

**贴图 `sprite224` 是不是环？——不是**（复核者亲手解 PNG 后逐列量）：`sprite224 = {x:32, y:0, w:16, h:768}`，
而 `etama3.png` 只有 128×128（`h=768` 是 ZUN 的「纵向重复」长条手法；同 entry 的 227/228 = 16×128 / 16×256 佐证）。
逐列 alpha 剖面（x=28…47，对 y 平均）：`α = 101,36,33,69,164,228,**237**,220,136,120,51,34`，
RGB 从两侧的红 (200,57,58) 到中心的粉 (226,147,172) ⇒ **它是一条 16 px 宽的「径向渐变剖面带」**，
环形的「圈」完全来自 §2.5 的顶点几何。**`sprite(sprite224)` 必须保留**（C 代码从 `vm+0x224` 取 sprite 矩形做 UV、
从 `vm+0x1f0` 取 `color_1`）——换贴图就会换颜色/UV。

### 2.5 ★★ 环的几何是**引擎 C 代码**画的（✅ 一手；这条推翻了我自己的第一版判断）

> **我第一版的错**：因为 `v2.anmm` 的 opcode 表（0..89）**确实没有** `textureCircle`(600)/`textureArc`(601/602)/
> `drawPoly`(604)/`drawPolyBorder`(605)/`drawRing`(611) 那一族（那是 v8 时代才有的，见 `v8.anmm:140`–`151`），
> 我推成「TH09 只能靠贴图 + `scale`」。**错在把「没有 opcode」当成「没有这个能力」**：
> TH09 把这件事放在 **EffectSys 的绘制回调**里，用 C 代码现场生成顶点。

```
th09:0x40d470（tick cb，把 vm 字段搬进 slot）
    slot[0x8c] = vm[0x100]    ; int_vars[0] = 64 ⇒ count（分段数）
    slot[0x88] = vm[0x18]     ; scale.x = 环带半宽
    slot[0x7c] = vm[0x288]    ; ★ pos2.x = 半径（脚本 posMode(1) 之后 pos/posTime 写的就是这里）
    slot[0x94] = vm[0x28c]    ; pos2.y（= 0 ⇒ 走「双环」分支）
    slot[0x80] = vm[0x8]      ; rotation.z = 初相
th09:0x40dc70（draw cb）
    step = 2π / count                      ; 常量 0x48e4c4 = 6.2831855
    R0   = scale.x / cos(step/2)           ; 正多边形内切修正（fsin 0x401070 = th09:0x401070）
    外圈半径 = pos2.x + R0 ；内圈半径 = pos2.x − R0     ; ⇒ 实测是一条有厚度的【环带 annulus】
    每个角度写 2 个顶点、步长 0x1c、共 2×(count+1) 个
    th09:0x439dc0(vm0, slot[0xcc], 2*count+2)   ; 用 sprite 矩形 + color_1 算 UV/色
    th09:0x43b4d0(AnmManager, vm0, slot[0xcc], 2*count+2)   ; 画三角带
```

⇒ **TH09 的固灵结界 = 一条 64 段的环带**（中心半径 `r(t)`、半宽 `halfw`、加算、随机初相），
**不是环形贴图，也不是 anm 自己的 opcode**。顶点缓冲 `slot+0xcc` 由 spawn cb 分配 `0x1c38` 字节（258 × 0x1c）。
全 exe 只有 effect id **6 / 8 / 0xd / 0xe / 0xf** 用这套 draw cb。

### 2.6 ★★ 半径口径：TH09 自己**没有**做到「所见即所消」（✅ 一手，直接回答用户第 3 问）

| 侧 | 半径式子 | 过程 |
| --- | --- | --- |
| **判定（消弹）** | `r(t) = dr × t`，`dr = 4.0 / 4.0 / 3.5`，`t = 0..48/64/128` | 从 **0** 线性长 |
| **绘制（环带）** | `r(t) = 16 + (R_target − 16) × t/T`，`R_target = 192 / 256 / 448` | 从 **16** 线性长 |

⇒ **两者只在终点相等**（`4.0 × 48 = 192`、`4.0 × 64 = 256`、`3.5 × 128 = 448`），过程**不相等**
（前中段环比消弹圈**大**：`16 + 3.67t > 4t` 直到 `t = T`）。
**这就是 ZUN 的处理方式：把两套式子都写成「终点 = 同一个硬编码数字」，而不是让它们共用一个 float。**
⇒ 对我们的口径讨论（§4）是一条一手旁证：**「环 = 清屏半径」这个诉求连原版都没做到逐帧成立**，
所以我们的推荐（让四处共用一个 `R_clear`）**比 TH09 更严**，而不是「照 TH09 做」。

### 2.7 从 TH09 能借 / 不能借（✅ 判断）

| 元素 | 能不能借 | 说明 |
| --- | --- | --- |
| 「环带」这个形状（有厚度的圈，不是实心盘） | ✅ 概念可借 | TH09 用的是 64 段 annulus（`r ± halfw`）；TH18 有现成的 `drawRing(r, t, n)`（§3）**不用自己写顶点** |
| 半径曲线 = **线性扩张到目标半径** | ✅ 可借 | TH09：16 → 目标 / 全长；半宽同步收窄（24 → 16） |
| alpha 曲线：**20 帧淡入 + 尾 10 帧淡出** | ✅ 可借 | 三段脚本都是这个形状（与我们的 20 帧寿命很搭） |
| 随机初相 `%RANDRAD`（让每次的环不一样） | ✅ 可借（❓ 自定） | 我们的 anm 也可以写 `%RANDRAD` |
| 三级的半径/时长数字表 | 🟡 只当参照 | 那是 TH09 的弹速弹量下的平衡值；我们的清屏是**一次性 86 px** |
| **顶点生成 / 特效池 / 双回调 / `posMode(1)⇒vm+0x288` 的隐式约定** | ❌ 抄不动 | 全是 TH09 `EffectSys` 私有；TH18 侧用 `drawRing` 代替 |
| 触发与门控（`state=3` + `0x303c8` + 两个 float + 节点系统） | ❌ 抄不动 | 与 TH09 的 player state 机纠缠 |
| **未取到**（两条，明细见 §7 #1） | — | ① 充能每帧增量的被乘常数 `th09:0x4b36b8`；② 输入位 → 物理键（bit2/bit4 谁是 Shot 谁是 Bomb） |

## 3. TH18 侧画「半径可编程的环」的现成手段

### 3.1 ★★ 环的半径怎么给：`drawPoly` 的参数 × `scale`（一手，本条决定了全部方案）

一手（本仓已完成的反汇编，`engine/anm/th16/09-drawing.md`）：

| 事实 | 一手 |
| --- | --- |
| `604 drawPoly` = `f r, $ n` ⇒ 写 `sprite_size.x = r`、`I0 = n`（渲染模式 17） | `th16:0x4626eb`，同篇 §1.1 表 |
| `605 drawPolyBorder` 同签名（模式 18）；`611 drawRing` = `f r, f t, $ n` ⇒ 再加 `sprite_size.y = t`（模式 19） | `th16:0x462766` / `th16:0x462778` |
| **绘制时最终尺寸 = `sprite_size × scale`**（`mulss xmm0,[esi+0x50]`） | `th16:0x468722` / `th16:0x4689b3`，同篇 §4.4 末与 §8 |
| ZUN 自家数据里 `604/605` 的 radius **46/46 全是字面量**（想动态改半径就改 `scale`） | 同篇 §6「证伪过的竞争假设」 |

⇒ **`drawPoly(1.0f, 32)` 的 VM，其可见半径（px）= `1.0 × scale.x`**。
`drawPoly(R, n)` + `scale = 1` 则是「半径 R px 的 n 边形」。**这就是「半径可编程的环」的引擎原生手段**，无需自造。

**本仓的旁证（两条独立一手链，都印证「scale 就是 px 半径」）**：

| 旁证 | 内容 | 出处 |
| --- | --- | --- |
| 赛季释放的同心双圈 | 脚本 120/121 是 `drawPoly(1.0f, 32)`，卡侧 `ce_anm_set_scale(id, sc, sc)`，`sc = rel_radius × grow`；内圈显式 `+ 16.0f`（「第二个圆比第一个大 **16**」= 16 **px**） | `native/cards/season.c:997`–`1003`（内圈 `si = sc + 16.0f`）、`season_core.c:125` |
| 咆哮的全屏渐变圆 | 脚本 155 是 `drawPoly(1.0f, 32)` + `scaleTime(60, 1, 600.0f, 600.0f)`，注释直接叫它「**全屏**渐变圆」 ⇒ 600 倍 = 半径 600 px | `assets/ability/scripts/155_self_aura.anm.txt` |
| 程序画的描边 | `drawPolyBorder(62.0f, 48)` 注释写「**外径 62**、48 边」、`drawPolyBorder(55.0f, 48)`「**半径 55**」 | `assets/ability/scripts/229_ufo_gauge_arc_border_out.anm.txt` / `230_…in.anm.txt` |

> ★ **这条语义的第一次实机验证（🟡 → ✅，2026-09-16 同一天）**：卡 111 的上一版范围环（脚本 277）正是
> `drawPoly(1.0f, 32)` + 卡侧 `scale = 0.4 + 1.6 × (charge/300)`（`native/cards/signal.c` 的旧行）——
> 按上面的语义那只有 **半径 0.4 → 2.0 px**。用户实跑的原话是「**shift蓄力依然啥都没有，只有消弹结果**」，
> 与推断吻合；同一天由 `2026-09-16j` 那一轮改成 `drawPolyBorder(1.0f, 64)` + `scale = 真实半径(px)`。
> ⇒ **「`scale` 是倍率、不是半径」这条以后可以直接引用实跑，不必再走一遍反汇编**（详见 §1.2）。

### 3.2 ★★ TH18 原生 `ability.anm` 自己就有一族「半径可见的环」：script 33 / 34 / 35

一手（`local/th18.v1.00a/anm/ability/ability.anm.txt:998`–`1042`，`thanm -l 18` 的文本；TH18 的 anm 版本 = **8**，与 `v8.anmm` 的 opcode 表对应）：

```
script script33 {                       ← 「三环」那一支（自己再起 34 / 35 两个子 VM）
    blendMode(1); sprite(-1); layer(15); type(1);
    interruptLabel(2);
    drawRing(80.0f, 16.0f, 64);         ← ★ 半径 80 / 厚度 16 / 64 边
    color(255, 32, 32);
    scriptNew(script34); scriptNew(script35);
    scale(0.0f, 0.0f);
    scaleTime(20, 0, 1.0f, 1.0f);       ← ★ 20 帧从 0 倍扩到 1 倍 ⇒ 「环从中心长到满半径」
+10: alphaTime(10, 0, 0);
+20: delete();
}
script script34 { … drawRing(96.0f, 2.0f, 64); … }        ← 细环（半径 96 / 厚度 2）
script script35 { … drawPolyBorder(112.0f, 64); … }        ← 最外圈描边（半径 112 的 64 边形）
```

| 事实 | 一手 |
| --- | --- |
| `drawRing`（**611**）在 TH18 **有真实数据**：原生 `ability.anm` script34 `drawRing(96.0f, 2.0f, 64)`、script33 `drawRing(80.0f, 16.0f, 64)` | `ability.anm.txt:1000` / `:1023` |
| `drawPolyBorder`（605）在 TH18 也有真实数据：script35 `drawPolyBorder(112.0f, 64)` | `ability.anm.txt:1036` |
| 三个环全是 **`sprite(-1)`**（不消耗任何贴图）+ `layer(15)` + `blendMode(1)`（加算）+ `type(1)` + 红 `color(255,32,32)` + **10 帧淡出 / 20 帧自删** | 同上 |
| `611` 在 `v8.anmm` 的行注是 **`# TH15+`** | `local/vendor/thpages/static/mapfile/v8.anmm:151` |
| script34 由引擎代码起：`0x465bc4`–`0x465be0`（`AnmLoaded__instantiate_vm_to_world_list_back` `0x405bf0`，`(layer=-1, script=34)`）—— 所在函数是 **`MainMenu__on_draw` `0x465870`**（🟡 语义：菜单装饰，与玩法无关） | `th18:0x465bc4`；函数名见 `local/th18.v1.00a/th18-funcs.json` |

> ⚠ **`drawPoly` 与 `drawPolyBorder`/`drawRing` 不是一回事**（这条决定了环怎么挑）：
> `drawPoly` 是**实心**多边形（本仓自己的用词：`120_season_ring_out.anm.txt:3`「32 边形当**实心圆**」、
> `CARDS.md:244`「两个 `drawPoly(1.0f,32)` **实心圆**」），而 `drawPolyBorder` 是**描边**、
> `drawRing` 是**带厚度的环**（本仓 `AUDIT.md:5769` 也把它们并列成「整圈 / 整多边形」的那一族）。
> ⇒ **要在弹幕区里画「一圈界线」，`drawPoly`（实心盘）是错的工具**；`drawRing` / `drawPolyBorder` 才对。

### 3.3 原生 `ability.anm` 的 15 个绘制命中点 + 「谁最适合当半径可编程的环」

原生 `ability.anm` 的规模（实测）：**7 entry / 109 sprite / 68 script（script0..67）**；我们追加的脚本从 **68** 起连续编号
（`assets/ability/entries/ORDER.txt:1` 的「entry = 原生 7 + 行序、sprite = 原生 109 + 行序」同理；脚本连续性由
`assets/build_ability.py:211`–`220` 硬门保证）。15 个绘制的命中点里，**只有 4 个是 `sprite(-1)` 且不需要贴图**：

| # | script | opcode + 字面参数 | layer | blend | 脚本内的 scale 插值 | 存活 | 备注 |
| --- | --- | --- | :-: | :-: | --- | --- | --- |
| 1 | **script34** | **`drawRing(96.0f, 2.0f, 64)`** | 15 | 1 | **一条都没有（全表唯一）** | `+20: delete()` | ★ **最佳原型**（§3.1 的判据全中） |
| 2 | script33 | `drawRing(80.0f, 16.0f, 64)`（厚环）+ `scriptNew(34/35)` | 15 | 1 | `scale(0,0)`+`scaleTime(20,0,1,1)` | `+20: delete()` | 三环同心 + 由中心扩张 |
| 3 | script35 | `drawPolyBorder(112.0f, 64)` | 15 | 1 | 无 | `+20: delete()` | 厚度不可独立给 |
| 4 | script49 | `drawPoly(1.0f, 32)` | 6 | 0 | `scaleTime(60,1,600,600)` | `+60: delete()` | 本仓 277 的配方来源（实心盘） |

其余 11 个命中点（**同一份 `ability.anm`**，逐条；全表里除了上面 4 条，其余都**要贴图或是实心盘**）：

| # | script | opcode + 字面参数 | layer | 贴图 | 备注 |
| --- | --- | --- | :-: | --- | --- |
| 5–6 | script29 / script32 | `texCircle($I0)`（I0=32 / I1=10） | 6 / 21 | entry0 `sprite106` | 要贴图；script31 每帧 `scriptNew` 起一支 32 |
| 7–9 | script42 / 43 / 44 | `texCircle($I0)`（I0=64 / I1=6） | 13 | entry2 `sprite104` | 要贴图；三色（蓝青 / 黄 / 品红） |
| 10 | script46 | `drawPoly(1.0f, 32)` | 3 | `sprite(-1)` | 实心盘；`scaleTime(20,1,120,120)`，`stop()` 常驻（长命模板） |
| 11 | script59 | `drawPoly(1.0f, 40)` | 22 | `sprite(-1)` | 实心盘；`scaleTime(30,4,64,64)`；script58 起 4 支 |
| 12–13 | script61 / script62 | `drawPoly(1.0f, 3)` | 22 | `sprite(-1)` | 3 边形（退化三角）；`scale(10,10)→60→64`，120 帧 |
| 14 | script63 | `drawPoly(1.0f, 40)` | 22 | `sprite(-1)` | 同 12 的时序 |
| 15 | script67 | `drawPoly(1.0f, 40)` | 22 | `sprite(-1)` | `scaleTime(30,4,64,64)` 被紧接的 `scale(64,64)` 覆盖成 64→8 |

★ **判据（缺一不可，照 `native/sdk.h:330` 的约束来）**：① `sprite(-1)` 不吃贴图 ② 带厚度参数（只有 `611 drawRing` 有）
③ **脚本里没有任何 `scale`/`scaleTime`**（否则卡侧 `ce_anm_set_scale` 每帧被插值器盖掉）④ 有 `interruptLabel` 能被卡侧收尾。
**四条全中的只有 `script34`** ⇒ 新脚本照它抄指令，把半径/厚度换成我们的数（§5）。
⚠ 原生那 15 条**没有一条**是「消弹范围环」——它们全是 bomb / 爆炸 / 蓄力光环的一次性特效；
`script33/34/35` 那一族由 `MainMenu__on_draw`（`0x465bc4` 起，🟡 语义=菜单装饰）起。

### 3.4 能 spawn 哪几本的脚本：**只有 `ability.anm` / `abcard.anm` / `abmenu.anm` / `pl0X.anm`**

| anm 槽 | 能不能起脚本 | 一手 |
| --- | :-: | --- |
| `ability.anm`（我们的追加目标） | ✅ | `CE_ABILITY_ANM()` = `*(*(zAbilityManager)+0x0c)`，`native/sdk.h:83` |
| `abcard.anm` / `abmenu.anm` | ✅（画卡图/HUD 用） | `native/engine.h:271`/`:272` |
| `pl0X.anm` | ✅（本仓已用 `build_player_anm.py` 追加过脚本） | `native/engine.h:368`、`native/Makefile:50`–`58` |
| `effect.anm` / `bullet.anm` / `front.anm` / `enemy.anm` / `stXX*.anm` | ❌ **够不到**（没有「按名找 AnmLoaded」的助手） | `native/engine.h` + `sdk.h` 只有上面 4 条槽位访问器 |

⇒ **原生 `effect.anm` 里那些漂亮的原型（script101 `drawPolyBorder(1.0f,12)` @层 16、script85/87/…/97 的 0→384 px 扩散波）
一条都不能直接 spawn**，只能用它们的**配方**（把指令抄进我们自己的 `ability.anm` 脚本）。

### 3.5 本仓已有的「半径环」先例（可抄，不必新发明）

| 先例 | 脚本 | 半径怎么给 | 判定半径 = 环半径？ |
| --- | --- | --- | :-: |
| **116 季节（TH16）** | 120 / 121 / 135（`drawPoly(1.0f,32)`） | 卡侧每帧 `ce_anm_set_scale(id, sc, sc)`，`sc = rel_radius × grow`；内圈 `+16.0f` px | ★★ **是**：`ce_cancel_radius(pos, s->rel_radius, …)` 与 `sc` 同源 |
| 111 二小姐蓄力光环 | 277（`drawPoly(1.0f,32)`，实心盘） | 卡侧每帧 `ce_anm_set_scale(id, k, k)`；⚠ `k` 只有 0.4→2.0 | ❌ 与消弹半径无关 |
| 117 动物灵（TH17） | 155（全屏渐变盘）/ 151–153（`texCircle` 光圈） | **脚本内** `scaleTime` ⇒ 卡侧不能写 scale | ❌ |
| 115 绀珠传（TH15） | 106（擦弹圈**贴图**环） | 脚本内 `scale(2.48→1.155)`，卡侧只写 pos | ❌ 半径 = 3.6 × 判定半径，不是消弹半径 |
| 112 星莲船（TH12） | 223（`drawPolyBorder(1.0f,5)` 五边形）/ 229 / 230 | 223 脚本内 `scaleTime(10,4,20,20)`；229/230 半径写死 62/55 | ❌ |
| **109 花映塚（TH09 复刻）** | **一个都不画** | — | ⛔ **证伪**：`grep -c ce_anm native/cards/pofv.c` = **0**，`pofv.c:30` 明写「本卡当前不建任何 ANM VM」 |
| 110 风神录（TH10） | 无视觉（只 `ce_cancel_radius`） | — | ⛔ 同上 |

★ 两条盘点结论：
1. **本仓唯一的「环半径 = 消弹半径」样板是 116（`native/cards/season.c:923`–`1003`、`:1153`）** —— 111 的结界环照它抄，
   只把 `R` 换成 §4 的 `R_clear`（86.00 px）。
2. **114 / 116 里没有任何「半径环」先例可借**（109 干脆什么都不画，114 的 223/229/230 半径都是写死的原值）
   ⇒ 这一格确实是**新的**，但做法完全能复用既有管线（§5）。

### 3.6 层号：环画在哪一层

| 层 | 归谁 | 对「范围环」的意义 | 一手 |
| --- | --- | --- | --- |
| 6 | 世界列表靠底（原生 `ability.anm` script49 = 蓄力光环就在这层） | **在弹幕之下**：环会被弹幕盖住，看不清边界 | `th18:0x487670` 起的层桩表 |
| 13 | 道具 / 原生卡 42 小槌那一层 | 与小槌同层，仍在弹幕之下或齐平 | `assets/ability/scripts/278_flan_clear_burst.anm.txt` 的 `layer(13)` |
| **16** | 「**子弹之上**」——原生 `abcard.anm` script14（场地中央展示卡）用的就是它；该层也是原生 `effect.anm` script101 描边环的层（我们够不到那支 VM，但层号可借） | ★ 边界永远压在弹幕上面 | `engine/anm/th18/01-vm-instantiate.md:56`、`local/th18.v1.00a/anm/effect/effect.anm.txt:2088` |
| 19 | 世界列表最高一层；本仓 107 森罗结界环用的层（❓ 自定） | 同上，更靠上 | `mods/th18.v1.00a/card-expand/CARDS.md:252` |

⇒ 推荐写 **`layer(16)`**（有原生先例、且「子弹之上」这句话本身有一手出处）；若要与既有 107 一致则写 19。
（原生三环 script33/34/35 用的是 **15**，与子弹大致齐平。）

## 4. 半径口径的统一建议

### 4.1 三条式子摆在一起（全部一手）

| # | 半径 | 式子 | 单位/量纲 | 一手 |
| --- | --- | --- | --- | --- |
| ① | **引擎擦弹圈（圆弹）** | `d² < (pr + max(br/2.5, 40.0))² + br²` | px；`pr` = 自机判定半径（原生 3.0）、**`br` = 每颗弹自己的半径** | `0x45cd90`（`0x4b921c` = 2.5、`0x4b92c4` = 40.0） |
| ② | **引擎擦弹圈（矩形弹）** | 弹框外扩 `24.0` 后与自机框做 AABB | px；`br` 的一半也参与 | `0x45cc20`（`0x4b92a4` = 24.0） |
| ③ | **清屏半径（我们的卡）** | `2 × (pr + 40.0)` = `2 × (3.0 + 40.0)` = **86.00 px** | px | `signal_core.c:709`（`SIGNAL_FLANDRE_RADIUS_MUL × (r + 40.0×100)`） |
| ④ | **特效环半径（现状）** | **已经 `= ③`**（`ring_r` 直接用 `signal_flandre_cancel_radius_x100()`） | px | `native/cards/signal.c:1563`–`1575`（已落地，见 §1.2） |
| ⑤ | **TH15 式抖动半径**（另一个 agent 在做） | 待定 —— 建议 `= ③` | px | — |

### 4.2 差异项在哪里

- **①/② 与 ③ 的差 = 弹自己的 `br` 那一项**：擦弹圈是**逐弹**的（大弹擦得更远），
  而清屏半径是一个**单值圆**（86 px）。取 `br` 的下界（`br ≤ 100` 时 `max(br/2.5,40) = 40`）算：
  擦弹圈最小 = `3 + 40 = 43 px`，而清屏半径 86 px 恰好是它的 **2 倍**。
- 但 `br` 一大，擦弹圈就**越过 43**：`br = 250 px` 的弹，擦弹圈 = `3 + 100 = 103 px > 86`，
  即「擦到了但松手消不到」——**用户口径里的「2×擦弹圈」在数学上只对「小弹」成立**。
- 矩形弹（②）根本没有「圆」可言：它是一条 AABB 判据，**画不成一圈**。

### 4.3 推荐口径（自洽，且改动代价最小）

**把「一个圆」定为卡的权威半径 `R_clear = 2 × (pr + 40.0)`（86 px），凡是要画出来的/要按圈判定的一律用它；
引擎那条逐弹擦弹圈只留给「真的在复刻擦弹」的地方。**

| 用途 | 推荐口径 | 理由 | 改动代价 |
| --- | --- | --- | --- |
| **清屏判定** | `R_clear`（现状，不动） | 用户口径 + 已实跑 | 0 |
| **特效环** | **`R_clear`**：`ce_anm_set_scale(id, R_clear, R_clear)`（**现状已如此**） | 环画的就是**真的被消掉的范围**，这是它唯一的职责 | ✅ 0（已落地）；⚠ 但**松手那一帧环就被收了**（§1.2），要补的是「清屏那一刻也有环」 |
| **红染**（⑧-3） | **改成 `d ≤ R_clear`**（单一圆判据） | 红染是「会/已经被消掉」的预告；改成同一条圆后**环 = 红染 = 清屏**三者的边界逐帧恒等 | `signal_bullet_in_graze()` 的**红染那两处**调用点（`signal.c:1188` 的 keep 判定 / `:1216` 的进圈判定）换成新判据 = **改 2 行 + 加 1 个小判据**；①擦弹计数那处（`signal.c:1111`）**不动** |
| **TH15 式抖动** | **`R_clear`** | 同一条圆，抖动只是表现层相位 | 另一 agent 的合作面（约定同一个宏） |
| **① 大小姐的擦弹计数** | **保留引擎擦弹圈**（不动） | 那一支的语义就是「引擎意义上的擦弹」，`br` 项是它的定义的一部分 | 0 |

★ **为什么不反过来（把环/红染改成引擎的逐弹擦弹圈）**：一圈**画不出**逐弹不同的半径；
若要「按 `br` 逐弹变色」则红染会变成**边界模糊的渐层**，与「看得出一圈消到哪」的用户口径直接冲突。
⇒ 代价最小的一侧是**动红染**，不是动环。

★ **TH09 的先例支持这个方向（一手，§2.6）**：TH09 自己也没让「画出的环」与「消弹圈」逐帧相等 ——
它的环从 16 长到 192，消弹圈从 0 长到 192，**只在终点重合**（`4.0 × 48 = 192`）。
⇒ 我们让四处**共用一个 `R_clear`**（逐帧恒等）**比 TH09 更严**；这不是「照 TH09 做」，是「比它更干净」。

★ **本仓已有先例选了另一条路（115 绀珠传）**：它的擦弹圈环（脚本 106）画在 **50.8 px** = `3.6 × 3.0 + max(br/2.5,40)`
的**下界值**，而红染判据仍是**逐弹**的 `3.6 × 判定半径 + max(br/2.5,40)`（`native/cards/lolk.c:64`、`106_lolk_ring.anm.txt` 的头注）
⇒ **115 接受「环 = 下界那一圈、与红染不完全同义」**。111 现在的情况正相反：环 = `R_clear`（86 px）**比红染边界（43 px 起）大**，
⇒ 圈内 43–86 px 那一段的弹会「在环里但不变红」，比 115 那种「圈比红染小」更容易被用户看出来。**这就是要改红染的理由。**

⚠ **一个必须写进注释的已知让步**：改成 `d ≤ R_clear` 之后，红染**不再等价于引擎的擦弹判定**
（`br` 大的弹在 86 px 外就已经被引擎算作擦弹、但不再变红）。这是有意的取舍，要登记 `TUNING.md`。
若用户要求「红染必须与引擎擦弹严格同义」，那就回到 4.2：**环只能画 43 px（下界）那一圈**，
并在文档里写明「大弹的擦弹圈大于环」，二选一，不能两全。

### 4.4 实施上的一句话

新增一个共享宏（或 `signal_core.h` 里的 `signal_flandre_clear_radius()` 已有函数的复用），
让环 / 红染 / 抖动 / 清屏四处**取同一个数**：`r = 2 × (CE_PLAYER_HITBOX_R_NF + 40.0)`。

## 5. 落地建议

> **本节除标「复刻」的行外，整节是 ❓ 自定**（数值、颜色、层号、时长），按 `CLAUDE.md` 的六问：第 1 问（原版是什么）、
> 第 3 问（TH18 有没有现成代码）在 §2/§3 已答完；下面全是取舍，要进 `TUNING.md`。

### 5.1 现状：范围环**已经**落地了（`2026-09-16j`），剩下的是「清屏那一刻」

**已经做到**（§1.2 快照）：脚本 277 = `drawPolyBorder(1.0f, 64)`（底本 = 原生 TH18 `ability.anm` script35 —— **复刻**），
卡侧每帧 `ce_anm_set_scale(id, ring_r, ring_r)`，`ring_r` **直接就是清屏半径**（`signal_flandre_cancel_radius_x100()` → 86.00 px）。
⇒ **「半径 = 参数 × scale」这条语义已经被实机验证**（上一版把 scale 当半径写给 0.4→2.0，实跑看不见）。

**还没做到**：环 **只在蓄力期**存在 —— 松手那一帧 `ce_anm_interrupt(id, 1)` 就把它收了（20 帧淡出），
而**消弹就发生在那一帧** ⇒ 用户要的「一眼看出消到哪」，在最关键的那一帧上仍然只有一记爆闪 278。

### 5.2 补「清屏那一刻的环」：两条路（选一条）

| | 路 1 · 松手不收回 277（**最小改动，推荐先试**） | 路 2 · 新增脚本 279 做「扩张环」 |
| --- | --- | --- |
| 做法 | 松手那一支**不调** `ce_anm_interrupt`（也不清句柄），让脚本按自己的 20 帧淡出自然收 —— 清屏就落在这 20 帧里 | 照原生 **script33** 的写法新建一条：`scale(0,0)` + `scaleTime(20,0,1,1)`（**环从中心扩到满半径**），半径 = `R_clear` |
| 观感 | 环保持原大小、原地淡出（用户能看到「刚才那一圈就是消掉的范围」） | 更像 TH09 的「扩张结界」（§2.4：TH09 就是 16→192 扩张），与 278 爆闪同时放 |
| 代价 | 改 3 行（去掉 interrupt / 句柄交给 reset 兜底） | 新脚本 + 卡侧 spawn/收尾 + `make anm` + `AUDIT.md` §BE 登记 |
| 编号 | **不动**（仍是脚本 277） | 新脚本 **279**（下一个空号）；`sprite(-1)` ⇒ **零 sprite 消耗** |
| 风险 | 环会多留 20 帧（若用户觉得「消完还挂着」就改回 20 帧以内） | `drawRing`（611）在世界层没实证 🟡（原生只在 `MainMenu__on_draw` 用过）⇒ 稳妥起见**用 `drawPolyBorder`（605，本仓已实跑过）** |

★ 若走路 2，本条**不需要**新 sprite、**不需要**动 `assets/ability/entries/ORDER.txt`（`sprite(-1)`）
⇒ `ORDER.frozen.txt` 的 186 行冻结段**必然不红**；编号从 `assets/build_ability.py:211`–`220` 的连续性硬门出来就是 **279**。

### 5.3 新增 / 修改清单

| # | 动作 | 文件 / 位置 | 内容 | 类别 |
| --- | --- | --- | --- | --- |
| 1 | **已落地（他人）** | `assets/ability/scripts/277_flan_charge_ring.anm.txt` + `native/cards/signal.c:1543`–`1586` | 范围环 = `drawPolyBorder(1.0f, 64)`（底本 = 原生 script35）+ 卡侧 `scale = R_clear` | **复刻**（`2026-09-16j`） |
| 2 | **修改（路 1，推荐）** | `native/cards/signal.c:1576`–`1586`（松手那一支） | **去掉** `ce_anm_interrupt(s->flan_ring_vm, 1)`，让环按脚本的 20 帧淡出跨过清屏那一帧；句柄清零仍要做（防重复） | ❓ 自定（时长/观感） |
| 3 | **新增（路 2）** | `assets/ability/scripts/279_flan_clear_barrier.anm.txt` | 底本 = 原生 **script33 的扩张写法** + **script35 的 `drawPolyBorder` 图元**（避开 `drawRing` 的 🟡） | **复刻配方** |
| 4 | 同上 | 同上 | 半径（`drawPolyBorder(1.0f, 64)` + 卡侧 scale，或字面量 `86.0f`）、`layer`（13 与 277/278 一致 / 16「子弹之上」）、颜色（277 已是 `(120,220,255)`）、扩张时长 | ❓ 自定 |
| 5 | **修改（路 2）** | `native/cards/signal.c:1601`–`1621`（松手那一支） | 在 `ce_cancel_radius` 前后 spawn 环 + `ce_anm_set_pos`；要逐帧给半径就存句柄（照 `signal.c:289` 的 `flan_ring_vm`） | 复刻配方 / ❓ 自定 |
| 6 | **自动生成** | `native/anm_ids.h`、`native/build/ability.anm` | `make anm`（= `assets/build_ability.py`）+ `make anm-verify`；⚠ **DLL 与 anm 必须同时发布** | 生成物 |
| 7 | **不用动** | `assets/ability/entries/ORDER.txt` | `sprite(-1)` ⇒ 零 sprite 消耗（下一个可用号仍是 entry 54 / sprite 435） | — |
| 8 | **收尾** | `signal.c` 的 `signal_support_reset()`（`signal.c:1714`；光环清理在 `:1757`–`1761`） | 路 2 的新句柄要在这里收（`METHOD.md` 的长持续效果铁律：删卡发生在商店里，那一刻只有 `on_delete` 跑得到） | 纪律 |
| 9 | **修改（口径统一）** | `native/cards/signal.c:1188` / `:1216` | 红染的两处判据换成「`d ≤ R_clear`」（环已经是 `R_clear`；不换则环与红染边界不同） | ❓ 自定（§4.3） |
| 10 | **登记** | `TUNING.md` / `NEXT.md` / `AUDIT.md` §BE | 全部 ❓ 自定项 + 实跑验收点 + 「动了 `assets/ability/scripts/**` = 共享件」 | 纪律 |

### 5.4 实跑要看的三件事（进 `NEXT.md`）

1. **清屏那一帧有没有环**（路 1/路 2 的验收点）—— 这是用户这一轮的**原话诉求**。
2. 环的半径与「松手那一帧实际消掉的弹」的**最远距离**对不对得上（86 px；若有 1 帧/1 px 的偏差要记下来）。
3. 若走路 2 且用了 `drawRing`（611）：它**在层 13/16 画不画得出来**（原生只在 `MainMenu__on_draw` 用过 🟡）
   —— 画不出来就退 `drawPolyBorder`（605，本仓 223/229/230 已实跑过）。

## 6. 复现命令

```bash
# --- TH18 侧：环半径语义（本仓已完成的反汇编，直接读）---
sed -n '90,130p' engine/anm/th16/09-drawing.md        # opcode 604/605/611 的字段落点
grep -n 'sprite_size' engine/anm/th16/09-drawing.md   # 「最终尺寸 = sprite_size × scale」

# --- TH18 侧：清屏半径与擦弹圈的实现 ---
sed -n '1040,1078p' mods/th18.v1.00a/card-expand/native/cards/signal.c   # 擦弹圈两个式子（圆/矩形）
sed -n '700,720p'   mods/th18.v1.00a/card-expand/native/cards/signal_core.c  # 清屏半径 = 2×(pr+40)
grep -n 'SIGNAL_FLANDRE_\|SIGNAL_GRAZE_' mods/th18.v1.00a/card-expand/native/cards/signal*.h mods/th18.v1.00a/card-expand/native/cards/signal.c

# --- TH18 侧：蓄力光环/爆闪现状 ---
cat mods/th18.v1.00a/card-expand/assets/ability/scripts/277_flan_charge_ring.anm.txt
cat mods/th18.v1.00a/card-expand/assets/ability/scripts/278_flan_clear_burst.anm.txt

# --- TH09 侧（只读；数据包解到 /tmp，别在仓库里解）---
cd /home/nipporita/renkolab && objdump -d -M intel local/th09.v1.50a/th09.exe > /tmp/th09.dis
mkdir -p /tmp/th09x && ./local/vendor/thtk/build/thdat/thdat -x 9 -C /tmp/th09x local/th09.v1.50a/th09.dat pl00.anm
./local/vendor/thtk/build/thanm/thanm -l 9 /tmp/th09x/pl00.anm -m local/vendor/thpages/static/mapfile/v8.anmm
```

## 7. 还缺什么（明确未取到）

| # | 缺什么 | 试过哪些方法 | 影响 |
| --- | --- | --- | --- |
| 1 | TH09 侧还没取到的两条：① 每帧充能增量的被乘常数 `th09:0x4b36b8`（**全 exe 只有读、无写点**，落在 `.data` 的 0 填充区）；② **输入位 → 物理键**（bit2/bit4 谁是 Shot 谁是 Bomb；查过键配置记录 `0x4ace70/0x82/0x94` ↔ `0x4b3488`、键名表 `0x4a1e04`、DirectInput/`GetKeyboardState` 附近，没找到 scancode 的写入者） | 见 §2.7 | 不影响本文任何结论（bit1 = 低速&蓄力键已经由菜单字符串 + `0x420255` 证实） |
| 2 | `drawRing`（611）的 `r`/`t` 在**顶点层**的确切几何（「中心半径 + 环宽」还是「外径 + 厚度」，`sprite_size.y` 乘的是 `scale.y` 吗） | 通读 `engine/anm/th16/09-drawing.md`（只有 mode 9/13/14/24/25 的闭式，poly 族 17/18/19 三支未解）；TH18 侧只拿到 `611 → mode 19`、handler `th18:0x47b4a6`、writer `0x483060`、恒整圈 | 🟡 落地时**照原值抄 + 实跑一眼**即可绕开；不想赌就退 `drawPolyBorder` |
| 3 | `drawRing`/`drawPoly` 到底是不是「中心→边缘的径向渐变」（`color` vs `color2` 分别落在哪些顶点） | 只找到本仓自己的措辞（「实心圆」「渐变圆」），`engine/anm/th16/09-drawing.md` 只在 mode 9 那条写了「内外可用不同颜色」，**没写 poly 族** | 🟡 只影响观感（想「中心透明、边缘亮」的柔和环就得先验） |
| 4 | 原生 `ability.anm` script33/34/35 那一族**在游戏里是谁在用** | 反查 `0x405bf0` 的调用点：`th18:0x465bc4`（script 34，所在函数 = `MainMenu__on_draw` `0x465870`）与 `th18:0x455d8f`（script 66）；**卡侧**的调用点没穷举 | 🟡 不影响「配方可复用」，只影响「原版是什么」的叙述 |
| 5 | `layer(16) = 子弹之上` 的第二条独立一手 | 只有 `engine/anm/th18/01-vm-instantiate.md:56` 一句 + 原生 `effect.anm` script101 也落在 16（层号巧合，不是语义证据） | 🟡 实跑一眼就知道；退路是 107 用过的 19 |
| 6 | 顺带登记（**未改**）：`engine/card/th18/10-extensibility-limits.md:102` 写原生 `ability.anm` 有「267 sprite」= 过期值，实测 **109**；`assets/ability/entries/ORDER.txt:41` 的用量注释仍是 2026-09-13 的旧值 | 由资产盘点专项跑仓里的 `tools/capcheck.py` 核出 | 文档准确性问题，与本需求无关 |

## 8. 一句话总结

**要的那一圈不用自造**：TH18 原生 `ability.anm` 的 **script34**（`sprite(-1); layer(15); blendMode(1); drawRing(96.0f, 2.0f, 64); +10: alphaTime(10,0,0); +20: delete();`）
就是「一圈半径可编程、带厚度、不吃贴图、20 帧自删的环」——照它抄一条新脚本（**279**），
半径交给卡侧每帧 `ce_anm_set_scale(id, r, r)`，而 `r` **就用清屏那一个数**
（`signal_flandre_cancel_radius_x100()` → **86.00 px**，`native/cards/signal.c:1601`–`1606` 已经算好了）。
**不需要新 sprite、不需要动 `ORDER.txt`、不会碰 `ORDER.frozen.txt` 的冻结段**；红染那一支若要与环同义，
把 `signal.c:1188` / `:1216` 两处判据换成同一个圆即可（代价 2 行）。
**TH09 的固灵结界已经取到全链**（`etama.anm` script 91/92/93 + 引擎 C 代码画的 64 段环带，§2.4/§2.5），
它给我们的**不是配方而是两条判据**：① 环必须是「有厚度的圈」而不是实心盘；② 连 ZUN 也只把绘制半径与消弹半径
**对齐终点**，我们共用一个 `R_clear` 是更强的做法（§2.6）。
