# th07 森罗结界「持续期间」画的是什么 —— 一手反汇编（id 731 的脚本 / 绘制侧入口 / 全屏变暗）

> **版本**：TH07 v1.00b（`th07.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th08:0x…`。
> **方法**：一手平坦反汇编 `local/_scratch/th07.asm`（`objdump -d -M intel`）+ exe `.data` / 常量直读
> + 自写 anm v2 解析器**逐字节**读 `etama.anm` entry2/entry3 的脚本（不信 `thanm` 的转写）
> + `thanm -l 7` / `thanm -x 7` 双向核对 + 贴图**像素统计**（本模型不能看图，改用径向 alpha 分布描述）
> + ExpHP `local/vendor/th-re-data/data/th07.v1.00b/`（funcs / statics / type-structs-own）对名。日期 2026-09-12。
> **可信度**：§2–§5 的地址 / 指令 / 字节 / 常量全 ✅ 一手（含「opcode 1 → EffectManager 收槽」三处独立证据）；
> §6 的贴图外观 ✅（像素统计，非目视）；§4.3 的「罩住哪些层」🟡；§8 的 TH08 对照 🟡（只对 idiom，未读 TH08 解释器）。
> 全文**没有**对 `AUDIT.md` / `TUNING.md` / 其它 `engine/**` 文档做任何改动（点名见 §7）。

## 0. 一句话结论

原版「森罗结界持续期间」**有**视觉，但**不是那个环**：

| 视觉 | 是什么 | 活多久 | 一手 |
| --- | --- | --- | --- |
| 全屏 sprite **变暗到 37.5 %** | 结界每帧把 `ARGB(0x80, 0x30,0x30,0x30)` 灌进 Stage → 再拷进 `AnmManager+0x0` 全局色乘子 | **整整 540 帧**（= 结界全长） | `0x44162f` / `0x44233b` / `0x406b71` |
| 自机贴图**红白每 2 帧闪** | `player+0x1b8` = `0xffff0000` / `0xffffffff`（`timer.value % 4 < 2` 取红） | **整整 540 帧** | `0x441574` / `0x442280` |
| 一格 256 px **品红魔法阵**（等价 type `0x1c` → 全局 id **731**） | `etama.anm` entry3 的 `etama4.png`，跟随自机 | **只 ~33 帧**（0.55 s）就被 `EffectManager` 回收 | §2 / §3 |
| 结界槽（樱点条）的量 | `ds:0x62f890` = 50000×剩余/总 + 樱点 | 540 帧 | `0x4414e5` |

⇒ **前一个代理「id 731 是一次性 33 帧闪光」这个结论是对的**（只是它记的机制不对：不是 `delete()` 指令，见 §3）；
但它据此推出的「**原版持续期没有任何视觉**」**是错的** —— 持续期有全屏变暗 + 自机红白闪两件，只是它们**不是环**。

★ 另有一条**数据自相矛盾**要留档：发动时 C 侧给这个环设的**缩放插值时间基 = 540 帧**（`1.0 → 0.25`，`0x441b56`），
而脚本自己 `wait(32)` 后就把 VM 停掉（§3）⇒ 零售数据像是「本来打算让环挂满全程」。无法用别的 build 对照（本机只有 v1.00b 样本）。

## 1. 证据前提

| 项 | 值 |
| --- | --- |
| exe | `local/th07.v1.00b/th07.exe`，4 段（`.text` `0x401000` / `.rdata` `0x48d000` / `.data` `0x49c000` / `.rsrc` `0x1366000`），imagebase `0x400000` |
| 平坦反汇编 | `local/_scratch/th07.asm`（9,379,863 B；同 `th07_full.asm`） |
| anm 侧 | `local/vendor/thtk/build/thanm/thanm -l 7 -o local/th07.v1.00b/dat/etama.anm`（贴图 `-x 7`） |
| 符号 | ExpHP `funcs.json`（701 条）/ `statics.json`（`STAGE` `0x1347b00`、`ANM_MANAGER_PTR` `0x4b9e44`、`EFFECT_MANAGER` `0x12fe250`）|
| 前置文档 | [`../../anm/th07/01-anm-id-space.md`](../../anm/th07/01-anm-id-space.md)（type→id→脚本全链路，本文大量复用）|

