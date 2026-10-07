# TH08 源码 ⇄ TH09 对照索引（同引擎族的「线索级」语义参考）

> **版本**：TH09 v1.50a（`local/th09.v1.50a/th09.exe`，685,056 B，imagebase `0x400000`，md5 `cf634df46e05552e104fa97a971aaac0`）。本文**裸地址默认属该版本**；TH08 侧地址一律写成 `th08:0x…`。
> **方法**：TH08 侧 = 社区源码重建 `local/vendor/th08-decomp`（`N0zoM1z0/th08`，MIT，commit `d2a00f4`）的 `src/` + `config/mapping.csv`；TH09 侧 = Ghidra 语料 `local/th09.v1.50a/decomp/all.c` 与社区符号 `local/th09.v1.50a/decomp/INDEX.tsv`，凡标 ✅ 的行都用 `tooling/ghidra/th20_show.py` 读过 TH09 反编译。
> **可信度**：**✅ 36 行**（该行 TH09 侧行为已读反编译确认；有 TH08 对应物的两侧都读过）· **🟡 13 行**（仅线索：只读了 TH08 侧或只看了 TH09 函数头，对应关系没有锚）。

## 0. 使用纪律：TH08 源码只能当「线索级」参考

**一句话**：TH08 源码回答「这个类大概干什么、这个字段大概什么意思」；它**不回答**「TH09 在某个地址上一定这么干」。
任何要写进仓库的 TH09 结论，都必须落到 `th09:0x…` 上重新取证。

1. **它是「纯结构迁移」，不是「有锚迁移」**。按 [`METHOD.md`](../../METHOD.md) 的「迁名的两类」：TH09 二进制里没有任何东西证明「这个函数就是 TH08 那个」，
   两边只有**形状相似**（调用图同构、常量签名、同名字段）⇒ 按纪律**不入库为结论、只当线索**；要升 ✅ 必须过 TH09 自己的内容验证
   （本文件 ✅ 行的做法就是：读 TH09 反编译，对常量、字段偏移、调用点逐条比对）。
2. **指认错误比没有证据更危险**（见 [`community-sources.md`](community-sources.md) 的 TH06NC 条目：连「双向唯一」的判据都出过假阳性）。
   所以本文件**逐行标可信度**，🟡 行不写结论、只写「什么证据才能确认」（§4）。
3. **「用一个名字」基本无风险，「转发整份地址↔名字数据集」才是要挡的**（同上条目）。本文只做前者：借词汇、不搬地址。
4. **可用面**：TH06–TH09 是同一引擎族（TH10 才换代）⇒ 通用类（Timer / Rng / AnmManager / AsciiManager / Player / EnemyManager）的映射最可靠；
   **本作特化**（花映塚的 side 结构、vs 模式、`ExAttackManager`、`UpdateFuncRegistry`）最不可靠，TH08 侧根本没有对应物。
5. ⚠ **先记住这条架构差异**：TH08 用**链式回调**（`Chain` / `ChainElem` / `AddToCalcChain` / `AddToDrawChain`，优先级是 `CHAIN_PRIO_*` 小整数）；
   TH09 用**更新函数注册表**（`UpdateFuncRegistry__*`，`register__on_tick` / `register__on_draw(priority)`，优先级是另一套编号，如 `0x22` / `0x17`）。
   ⇒ **「谁先跑」不能照搬**；TH08 的 `Chain` 顺序在 TH09 上无效。同理：TH08 是单人游戏，TH09 分 side（`GuiSide`、`Supervisor+0x348` 的 side 基址）。
6. **不要照搬结构体布局**。通用类里至少 Timer 与效果槽两处布局/约定**不同**（§3 第 1、4 条）。字段偏移一律以 TH09 自己的反编译为准。

## 0.5 本轮拉取记录（怎么拿到的、能不能用）

| 项 | 值 |
| --- | --- |
| 方式 | **codeload tarball**：本机 `github.com:443` 超时（`git clone` 与网页都不通），`raw.githubusercontent.com` / `api.github.com` / `codeload.github.com` 可达 |
| 落盘 | `local/vendor/th08-decomp/`（gitignored）；93 个 `src/` 文件 + `config/` 账本 |
| 上游 | `N0zoM1z0/th08`（MIT，commit `d2a00f4acb4a13916e07a00bd13f714961714628`，2026-09-19）；它保留 `GensokyoClub/th08` 公开历史到 `7ad3792`，之后独立开发 |
| 备选 | `GensokyoClub/th08`（同目标 exe，公开快照，2026-09-05 起转为私下开发）；`happyhavoc/th08` README 404 ⇒ **不可用** |
| 目标 exe | TH08 1.00d `sha256 330fbdbf…`；**我们的样本** `local/th08.v1.00d/th08.exe` 是另一个 1.00d 构建（`sha256 9d669602…`，size 同为 840,704） |
| ✅ 地址可用性（本轮实测） | `config/mapping.csv` 的 **1100 条 `th08::` 函数入口 1100/1100 落在 `.text`**，其中 **1091 条（99.2%）** 在我们样本里就是 `push ebp; mov ebp, esp` 开场 ⇒ 地址空间对我们这份样本可用 |

