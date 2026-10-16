# HANDOFF-109-BOSS-BG-WARP：boss 背景扭曲 ＋ 两件观感件（**三包可独立分派**）

> **状态**：★ **只读调研已完成**（2026-10-15；anm 直读 + exe 反汇编 + 实图核对）。**未编译、未改卡**。
> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文**裸地址默认属它**；引用别的版本写成 `th16:0x…`。
>
> **这份是什么**：把 109 的「boss 背景/特效」三件事切成**一个子 agent 一份**的工单：
> **WP-A 红色旋转魔法阵**（草案已写 ✓ 便宜）／**WP-B boss 附近红色叠加（红雾）**（草案已写 ✓ 便宜）／
> **WP-C ★★ 真·局域背景扭曲**（机制已查清、**零实现**，**必须单独一人**）。
> 每包写全：目标 → 一手依据（附地址）→ 要动的文件 → 步骤 → 验收 → 禁区 → ❓待定。
>
> **相关**：[`../../../engine/enemy/th18/07-boss-aura-mesh.md`](../../../engine/enemy/th18/07-boss-aura-mesh.md)（§9.14–§9.27 一手全过程，**WP-C 的证据全在那**）、
> [`../../assets/ability/_draft_109/README.md`](assets/ability/_draft_109/README.md)（两件观感件的脚本草案）、
> [`HANDOFF-109.md`](HANDOFF-109.md)（109 整卡交接）、[`WORK-PACKAGES.md`](WORK-PACKAGES.md)（分包纪律）、
> [`GATES.md`](GATES.md)、[`AUDIT.md`](AUDIT.md)（共享件登记）。

---

## 0. owner 口径（权威，逐字；WP-C 的验收就照这个）

1. 「它是一个**切实的从 x,y → x',y' 的扭曲映射**，只是，**x,y 距离 boss 过大的时候 x = x', y = y'**」
2. 「**连续变形**（像水面/玻璃，线条变弯）」「边界是**圆形**（跟魔法阵一样）」「**只有背景**在扭，敌弹/自机/道具不受影响」
3. 「**所有 boss，所有非符**」「只要**被定义为 boss 就存在**，不过**在 boss 本体出现**，**剧情里不出现**；
   **跟那个红色魔法阵同时出现同时消失**」
4. 「**TH16 5 面双 boss 的时候都没出现**」——owner 认为**可能是 bug**（★ 已核：TH16 **有**同一套代码，见 §5.2.6）
5. 红魔法阵要**先安排上**；另有「boss 附近背景**红色的叠加**」= ★ **跟着 boss 缓慢翻动的一圈红雾/红光，边缘不硬**

---

## 1. 三包总表

| 包 | 目标 | 现状 | 独立度 | 预估 | 要先定的 |
| --- | --- | --- | --- | --- | --- |
| **WP-A** | 红色旋转魔法阵进 109 | ★ **脚本草案已写**（未编译）| 与 B 同管线，建议同一人 | 半天 | `type(8)` 的**层号**（§3.4）|
| **WP-B** | boss 附近「红雾」叠加 | ★ **脚本草案已写**（未编译）| 与 A 同管线 | 半天 | 要不要照原生三档缩放 |
| **WP-C** | ★★ **真·局域背景扭曲** | 机制已查清、**零实现**；卡侧**无顶点绘制设施** | ★★ **必须单独一人到底** | 见 §5.3 | 半径/强度 + 是否「只作用于背景」 |

★ **A/B 与 C 的文件集刻意不重叠**：A/B 只碰 `assets/ability/**` ＋ `native/cards/pofv.c` 的起 VM 一处；
C 要碰的是**新设施 + 渲染时机**（§5.4）。两者可以同时开工。

---

## 2. 共同前提（**每个包都先读这三段**）

### 2.1 构建 / 门禁

