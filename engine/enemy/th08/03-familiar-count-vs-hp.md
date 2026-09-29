# 03 · TH08（永夜抄）**使魔数 vs 敌人 HP**：`0x41fd40` 的语义、分档结论与三条反例

> **版本**：TH08 v1.00d（`th08.exe`，imagebase `0x400000`，样本 `local/th08.v1.00d/`；反编译 `local/vendor/th08_dec`）。
> 本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：一手 —— ① ECL 反汇编：`local/vendor/thtk/build/thecl/thecl -d 8 -m local/vendor/eclmap/eclmap/th08.eclm <dat/ecldata*.ecl>`
>（24 个文件；本批的文本、统计脚本与 37 行 JSON 在本机草稿区 `local/_scratch/th08f/` —— **草稿不入库**）；
> ② `objdump -d` 逐指令读 `th08.exe`（全库转储 `local/_scratch/th08_full.asm`）；
> ③ ExpHP 符号/结构体名（`local/vendor/th-re-data/data/th08.v1.00d/`）。
> **可信度**：`0x41fd40` / `0x41f000` / `0x41fd20` 的正文、`+0x3380` 的三处 `+1` 与一处 `−1`、两个读点
> ✅（逐指令，本批复核）；台数 vs HP 的分档结论 ✅（40 个「一次性」样本，Spearman ρ = 0.882 / R² = 0.59）；
> 「**角色 > HP**」这条归因 🟡（样本够但不是判决性实验）；`$FAMILIAR_COUNT` = gvar 10096 那一条 🟡
>（来自本批统计的口径，本文未逐指令复核）。
> **用途**：卡 108（TH18 移植永夜抄的人妖系统）的**惰性绕转使魔**参数 —— 台数分档表、一手血量基准表、
> 半径分档。消费方：[`youkai_core.h`](../../../mods/th18.v1.00a/card-expand/native/cards/youkai_core.h)、
> [`youkai.c`](../../../mods/th18.v1.00a/card-expand/native/cards/youkai.c)；
> 决策记录见 [`AUDIT.md` §BE #150](../../../mods/th18.v1.00a/card-expand/AUDIT.md) 与
> [`TUNING.md` §2.1ccc-26](../../../mods/th18.v1.00a/card-expand/TUNING.md)。
> 姊妹篇：[`01-familiars-and-anm.md`](01-familiars-and-anm.md)（使魔 = 真敌人 + 旗标）、
> [`02-familiar-anm-57-58.md`](02-familiar-anm-57-58.md)（另外两套外观）。

## 0. 一句话结论

1. **`th08:0x41fd40` 数的是「`[e+8]` 这条链有多长」**：门 = `th08:0x41f000`（`[e+0x2da4] == 0` 且 `[e+8] != 0`），
   循环 = `cur = [cur+8]`、每跳 +1，返回值**不含自己**。ExpHP 给的名字是
   `Enemy::sub_41fd40__possibly__get_familiar_count`（`possibly` = 社区名不确定）——
   **它确实是击破撒点用的那个数**：`th08:0x42adbc call 0x41fd40` → `th08:0x42adc4 je`（0 ⇒ 整段撒点跳过）。
   v1.00d **没有嵌套链**（见 §2 的 `$FAMILIAR_COUNT`）⇒ 「链长」与「使魔数」在这一版**恒等**。
2. **台数与 HP 强单调但不是一个公式**：40 个「一次性撒完」样本上 ρ = 0.882 / R² = 0.59，
   数据是**角色分档**（中ボス / 撒き役）⇒ 我们按**分档表**取，不拟合。
   最常见四个台数 = **{2, 4, 8, 13}**（一次性样本：4×14、2×12、8×5、13×4、9×3、5×2）。
3. **三条反例**（别把「大怪 ⇒ 使魔多」当铁律）：`ecldata3 Sub13/Sub14`（HP 500/600 却**循环撒 50 台**）、
   `ecldata2 Sub7`（HP 1000 只 4 台）、`ecldata2 Sub11`（HP 400–500 有 9 台）⇒ **角色才是自变量**；
   另有 80+ 个 HP ≥ 4000 的 sub **一台使魔都没有**（如 `ecldata6 Sub32` HP 20000）。
4. **使魔的血是脚本给的**：`familiarCreate` 第 4 实参（`th08:0x42a680` 里落到 `enemy+0x2dfc`），
   道中实测集合 = **50 / 100 / 180 / 200 / 300 / 400 / 600 / 2400 / 2600**，**道中几乎恒为 100**；
   218 个使魔子程序里**没有一个**用 `lifeSet` 覆盖。
5. **绕转半径是每个台数段各自写的**：`moveCircle` 第 1 实参 —— 道中主力 `r=100`（stage1–3 小怪）、
   `r=60`（stage4A/4B 的 Sub5）、`r=20`（stage4b–8 的大怪）⇒ **台数越多、圈越紧**。

## 1. `th08:0x41fd40` 的正文（逐指令）

```
41fd40  push ebp / mov ebp,esp / sub esp,0xc
41fd46  mov [ebp-0xc],ecx          ; this
41fd4c  mov [ebp-0x8],eax→this     ; cur = this（[ebp-0x8]）
41fd4f  mov [ebp-0x4],0            ; count = 0
41fd56  mov ecx,[ebp-0xc]
41fd59  call 0x41f000              ; ★ 门（见下）
41fd5e  test eax,eax / je 41fd7f   ; 门假 ⇒ 直接返回 count
41fd62  mov ecx,[ebp-0x8]
41fd65  cmp [ecx+0x8],0 / je 41fd7f ; 链尾（下一跳为 0）⇒ 停
41fd6b  mov edx,[ebp-0x8] / mov eax,[edx+0x8] / mov [ebp-0x8],eax
41fd74  mov ecx,[ebp-0x4] / add ecx,1 / mov [ebp-0x4],ecx
41fd7d  jmp 41fd62                 ; 继续走
41fd7f  mov eax,[ebp-0x4] / leave / ret
```

门 `th08:0x41f000` 的正文（本批复核）：`cmp [eax+0x2da4],0` **且** `cmp [ecx+0x8],0`
⇒ 两者都取「非 0 / 0」的特定极性才返回 1（`[e+0x2da4] == 0 && [e+8] != 0`）。
另一个姊妹函数 `th08:0x41fd20`（`HasOwnerEnemy` 那个名字的来路）只做一件事：
`cmp [eax+0x2da4],0 / setne cl` ⇒ **返回 `[e+0x2da4] != 0`**，即「这台敌人有没有主人」。
⇒ 两个函数的判据都落在同一个字段 **`e+0x2da4`** 上，一个问「有没有主」、一个问「有没有链」。

## 2. `+0x3380`：使魔计数的三个 `+1`、一个 `−1`、两个读者

| 位置 | 做了什么 | 一手 |
| --- | --- | --- |
| `th08:0x41b0de` / `th08:0x41b28e` / `th08:0x41b4af` | `mov ecx,[eax+0x3380]` / `add ecx,1` / `mov [edx+0x3380],ecx` | 创建使魔那三条分支的尾巴（opcode 90/91/92 一族） |
| `th08:0x42d63a` / `th08:0x42d64c` | `mov edx,[ecx+0x3380]` / `sub edx,1` / `mov [ecx+0x3380],edx` | **唯一的 `−1`**，在死亡处理里（`0x42d625 call 0x41fd20` 之后） |
| `th08:0x42b10d` / `th08:0x42b150` | 读它（`Enemy::sub_42adb0` 撒点那一族） | 击破撒点的颗数依据 |
| `th08:0x4379f4` | `push [eax+0x3380]`（另一个读者） | anm / 显示那一侧 |

⚠ 字段名（`__familiar_count__but_not_total`）来自 ExpHP 的字段表，不是本文的发明；
「**but_not_total**」这个后缀正是 §0 结论 1 里那句「链长 ≠ 总数」的来源。🟡 `$FAMILIAR_COUNT` = gvar 10096
（getter `th08:0x4207b7` 第 96 项 → `th08:0x420701`）= **total 语义**；v1.00d **没有嵌套链**
⇒ 两者在实战数据上**恒等**（本批统计口径，未逐指令复核）。

## 3. 台数 vs HP：分档结论（卡 108 用的就是这张表）

统计口径：24 个 ECL / 1481 条目 / **1016 个 `familiarCreate*` 站点**；只取「**一次性撒完**」的
**40 个 (HP, n) 样本**（把「循环撒き型」剔掉，见 §4）⇒ Spearman **ρ = 0.882**、R² = 0.59。

| 父敌最大血量 | 台数 | 样本（中位数/集合） |
| --- | --- | --- |
| `<= 200` | **2** | ≤200 档实测全是 2（{2,2,2,2,2}） |
| `<= 1000` | **4** | 200–500：{2×7, 4×9}；500–1000：{4,4,4,4,5,5,9,13} ⇒ 中位 4 |
| `<= 2000` | **8** | 1000–2000：{4, 8×4, 9, 13×3} ⇒ 中位 8 |
| 其余 | **13** | >2000 实测 {13}；也是全表最大档 |

**为什么不用连续公式**：ρ 高（0.882）但 R² 只有 0.59 —— 数据是**角色分档**（中ボス / 撒き役 / 大怪），
不是同一条曲线；拟合出来的小数台数在语义上也不成立（台数是整数、而且每台都是一次 `familiarCreate`）。
⚠ 原始道中全表里还有 **15 / 50 / 53** 这类台数（如 `ecl6 Sub0` HP 1000 → 15、`ecldata3 Sub13` → 50、
`ecl8 Sub29` HP 4000 → 53）：50 已确认是**循环撒き型**；15/53 的归属未逐条核 🟡
⇒ 本表的档值取「一次性样本」的中位数，最后一档封在 **13**。

## 4. 三条反例（**必须一起读**，否则会把这张表当铁律）

| 反例 | 现象 | 结论 |
| --- | --- | --- |
| `ecldata3 Sub13` / `Sub14` | HP **500 / 600**，却**循环撒 50 台** | 台数与 HP **脱钩**：决定它的是**存活帧数 / 生成间隔**，不是血 |
| `ecldata2 Sub7` | HP **1000** 只有 **4 台** | 同为「中ボス」角色，HP 高不代表使魔多 |
| `ecldata2 Sub11` | HP **400–500** 却有 **9 台** | 撒き役角色可以「血少但台数多」 |

**归因**（🟡）：自变量是**角色**（中ボス / 撒き役 / 大怪 + 道中波次设计），HP 只是与角色**相关**；
另有 80+ 个 HP ≥ 4000 的 sub（`ecldata6 Sub32` HP 20000 等）**一台使魔都没有** ——
「大怪一定有使魔」这句话在 TH08 里也不成立。

## 5. 半径 / 血量 / 掉落 / hitbox 的实测读数

| 项 | 一手读数 | 出处 |
| --- | --- | --- |
| 绕转半径（`moveCircle` 第 1 实参） | `r=120 ×148 / r=100 ×104 / r=20 ×93 / r=60 ×10` | 本批统计（全 24 文件、变量实参按展开计）；本次可复核的 19 个道中/SP 文本里 `r=20 ×32 / r=100 ×18 / r=120 ×14 / r=60 ×4` |
| 半径 × HP 的关系 | stage1–3 小怪 `r=100`、stage4A/4B 的 Sub5 `r=60`、stage4b–8 的大怪 `r=20` | 37 行道中全表 ⇒ **HP 越大、台数越多、圈越紧** |
| 角速度（第 3 实参） | `0.05235988`（3°/帧）、`0.8333333`、`1.5`、`2.5`、`3.0` 等 | 道中全表；卡 108 取 3°/帧 |
| 使魔 HP（`familiarCreate` 第 4 实参，落 `enemy+0x2dfc`） | 道中实测 **50 / 100 / 180 / 200 / 300 / 400 / 600 / 2400 / 2600**，**道中几乎恒 100**；218 个使魔子程序**无一** `lifeSet` 覆盖 | `th08:0x42a680`：`cmp [ebp+0x10],0` / `jl` / `mov [e+0x2dfc],…` |
| hitbox | **每只使魔都是 `hitboxSet(24.0f, 24.0f)`**（opcode 77，handler `th08:0x41a840`） | 道中全表 60/60；**没有任何使魔设 `hurtboxSet`** |
| 出生保护 | `setInvuln` 实测 **30**（7 处）/ **120**（6 处） | 道中全表 ⇒ 「多数有出生保护」 |
| `drop_main`（第 5 实参，落 `enemy+0x3304`，消费点 `th08:0x42bea0`） | 统计口径 **80% 是 `-2`（不掉道具）**，其余多为 `dropExtra`；但**被自机击破固定掉 1 刻符** | 前者本批统计；后者一手 `th08:0x42b29b` |

**由 `moveCircle` 的两个地址得到的一条机制**（卡 108 的「**定圆心**」就是照它做的）：
`moveCircle`（opcode 73，handler `th08:0x41a4b7`）把**圆心设成使魔自己当前的位置**
（`th08:0x41a50b`–`0x41a52a`：`+0x2dd0 := +0x2d34` 三个 dword 拷贝），而
`familiarCreate`（opcode 91，handler `th08:0x41f280`）的生成点 = **本体位置 + (x,y)**
（`th08:0x41f2ba`–`0x41f2d3` `Float3::operator+=` `parent->final_pos`；脚本全写 `0,0`）
⇒ **圆心 = 生成瞬间那一点，本体后来飞走圆心不跟**。

## 6. 24 个 ECL 的清单（条目数 = `^sub SubN()` 定义数）

| 文件 | 条目数 | 含 `familiarCreate` 的 sub 数 | `familiarCreate*` 站点数 |
| --- | --- | --- | --- |
| `ecldata1.txt` | 53 | 10 | 33 |
| `ecldata1sp.txt` | 43 | 6 | 21 |
| `ecldata2.txt` | 65 | 14 | 86 |
| `ecldata2sp.txt` | 53 | 8 | 59 |
| `ecldata3.txt` | 78 | 21 | 110 |
| `ecldata3sp.txt` | 64 | 13 | 92 |
| `ecldata4a.txt` | 61 | 9 | 36 |
| `ecldata4asp.txt` | 52 | 8 | 32 |
| `ecldata4b.txt` | 78 | 12 | 96 |
| `ecldata4bsp.txt` | 72 | 10 | 84 |
| `ecldata5.txt` | 90 | 16 | 101 |
| `ecldata5sp.txt` | 53 | 9 | 34 |
| `ecldata6.txt` | 88 | 9 | 14 |
| `ecldata6sp.txt` | 81 | 7 | 10 |
| `ecldata7.txt` | 89 | 11 | 29 |
| `ecldata7sp.txt` | 84 | 10 | 32 |
| `ecldata8.txt` | 156 | 20 | 84 |
| `ecldata8sp.txt` | 117 | 15 | 61 |
| `ecldata_al.txt` | 9 | 1 | 2 |
| `ecldata_rm.txt` | 9 | 0 | 0 |
| `ecldata_sk.txt` | 10 | 0 | 0 |
| `ecldata_yk.txt` | 20 | 0 | 0 |
| `ecldata_ym.txt` | 12 | 0 | 0 |
| `ecldata_yy.txt` | 12 | 0 | 0 |

合计 **1016** 个站点（与 §3 的统计口径一致）；
「条目数」这一列是**本文自己的计数口径**（`^sub SubN()`），统计脚本那份的 1481 把
非 `SubN` 命名的小节也算进去了 ⇒ 两个数不矛盾。

## 7. 卡 108 怎么用它（映射，便于对照）

| 一手事实 | 卡 108 的用法 | 备注 |
| --- | --- | --- |
| §3 的分档表 | `yk_orbit_count_for_hp()`（`youkai_core.c`） | 父敌**最大血量**（`CE_ENEMY_HP_MAX`）定 HP 档 |
| §3 的趋势（后面的面台数更多） | `yk_orbit_stage_bonus()`：`min(面号 − 1, 5)`，总台数 = `clamp(HP 档 + 加成, 1, 18)` | ❓ **owner 口径**（「没过一面，同样血量的使魔数 +1」）；本文档只提供趋势依据 |
| §5 的半径段 | `yk_orbit_radius_for_count()`：`n<=2 ⇒ 100`、`n<=5 ⇒ 60`、其余 ⇒ 20 | ❓分档界（2/5）是我们的取法 |
| §5 的使魔 HP | `YK_ORBIT_HP_ONEHAND`（按台数档 100/100/300/600）× `YK_ORBIT_HP_SCALE`（**当前 = 1**）⇒ 就是一手原值 | **一手结构 + 缩放旋钮**（旋钮留着；TH18 vs TH08 的 DPS 差异核查另行进行），❓自定 |
| §5 的 `moveCircle` 语义 | **定圆心**：圆心 = 建壳那一刻的父敌坐标，之后不跟 | 一手：`0x41a50b`–`0x41a52a` + `0x41f2ba`–`0x41f2d3` |
| §5 的 hitbox / 无 hurtbox | 打得掉、但没有体术（不进 `s_map`） | owner 口径与一手一致 |
| §5 的 `0x42b29b` | 被自机击破掉 **1 刻符**（`yk_kill_burst` 尾巴那一支） | 使魔被打死**不爆盘** |
| §4 的循环撒き型 | **不做**（本卡只补一次性的一圈） | 所以分档表只覆盖「一次性」那一类 |