**线索级入口**（TH08 侧）：`local/vendor/th08-decomp/docs/SEMANTIC_INDEX.md`（子系统→文件对照）、`docs/EFFECT_STORAGE.md`、
`config/mapping.csv`（名字↔地址）、`config/matches.csv`（逐函数匹配率）。与先前的 `local/vendor/th08_dec`（Patchouli-CN，另一条 reccmp 谱系，MIT）互为交叉源。

---

## 1. 对照表

| TH08 文件 / 类 / 方法 | TH09 地址 | TH09 名字 | 看起来做什么 | 可信度 |
| --- | --- | --- | --- | :---: |
| `ZunTimer::ZunTimer` / `Initialize`（`src/Supervisor.hpp`）| `0x403390` | `Timer__constructor` | 构造 = 清零：`current=0`、`previous=0xfff0bdc1`、`subFrame=0` | ✅ |
| `ZunTimer::SetCurrent`（`operator=`）| `0x4014d0` | `Timer__set_current_without_previous` | 时间置 value：`+8=value`、`+0=`哨兵、`+4=(float)value` ⚠float 槽约定见 §3 | ✅ |
| `ZunTimer::SetCurrent` 的 thunk | `0x401500` | `Timer__set_current_without_previous__stub` | 只转发到 `0x4014d0`；ANM 的 opcode 4/5 走这条 | ✅ |
| `ZunTimer::Tick`（`previous=current` + `TickTimer`）| `0x401510` | `Timer__sub_401510` | `previous=current`，再转 `Supervisor::TickTimer(&current,&subFrame)` | ✅ |
| `Supervisor::TickTimer`（`th08:0x447421`）| `0x42f4f0` | 无社区名（`FUN_0042f4f0`）| 亚帧累加 × `framerateMultiplier` → 整数帧进位 | ✅ |
| `ZunTimer::Increment` / `Decrement` 本体 | `0x42f490` | `Timer__sub_42f490` | 带 delta 的抽帧：`prev=cur`；`subFrame+=delta×mult`；`current=(int)subFrame` | ✅ |
| `ZunTimer::Increment` / `Decrement` 的 ±n 包装 | `0x406600` / `0x406620` | `Timer__sub_406600` / `Timer__sub_406620` | ±n 直接丢给 `0x42f490` | ✅ |
| `ZunTimer::operator--`（`Decrement(1)`）| `0x406670` | `Timer__sub_406670` | 每帧 −1 | ✅ |
| `ZunTimer::operator int` | `0x435f00` | `Timer__get_current` | 读整数帧 `+8` | ✅ |
| `ZunTimer::operator float` / `subFrame` | `0x4014f0` | `Timer__get_subframe` | 读 float 槽 `+4` ⚠两作语义不同，见 §3 | ✅ |
| `ZunTimer::operator>` `>=` `<=` `==` | `0x403de0` `0x401520` `0x406720` `0x404950` | `Timer__is_gt` / `is_ge` / `is_le` / `is_eq` | 全部是对 `current`(`+8`) 的整数比较 | ✅ |
| `Supervisor::Supervisor` | `0x431500` | `Supervisor__constructor` | 整块清零 + 3 个 Float3 向量 + `flags \|= 0x140` | ✅ |
| **（TH08 无对应物）** | `0x401440` | `Supervisor__sub_401440` | 写 `SUPERVISOR+0x348 = SUPERVISOR+0x78+0xf0×side`：**当前 side 的基址** | ✅ |
| `Supervisor::OnUpdate` | `0x431110` | `Supervisor__on_tick` | 主循环：输入采样 / 状态机 / 渲染前置 / 网络回放缓存 | 🟡 |
| `Supervisor::StartupThread`（`th08:0x4464c8`，1391 B）| `0x41af2d` | `Supervisor__thread_start`（1689 B）| 大初始化（输入 / 音频 / anm / 各 manager 注册）⚠名字更贴 `StartupThread` | 🟡 |
| `Supervisor::RegisterChain` | `0x432090` | `Supervisor__on_registration` | 注册自己 + 各 manager 的 tick/draw | 🟡 |
| `Rng::GetRandomU16`（`src/Global.cpp`）| `0x42ae20` | 无社区名（`FUN_0042ae20`）| 16 位 LCG：`(seed^0x9630)+0x9aad`；`seed=u*4+(u>>14)`；计数 +1 | ✅ |
| `Rng::GetRandomU32` | `0x42ae50` | 无社区名（`FUN_0042ae50`）| 取两次 u16 拼 32 位，**先取的在高 16 位** | ✅ |
| `Rng::GetRandomU32InRange` | `0x4048e0` | `Rng__rand_upto_n` | `range ? rand32() % range : 0` | ✅ |
| `Rng::GetRandomF32` | `0x42ae70` | `Rng__rand_0_to_1` | `rand32() × 2.3283064e-10`（= /2³²）| ✅ |
| `Rng::GetRandomF32Signed` | `0x42aea0` | `Rng__rand_neg_1_to_1` | `rand32() × 4.6566129e-10 − 1`（= /2³¹ − 1）| ✅ |
| `Rng::GetRandomF32SignedInRange` | `0x406200` | `Rng__rand_neg_x_to_x` | `rand_neg_1_to_1() × range` | ✅ |
| `EffectManager::SpawnEffect` | `0x40ccc0` | `EffectManager__sub_40ccc0_create_something` | 环形槽分配（`0xd8`/槽，`+0xc4` 占用字节），满则退回固定槽 ⚠布局见 §3 | ✅ |
| `EffectManager::OnUpdate` | `0x40cdd0` | `EffectSys__on_tick` | 复位 3 条 draw 链 → 遍历池：空闲槽释放指针、活跃槽跑 VM + `+0xbc` 回调 | ✅ |
| `EffectManager::OnDraw` | `0x40cfb0` | `EffectSys__on_draw_x` | 遍历 draw 链：有回调走回调，否则 `AnmManager__draw_vm` | ✅ |
| `EffectManager::RegisterChain` / `LoadEffectResources` | `0x40d1c0` | `EffectSys__on_registration` | 预载效果 anm / 分配池并注册 | 🟡 |
| `AsciiManager::AddedCallback` | `0x4344e0` | `AsciiManager__on_registration` | 整块清零（`0x382b` dword）+ 预载 `ascii.anm` / `capture.anm` | ✅ |
| `AsciiManager::RegisterChain` | `0x435ca0` | `AsciiManager__initialize` | 注册 on_tick + 两支 on_draw（TH09 优先级 `0x22` / `0x17`）| ✅ |
| `AsciiManager::OnUpdate` | `0x435b00` | `AsciiManager__on_tick` | 遍历 popup：`inUse` → 上浮 + 计时，超 60 帧回收 | ✅ |
| `AsciiManager::AddString` | `0x4342a0` | `AsciiManager__draw` | **不是 draw**：往 `0x100` 槽 × `0x60` 的字符串表里追加 | ✅ |
| `AsciiManager::AddFormatText` | `0x434330` | `AsciiManager__drawf` | `vsprintf` 进 512B 栈缓冲 → `AddString` | ✅ |
| `AsciiManager::OnDrawLowPrio` / `OnDrawHighPrio` | `0x435c00` / `0x435c60` | `AsciiManager__on_draw_22` / `on_draw_17` | 两段优先级的实际绘制 | 🟡 |
| `AnmManager::ExecuteScript` | `0x436f30` | `AnmManager__run_vm` | ANM VM 解释器：时间门 + opcode switch + interrupt | ✅ |
| `AnmManager::PreloadAnm` | `0x43c7a0` | `AnmManager__preload_anm` | `(slot, "xxx.anm")` 载入 + 自旋等加载完，失败/关闭时返 0 | ✅ |
| `AnmManager::DrawInner` / `Draw2D` 家族 | `0x43ab50` | `AnmManager__draw_vm` | 按 VM 标志选绘制路径（2D/3D/无旋转…）| 🟡 |
| `AnmManager::AnmManager` | `0x43ce30` | `AnmManager__constructor` | 构造（D3D 设备 / 顶点缓冲 / 纹理槽）| 🟡 |
| `EclManager::RunEcl`（`src/EclRun.cpp`）| `0x4086c0` | `EnemyManager__ecl_run` | ECL 解释器主循环：`secondaryTime`/`time` 递减 + opcode switch ⚠类归属见 §3 | ✅ |
| `EnemyManager::OnUpdate` | `0x410730` | `EnemyManager__on_tick` | 遍历敌机 → `ecl_run` + 敌机 VM 每帧 step + 掉落/死亡 | ✅ |
| `EnemyManager::OnDrawImpl` | `0x411d50` / `0x411db0` | `EnemyManager__on_draw_x` / `on_draw_y` | 两段优先级的敌机绘制 | 🟡 |
| `EnemyManager::SpawnEnemy1` / `SpawnEnemy2` | 无社区名 | `FUN_0040f170` / `FUN_0040fcc0` | 生成敌机（TH09 走波次表 `0x4a7c88`，TH08 是直接调用）| 🟡 |
| `Player::Player`（`th08:0x449ca0`）| `0x41f070` | `Player__constructor` | 构造：1×AnmVm + 7×Timer + 4 条向量（TH08 反汇编同形：1×AnmVm + 6×Timer + 4×向量）| ✅ |
| `Player::AddedCallback` | `0x41ee50` | `Player__on_registration` | 按 `shotType` 查 `plXX.sht` 表加载 + 预载自机 anm | ✅ |
| `Player::OnUpdate` | `0x41e900` | `Player__on_tick` | 冻结检查 → 状态机（死亡/复活/移动）→ 移动与子机 | ✅ |
| `Player::OnDrawHighPrio` / `DrawBullets` | `0x41eb10` / `0x41f680` | `Player__on_draw_x` / `Player__draw_bullets` | 自机 / 自机弹绘制 | 🟡 |
| `Gui::UpdateStageElements`（`th08:0x435900`）| `0x418a90` | `GuiSide__on_tick` | 12 个 front VM 每帧 step + boss 血条状态机 ⚠TH08 `frontVms[16]`、TH09 分 side | ✅ |
| `Gui::OnUpdate` + `Gui::RunMsg` | `0x417630` | `Gui__on_tick` | 立绘/对话框 VM + 消息推进 | 🟡 |
| `Gui::DrawStageElements` / `DrawGameScene` | `0x4193e0` | `GuiSide__on_draw` | 关卡 UI（血条 / 计时 / 分数）绘制 | 🟡 |
| `Spellcard::OnUpdateImpl`（`th08:0x416b90`）| `0x4041f0` | `Spellcard__on_tick` | 符卡演出状态机（TH09 是 vs 模式，语义可能不同）| 🟡 |
| **（TH08 源码无 ghost / spirit 类，`grep -ri` 零命中）** | `0x4086c0` `0x410730` | `EnemyManager__ecl_run` / `EnemyManager__on_tick` | 花映塚的「幽灵」= `enemy.ecl` 的 Sub0/1/2（+ flags bit12 的固灵），**不是独立类** | ✅ |

