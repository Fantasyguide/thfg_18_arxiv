# anm/th18 §1 — 从已装载 ANM 起一个 VM（instantiate / set_sprite / 按 id 删）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 一手反编译（2026-09-04），为 card-expand 的「发动亮牌」特效而反；ANM 装载器与脚本 VM 本身**没碰**，仍见 [OVERVIEW](../OVERVIEW.md)。

## 0. 结论

| 函数（ExpHP 名） | 地址 | ABI | 做什么 | 可信度 |
| --- | --- | --- | --- | --- |
| `AnmLoaded__instantiate_vm_to_world_list_back` | `0x405bf0` | thiscall(`AnmLoaded*`; `int* out_id`, `int script`, `int layer`, `void** out_vm`) **ret 0x10** | 从该 anm 的脚本表克隆一个 VM、实体坐标置 (0,0,0)、`layer ∈ [0,0x18)` 时写 `vm+0x18` 并置标志、跑一帧、挂 world 列表尾、把 id 写回 `*out_id` | ✅ |
| `AnmLoaded__set_sprite` | `0x477b00` | thiscall(`AnmLoaded*`; `vm`, `sprite_idx`) **ret 8** | 按 sprite 表（`anm+0x11c`，每项 0x44）给 VM 写 UV / 尺寸；`anm+0x108 == 0`（未装载）返回 −1 | ✅ |
| `AnmManager__get_vm_with_id` | `0x488b40` | thiscall(`AnmManager*`; `id`) **ret 4** | fast 数组（`id & 0x7fff` 下标，步长 0x624，校验 `is_alive` 与 id）或 world / ui 链表；返回 VM 或 0。Tenshi/Miko/Remilia 每帧用它写 `vm+0x5f0` | ✅ |
| `AnmManager__interrupt_tree` | `0x488be0` | stdcall(`id`, `n`) **ret 8** | 自取 `ANM_MANAGER_PTR`，找到 VM 写 `+0x494 = n`（pending interrupt）并递归子树；脚本侧 `interruptLabel(n)` 接 | ✅ |
| `AnmManager__sub_488cf0` | `0x488cf0` | stdcall(`anm_id`) **ret 4** | `get_vm_with_id` 找到就标记删除（`vm+0x538 |= 0x80`）并递归子树 | ✅ |
| `AnmManager__interrupt_tree` | — | (`anm_id`, `n`) | 让脚本跳到 `interruptLabel(n)`（Tenshi 收尾用 1 = 淡出） | 🟡 只看了调用点 |

## 1. 发现