## 2. id 731 的脚本：逐字节原文 ✅

自写解析器直读 `etama.anm` entry3（文件内 `0x62e64`，v2，1 sprite / 1 script；script0 在 entry+`0x80`）。
指令头 = `{s16 opcode; u16 size; s16 time; u16 mask}`，参数从 `+8` 起（与解释器一致：`0x450dc1` 读 opcode、`0x4538cb` 读 size、`0x450db1` 读 time、`0x450e37` 读 mask）：

```
off      op  size time mask  args                                
+0x0080   3   12    0   0    sprite 0（文件内号；= etama4.png 整张 256×256）
+0x008c  13   20    0   0    0.0f, 0.0f, 0.0031415927f           // π/1000
+0x00a0   8   12    0   0    0
+0x00ac  34   20    0   0    30, 0, 160
+0x00c0   7   16    0   0    1.0f, 1.0f
+0x00d0  37   16    0   0x1  $I0(10000), 32
+0x00e0  79   12    1   0x1  $I0
+0x00ec   1    8    1   0    （无参）
```

`thanm -l 7` 的转写（`script 0 script64`）与上表逐条一致 ✅ —— **最后一条是 opcode 1，不是 0xffff 哨兵**。

### 2.1 每条 opcode 的一手语义

| op | handler | 干了什么 | 一手地址 |
| --- | --- | --- | --- |
| 3 | `0x450e1f` | `vm+0x1c0 \|= 1`（bit0 = 可见）；参数=文件内 sprite 号，`+ base` 后交 `AnmManager::set_vm_sprite` | `0x450e28` / `0x450e7b` / `0x450e8d` |
| 13 | `0x451242` | 三个 float → `vm+0xc` / `+0x10` / `+0x14`（**旋转速度 xyz**），再 `flags \|= 4` | `0x45127c` / `0x4512b9` / `0x4512f6` / `0x451302` |
| 8 | `0x450f40` | `vm+0x1bb = arg & 0xff`（**alpha 字节**） | `0x450f4f` |
| 34 | `0x452060` | **透明度渐变**：`vm+0x9c` 计时 = arg0；`vm+0xc2` = arg1（模式）；`vm+0x22b` = 当前 alpha；`vm+0x22f` = arg2（目标 alpha） | `0x4520de` / `0x452103` / `0x452115` / `0x452124` |
| 7 | `0x450eac` | 两个 float → `vm+0x18` / `+0x1c`（**scaleX / scaleY**），再 `flags \|= 8` | `0x450ee6` / `0x450f23` / `0x450f2f` |
| 37 | `0x452429` | `*AnmVm::get_int_var_ptr(vm, instr+8, mask) = arg1` ⇒ **整数寄存器赋值**（`$I0 = 32`） | `0x45246e` / `0x452479` |
| 79 | `0x4518d4` | **等 N 帧**：`vm+0x3c` 计时 = arg；每帧 `0x4394c7` 减 1；没到点就 `jmp 0x4538e2`（**不前进指令指针**） | `0x4518e4`–`0x45192d` / `0x4519ac` / `0x439517` |
| 1 | `0x450df3` | `flags &= ~1` ⇒ 直落 opcode 2 的实体：`vm+0x1e0 = 0`（停指令流）⇒ `return 1` | `0x450dfc` / `0x450e0b` / `0x450e15` |

- `$I0` = 整数寄存器 `10000` ✅（thtk `thanm/reg.c` 的 `regs_v2_v3_v4_v7_v8` 表；`thanm` 把变量参数印成 `[10000]`）。
- opcode 13 写的是**速度**而不是角度 ✅：同三个字段在「等下一帧」的尾巴里被积分成角度（`vm+0x0/+0x4/+0x8 += vm+0xc/+0x10/+0x14`，`0x4538f5`–`0x453924`）。
- `flags` 的读法（本文多处要用）：bit0 可见、bit2 旋转、bit3 缩放、bit4 渲染态（`0x44eed4` 读，z-write）、bit12 渲染态；初始化 `[+0x1c0] = 7`（`0x401138`）。