---

## 2. ✅ 行的 TH09 侧证据（读的就是这些）

**Timer（14 行）** —— TH08 侧 `src/Supervisor.hpp` 的 `struct ZunTimer`（`previous` `+0` / `subFrame` `+4` / `current` `+8`，共 `0xc` B）。

- `0x403390` `Timer__constructor` → `FUN_004014a0`：`+8=0`、`+0=0xfff0bdc1`、`+4=0`（= TH08 `Initialize`；哨兵常数 −999 vs −999999 不同，语义同为「无上一帧」）。
- `0x4014d0` `Timer__set_current_without_previous`：`+8=arg`、`+0=0xfff0bdc1`、`+4=(float)arg`；`0x401500` 的 `__stub` 就是它的转发。
- `0x401510` `Timer__sub_401510` → `FUN_004014b0`：`previous=current`，再 `FUN_0042f4f0(&SUPERVISOR, &current, &subFrame)`。
- `0x42f4f0`（对应 TH08 `Supervisor::TickTimer` `th08:0x447421`）：`mult = SUPERVISOR+0x5b8`（帧率倍率，🟡 这个**名字**借自 TH08，值本身已核）；`mult ≤ 0.99` 时 `*sub += mult; *frames = (int)*sub`，否则 `(*frames)++; *sub += 1`（objdump 已核 `0x42f4f0..0x42f536`）。
- `0x42f490` `Timer__sub_42f490`：`prev=cur`；`subFrame += delta × (mult ≤ 0.99 ? mult : 1)`；`current=(int)subFrame`；若 `ds:0x4b36d4 & 0x20`（TH08 里对应 `g_Supervisor.flags.forceExtraTimerStep`，🟡 位号未逐位核）再置哨兵并把整数部分加回 float 槽。
  `0x406600/0x406620` 是 ± 包装，`0x406670` 是 −1 包装（= TH08 `Decrement(1)`）。