`CardTenshi__c_press` `0x40ebf0` 发动时**内联**了一份 `0x405bf0` 的逻辑：`ABILITY_MANAGER->ability_anm(**+0x0c**；`+0x10` 才是 `abcard_anm`，`+0x14` `abmenu_anm`) → `+0x134` 引用计数 +1 →
`AnmManager__allocate_new_vm` → `AnmManager__sub_407420(anm, vm, 0x1c)`（把脚本表第 0x1c 项、步长 0x60c、共 0x151 dword 拷进 VM；**无边界检查**，脚本号越界就是垃圾 VM）→
`vm+0x18 = 13`（层）→ 标志 `&= ~0x200000 | 0x101000` → `vm+0x5f0..0x5f8` = 卡记下的玩家坐标 → `AnmVm__run` → `insert_in_world_list_back` → id 存 `card+0x1c`。
独立函数版把坐标固定成 (0,0,0)。HUD / 编成 / 图鉴则用 `abcard_anm(+0x0c)` 走 `instantiate_vm_to_ui_list_front` 再 `set_sprite(entry+0x2c)`
（[`../../card/th18/11-sentinels-56-57.md`](../../card/th18/11-sentinels-56-57.md)）。

## 2. 验证

- `0x405bf0` 反汇编：`mov edi, ecx`（this）、`[ebp+8]` out_id、`[ebp+0xc]` script、`[ebp+0x10]` layer、`[ebp+0x14]` out_vm，尾 `ret 0x10`——唯一出口。
- `0x477b00`：两个出口都 `ret 8`（`+0x108 == 0` 早退与正常路径）。
- `0x488cf0`：`push [ebp+8]` 后调 `get_vm_with_id`，唯一出口 `ret 4`。
- `sub_407420` 的脚本表：`anm+0x10c` 基址、步长 `0x60c`——与 [`../th16/README.md`](../th16/README.md) 里 th16 的「`mgr+0x10c` 步长 `0x5fc` 模板数组」同构（th18 模板多 0x10 字节）。

## 2b. 卡里写 UI VM 脚本的两条硬约束（2026-09-10 实跑踩出来的）✅

1. **别在脚本里写 `pos()`**。UI 列表的 VM 位置由 C 每帧 `ce_anm_set_pos` 写；脚本里的 `pos()`
   会把它覆盖成脚本里的值。从 TH16 搬 `front.anm` 的季节槽脚本时，`pos(-377, 888)` 是 TH16 的槽坐标，
   在 TH18 的 UI 空间里等于屏幕外 → **整组 VM 消失**（数字是 ASCII 文字画的，所以还在）。
2. **别用 opcode 612（`drawRectBorder`）**。thtk 的 `v8.anmm` 标它 `TH16+`，而 TH18 的**所有原生 anm
   里零使用**（`grep drawRectBorder local/th18.v1.00a/anm/*/*.anm.txt` 无命中）。用它会出问题
   （2026-09-10 实测：改用它之后游戏启动即 `C0000005`，改回 `drawRect` 正常）→ 外框用
   `drawRect(102,10)` 打底 + 内部填充盖住 1 px 即可。
3. 层号：UI 列表 VM 的层 24–32 会被折到 37–45 组；**层 20–23 也能画**（相机 1），
   但 20–23 **不参与暂停模糊**（模糊只作用在层 3–19 的游玩区 surface）。
4. **同层叠放顺序 = 生成顺序：后生成的画在上面**（一手 `0x4882b8`）。
   `ce_anm_spawn_ui` 走 `AnmLoaded__instantiate_vm_to_ui_list_back` `0x409670` →
   `AnmManager__insert_in_ui_list_back` `0x488490`（节点 = `vm+0x574`，`node[0]=vm`、`node[1]=next`、
   `node[2]=prev`，**尾插**、`ui_list_tail` 后移）；`AnmManager__render_layer` `0x4882b8`
   先走 `mgr+0x6f0`（`world_list_head`）再走 `mgr+0x6f8`（`ui_list_head`），**两条都是表头 → 表尾**
   ⇒ 尾部的（最后生成的）最后画 = 压在上面。
   复刻原版层内叠放时按原版 `scriptNew` 次序生成即可（季节槽就靠这条：背景条先生成、进度条后生成）。

## 3. 坐标与层（与 ANM 脚本的配合）

- `vm+0x54/+0x58` = scale x/y（ExpHP；HUD 充能条 `0x408a53` 写 +0x58 当 fill 比例，同时 `+0x70` uv_scale.y、`+0x80` uv 滚动）；`vm+0x524` color_1 是 D3DCOLOR `0xAARRGGBB`。
- 实体坐标 `vm+0x5f0` 是 ECL 坐标；`originMode(1)` 下 (0,0) 是**弹幕区上边框中点**（x 居中、y 从顶部起算；2026-09-04 实跑 ✅：
  2D 配方脚本 pos(0,0,0) 出现在上边框中点）。场地正中 = **(0, 224)**（384×448 的区域）。thpages 说的「ECL (0,0)」就是这个点，不是几何中心。
- `layer` 参数写 `vm+0x18`；脚本里的 `layer(n)` 会再覆盖。原生「场地中央展示卡」的 `abcard.anm script14` 用 `layer(16)`（子弹之上）；Tenshi 要石用 13。
- **世界层 VM 的坐标与尺寸都是「640 空间」的 ECL 单位：1 单位 = 2 表面 px**（scale 2 时）。
  一手换算：表面 = `(640 + x × DAT_0056aca0, 32 + y × DAT_0056aca0)`（`DAT_0056acb4 = ac80/2 = 640`、
  `DAT_0056acb8 = scale × 16 = 32`，`FUN_00454b20` `0x454b20` 尾）；游玩区 = ECL x ∈ [−192,192]、y ∈ [0,448]。
  ⚠️ 与 `resolutionMode` 的相互作用（`AnmManager__draw_vm` `0x4817d8`/`0x481b3f`：
  `vm+0x538 & 0x1c00000` = 0 → 尺寸 ×1；= `0x400000`（`resolutionMode(1)`）→ × `DAT_0056aca0`；
  = `0x800000`（`resolutionMode(2)`）→ × `DAT_0056aca0 × 0.5`）：
  **世界层里 `resolutionMode(2)` 的尺寸就是世界单位**，所以从 UI 层（1 单位 = 1 表面 px）搬过来的脚本
  要**把尺寸 ÷2** 才保持屏幕大小（2026-09-10 季节槽/绀珠传面板搬迁时踩过：漏了这步 → 字与条整体放大一倍）。
- 一次性特效让脚本自己 `delete()`，卡对象不必记 id；需要外部收尾才走 `interrupt_tree` + `0x488cf0`（Tenshi 模式）。

### 3a. ★ 两个坐标字段是**相加**的（2026-09-13 一手；「VM 画在屏幕外」类 bug 的定式）✅

- **发现**：`AnmManager__draw_vm` 的绘制位置 = **`pos + entity_pos + [vm+0x4e4]`**（`0x48171f` 起：
  `MOVSS XMM0,[EDI+0x30]` → `ADDSS XMM0,[EDI+0x5f0]` → `ADDSS XMM0,[EDI+0x4e4]`，x/y/z 三组同形）。
- **验证**：`AnmLoaded__instantiate_orphan_vm_to_world_list_back`（`0x488a80`）把**模板 VM 的 `+0x30`**（= `pos`）
  拷进新 VM 的 `+0x4e4`（`0x488adb`）⇒ `+0x4e4` 是"另一份 pos"，不是无关字段。
- **结论**：
  - `+0x2c..0x34` = **`pos`**（脚本 `pos()` / `posTime()` 写的就是它）；
  - `+0x5f0..0x5f8` = **`entity_pos`**（两个 instantiate 入口的第 4 个参数写的都是它：
    世界版 `0x405bf0` 一族、UI 版 `0x409670` 的 `MOVQ [ESI+0x5f0],XMM0`，见 `0x4096d7`）；
  - 两者的**和**才是画到屏幕上的点。
- **⇒ 落笔纪律**：给 VM 定位置走**其中一条路**，别两条都按"绝对坐标"写。
  典型错误：出生时把基点传给了 instantiate（写进 `entity_pos`），之后又用 `ce_anm_set_pos()`
  （`engine.h` 的 `CE_ANM_VM_POS` = `+0x5f0`，也是 `entity_pos`）写"绝对坐标"⇒ 覆盖同一个字段其实无害，
  但**脚本里还有 `pos()` 的话就会叠加**（2026-09-13 指示槽游标踩的就是这一类：脚本 `pos(48,0)`
  + 卡侧写 `entity_pos = 基点` ⇒ 想要的是"基点 + 48"，写的却是"基点"，结果差 48 px）。

### 3a2. ★★★ 硬上限：**anm 的 entry 序号只能到 255**（2026-09-13 一手 + 实跑合证）✅

- **发现**：`sprite()` 指令解析出来的**矩形/UV 是对的**（实跑日志：spirit 461 / 生效矩形 32×8 都对得上我们切图），
  但画出来的**贴图是别人的**（一个"窗口"）⇒ 错的是 sprite **所属 entry** 的纹理引用。
- **验证**：
  1. **sprite 序号 >255 没问题**：原生 `enemy.anm` = **7 entry / 344 sprite**，
     脚本里**引用到 `sprite342`**（`thanm -l`）⇒ 全局 sprite 表 469 项是正常用法。
     （★ 2026-09-13 订正：这里旧文写的「每 entry 最多 120」**是错的** —— 原生单 entry 最高 **368**
     sprite，见 [`06-entry-limit-and-multi-sprite.md`](06-entry-limit-and-multi-sprite.md) §2。）
  2. **entry 序号没有原生先例**：全表普查 TH18 的 anm，最大是 `abcard.anm` 的 **118 entry**，
     其余 ≤34 ⇒ **255 就是上限**（多出来的 entry 的贴图引用回绕 ⇒ 画成别的贴图）。
  3. **机制**（2026-09-13 一手升级）：贴图键 = `(anm 槽号 << 8) | entry 下标`（解析器 `0x486d38`），
     绑贴图时只取**低 8 位**（`0x47e1a4`）⇒ 每 anm **256 entry（下标 0..255）**，≥256 回绕。
     细节与 5 个 `SetTexture` 证据点见 [`06-entry-limit-and-multi-sprite.md`](06-entry-limit-and-multi-sprite.md) §1。
  4. 合证：我们 rebuild 出来的 `ability.anm` 有 **367 entry**，实跑里"排在 256 之后的"那批
     （指示槽框 359/360、满格提示 356、吃灵弹窗 361..366）**全部**画成别的图，而排在 <256 的
     （神灵 211、槽条 217..220）没被投诉过 —— 与"回绕"逐条吻合 🟡→✅（实跑复核见 `AUDIT.md` §CC）。
- **⇒ 落笔纪律**：往一个 anm 里加美术时**留意 entry 总数**（≤255）；
  超了就得把**多张图并进一个 entry**（引擎侧本来就支持：`AnmLoaded__set_sprite` `th18:0x477b00`
  用 `sprite 数组 + idx*0x44` 索引，一个 entry 多个 sprite 完全合法）。
  ⚠ 我们本仓的 `build_ability.py` 当时限死了"一个 entry 一个 sprite"，撞墙的根因就在这里（修法见 `AUDIT.md` §CC-3）。

### 3b. ★ 世界层 vs UI 层的**尺寸**空间（TH13 贴图搬过来要不要 ×2）

同一条 `draw_vm` 把两条链画到同一个表面（1280×960），但**两边的"1 单位"不一样**：

| 链 | 坐标单位 | 尺寸单位 | TH13 贴图搬过来 |
| --- | --- | --- | --- |
| 世界层（层 3–19，游戏区视口 408×472，`02` §1）| ECL（640 空间）1 单位 = 2 表面 px | **同坐标空间 = 1 art px 就是 1 ECL 单位** | **不用缩放**（1:1）✅ |
| UI 层（层 24+，整屏表面）| 表面 px（1 单位 = 1 px）| 表面 px | **×2**（TH13 的 UI 贴图按 640 宽画）|

- **一手证据（尺寸单位）**：TH18 自己的 UI 贴图**整体是 TH13 的 2 倍** —— `front/lifebar.png` 的
  sprite 是 `8×32 / 4×32 / 6×12`，TH13 同族是 `4×16 / 2×16 / 3×6`（逐项 2 倍，`thanm -l`）。
  而**世界层贴图不是 2 倍**：两作的 `bullet.anm` sprite 尺寸分布**逐项相同**（`16×16` 120 个 vs 111 个、
  `16×14` 96 vs 96、`8×8` 64 vs 64 …）⇒ 世界层贴图两作通用、1:1。
- **一手证据（坐标单位）**：UI 的位置换算见 `mods/.../cards/mof.c` / `signal.c` 的 `U640_TO_SURF = 2.0f`；
  世界层的位置换算 = `表面 = (640 + x×2, 32 + y×2)`，被用户 1280 截图实测钉死
  （**游玩区左边缘 257 / 底边 928** ⇐ `640 − 192×2 = 256`、`32 + 448×2 = 928`）。
- **⇒ 定式**：给世界层 VM 用 TH13 的世界层贴图**别缩放**；用 TH13 的 **UI** 贴图（HUD 槽、agauge 等）
  搬进 UI 层时**必须 ×2**（卡侧先例：`mof.c` 把 640 空间的量整体乘 2）。
  ⚠ 反例（2026-09-13 修）：把一手**场地坐标**字面量直接当表面 px 用，会把 HUD 画到屏幕外
  （113 指示槽：`(-196,448)` 直接 ×2 得 `(-392,896)`，x 为负 ⇒ 4 支 VM 全在屏外）；
  正确 = `(640 + x×2, 32 + y×2)` = `(248, 928)`。

## 3b. 渲染模式 8（`type(8)`，三维旋转）与相机偏移 —— 2026-09-04 实跑「显示不对」的根因

`AnmManager__draw_vm` `0x481210` 按 `(vm+0x534 >> 26) & 0x1f` 分派渲染模式：0/2 → `FUN_0047e8f0`、1/3 → `FUN_0047ed50`（2D：自己算四个顶点，
经 `FUN_0047dce0` 写进精灵批缓冲）、**8 → `FUN_00480160`**（三维：`D3DXMatrixRotationX/Y/Z` 按 `rotationSystem`（`vm+0x538>>4&7`）拼进
`vm+0x414` 的 WORLD 矩阵，`SetTransform(WORLD)` 后画 `AnmManager+0x3120e18` 的单位四边形，FVF `XYZ|TEX1`，颜色走 `TEXTUREFACTOR`）；
15 = 8 外加 `D3DRS_FOGENABLE`（`FUN_00454760/4547a0`）。两类路径都是 XYZ 顶点，共用当前相机的 VIEW/PROJECTION。

**差别**：2D 路径写顶点时加 `AnmManager+0xd0/+0xd8`（x）与 `+0xd4/+0xdc`（y），模式 8 **不加**。这两组由
`Supervisor__set_camera_by_index` `0x41b330`（`+0xd8/+0xdc = camera+0x104/+0x108`）与 `FUN_004548e0`（`+0xd0/+0xd4 = camera+0xfc/+0x100`，
同时 `SetTransform(VIEW, camera+0x60)`、`SetTransform(PROJECTION, camera+0xa0)`）从当前相机复制。
四台相机（`zSupervisor+0x25c` 起，各 0x164；`FUN_00454b20` 初始化）只有 **相机 2** 的 `+0x104/+0x108` 非零：
`Supervisor__sub_454f50` `0x454f50` 置为 `(DAT_0056ac98 − DAT_0056ac80) × 0.5`（`0x4b9138` = 0.5）= 游戏区域在表面里的居中原点。
相机 2 是每帧开头 `FUN_004553b0` 与主循环 `FUN_00472fd0` 切到的默认相机——但**只管层 0–2 和 24+**；层 3–19 在优先级 0xa / 0xf / 0x1a 处切到的**相机 3**下画，
相机 3 的 2D 偏移为 0（[02-render-stages](02-render-stages.md) §1 一手订正）。
**订正**：原推断「模式 8 放层 16 会少加区域原点而偏位」**不成立**；层 12–19 下模式 8 与 2D 路径没有偏移差。第一轮空白的真因是 anm 槽拿错（O24′）。
原生把 `type(8)` 放层 20（`effect.anm` 7 处）/ 2 / 6 仍是事实，照抄即可；层 12–19 放模式 8 理论可行、未实测。
**透视**：exe 只导入 `D3DXMatrixPerspectiveFovLH`（无 Ortho），所有相机都是透视投影 ✅——绕 Y 转有近大远小。

## 4. 用法（card-expand）

`mods/th18.v1.00a/card-expand/native/sdk.h` 的 `ce_anm_spawn(anm, script, layer)` 只包 `0x405bf0`；
反转牌发动时起 `ability.anm` 追加的 `script68`（卡图副本 `sprite109`，`type(8)` 绕 Y 轴一圈），AUDIT O24。
脚本照原生层 20 配方：`layer(20); resolutionMode(1); type(8);`，不写 `originMode`（层 20 的原点由 `layer()` 设默认，见 §3b）；`pos(0, 224, 0)` 居中。
2026-09-04 实跑：2D 配方（`type(1)` / 层 16 / 绕 Z）在修正 `+0x0c` 后显示正常 ✅ → `ce_anm_spawn` + ability.anm 追加链路通；3D 配方待复跑。

## 5. 未答

- ~~世界层投影是否透视~~ → 全透视 ✅（§3b）。
- ~~层号 → 渲染阶段 → 相机的精确表~~ → [02-render-stages.md](02-render-stages.md) ✅（两个大桩的层号 41/42 🟡）。
- 模式 8 的 9 块预制四边形已读（[02 §4](02-render-stages.md)）：几何居中；实跑顶边对齐的原因仍未定位，用 `pos` 上移半张补偿 🟡。
- `layer(n)` 指令对 originMode / resolutionMode 的默认设置：未反，沿用原生层 20 脚本的写法。
- `AnmManager__interrupt_tree` 的 ABI 没看（未用）。