## 3. 它结尾是 `delete()` 还是 `stop()`？—— 都不是，但**效果等于 delete** ✅

**① opcode 1 = 「藏起来 + 停指令流 + 返回 1」**（`0x450df3`：`flags &= ~1` → `vm+0x1e0 = 0` → `return 1`）。

**② 藏起来 = 真的不画** ✅：`AnmManager::draw_vm` 的闸门里有 `if (!(flags & 1)) return -1`（`0x44f9d0`–`0x44f9e0`），
另有 `!(flags & 2) → -1`（`0x44f9e8`）与 `alpha(+0x1bb) == 0 → -1`（`0x44f9ff`–`0x44fa10`）。

**③ `return 1` = 被 EffectManager 收尸** ✅：`EffectManager::on_tick_0b` `0x41c790` 扫 408 槽（`0x41c844`，stride `0x2d8`），
对活跃槽（`+0x2cc`，`0x41c854`）先跑 update（`+0x2c8`，返回 ≠ 1 ⇒ 杀，`0x41c890`），再 `call 0x450d60`（`0x41c8a3`）——
**`test eax,eax; je`：非 0 就把 `+0x2cc` 清 0（`0x41c8a8`–`0x41c8b1`）**。
而 `AnmManager::run_vm` 只有两处会返回非 0：入口「没有当前指令」（`0x450d73`）与 opcode 1/2/「未知 opcode」那几支；
正常「等下一帧」的出口是 `xor eax,eax`（`0x4540d1`）⇒ 返回 0 = 活着 ✅。

⇒ 结论：**TH07 的 effect VM 没有「停下来但继续画」这条路**（opcode 1 与 opcode 2 都返回 1 ⇒ 都被收槽；
想活着只能像 `etama.anm` script63 那样用 `ins_4` 跳回自己、永不结束）。
所以 id 731 **只画 ~33 帧**：帧 0 生成 → 帧 0–30 alpha 从 0 渐显到 160（op34）→ 帧 ~32 等满 → 帧 ~33 执行 opcode 1 → 收槽。

**④ 玩家侧的句柄变悬空**（不是 bug，只是死槽指针）：`Player::sub_441330` 每帧仍写 `[player+0xb7e6c]+0x24c`（`0x4414a2`–`0x4414bd`），
结束时再写一次 `[旧环+0x2cc] = 0`（`0x4417e1`）—— 对一个已经 `+0x2cc == 0` 的槽，写它无害 ✅。

## 4. 「持续期」那段逻辑里还有什么（`Player::sub_441330` `0x441330`）✅

### 4.1 状态机与时长

- 状态字节 `player+0x2408`：`0x441369` 判 3、`0x441484` 判 4。**state 4 = 森罗结界**、state 3 = 灵击后的无敌段（§5.3）。
- **时长 540 帧** ✅：发动时 `player+0x16a00` 计时 = `{-999, 0, 540}`（`0x4419f7`–`0x441a0e`），
  每帧 `0x4394c7` 减 1（`0x44151f`），`<= 0` 就收结界（`0x441530`）。
- effect VM 另有一套**插值通道**（「等下一帧」的尾巴里按 `vm+0x8c + i·0xc` 扫 5 槽，`0x4539c7`）：
  `0x453e1a` 写 alpha、`0x453ee9` / `0x453f0d` 写 `vm+0x18/+0x1c` 缩放、`0x453e53` 写角度 ✅；
  另有 12 字节计时器在 `+0x60` / `+0x78` / `+0x9c` / `+0xb4`（`{-999, 0, 值}`，`0x4394c7` / `0x43958d` 推进）。

### 4.2 自机贴图红/白每 2 帧闪（两处都写，合计覆盖整段）✅