- `0x403de0`/`0x401520`/`0x406720`/`0x404950` 四个比较器分别读 `+8` 与 arg 比 `>` `>=` `<=` `==`；`0x435f00` 读 `+8`；`0x4014f0` 读 `+4`。

**Supervisor**

- `0x431500` `Supervisor__constructor`：`initialize_vector_without_destructor(this+0x78, 0xf0, 3, Float3__constructor)` + 清零 `0x1ec` dword + `flags |= 0x140`（TH08 `Supervisor::Supervisor` = `memset` + 置 `dummyMidiTimerEnabled`/`scoreBackupPending` 两 flag）。
- `0x401440` `Supervisor__sub_401440`：`*(this+0x348) = param*0xf0 + 0x78 + this`；`Player__on_tick`、`GuiSide__on_tick`、`EffectSys__on_tick` 都先调它 ⇒ 是「切到第 param 个 side 的全局上下文」；**TH08 单人版没有此物**。

**Rng（6 行）** —— TH08 侧 `src/Global.cpp`。

- `0x42ae20`：`u=(*seed^0x9630)+0x9aad; *seed=u*4+(u>>14); gen++`。TH08 `GetRandomU16` 是 `temp=(seed^0x9630)-0x6553; seed=((temp&0xc000)>>14)+temp*4`。
  两者**逐位等价**：`−0x6553 ≡ +0x9aad (mod 2¹⁶)`，且 u16 的 `u>>14` 就是 `(u&0xc000)>>14` ⇒ **PRNG 双作同流**（这条可以直接拿 TH08 源码推 TH09 的随机序列）。