| 命令 | 作用 |
| --- | --- |
| `make -C native anm` | 重编 `ability.anm`（消费 `assets/ability/{entries/ORDER.txt,scripts/*.anm.txt}`）＋ 生成 `anm_ids.h` |
| `make -C native anm-verify` | 复核最终 `ability.anm` 的脚本 / sprite / entry / 矩形对应 |
| `make -C native check` | quick + cards-check + anm-verify + sht-verify + snd-check + dllx87 + dllverify |
| `make -C native release` | 唯一发布路径（先过 `release-check`，发布后自动记账）|
| `python3 tooling/check-docs.py` | 文档规范（表格单元格 ≤200 字符等）；**每次改文档都跑** |

### 2.2 三条硬规矩（踩过）

1. **`entries/ORDER.txt` 只许 append**：第 k 个数据行 → `entry 7+k` / `sprite 109+k`；`ORDER.frozen.txt` 逐行 sha256 核对，
   **挪动/删改直接报错退出**。一个 anm **最多 256 entry**（当前 48，余量足）。
2. ★ **DLL 与 `ability.anm` 必须同时发布**：DLL 里烘着 `anm_ids.h` 的宏值。发布后 **4 份 DLL 必须同 md5**
   （规矩在 `local/DLL-SYNC.md`，那是本机文件）。
3. **新增站点（breakpoint）要记账**：`native/sites.py` 的 `SITE_BUDGET = 24`，**当前余量 0**
   （`GATES.md` §265）⇒ 谁要加站点，**先** `GATES.md` §8 记账 + `AUDIT.md` §BE 登记 + **owner 拍板**，且同一时间只允许一个包在做。

### 2.3 一手纪律（`METHOD.md`）

* **UI 与动画一律先从原代码找**（脚本号 / 地址 / 数值 / 触发条件），照抄实现；**贴图走原生切图，别自己画**。
* 原版确实找不到的标 **❓ 自定** 并登记，**不许包装成「复刻」**。
* 一手代码（附地址） > 推断 > 社区单源。

### 2.4 坐标空间（WP-A/B/C 都要用）

* 世界坐标：x ∈ ±192（**已以 0 为中心**）、y ∈ [0,448]；`ce_anm_set_pos` / `vm_set_world_pos` 吃**世界坐标**。
* **UI 列表**（`ce_anm_spawn_ui`）用**表面 px**：游玩区 **384×448**、在 **640 空间**里左上角是 `(128,32)`，表面 px = 再 ×2
  （一手：[`HANDOFF-109-HUD-STATE.md`](HANDOFF-109-HUD-STATE.md):67）。
* ★ 选错列表的典型症状：整组特效落在屏幕中间（`pofv.c:736` 的注释记着这次）。

---

## 3. WP-A：红色旋转魔法阵（**照抄件**）

### 3.1 目标

在 109 的场上出现一支**红色六芒星魔法阵**：60 帧从 0 放大到 1 → **持续自转** → 之后**呼吸**（缩放/透明度来回）。
**只放这一件，不碰扭曲。**

### 3.2 一手依据（照抄什么；证据 `07-boss-aura-mesh.md` §9.14／§9.26）

| 项 | 一手 |
| --- | --- |
| 贴图 | ★ **`effect/eff_magicsquare.png`（256×256）** = 红/粉六芒星 + 内部灰白模糊（我看过图）|
| 原生脚本 | ★ **TH18 `effect.anm` script105**（原生 sprite33 = 同一张图）；伴随 script106 = 「炸开一圈」收尾 |
| 原生数值 | 见下方「§3.2a 原生 script105 全文」| 
| 汇编可行性 | `rotationMode`(437)/`angleVel`(415)/`zWriteDisable`(305)/`nop`(0) **都在** `local/vendor/eclmap/anmmap/v8.anmm` ✓ ⇒ thanm 能汇编 |

**草案已在**：[`461_pofv109_magicsquare.anm.txt`](assets/ability/scripts/461_pofv109_magicsquare.anm.txt)（逐行复刻；★ 2026-10-15 已**从 `_draft_109/` 转正到 `scripts/`** ✓）。

#### §3.2a 原生 script105 全文（照抄源；`th18` `effect.anm`）

