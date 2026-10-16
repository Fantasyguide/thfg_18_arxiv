# _draft_109 — 卡 109 两件"观感件"的**草案**（2026-10-15）

> **状态：草案，未编译、未改卡**（owner 口径：先写 anm 脚本草案，不编译不改卡）。
> ★ **派工看这份**：[`../../HANDOFF-109-BOSS-BG-WARP.md`](../../../HANDOFF-109-BOSS-BG-WARP.md)（P13 = 本文两件；**P14 = 真背景扭曲，单独分包**）。
> 目录位置有意放在 `assets/ability/_draft_109/`：`build_ability.py` 只 glob `assets/ability/scripts/*.anm.txt`
> （`assets/build_ability.py:43 SCRIPTS = HERE / "ability" / "scripts"`）⇒ **这个目录不会被构建拾取** ✓。

两件东西，**都是原生配方的 1:1 复刻**（一手出处都在 `engine/enemy/th18/07-boss-aura-mesh.md` §9.14／§9.26，th18 v1.00a）：

| 草案 | 是什么 | 原生出处 |
| --- | --- | --- |
| `461_pofv109_magicsquare.anm.txt` | **红色旋转魔法阵**（放大 → 持续自转 → 呼吸循环）| TH18 `effect.anm` **script105**（+ 伴随 script106），贴图 `effect/eff_magicsquare.png` |
| `462_pofv109_boss_aura_red.anm.txt` | **boss 附近那圈红雾/红光**（8 帧轮播，加算红）| TH18 `enemy.anm` **script107**（该族 36 支，scripts 106…158），贴图 `enemy/enemy_aura.png` sprite152–159 |
| ★ `463_…_boss_aura_darkblue.anm.txt` ＋ `464_…_boss_aura_plume.anm.txt` | ★ **boss 的深蓝色「霸气」**（随机成簇的深蓝加算云 ＋ 烟柱）| TH18 `effect.anm` **script116／115**（该族含白雾伴侣 109），贴图 `effect/eff_aura.png` sprite34/35 |

---

## 1. `ORDER.txt` 追加行（**等点头再追加**；该表只许 append）

### 1a. 魔法阵（单图，一行一个 entry）

```
MAGICSQUARE            local/th18.v1.00a/anm/effect/effect/eff_magicsquare.png        256x256
```

`local/` 开头的源图 = 原生贴图、构建时取（`ORDER.txt` 语法 ①）⇒ 这一行可以直接用。
若想按本仓 `make_lolk_ring_art.py` 的老规矩**派生**一份进 `assets/`，就先存成
`assets/ability/boss_aura/magicsquare.png`，再把源图那栏换成它（内容不变，只是入库）。

### 1b. 红雾（8 帧，建议显式图集段）

原生 8 帧在 `enemy_aura.png` 里是 **48×48 一格**，`x = 0,48,96,…,336`，`y = 0`（`engine/.../07` §9.26 的一手矩形）。
**推荐先切 8 张小图**（与本仓 `lolk/RING.png` 同规矩：原生图在 `local/` 是 gitignored，入库的是派生件）：

```bash
python3 - <<'PY'
from PIL import Image
src = Image.open('local/th18.v1.00a/anm/enemy/enemy/enemy_aura.png')
for i in range(8):
    src.crop((i*48, 0, i*48+48, 48)).save(f'assets/ability/boss_aura_red/RED_AURA_{i}.png')
PY
```

然后追加（画布 384×48；成员按段内顺序拿**连续** sprite 号，`RED_AURA_0` 就是第 0 个）：

```
ATLAS ATLAS_BOSS_AURA_RED 384x48
RED_AURA_0             ability/boss_aura_red/RED_AURA_0.png                           48x48
RED_AURA_1             ability/boss_aura_red/RED_AURA_1.png                           48x48
RED_AURA_2             ability/boss_aura_red/RED_AURA_2.png                           48x48
RED_AURA_3             ability/boss_aura_red/RED_AURA_3.png                           48x48
RED_AURA_4             ability/boss_aura_red/RED_AURA_4.png                           48x48
RED_AURA_5             ability/boss_aura_red/RED_AURA_5.png                           48x48
RED_AURA_6             ability/boss_aura_red/RED_AURA_6.png                           48x48
RED_AURA_7             ability/boss_aura_red/RED_AURA_7.png                           48x48
```

（也支持显式段 `成员名 源图 @x,y,WxH` 直接指向那一张大原生图；但成员矩形 = **成员原图自己的尺寸**这一条
会变得含糊 ⇒ 切小图更稳。）

---

## 2. 脚本号