- `0x42ae50`：`hi = FUN_0042ae20(); lo = FUN_0042ae20(); return lo&0xffff | hi<<16` = TH08 `GetRandomU16()<<16 | GetRandomU16()`。
- `0x4048e0`：`range ? FUN_0042ae50(this) % range : 0` = TH08 `GetRandomU32InRange`。
- `0x42ae70`：`rand32() × 2.3283064e-10`（= 2⁻³²，与 TH08 `GetRandomF32` 的 `/UINT_MAX` 同值，TH08 注释已指出「除数被浮点舍入成 2³²」）。
- `0x42aea0`：`rand32() × 4.6566129e-10 − 1`；`4.6566129e-10` = 2⁻³¹，等于 TH08 `GetRandomF32Signed` 的 `/INT_MAX − 1.0`（常数已从 `th09.exe` `0x48fb80` 读出核对）。
- `0x406200`：`rand_neg_1_to_1() × range` = TH08 `GetRandomF32SignedInRange`。

**EffectManager / EffectSys**

- `0x40ccc0`：环形取槽（`idx=*(this+0)`，槽 = `*(this+0x30) + idx*0xd8`），跳过着槽（`slot+0xc4 != 0`），交 `FUN_0040ca60` 初始化，`count--` 到 0 收工，满则退回固定槽 `(*(this+0x38)+*(this+0x34))*0xd8 + *(this+0x30)`。TH08 `SpawnEffect`：`nextEffectIndex` 环形 + 跳过 `active` + `count--`（TH08 池 653 槽，`nextEffectIndex` 上限 `0x200`）。
- `0x40cdd0`：复位三条 draw 链（`+0x2c4/+0x2c8/+0x2cc` 与 `+8/+0x110/+0x1e8/+0x2c0`）→ 遍历 `*(this+0x34)+*(this+0x38)` 个 `0xd8` 槽 → 空闲槽 `FUN_00401360(slot+0xcc)`、`FUN_00401360(slot+4)` 并清零（= TH08 释放 `effect->vertices`）→ 活跃槽 `activeCount++`、跑 `slot+0xbc` 回调、逐个 `AnmManager__run_vm(ptr, slot+4 + k*0x2a4)`，按返回值回收 → 按 `+0xd0/+0xca/+0xc8` 挂进三条 draw 链。TH08 `EffectManager::OnUpdate` 同构（复位 5 个 draw 组哨兵 → 653 个 `Effect`：空闲释放 `vertices`、活跃判 `deathbombFreezeActive` 后 `updateCallback` + `ExecuteScript` + `timer++` → 挂 draw 组链）。
- `0x40cfb0`：遍历 draw 链，`slot+0xc0 != 0` 走回调，否则 `AnmManager__draw_vm`；TH08 `EffectManager::OnDraw` 同构（`drawCallback` 否则 `Draw2D(&effect->vm)`，位置 = `position + arcadeTopLeft + pos2`，`z=0.07`）。

**AsciiManager**

- `0x4344e0`：`for(i=0x382b) *p++=0` 整块清零 → `AnmManager__preload_anm(ANM_MANAGER_PTR, 1, "ascii.anm")` → `(..., 3, "capture.anm")` → 失败返 `0xffffffff`。TH08 `AddedCallback`：`memset` + `PreloadAnm(ANM_FILE_SLOT_ASCII, "ascii.anm")` + `(ANM_FILE_SLOT_CAPTURE, "capture.anm")` + `Reset()` + `InitializeVms()`。
- `0x435ca0`：`UpdateFunc__initialize(&ASCII_MANAGER_ON_TICK, AsciiManager__on_tick)` + `register__on_tick(..., 1)`，再注册 `on_draw_22` / `on_draw_17`。TH08 `RegisterChain` 是 `AddToCalcChain` / `AddToDrawChain` 两条 draw 链。
- `0x435b00`：popup 槽长 `0x38`；以 ZunTimer 为基准时 `inUse` 在 `+0x14`、`position.y` 在 `-0xc`（换算成 popup 基址 = `+0x30` / `+0x10`，与 TH08 `AsciiManagerPopup` 逐一对应）；两 side × 100 槽；活跃者 `position.y -= framerateMultiplier × 0.2f`（常数 `0x490f3c` 已读）+ `Timer` tick，`Timer__is_gt(t, 0x3c)` 即 > 60 帧回收。TH08 `OnUpdate`：`position.y -= 0.5f × mult`、`timer++`、`> 60` 回收 ⚠**数值不同**（§3）。
- `0x4342a0`：`count = *(this+0x8264)`，`< 0x100` 才写；槽 = `this+0x2264 + n*0x60`；`strcpy(text)` → position `+0x40` → color `+0x4c` / scaleX `+0x50` / scaleY `+0x54` → `isSelected(+0x58)=0`、`isGui` `+0x5c`。TH08 `AsciiManagerString` = `char text[64]; Float3 position; D3DCOLOR color; f32 scaleX, scaleY; ZunBool isSelected, isGui;`，`sizeof == 0x60` ⇒ **逐字段对上**。
- `0x434330`：`vsprintf(buf[512], fmt, args)` → `AsciiManager__draw` = TH08 `AddFormatText`。