```c
sprite(sprite33);                  // eff_magicsquare.png
layer(6); type(8); blendMode(1); zWriteDisable(1);
rotate(-0.7853982f, -0.7853982f, 3.1415927f);
rotationMode(3);
rotateTime(60, 4, 0.34906584f, 0.5235988f, -3.1415927f);
alpha(128);
scale(0.0f, 0.0f); scaleTime(60, 4, 1.0f, 1.0f);
+60:  nop(); angleVel(0.0f, 0.0f, -0.01308997f); stop();
offset208:  scaleTime(60, 9, 0.8f, 0.8f);  alphaTime(60, 9, 128);
+60:  scaleTime(60, 9, 1.0f, 1.0f);  alphaTime(60, 9, 96);
+60:  jmp(offset208, 60); stop();
```

### 3.3 步骤

1. **图**：直接走原生图 —— `ORDER.txt` 追加 `MAGICSQUARE  local/th18.v1.00a/anm/effect/effect/eff_magicsquare.png  256x256`
   （`local/` 开头的源图 = 原生贴图、构建时取 ✓）。若想入库就派生成 `assets/ability/boss_aura/magicsquare.png` 再引用。
2. **脚本**：草案文件挪进 `assets/ability/scripts/`，**改名为连续号**（当前最大 460 ⇒ 用 `461_…`）。
3. **编**：`make -C native anm`（会新增 `CE_ANM_ABILITY_SCRIPT_*` 宏）。
4. **C 侧起 VM**：在 `native/cards/pofv.c` 里照**现成模板**写：
   * 场上效果用 **世界列表** `ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_…, layer)`（模板：同文件 `s_storm_vm` / `s_spark_vm`）；
   * 每帧刷位置：`vm_set_world_pos(id, x, y)`（同文件已有），需要改大小/透明度用 `ce_anm_set_scale` / `ce_anm_set_color`；
   * 收尾别硬删：发 `ce_anm_interrupt(id, 1)` 让脚本自己淡出（`pofv.c:2060` 附近记着这条教训）。
5. **验**：`make -C native anm-verify` → 实跑（日志路径见 `local/GAME-LOG.md`）→ 截图对观感。

### 3.4 验收 / 坑

* **验收**：实跑里能看到「放大 → 自转 → 呼吸」三阶段；符卡结束/删卡时干净消失（无残留 VM）。
* ★ **坑 1（要 owner 定）**：**层号**。原生是 `layer(6)`（背景层）；本仓 `type(8)` **已实测**的先例是
  `layer(20) + resolutionMode(1)`（`assets/ability/scripts/68_reverse_flash.anm.txt`；机制与相机偏移见
  [`../../../engine/anm/th18/01-vm-instantiate.md`](../../../engine/anm/th18/01-vm-instantiate.md) §3b）。
  草案先按**原生** `layer(6)`；要它压在场上显眼就改 `layer(20)` + 补 `resolutionMode(1)`。
* **坑 2**：`type(8)` 是**三维旋转**模式 ⇒ 与其他层的遮挡关系要实跑确认（§3b 有"显示不对"的实跑记录）。
* **坑 3**：VM 的 `scale.x` 乘的是 **sprite 矩形宽**（`th18:0x481703`）⇒ 换图/换矩形会让脚本里的 `scale` 变味。

---

## 4. WP-B：boss 附近的「红色叠加」（红雾）（**照抄件**）

### 4.1 目标

boss 身上挂一圈**边缘不硬的红色翻动光雾**（8 帧轮播，加算）。

### 4.2 一手依据（`07-boss-aura-mesh.md` §9.26）

| 项 | 一手 |
| --- | --- |
| 贴图 | ★ **`enemy/enemy_aura.png`** 的 **sprite152–159**（8 帧 **48×48**：`x=0,48,…,336`，`y=0`）；糊边烟雾团 |
| 原生脚本 | ★ **TH18 `enemy.anm` script107**（该族 **36 支**：scripts 106…158，全 `layer(6); type(1);`）|
| 原生数值 | `color(255,64,64)`（红）+ **`blendMode(1)`（加算）** + `scale(1.0f,1.0f)`；随后每 **+5 帧**换 `sprite152…159`，最后 `jmp(offset72,0)` |
| 同色另两档 | **script125** = `scale 1.2/1.32`、**script143** = `1.5/1.65`（想更大就照抄这两个数）|
| 减算红 | `color(255,0,0)` + `blendMode(2)`：scripts 121／139／157 |