当前最大 = **460**（`scripts/450…460`），本草案占 **461／462**。
转正时：文件挪进 `assets/ability/scripts/`（名字里的号必须连续）、`make anm` 重编、
`anm_ids.h` 会多出 `CE_ANM_ABILITY_SCRIPT_*` 宏 ⇒ ★ **DLL 与 `ability.anm` 必须同时发布**（DLL 里烘着宏值）。

## 3. C 侧怎么挂（现成 helper，`native/sdk.h`）

```c
uint32_t a = ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_POFV109_MAGICSQUARE, boss_pos);  // 世界列表版是 ce_anm_spawn(anm, script, layer)
ce_anm_set_pos(a, boss_x, boss_y, 0.0f);        // 每帧跟着 boss 就每帧刷（scripts 里 layer/resolutionMode 自己写）
ce_anm_set_color(a, 0xFFFFFFFFu);               // 需要改色/透明度时用（§9.26 的成对脚本就是靠 color/blendMode 分档的）
```

* 红雾那支同理：起在 boss 位置、每帧 `ce_anm_set_pos` 刷一下即可（原生脚本自己会 8 帧轮播 ✓）。
* ★ 想换档（更大/更小）：原生同色还有 **script125**（`scale 1.2×1.32`）与 **script143**（`1.5×1.65`）
  —— 直接改草案里的 `scale(...)` 两行即可（数值见 §9.26 的表）。

## 4. 还没定的点（需要 owner 一句话）

1. **魔法阵的层号**：原生是 `layer(6)`（背景层）；本仓 `type(8)` 已实测先例是 `layer(20) + resolutionMode(1)`
   （`scripts/68_reverse_flash.anm.txt`，机制见 `engine/anm/th18/01-vm-instantiate.md` §3b）。
   草案先按**原生**写 `layer(6)`，要浮到前景就改两行。
2. **要不要带原生的伴随 script106**（`layer(18); type(1); blendMode(1); scale(2,2); scaleTime(20,4,0,0); +20: delete()`）
   —— 那是"炸开一圈"的收尾，草案暂缺。
3. **谁起、什么时候起**（卡 109 的哪个阶段：符卡宣言？整张卡期间？）—— 那是 C 侧的事，不在本草案里。

---

## 5. ★ 追加件：boss 的**深蓝色「霸气」**（owner 2026-10-15 新看到的一件）

> owner 原话：「一手 boss 还有一个现在看清似乎是**深蓝色**的『霸气』？**大小跟魔法阵大小差不多**」

**配方（一手，`engine/enemy/th18/07-boss-aura-mesh.md` §9.28／§9.28a）**：
原生是 `effect.anm` 的**光环云族** —— 每台 VM 靠引擎随机量（`%RANDF`/`%RANDF2`/`%RANDRAD`）自定位置/大小/朝向，
**多起几台 = 一片随机云**（单支 50–130 px、成簇 ≈200–300 px ⇒ 与魔法阵 256 px 同量级 ✓）。本色档三支：

| 原生 | 内容 | 草案 |
| --- | --- | --- |
| **script116** | `sprite35; blendMode(1); layer(3); type(1); rotate(0,0,%RANDRAD); color(0,0,160);` 深蓝加算 | ★ `463_pofv109_boss_aura_darkblue.anm.txt` |
| **script115** | `sprite34; layer(6); anchor(0,2); pos(%F2,0,0); color(32,32,160);` 深蓝烟柱 | ★ `464_pofv109_boss_aura_plume.anm.txt` |
| **script109** | `sprite35; layer(6); color(255,255,255);` 同位置白雾伴侣 | 草案暂缺（想要亮点再补 465）|

**图**：`effect/eff_aura.png` 里 sprite34/35 都只有 **48×48**（`x=0` 与 `x=48`，`y=0`）。切法：

```
python3 - <<'PY'
from PIL import Image
src = Image.open('local/th18.v1.00a/anm/effect/effect/eff_aura.png')
src.crop((0, 0, 48, 48)).save('assets/ability/boss_aura/AURA_PLUME.png')   # = sprite34
src.crop((48, 0, 96, 48)).save('assets/ability/boss_aura/AURA_PUFF.png')   # = sprite35
PY
```

**ORDER.txt 追加行**（与 §1b 的红雾图集并列；都是 append）：

```
ATLAS ATLAS_BOSS_AURA 96x48
AURA_PLUME             ability/boss_aura/AURA_PLUME.png                               48x48
AURA_PUFF              ability/boss_aura/AURA_PUFF.png                                48x48
```

**C 侧**：这支要"一片"，所以**起多台**（比如 6–10 台 463 ＋ 3–5 台 464），每帧 `vm_set_world_pos` 跟着 boss；
每台的随机量由引擎给 ⇒ **不需要 C 侧算随机** ✓。别一帧起太多（`sdk.c:1058` 的注释：一帧建太多 VM 会失败）⇒ 分帧补。