**AnmManager**

- `0x436f30`：`if (vm[0x88]==0) return 1`（= TH08 `currentInstruction == NULL`）；`flagsWord(vm+0x1f8) & 0x80000 → return 0`（= TH08 `flag19`，TH08 `Player::OnUpdate` 正是用 `flagsWord |= 0x80000` 在冻结帧停 VM）；`*(i16*)(vm+0x1fe) != 0 → interrupt 分支`（= TH08 `pendingInterrupt`）；`Timer__get_current(vm+0x38) >= instruction->time` 才 `switch(opcode)`。TH08 `ExecuteScript` 同构（`currentInstruction`/`flag19`/`pendingInterrupt`/`while (instruction->time <= currentTimeInScript)` + opcode switch）。
- `0x43c7a0`：`FUN_0043c610(this, slot, name)`（= `ReadAnmEntries`）→ `piVar1[6]=1`（待载计数）→ `Sleep(1)` 自旋到计数 0 或关闭请求 → `(closeReq ? 0 : ptr)`。TH08 `PreloadAnm` 逐行同义（`numberEntriesToBeLoaded=1` + `Sleep(1)` 自旋 + `subthreadCloseRequestActive` 返 NULL）。

**ECL / EnemyManager**

- `0x4086c0`：`local_104 = param+0x288`（ECL 上下文），取 `instruction`；`Timer__get_current(ctx+0x94) < 1` 就退出本帧，否则 `Timer__sub_406670(ctx+0x94)` 与 `Timer__sub_406670(ctx+8)` —— 正是 TH08 `RunEcl` 里 `secondaryTime--` / `time--` 的「本帧到此为止」；随后按 opcode 分派（语料头注明 **148 个 ecl opcode case**）。TH08 `RunEcl`（`src/EclRun.cpp`）同构，但**类归属不同**（§3）。
- `0x410730`：调 `Supervisor__sub_401440(&SUPERVISOR, *(param+0x31c))`（第二个实参疑为 side 下标 🟡）→ 波次计时 `Timer__is_gt(+0x2ac3c8)` 决定 `FUN_0040fcc0` 生成 → 每敌机 `EnemyManager__ecl_run(this)`（函数内 `0x4086c0`）+ `AnmManager__run_vm` 跑敌机 VM（含 trail VM）→ `Timer__sub_406670` / `Timer__set_current_without_previous__stub` 计时 → `FUN_0040f860` 加分。TH08 `EnemyManager::OnUpdate`（`src/EnemyManagerUpdate.cpp`）同构：逐敌机 `g_EclManager.RunEcl(enemy)` + `g_AnmManager->ExecuteScript(&enemy->vm)` + `secondaryVms`。

**Player**

- `0x41f070`：`AnmVm__constructor` ×1、`Timer__constructor` ×7、`initialize_vector_without_destructor` ×4（option `0x2f4×4`、bullet `0x484×0x80` 等）。TH08 `th08:0x449ca0` 反汇编：`Float3::Float3` ×14、`ZunTimer::ZunTimer` ×6、`eh_vector_constructor_iterator` ×4、`AnmVm::AnmVm` ×1 ⇒ **同形**（TH08 上游 `src/Player.cpp` 的这个 ctor 还是空壳，只能用反汇编）。
- `0x41ee50`：按 `shotType` 取 `(&PTR_s_pl00_sht_004a1238)[shotType*6]`（`plXX.sht` 名字表）→ `FUN_0041bbe0` 加载 → 写回 `DAT_004a8160[side]` 缓存，并从 `0x4a1bd0/0x4a1b90/0x4a1b50/0x4a1b10/0x4a1ad0` 五张按角色的表各取一项写到 `+0x303f0..0x30400`。TH08 `Player::AddedCallback`：`Player::LoadShtFile(&primaryShtFile, g_Player1ShtFiles[shotType])` + `PreloadAnm(g_PlayerAnmFilenames[...])` + 按 shotType 的表设字段。
- `0x41e900`：`flags & 0x1800` 冻结直接返回 → `DAT_004a7ec4/dec4` 之外做 `FUN_0041dc20` 碰撞区 → `FUN_00435ec0` 取状态分派（5/4/2/1）→ `Supervisor__sub_401440` → 移动/子机。TH08 `Player::OnUpdate`：`scriptedUpdateFreeze` 早退（并把两个 effect VM 的 `flagsWord` 置 `0x80000`）→ `UpdateCollisionRegions` → `UpdateBombState` → 死亡/复活状态机 → `UpdateMovementAndOptions`。