| 位置 | 一手 |
| --- | --- |
| tick：`(timer.value % 4) < 2` → `player+0x1b8 = 0xffff0000`（红）否则 `0xffffffff`（白） | `0x441553`–`0x44158a` |
| draw（`Player::on_draw_06` `0x4420b0`）同一套、同一条件 | `0x44225c`–`0x442296` |
| `player+0x1b8` 就是自机本体 AnmVm 的颜色（自机是**内嵌 VM**：pos `+0x1c8/+0x1cc`、id `+0x1d8`、指令指针 `+0x1e0`） | `0x442102`–`0x442149`、`0x441127` |
| 颜色进顶点 diffuse 的路径 | `0x44f470`（读 `vm+0x1b8`）→ `0x44f485`–`0x44f509`（逐通道乘全局色）|

⇒ 结界期间自机贴图**每 2 帧在红与白之间跳**（配合 4.3 的变暗，实际是「暗红 ↔ 暗白」）。

### 4.3 全屏 sprite 变暗到 37.5 %（这才是最显眼的持续视觉）✅

- 两处都调 `Stage::sub_406930(STAGE, color)`（ecx = `0x1347b00` = `STAGE`）：tick `0x44162a`、draw `0x44233b`。
- 颜色 = `ARGB(0x80, g,g,g)`，`g` 由 `ds:0x4d44e0` 算（`0x44158d`–`0x441626`，draw 侧同码 `0x442299`–`0x442332`）：
  `x < 30` → `g = 0x80 − x·80/30`；`x ≥ 510` → `g = 0x80 − (540−x)·80/30`；**其余恒 `0x30`（= 48）**。
  ⇒ 48/128 = **0.375**，且 540/30/510 三个数与结界的 540 帧**完全对齐**（所以 `ds:0x4d44e0` 应是「结界已过帧数」🟡，写点没找到，见 §9）。
- `Stage::sub_406930` `0x406930`：`STAGE+0x52af == 0` 时整体赋值 `STAGE+0x52ac = color`，否则逐字节 `(arg+old)>>1`（平滑）。
- **真正的施力点**：`Stage::on_draw_03`（`0x4069d0`）在 `0x406b52`–`0x406b81` 把 `STAGE+0x52ac` 读出来、写进
  **`AnmManager+0x0`**（并置 `+0x4 = 1`），随后把 `STAGE+0x52ac..af` 复位成 `{0x80,0x80,0x80,0x00}`（`0x406b86`–`0x406ba4`）。
  而 sprite 顶点色在 `0x44f485`–`0x44f509` 逐通道 `(c·b)>>7` —— `b` 就是 `AnmManager+0x0` 那 4 个字节。
- 覆盖范围 ✅/🟡：凡是**此后**走 `AnmManager::draw_vm` 画的 sprite 都被乘暗（`Stage::draw_vms` `0x407900` 只被
  `Stage::on_draw_04` `0x406de0` 调，在 `0x406dc3` 等，**在 `on_draw_03` 之后** ⇒ 背景 VM 也在内）。
  3D 背景（`on_draw_03` 里更早经 `[dev+0xa0]` 画的那部分）**可能不在内** 🟡 —— 未逐帧验证绘制顺序。

### 4.4 结界槽（HUD 侧）✅ / 🟡

- 每帧 `ds:0x62f890 = ([player+0x16a08] × 50000) / [player+0x16a14]`（钳 ≥0），再 `+= [GAME_MANAGER+8]→+0x88`：
  `0x4414db`–`0x4414e5`（`imul 0xc350` = 50000）、`0x4414ea`（钳 0）、`0x441508`（加樱点）。
- 读点只有 4 处：`0x404c06`、`0x4428aa`、`0x44332e`、`0x4438ab`（🟡 这三个函数的「HUD/结算」身份未逐个坐实）。
- 结界结束时 `ds:0x62f890` 复位成樱点原值（`0x4416ee`），并把「结界期间累计的差」加进 `[GM+8]→+0x4`（`0x4416c6`）。

### 4.5 明确**没有**做的（排除项）✅

