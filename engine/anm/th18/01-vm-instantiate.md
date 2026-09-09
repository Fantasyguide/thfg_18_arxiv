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
2. **别用 opcode 612（`drawRectBorder`）**。thtk 的 `v8.anmm` 标它 `TH16+`，而 TH18 的**所有零售 anm
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
- `layer` 参数写 `vm+0x18`；脚本里的 `layer(n)` 会再覆盖。零售「场地中央展示卡」的 `abcard.anm script14` 用 `layer(16)`（子弹之上）；Tenshi 要石用 13。
- **世界层 VM 的坐标与尺寸都是「640 空间」的 ECL 单位：1 单位 = 2 表面 px**（scale 2 时）。
  一手换算：表面 = `(640 + x × DAT_0056aca0, 32 + y × DAT_0056aca0)`（`DAT_0056acb4 = ac80/2 = 640`、
  `DAT_0056acb8 = scale × 16 = 32`，`FUN_00454b20` `0x454b20` 尾）；游玩区 = ECL x ∈ [−192,192]、y ∈ [0,448]。
  ⚠️ 与 `resolutionMode` 的相互作用（`AnmManager__draw_vm` `0x4817d8`/`0x481b3f`：
  `vm+0x538 & 0x1c00000` = 0 → 尺寸 ×1；= `0x400000`（`resolutionMode(1)`）→ × `DAT_0056aca0`；
  = `0x800000`（`resolutionMode(2)`）→ × `DAT_0056aca0 × 0.5`）：
  **世界层里 `resolutionMode(2)` 的尺寸就是世界单位**，所以从 UI 层（1 单位 = 1 表面 px）搬过来的脚本
  要**把尺寸 ÷2** 才保持屏幕大小（2026-09-10 季节槽/绀珠传面板搬迁时踩过：漏了这步 → 字与条整体放大一倍）。
- 一次性特效让脚本自己 `delete()`，卡对象不必记 id；需要外部收尾才走 `interrupt_tree` + `0x488cf0`（Tenshi 模式）。

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
零售把 `type(8)` 放层 20（`effect.anm` 7 处）/ 2 / 6 仍是事实，照抄即可；层 12–19 放模式 8 理论可行、未实测。
**透视**：exe 只导入 `D3DXMatrixPerspectiveFovLH`（无 Ortho），所有相机都是透视投影 ✅——绕 Y 转有近大远小。

## 4. 用法（card-expand）

`mods/th18.v1.00a/card-expand/native/sdk.h` 的 `ce_anm_spawn(anm, script, layer)` 只包 `0x405bf0`；
反转牌发动时起 `ability.anm` 追加的 `script68`（卡图副本 `sprite109`，`type(8)` 绕 Y 轴一圈），AUDIT O24。
脚本照零售层 20 配方：`layer(20); resolutionMode(1); type(8);`，不写 `originMode`（层 20 的原点由 `layer()` 设默认，见 §3b）；`pos(0, 224, 0)` 居中。
2026-09-04 实跑：2D 配方（`type(1)` / 层 16 / 绕 Z）在修正 `+0x0c` 后显示正常 ✅ → `ce_anm_spawn` + ability.anm 追加链路通；3D 配方待复跑。

## 5. 未答

- ~~世界层投影是否透视~~ → 全透视 ✅（§3b）。
- ~~层号 → 渲染阶段 → 相机的精确表~~ → [02-render-stages.md](02-render-stages.md) ✅（两个大桩的层号 41/42 🟡）。
- 模式 8 的 9 块预制四边形已读（[02 §4](02-render-stages.md)）：几何居中；实跑顶边对齐的原因仍未定位，用 `pos` 上移半张补偿 🟡。
- `layer(n)` 指令对 originMode / resolutionMode 的默认设置：未反，沿用零售层 20 脚本的写法。
- `AnmManager__interrupt_tree` 的 ABI 没看（未用）。