**Gui**

- `0x418a90`：先 `Supervisor__sub_401440` 切 side，再对 12 个 stride `0xa9` dword 的 VM 逐个 `AnmManager__run_vm`，然后读一个经 `param_1[0x299c]` 间接取到的对象 `+0x30414`（自机结构尾部，TH09 `Player__on_registration` 也写 `+0x303f0..+0x30400`；🟡 是否属 Player 未坐实）走 boss 血条状态机。TH08 `Gui::UpdateStageElements` 同构：`frontVms[12]` 的 interrupt/`IsStopped` 状态机 + `bossUIOpacity += 4` 之类 ⚠TH08 数组是 `frontVms[16]`、且是单份而非 per-side。

**ghost / spirit**

- TH08 源码里**没有** ghost/spirit 类（`grep -ri 'ghost|spirit' src/` 零命中）⇒ 无对照可做。TH09 的幽灵是 `enemy.ecl` 的 `Sub0/1/2`（新月/绿/黄/红），「固灵」不是新对象而是同一敌机置 `+0x3380` bit12 + 换 `enemy.anm` script 23/24/25；一手过程见 [`../versus/th09/02-ghost-and-seal.md`](../versus/th09/02-ghost-and-seal.md)。
  本表只登记它的**执行入口**：`0x4086c0`（ECL）与 `0x410730`（管理器的每帧遍历）。

---

## 3. 名字像、但不是同一物（假阳性/差异登记）

> [`METHOD.md`](../../METHOD.md) 要求「批量手法每一轮都要人工过一遍」并登记假阳性。以下是本轮的实际差异。

| # | TH09 名字 | 实际是什么 | 差异/证据 |
| --- | --- | --- | --- |
| 1 | `Timer__set_current_without_previous` / `Timer__get_subframe` | float 槽 `+4` 在 TH09 是**总时长**，TH08 `ZunTimer::subFrame` 是 **0..1 的小数**（TH08 `operator float()` = `current + subFrame`）| TH09 `0x42f490` 里 `current = (int)subFrame`；TH08 `Increment` 里 `while (subFrame >= 1) current++, subFrame -= 1`。⇒ **布局同、约定不同**，别照搬 |
| 2 | `AsciiManager__draw` | **不是绘制**，是 `AsciiManager::AddString`（往 `0x100`×`0x60` 字符串表追加）| 反编译里没有 D3D 调用，只有 `strcpy` + 字段拷贝；字段与 TH08 `AsciiManagerString`（`sizeof 0x60`）逐一对上 |
| 3 | `Supervisor__thread_start`（1689 B）| 更贴 TH08 **`StartupThread`**（`th08:0x4464c8`，1391 B）而不是 `ThreadStart`（97 B）| 大小 + 函数体（输入/音频/anm/注册）比对；⇒ 名字是线索不是结论 |
| 4 | `EffectManager__sub_40ccc0_create_something` / `EffectSys__*` | TH09 效果槽 = **`0xd8` B，VM 用「指针 + 计数」外挂**（`+4` 指针、`+8` 个数、`+0xc4` 占用）| TH08 `Effect` = **`0x360` B，`AnmVm` 内联在 `+0`**（`position` 在 `+0x2a4`）。⇒ 子系统同族、对象模型不同代 |
| 5 | `EnemyManager__ecl_run` | TH08 里 ECL 解释器属于**独立类 `EclManager`**（`th08:0x4184b0`），不在 EnemyManager 里 | TH09 名字把它挂在 EnemyManager 名下；读字段时按 TH09 自己的 `param+0x288`/`+0xb38` 走，别套 TH08 `Enemy*` 的 `activeEclContext` |
| 6 | `AsciiManager__on_tick` | 机制同 TH08 `OnUpdate`，但**上浮常数 0.2f（TH09，`0x490f3c`）vs 0.5f（TH08）**，popup 数 **100×2 vs 723** | 反编译与常数读取；⇒ 移植动画数值不能照抄 |
| 7 | `AsciiManager__initialize` 的 `0x22` / `0x17` | TH09 注册表的优先级编号，**不是** TH08 的 `CHAIN_PRIO_DRAW_ASCIIMANAGER_HIGH/LOW_PRIO = 14/20` | 两套编号不同轴；`on_draw_22` 与 `on_draw_17` 各自对应 High 还是 Low **未定**（见 §4） |
| 8 | `GuiSide__on_tick` 的 12 个 VM | TH08 `Gui::frontVms[16]` 是单份；TH09 是**每 side 一份**、循环 12 次（stride `0x2a4`）| `0x418a90` 的 `iVar8 = 0xb` 循环；`Supervisor__sub_401440` 切 side |