| 猜测 | 实测 | 一手 |
| --- | --- | --- |
| 换自机 anm 脚本 / sprite | ❌ 没有：结界路径里既不写 `player+0x1d8`，也不调 `AnmManager::load_script_into_vm` | `0x441330`–`0x441e75` 内无该写点 |
| 自机变 `scale` | ❌ 只在 state 1（复活/登场）与 state 3 起点动 `+0x18/+0x1c`；state 4 不动 | `0x4410e5`–`0x441224`、`0x441705` |
| 跟随自机的**独立大环 VM** 活满全程 | ❌ 那个「活到结束」的环是 **type `0x19`（id 730）**，属**灵击（炸弹）**，不是森罗结界 | §5.3 |
| 屏幕边缘 / 全屏遮罩的**独立** effect | ❌ 没有：变暗走 `Stage` → `AnmManager` 全局色，不是 effect | §4.3 |
| 持续期**周期重发** effect | ❌ state 4 每帧逻辑里**一个 spawn 都没有**（只有一个发动点） | `0x44148d`–`0x441634` |
| 弹幕 / 背景单独的着色改变 | ❌ 除 §4.3 的全局乘子外没有第二套 | 同上 |

## 5. 发动 / 续 / 崩：三条路径 ✅

### 5.1 发动（自动 = 樱点槽溢满；手动 = 炸弹键）

- **自动**：`0x42f602`–`0x42f652`：累加器 `[GM+0x9620] += arg`，`>= [GM+8]→+0x88 + 50000` 时钳住并
  `call 0x441960`（ecx = `0x4bdad8` = 自机单例）。
- **手动**：`Player::tick_bombing_state` `0x4409f0` 里 `player+0x240d == 2` 时 `call 0x441960`（`0x440a59`）。
- 另有 `0x4428ce` / `0x4438e6` 两处调用（🟡 未逐个坐实）。
- **state 4 里再收到一次发动请求 = 续结界**：`0x441960` 的状态 switch（`0x441995`）只对 1/2/3 走「不能发动」，
  对 **state 4（与 0）走 default ⇒ 重设 540 帧 + 重发环**。

### 5.2 发动时 C 侧给环设了什么（`0x441a8e`–`0x441b74`）

| 字段 | 值 | 一手 |
| --- | --- | --- |
| spawn | `push -1(色) / 1 / 4(槽) / &自机+0x930(位置) / 0x1c(type)`，`ecx = 0x12fe250` | `0x441a9d` / `0x441aa4` |
| 计时 `+0xb4` | = 玩家结界计时（**540**）⇒ 缩放插值的时间基 | `0x441ad5`–`0x441b00` |
| 缩放 from / to | `+0x218/+0x21c = 1.0f`（X/Y）、`+0x220/+0x224 = 0.25f` | `0x441b13`–`0x441b41` |
| `+0xc8` | = 540 | `0x441b56` |
| 自转方向 | `+0x14 ×= -1.0f`（`0x498b5c` = −1.0）⇒ 脚本那 π/1000 反向转 | `0x441b5f`–`0x441b6b` |
| 句柄 | → `player+0xb7e6c` | `0x441b74` |
| 其它 | 杀旧 `+0xb7e6c` / `+0xb7e68`（`0x441a4c` / `0x441a68`）；SE `0x20` `0x24`；replay 位 `0x4b9e48+0xd6 \|= 8` | `0x441b83`–`0x441bb9` |

### 5.3 崩结界（灵击）`0x441bd0` ✅ —— 同一个 id 731，但参数完全不同

- 杀旧环（`0x441bee` `[旧+0x2cc] = 0`，`0x441bf8` 句柄清 0）→ 再 spawn 一个 type `0x1c`（`0x441c19`）。
- 新环：计时 `+0xb4 = 30`；缩放 `0.0625 → 1.3`（`0x441c7c` / `0x441c89` / `0x441c96` / `0x441ca3`）；
  **alpha 目标 = 0**（`+0x9c` 计时 30、`+0xc2 = 1`、`+0x22b` = 当前 alpha、`+0x22f = 0`：`0x441cd3`–`0x441d1b`）
  ⇒ **炸开 + 淡出**（30 帧）。