**草案已在**：[`462_pofv109_boss_aura_red.anm.txt`](assets/ability/scripts/462_pofv109_boss_aura_red.anm.txt)（★ 2026-10-15 已**从 `_draft_109/` 转正到 `scripts/`** ✓）。

### 4.3 步骤（与 §3.3 同，只有图那步不同）

1. **切 8 帧**（原生图在 `local/`，入库的是派生件；同 `lolk/RING.png` 的规矩）：
```bash
python3 - <<'PY'
from PIL import Image
src = Image.open('local/th18.v1.00a/anm/enemy/enemy/enemy_aura.png')
for i in range(8):
    src.crop((i*48, 0, i*48+48, 48)).save(f'assets/ability/boss_aura_red/RED_AURA_{i}.png')
PY
```
2. `ORDER.txt` **追加一个显式图集段**（确切 9 行见草案 README，段头 `ATLAS ATLAS_BOSS_AURA_RED 384x48`）。
3. 脚本挪进 `scripts/`（连续号）、`make -C native anm`。
4. C 侧：起在 boss 位置（**每帧刷** `vm_set_world_pos` 才"跟着 boss"）；原生脚本自己会 8 帧轮播。

### 4.4 验收 / 坑

* **验收**：boss 身上出现**边缘不硬的红色翻动光雾**；跟着 boss 移动；消失时无残留。
* **坑**：一个图集 = 一个 entry + **连续 sprite 号**，成员**矩形 = 成员原图自己的尺寸**（不许补边/缩放，否则 `scale` 变味）；
  红雾只有 1 支 VM，不会踩"一帧建太多 VM 会失败"那条（`sdk.c:1058` 的注释）。

---

## 5. ★★ WP-C：真·局域背景扭曲（**单独分包**）

### 5.1 目标（可验收形式，照 owner 口径 §0）

**非符期间、boss 在场**时：以 boss 为圆心、半径 R 内，**背景**发生**连续**的 `(x,y)→(x',y')` 变形；
**R 外与原画面逐像素一致**；**敌弹/自机/道具不受影响**；**符卡期间不出现**；**暂停/重放/无 boss 时表现与基线一致**。

### 5.2 一手：原版在哪、怎么做的（**证据全集在 `07-boss-aura-mesh.md`**）

| # | 事实 | 地址 / 依据 |
| --- | --- | --- |
| 1 | **触发 = 非符**：读符卡管理器 `[0x4cf2c0]`，测 `BYTE [eax+0x78] & 1`（bit0 = 符卡进行中）| `th18:0x41e7b4` / `0x41e7bd` / `0x41e7c1 jne 0x41ef0c`；宿主 `0x41e760` |
| 2 | **半径 112.0f** 写进舞台对象的 `+0x3414`；与平方距离比较（`0x41da82 comiss`）| `th18:0x41da0d`（`mov ...,0x42e00000`）；`0x41eae5` 再比一次 |
| 3 | **中心** = `[stage+0x360/364/368]`（3 分量位置）| `th18:0x41d383`–`0x41d393`（连续 `subss`）|
| 4 | **被改的 mesh**：句柄在 `[stage+0x3410]`，由 `0x42b1f0` 建（**17 段**、每段 `0x1dc` 字节、贴图经 `0x455d80` 解析）| `th18:0x41dac2`（建）/ `0x41dace`（存）/ `0x42b1f0`（ctor）|
| 5 | **逐顶点重算**：取 `[mesh+0x18]`/`[mesh+0x14]` 顶点区，调 `0x4a9ea0` 后 `addss` 把结果写回 `[esi]`/`[esi+4]` | `th18:0x41e790` / `0x41e843` / `0x41e87f` / `0x41e8c9` / `0x41e91c` / `0x41e92a` |
| 6 | 顶点格式 = 20 字节 `{x,y,z,rhw,D3DCOLOR}`（写 `[esi+0x13]=0x80` 命中 alpha 字节）| `th18:0x41e894` |
| 7 | mesh 的**贴图**经精灵号解析，且**含 `sprite(-1)`(0x7fff) 特例**（走 `[AnmManager+0x6f0]/[+0x6f8]` 那两条特殊表）| `th18:0x42b28c`–`0x42b2df` |
| 8 | ★ **TH16 有同一套结构**（同一半径常量、同一"标志位 `+0x78` bit0"分支）| `th16:0x40b9ff`（112.0f，宿主 `StageInner::run_std` `0x40b3b0`）、`th16:0x40c4fd`（宿主 `StageInner::step_fog` `0x40c4a0`）|