**TH09 独有、TH08 侧完全没有对应物**（TH08 源码在这几点上只能帮「概念」、帮不了字段）：
`GuiSide__*`（per-side GUI）、`ExAttackManager__*`（vs 模式 Ex 攻击）、`NetworkManager__*`（对战网络/回放）、`Stage__*`（场景管理器）、
`UpdateFuncRegistry__*`（更新函数注册表，替代 TH08 的 `Chain`/`ChainElem`）、`Supervisor+0x348` 的 side 上下文。

---

## 4. 🟡 行的「什么证据才能确认」

| 行 | 待确认 | 确认它需要什么 |
| --- | --- | --- |
| `Supervisor__on_tick`（`0x431110`）| 是否含 TH08 `OnUpdate` 的输入采样（按住 8 帧节奏 `g_NumOfFramesInputsWereHeld`）与状态机 | 读完整函数体，找按键位掩码与「held 计数」的常量（`0x1e` / `%8`）|
| `Supervisor__thread_start`（`0x41af2d`）| 与 TH08 `StartupThread` 的逐段对应 | 逐段比对：`SetupDInput` 调用、`text.anm` 预载、`AsciiManager::RegisterChain`、`bgm/thbgm.fmt` 字符串 |
| `Supervisor__on_registration`（`0x432090`）| 注册顺序/集合 | 数它注册了哪些 `on_tick`/`on_draw`，与 TH08 `RegisterChain` 的集合对账 |
| `EffectSys__on_registration`（`0x40d1c0`）| 效果 anm 文件名与槽数 | 读函数体拿字符串与池大小，与 TH08 `LoadEffectResources` / `effects[653]` 对账 |
| `AsciiManager__on_draw_22` / `on_draw_17` | 哪支是 High、哪支是 Low | 看各自调用的层（是否画 ASCII 字符串 vs 分数弹窗），或与 TH08 `OnDrawLowPrioImpl` / `OnDrawHighPrioImpl` 的分支对账 |
| `AnmManager__draw_vm`（`0x43ab50`）| 与 TH08 `DrawInner`/`Draw2D` 家族的对应关系 | 按 VM 标志分支逐个对账（TH08 是 `Draw2D`/`Draw3D`/`DrawNoRotation`/`DrawCameraFacingQuad` 一族）|
| `AnmManager__constructor`（`0x43ce30`）| 顶点缓冲/纹理槽布局 | 与 `docs/EFFECT_STORAGE.md`、TH08 `AnmManager::AnmManager` 对账 |
| `EnemyManager__on_draw_x` / `on_draw_y` | 两段优先级谁先谁后 | 与 TH08 `ENEMYMANAGER_HIGH_PRIO=8` / `LOW_PRIO=11` 的语义对账 |
| 敌机生成（`FUN_0040f170` / `FUN_0040fcc0`）| 与 TH08 `SpawnEnemy1/2` 的参数对应 | 先给这两个函数起名（读参数与调用点），再比 TH08 的 `(id, pos, ...)` |
| `Player__on_draw_x` / `draw_bullets` | 与 TH08 `OnDrawHighPrio` / `DrawBullets` 的对应 | 读绘制函数体，看是否画自机本体 / 自机弹 |
| `Gui__on_tick`（`0x417630`）| 与 TH08 `Gui::OnUpdate`+`RunMsg` 的对应 | 读消息 VM 推进与 `TH_BUTTON_SKIP` 的加速度逻辑 |
| `GuiSide__on_draw`（`0x4193e0`）| 与 TH08 `DrawStageElements` / `DrawGameScene` 的对应 | 看它画哪些元素（血条 / 计时 / 分数）|
| `Spellcard__on_tick`（`0x4041f0`）| TH09 vs 模式下符卡是否还是 TH08 那套演出 | 读状态机 + 比对 TH08 `Spellcard::OnUpdateImpl` 的状态数 |
| Timer / 效果槽的**类型表** | TH09 效果类型表 `0x4a0b50`（步长 `0x18`）与 TH08 `g_EffectTemplates`（`0xc`：`scriptIdx, updateCallback, initializeCallback`）的字段对应 | 逐项 dump TH09 表并与 TH08 表对字段（脚本号位置已知：type 3/4 → script 35/36）|

---

## 5. 相关文档

- [`community-sources.md`](community-sources.md) —— TH08 源码重建的登记与「不入库、只当参考」的认识论。
- [`../../METHOD.md`](../../METHOD.md) —— 「迁名的两类」（有锚 vs 纯结构）与五段证据链。
- [`../versus/th09/02-ghost-and-seal.md`](../versus/th09/02-ghost-and-seal.md) —— TH09 幽灵/固灵的一手过程（本文 §1 最末一行的出处）。
- [`../card/th09/01-pofv-charge-and-cancel-fields.md`](../card/th09/01-pofv-charge-and-cancel-fields.md) —— TH09 `Player::on_tick` `0x41e900` 的既有结论。
- [`../../games/th09.v1.50a/INDEX.md`](../../games/th09.v1.50a/INDEX.md) —— TH09 样本与覆盖登记。