- 然后循环 32 次 spawn **type `0x1d`**（id 690）：`0x441dc9`–`0x441e38`（起始角 `0xC0490FDB` = −π、步长 `0x498cfc` = 2π/32，`0x441dee` 判 32）。
- 收尾：state → 3（`0x441d5c`），`player+0x240d = 0`，`ds:0x12fe0c4` / `0x12fe0cc` 清 0。
- state 3 用**另一个**环（句柄 `player+0xb7e68`）：type `0x19` → id **730**（`etama3.png`），
  由 12 个 `Player::bomb_on_tick__*` 里的 `0x4084f0` 生成（首个 `0x4087da`，共 12 处：`0x40929f` `0x409ea3` `0x40a48b` `0x40a892` `0x40b01f` `0x40b8dd` `0x40bf0b` `0x40c3d7` `0x40cb67` `0x40d5ca` `0x40dbfc`）：
  缩放 from = 当前（脚本给 1.5）→ to `0.0625`（`0x40859f`），计时 = 玩家计时；颜色写 `+0x1b8/+0x1b9/+0x1ba = 0x40/0x40/0xff` = **红**（`0x4085e3`–`0x4085f7`）。
  它的脚本（`etama.anm` script63 = 全局 730）**以 `ins_4` 跳回自己收尾 = 死循环** ⇒ 它才是「停下来也继续画」的那种环。

⇒ 两种环的分工：**730（中空环，会一直画）= 灵击炸弹的无敌环；731（实心盘，33 帧）= 森罗结界的发动闪光。**

## 6. 两张贴图长什么样（像素统计 ✅，非目视）

| 贴图 | 用途 | 256×256 内的 alpha 分布 | 中心色 |
| --- | --- | --- | --- |
| `etama4.png`（id 731） | 森罗结界发动 | **实心盘**：中心 85、峰值 187（r 64–80），r>128 全 0 | 品红/粉（RGB 170,0,85）|
| `etama3.png`（id 730） | 灵击炸弹 | **中空环**：r<80 全 0，环带 r≈80–130（峰值 105 @ r112–128）| 白偏蓝（RGB ~200,205,245）|

两者都按 `sprite 尺寸 × scale` 画（`0x44fa4d`：`[sprite+0x30] × vm+0x18 ÷ 2.0` 当半宽，另一轴用 `[sprite+0x2c] × vm+0x1c`；`0x498a70` = 2.0）
⇒ 731 在 scale 1.0 时是**直径 256 px 的品红盘**（结界开场那 0.55 s），崩结界那 30 帧从 16 px 炸到 333 px 并淡出。

## 7. 与 `AUDIT.md` §BO-6 / `TUNING.md` A4 / `NEXT.md` 的关系（**本文不改它们**）

| 既有句子 | 判定 | 该改成 |
| --- | --- | --- |
| §BO-6「★ 结界『持续期』在原版没有任何视觉（逐条核过：那段每帧逻辑里一个 effect spawn 都没有）」 | ❌ **被本文推翻** | 保留「没有 spawn」这个事实，删掉「没有任何视觉」的推论：持续期有 §4.2 自机红白闪 + §4.3 全屏 37.5 % 变暗 |
| §BO-6「甲是**一次性 33 帧闪光**」 | ✅ 对 | 只补机制：opcode 1 停指令流 + `run_vm` 返回 1 ⇒ `EffectManager` 收槽（不是 `delete()` 指令）|
| §BO-6 表结构写成 `{u16 id; u16 pad; void* update; void* draw}` | ⚠ 与 `engine/anm/th07/01-anm-id-space.md` §4.5 冲突 | `+0x8` 是**一次性 on_create**（返回值非 0 ⇒ 杀实例），不是 draw |
| §BO-6「崩结界先杀旧 VM（`th07:0x441be5`）」 | ⚠ 地址差 9 B | 置 0 的指令在 `0x441bee`（`0x441be5` 是取句柄）|
| `TUNING.md` A4 行：「107 结界特效 VM ✅ 已落地」 | ⚠ 需要加一句 | 「零售持续期无环，但有全屏变暗 + 自机红白闪；卡里没搬（若搬 = 非复刻）」|
| `NEXT.md`「**107 结界『持续期』要不要视觉** \| 原版**一点都没有**」 | ❌ 同上 | 换成 §0 表的两行 + 让用户拍「要不要照抄变暗/红闪」|