### 5.3 已排除的路（**别重做**；每条附搜索范围）

| 排除 | 搜索范围（一手）| 文档 |
| --- | --- | --- |
| **shader** ✗ | TH18/TH16 里 `CreatePixelShader`/`SetPixelShader`/`CreateVertexShader`/`SetVertexShaderConstantF` **各 0 处**；d3dx9 只导入矩阵/向量数学 | §7.22 |
| **采样离屏背景面** ✗ | 全 `.text` 里 `ds:0x4ccfa0` **只 9 处**：渲染阶段 `0x455929/0x455bed`、清理 `0x45619a/0x4561aa`、**暂停** `0x458642`、设备重置 `0x4714f5/0x471505/0x471d13/0x471d23` | §9.11 |
| **整层 anm 偏移** ✗ | `AnmManager+0xd0…` 是**全局**偏移 ⇒ 做不出"局域" | §9.6 |
| **魔法阵贴图本身** ✗ | owner 否决（贴图不能扭背景）| §9.16 |
| **boss 自己的 anm 播屏幕面** ✗ | `anmSelect` 只选同文件槽号（`0x430dfd`）；`enemy.anm`/`e0x*.anm` 里 `sprite(-1)` **0 次** | §9.10 |
| **ECL 直接放** ✗ | ECL 里能建 `ScreenEffect` 的只有 **517** `setScreenShake`（组 62 → `0x434d8d`）| §9.10／§9.18 |
| **背景 3D 几何在顶点缓冲里** ✗ | 全 exe `CreateVertexBuffer` **只 3 处**：`0x4424b6`/`0x4487c9`/`0x4852f0` | §9.17 |
| **`vm+0x5c8`（VM 自建几何）那窝是它** ✗ | 那些生成器指纹是**粒子**（多处 `Rng__randf_*`、`fsincos` 0 处）| §9.23 |

### 5.4 可用的现成设施 / 缺什么

| 设施 | 状态 |
| --- | --- |
| 离屏背景面 `0x4ccfa0` | ★ **在**（暂停路径示范了怎么把它当图元画：`0x458480` → `0x487580`，§9.4／§9.11）|
| 渲染阶段（composite 会 push 那个面）| ★ `0x455610`（`0x3a`）/ `0x455bc0`（`0x3b`）|
| 卡侧 anm / 位置 / 站点机制 | ★ `ce_anm_*`、`vm_set_world_pos`、`native/sites.py`（站点要记账，见 §2.2）|
| ★ **D3D 设备指针 / `DrawPrimitiveUP` 设施** | ✗ **没有** —— 全 `native/` 里 `DrawPrimitiveUP` **只出现在注释**里（pofv.c / trance.c 引 TH13 原版）| 
| ★ mesh 的那份顶点数据到底怎么上屏 | 🟡 **未核实**：舞台区（`0x41d000–0x41f000`）内 `+0x144/+0x148/+0x14c` **调用数 = 0** ⇒ 它多半是通过 anm/VM 的绘制路画出去的，**接手的人第一步就该把这条坐实** |

### 5.5 两条实现路线（选一条，别脚踩两条）