## 8. 跨作旁证（TH08，🟡 只对 idiom） 

`thanm -l 8 local/th08.v1.00d/dat/etama.anm`：同一套写法在 TH08 里成批出现 ——
`ins_8(0)` 紧接 `ins_34(N, mode, target)`（如 `ins_34(15,0,255)` / `ins_34(30,0,255)` / `ins_34(10,1,160)`），
脚本尾部用 `ins_1()` / `ins_2()`，并且也有用 `ins_4(offsetNN, 0)` **跳回自己**的永续脚本（script114）。
⇒ 「op8 设 alpha、op34 = alpha 渐变、op1/op2 = 收尾、op4 = 跳转」这套**跨版本同型**（TH08 侧未读解释器，仅记 🟡）。
**不外推**：TH08/TH09 有没有「结界期间全屏变暗」没查（见 §9）。

## 9. ❓ 未定 / 需要用户拍

1. **数据矛盾**：发动时把环的缩放插值时间基设成 **540 帧**（`0x441b56`），可脚本 33 帧就收槽 ⇒ 环「本该挂满全程」。
   要不要按**数据本意**做（挂满 + 从 1.0 缩到 0.25），还是按**零售实际**做（33 帧闪光）—— 需要用户拍。
   本机只有 v1.00b 样本，**无法**用 v1.00a 验证这是不是该 build 的数据事故。
2. 🟡 `ds:0x4d44e0`（变暗曲线的 0..540 计数）**只有 8 个读点、没找到写点**（可能是通过指针/注册函数写的）。
   「= 结界已过帧数」是从 510/30 两个拐点与 540 总长推的。
3. 🟡 §4.3 的覆盖范围：3D 背景（`on_draw_03` 内更早画的那层）是否也被乘暗，未逐帧验证。
4. 🟡 `0x4428ce` / `0x4438e6` 两个 `0x441960` 调用点（`Player::on_registration` 附近 / 读 sht 的那个函数）未坐实触发条件。
5. ❓ 文档归属：TH07 的森罗结界更像「自机限时无敌态 + 第三条资源轴」，放 `engine/spirit/` 是按任务指定；
   若要并入别的子系统（`engine/player/` 之类）请主 agent 定。
6. ❓ 本文**没有入链**（不许改别的文档）⇒ 需要主 agent 在 `engine/spirit/OVERVIEW.md`（或 `README.md`）加一行链接。

## 附录 A：复现命令