**路线① "照原版"**：在背景画完、精灵画之前的那一档，**CPU 建 N×N 网格**（N 取 32 上下），
逐顶点套 `(x,y)→(x',y')`（以 boss 为圆心、半径 R 截断、R 外恒等），把**离屏背景面**当纹理 `DrawPrimitiveUP` 画上去。

* 里程碑：**M1 设施**（能画一张静止的全屏纹理四边形，且**不动画面**——像素级与基线一致）→
  **M2** 圆内整体位移/缩放 → **M3** 连续映射（线条变弯）→ **M4** 与符卡/暂停/**重放**共存 + 性能过线。
* 风险：坐标空间换算（§2.4）、渲染时机先后、**每帧 N×N CPU 重算的性能**、重放一致性。

**路线② "最省"**：先只做「把背景面按某缩放重画一份、裁在半径 R 的圆里」（等价于观感近似），
先把**设施**和**时机**打通，再升级到真映射。★ 适合把 M1 的风险先隔离掉。

### 5.6 验收 / 禁区 / 风险

* **验收**：§5.1 五条 → 写成**可复现步骤 + 截图/对比帧**（有/无扭曲、符卡中、暂停、重放各一份）。
* **禁区**：① 不改共享件而不登记（`AUDIT.md` §BE）；② 不动别人的卡；③ **不新增站点而不记账**（`GATES.md` §8 + owner 拍板）；
  ④ 不把"观感近似"包装成"复刻"（`METHOD.md` 铁律）。
* **风险清单**：像素对齐（表面 px vs 世界坐标）、暂停层的遮挡顺序（暂停罩是组 38 = `0x3e`）、
  多 boss（owner 说 TH16 双 boss 没出现，**别在没弄清前就假设双 boss 一定都要有**）、性能与掉帧。

### 5.7 ❓ 要 owner 拍的两条

1. **半径/强度**：照原生 `112`（像素？世界单位？）还是要自己试一档？
2. **"只作用于背景"**：原版是只有背景；如果我们的设施做不到按层分离，是宁可"敌弹也一起扭"还是宁可不做？

---

## 6. 分派建议

| 顺序 | 谁 | 做什么 | 完成后 |
| --- | --- | --- | --- |
| 1 | 甲（便宜、可立刻开工）| **WP-A + WP-B**（同一套 anm 管线，一个人手最顺）| 实跑截图 + `make anm-verify` 绿 |
| 2 | 乙（**独占**，人不要换）| ★ **WP-C**：先按 §5.4 最后一行把"mesh 怎么上屏"坐实，再按 §5.5 选路线 | 交出 M1 证据（能画且不动画面）+ 后续计划 |
| 3 | 我/owner | WP-A 的层号、WP-C 的两条口径（§5.7）| — |

★ **A/B 与 C 不要写成同一个包**：A/B 是 `assets/**` + `pofv.c` 一处起 VM；C 是**新设施 + 渲染时机 + 可能要站点**，
混在一起会让"站点预算"和"渲染时机"两个风险互相掩盖。

---

## 7. 参考（按可信度排序）

1. [`../../../engine/enemy/th18/07-boss-aura-mesh.md`](../../../engine/enemy/th18/07-boss-aura-mesh.md) —— **一手全过程**（§7.11–§7.22、§9.1–§9.27；含每次订正与否定证据）
2. [`../../assets/ability/_draft_109/README.md`](assets/ability/_draft_109/README.md) —— 两件观感件的草案与追加行
3. [`../../assets/ability/README.md`](assets/ability/README.md) —— `ability.anm` 追加规则（ORDER v2 语法 / 只许 append / 上限）
4. [`HANDOFF-109.md`](HANDOFF-109.md) / [`WORK-PACKAGES.md`](WORK-PACKAGES.md) / [`GATES.md`](GATES.md) / [`AUDIT.md`](AUDIT.md) —— 整卡、分包、门禁、登记
5. [`../../../engine/anm/th18/01-vm-instantiate.md`](../../../engine/anm/th18/01-vm-instantiate.md) §3b —— `type(8)` 三维模式的实跑注意