```bash
# 0) 平坦反汇编（已存在，勿重跑）：objdump -d -M intel local/th07.v1.00b/th07.exe > local/_scratch/th07.asm

# 1) 脚本逐字节（自写解析器，不信 thanm）：entry3 @ 0x62e64，script0 @ entry+0x80
python3 - <<'PY'
import struct
d=open('local/th07.v1.00b/dat/etama.anm','rb').read(); ent=0x62e64
ns,nsc=struct.unpack_from('<II',d,ent); sc=[struct.unpack_from('<II',d,ent+0x40+4*ns+8*i) for i in range(nsc)]
p=ent+sc[0][1]
while True:
    op,size,t,mask=struct.unpack_from('<hHhH',d,p); print(hex(p-ent),op,size,t,hex(mask),d[p+8:p+size].hex())
    if op in (-1,1): break
    p+=size
PY

# 2) thanm 侧（脚本原文 / 贴图）
local/vendor/thtk/build/thanm/thanm -l 7 local/th07.v1.00b/dat/etama.anm | sed -n '/script64/,/^}/p'
local/vendor/thtk/build/thanm/thanm -x 7 local/th07.v1.00b/dat/etama.anm    # → data/etama/etama4.png

# 3) 反汇编取段小工具（本文用的那个，落 /tmp 自取）
cat > /tmp/asmrange.py <<'PY'
import re,sys
p='/home/nipporita/renkolab/local/_scratch/th07.asm'
pat=re.compile(r'^\s*([0-9a-f]{6}):\t'); lo=int(sys.argv[1],16); hi=int(sys.argv[2],16)
for l in open(p,encoding='utf-8',errors='replace'):
    m=pat.match(l)
    if m and lo<=int(m.group(1),16)<hi: print(l.rstrip())
PY
python3 /tmp/asmrange.py 0x450d60 0x450ec0    # run_vm：op -1/1/2/3
python3 /tmp/asmrange.py 0x44f9a0 0x44fc10    # draw_vm 的 flags/alpha 闸门
python3 /tmp/asmrange.py 0x41c790 0x41ca10    # on_tick_0b：run_vm 返回非 0 ⇒ +0x2cc=0
python3 /tmp/asmrange.py 0x441330 0x4417fa    # state 3 / state 4 / 结束
python3 /tmp/asmrange.py 0x441960 0x441e78    # 发动 / 崩结界 / 32 颗
```

## 证据指针

**脚本侧**：id 731 = `etama.anm` entry3 script0（[`../../anm/th07/01-anm-id-space.md`](../../anm/th07/01-anm-id-space.md) §4.3）；
`ins_8` → `0x450f4f`；`ins_34` → `0x4520de` / `0x452115` / `0x452124`；`ins_79` → `0x4518e4` / `0x4519ac`；
`ins_1`(opcode 1) → `0x450df3`（`flags &= ~1` `0x450dfc`、`vm+0x1e0 = 0` `0x450e0b`、`return 1` `0x450e15`）；
`ins_4` 跳转写 IP `0x450fc4`；计时递减 `0x4394c7`（`0x439517`）。
**绘制侧**：`draw_vm` `0x44f9a0`（三闸门 `0x44f9d5` / `0x44f9f5` / `0x44fa0b`）；顶点色 `0x44f470` + 全局乘子 `0x44f485`–`0x44f509`；
`AnmManager+0x0` 写点 `0x406b71`–`0x406b81`；`Stage::sub_406930` `0x406930`；`Stage::on_draw_03` `0x4069d0`；`Stage::draw_vms` `0x407900`（被 `0x406dc3` 等调）。
**收槽**：`EffectManager::on_tick_0b` `0x41c790`（`0x41c8a3` call `run_vm`、`0x41c8a8`–`0x41c8b1` 杀）；`run_vm` 正常出口 `0x4540d1`。
**结界本体**：`Player::sub_441330` `0x441330`（state 4 `0x44148d`、红白闪 `0x441553`–`0x44158a`、变暗 `0x44158d`–`0x44162f`、槽 `0x4414e5`）；
`Player::sub_441670` `0x441670`（结束 `0x4417e1`）；发动 `0x441960`（540 `0x4419f7`、spawn `0x441aa4`、`0x441b74`）；崩结界 `0x441bd0`（`0x441bee` / `0x441c19` / 32 颗 `0x441e01`）；
`Player::on_draw_06` `0x4420b0`（红白闪 `0x442280`、变暗 `0x44233b`）；炸弹环 `0x4084f0` → `player+0xb7e68`。
**常量**：`0x498b5c` = −1.0、`0x498a70` = 2.0、`0x498a54` = 1.0、`0x498b00` = 30.0、`0x498af8` = 255.0、`0x498b08` = 64.0、`0x498a98` = 256.0。
**结构名**：ExpHP `local/vendor/th-re-data/data/th07.v1.00b/`（`AnmVm::draw_vm` / `AnmManager::run_vm` / `EffectManager::on_tick_0b` / `Player::sub_441330` /
`statics.json` 的 `STAGE` `0x1347b00`、`ANM_MANAGER_PTR` `0x4b9e44`、`EFFECT_MANAGER` `0x12fe250`）。
