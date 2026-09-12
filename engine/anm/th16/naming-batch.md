# TH16 ANM — 想要的 Ghidra 重命名（待 T10 统一落库）

> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>
> **这份文件是追加式的。** 每个 R 任务把自己想要的命名追加到末尾，由 T10 用 MCP 一次性落库，
> 再跑 `tooling/ghidra/symbols.py export th16`。**别在这里删别人的段落。**

---

## 批次 R1 · ANM v8 文件格式（来自 [`01-format.md`](01-format.md)）

### 函数改名

| 地址 | 现名 | 建议名 | 依据 |
| --- | --- | --- | --- |
| `0x46d0c0` | `AnmLoaded::load_one_script` | `AnmLoaded__preread_external_png_for_entry` | 它不碰 script，只在 `has_data==0` 且名字非 `@` 时预读同名 png |
| `0x46d1c0` | `FUN_0046d1c0` | `AnmLoaded__parse_next_entry` | **增量**：每次调用只解析一个 entry，用 `+0x128` 当续跑计数器；只有走到最后一个 entry 才预跑 script 模板 |
| `0x46d3b0` | `FUN_0046d3b0` | `AnmLoaded__parse_one_entry` | 解析单个 entry：建贴图 + sprite 表 + script 表 |
| `0x46c920` | `FUN_0046c920` | `AnmTextureSlot__create_from_png_memory` | D3DX 从内存解码 png，按 `x/y/w/h` 裁剪 |
| `0x46cb60` | `FUN_0046cb60` | `AnmTextureSlot__create_from_thtx` | 从 THTX 块建贴图 |
| `0x46cd30` | `FUN_0046cd30` | `AnmTextureSlot__create_blank` | `@` 名字走这里，建空贴图，返回字节数 |
| `0x46cd80` | `FUN_0046cd80` | `AnmTextureSlot__create_blank_screen_sized` | `@R` 名字走这里 |
| `0x46d8a0` | `FUN_0046d8a0` | `AnmLoaded__store_runtime_sprite` | 写 `+0x11c` 的 0x44 字节运行时 sprite 并算 UV |
| `0x46d690` | `FUN_0046d690` | `AnmManager__reload_or_free_pending_anms` | 遍历 31 个槽，按标志重建或释放 |

### 数据符号

| 地址 | 建议名 | 依据 |
| --- | --- | --- |
| `0x491b90` | `ANM_FORMAT_BYTES_PER_PIXEL`（`int[9]`） | `0x46cccb` / `0x46cd10` 按 format 索引取 bpp |
| `0x491bb4` | `ANM_FORMAT_TO_D3DFORMAT`（`int[9]`） | `0x46cc45` 按 format 索引取 `D3DFORMAT` |
| `0x494280` | `STR_ERR_ANM_BAD_VERSION` | version ≠ 8 的错误串（Shift-JIS） |
| `0x494244` | `STR_ERR_ANM_LOAD_FAILED` | 读文件失败 |
| `0x4942a0` | `STR_ERR_ANM_TEXTURE_LOAD_FAILED` | 外部贴图读不进来 |
| `0x4942e8` | `STR_ERR_ANM_TEXTURE_CREATE_FAILED_NAMED` | 建贴图失败（带 `%s`） |
| `0x49432c` | `STR_ERR_ANM_TEXTURE_CREATE_FAILED` | 建贴图失败（无名） |
| `0x4d9d2c` | `RESOLUTION_X` | `0x46d441` 写进 `entry+0x0a` |
| `0x4d9d30` | `RESOLUTION_Y` | `0x46d44c` 写进 `entry+0x0c` |
| `0x4d9d34` | `SCREEN_COORD_SCALE` | `0x46cbc6` 与 `2.0f` 比较决定是否降采样 |

### 结构体（建议建；本批只给字段图，不强求 T10 一次建完）

`AnmEntryHeader`（0x40 字节，见 [`01-format.md`](01-format.md) §3）、
`AnmSpriteRecord`（0x14）、`AnmScriptTableEntry`（8）、`AnmInstrHeader`（8）、
`ThtxHeader`（0x10）、`AnmTextureSlot`（0x18）。
`AnmLoaded` 已知字段：`+0x108`/`+0x10c`/`+0x110`/`+0x114`/`+0x118`/`+0x11c`/`+0x120`/`+0x124`/`+0x130`，
总大小 `0x13c`（`0x46cf80` 的 `operator_new`）。

### 注释（`set_comment`）

| 地址 | 注释 |
| --- | --- |
| `0x46d404` | ANM v8 版本闸门：只接受 version==8，无旧版本分支 |
| `0x46d4b4` | sprite 偏移表硬编码在 entry+0x40 → header 固定 0x40 字节 |
| `0x46d5c8` | script 表每项 8 字节 {i32 id; u32 offset}；id 被跳过，按位置索引 |
| `0x46cccb` | 源行距 = THTX.width * bpp[THTX.format]，与 entry.format 无关 |

---

## 批次 R2 · ANM VM 骨架（来自 [`02-vm-core.md`](02-vm-core.md)）

### 函数改名

| 地址 | 现名 | 建议名 | 依据 |
| --- | --- | --- | --- |
| `0x4630f0` | `AnmVm::leaf_4630f0__flag_534_24_only` | `AnmVm__integrate_velocities` | 把 angular_velocity / scale_growth / uv_scroll_vel 乘 `GAME_SPEED` 积分进对应字段 |
| `0x4632f0` | `AnmVm::sub_4632f0` | `AnmVm__build_geometry_for_render_mode` | 按 `flags_lo >> 0x19 & 0x1f` 的渲染模式算顶点 |
| `0x464d80` | `Timer::copy` | `Timer__set_from_timer_byval` | 20 字节传值，只取源的 `current` 转调 set_value 逻辑 |
| `0x46f0b0` | `AnmManager::interrupt_tree` | `AnmManager__interrupt_vm_and_direct_children` | 只遍历一层子节点，不递归 |
| `0x46f130` | `AnmManager::interrupt_tree_and_run` | `AnmManager__interrupt_and_run_vm_and_direct_children` | 同上，另对每个目标立刻 `AnmVm::run` |
| `0x46e660` | `AnmManager::sub_46e660_recursive__remove_presumably` | `AnmManager__mark_subtree_for_destroy` | 递归把子树 `flags_hi` 置 `0x40` 并挂上待销毁链 |
| `0x469e20` | — | `AnmVm__enter_render_mode_10` | `302 renderMode` 选到模式 10 时调，写 `vm+0x5c4 = 4` |

### 数据符号

| 地址 | 建议名 | 依据 |
| --- | --- | --- |
| `0x462cb8` | `ANM_VM_JUMPTABLE_LO`（`void*[53]`） | opcode ≤ 299 的跳表 |
| `0x462d8c` | `ANM_VM_JUMPINDEX_LO`（`u8[215]`） | 下标 = opcode + 1 |
| `0x462e64` | `ANM_VM_JUMPTABLE_HI`（`void*[81]`） | opcode 301–613 的跳表 |
| `0x462fa8` | `ANM_VM_JUMPINDEX_HI`（`u8[313]`） | 下标 = opcode − 301 |
| `0x4919e8` | `ANM_ON_TICK_CALLABLES`（`void*[6]`） | `0x45fa42` 按 `vm+0x5c4` 索引 |
| `0x4c0f44` | `ANM_ON_WAIT_CALLABLES` | `0x462bd3` 按 `vm+0x5c0` 索引；⚠️ 与 `ANM_MANAGER_PTR` 相邻，见 02 §10 |
| `0x490eb0` | `TIMER_SPEED_SOURCES`（`float*[4]`） | `Timer::increment` / `decrement` 按 `timer+0xc` 索引；`[0] = 0x4a5788` |
| `0x4a5788` | `GAME_SPEED` | 全局帧速率因子，默认 `1.0` |
| `0x4c1790` | `GLOBAL_SCROLL_DELTA`（`float[3]`） | `0x462a2d`：`flags_lo & 0x8000` 时加进 `vm+0x5e0` |
| `0x4944d4` | `TIMER_SPEED_EPS_LO`（`0.99f`） | `Timer::increment` 的整数快路径下界 |
| `0x4944e8` | `TIMER_SPEED_EPS_HI`（`1.01f`） | 同上上界 |

### 结构体（建议建）

`zAnmVm` 已有 ExpHP 的 `zAnmVmPrefix` / `zAnmVmSuffix` / `zTimer` 定义，本批**不改字段**，
只请把 [`02-vm-core.md`](02-vm-core.md) §3 里坐实的可信度回填成注释。

### 注释（`set_comment`）

| 地址 | 注释 |
| --- | --- |
| `0x45fba0` | ANM VM 主循环头：取指 = script_base(vm) + vm->instr_offset |
| `0x45fbc2` | ★ 时间闸门：ins.time <= vm->script_time.current 才执行，否则退出到帧尾 |
| `0x45fa55` | 早退：instr_offset < 0 表示脚本已被 delete/static 终止 |
| `0x45fac6` | interrupt 服务入口：从脚本开头线性扫 ins 5 <n>，ins 5 -1 为 default |
| `0x45fb8b` | interrupt 命中：脚本时间直接设成标签指令的 time 字段 |
| `0x462bf0` | opcode -1 与 opcode 1 共用：清可见位、instr_offset=-1、返回 1（请销毁） |
| `0x462c08` | opcode 2 static：instr_offset=-1 但返回 0，VM 保留并继续渲染 |
| `0x4629fa` | opcode 3/4 stop：Timer::decrement 抵消帧尾的 increment，脚本时间冻结 |
| `0x462c11` | 帧尾内联 Timer::increment(&vm->script_time)：每帧 += GAME_SPEED |

---

## 批次 R3 · Math + Jumps 组（来自 [`03-math-jumps.md`](03-math-jumps.md)）

### case label 改名（`AnmVm::run` `0x45f980` 函数体内的 label，不是函数）

社区 label 里 **122–131 只有裸数字**，另有 4 条名字与一手语义有出入。建议全部改成
`<opcode>__<助记>` 的统一形式（助记取 thpages 名，与 `03-math-jumps.md` §2 的表一致）。

| 地址 | 现 label | 建议 label | 依据 |
| --- | --- | --- | --- |
| `0x46045b` | `122` | `122__rand` | `div esi` 取余；`n == 0` 短路成 0；用 PRNG Stream B `0x4a6d80` |
| `0x460492` | `123` | `123__randF` | `0x402c70`（`[0,1)`）乘 `arg1` |
| `0x4604f2` | `124` | `124__mathSin` | 调 `0x4884c0`（sin 的 Taylor 系数指纹） |
| `0x46052d` | `125` | `125__mathCos` | 调 `0x488300`（与 sin 同构，扇区偏置多 `0x10` = +π/2） |
| `0x460568` | `126` | `126__mathTan` | 调 `0x488790`（同 `32/π` 归约 + 一条倒数 `divsd`） |
| `0x4605a3` | `127` | `127__mathAcos` | 调 `0x487af0`（asin Taylor 系数 + `sqrtsd` + π/2 与 π） |
| `0x4605de` | `128` | `128__mathAtan` | 调 `0x488010`（`abs(x)==inf → ±π/2`，`-1.0/x` 归约） |
| `0x460619` | `129` | `129__mathReduceAngle` | 调 `0x402d30`，第二参传 0；把 θ 折进 `[-π, π]`，**最多 34 次** |
| `0x460663` | `130` | `130__mathCirclePos` | 调 `0x464930`；`*x = cos(θ)*r`、`*y = sin(θ)*r` |
| `0x4606f5` | `131` | `131__mathCirclePosRand` | 两次 `0x402cb0`（**`[-1,1)`**）抽角与半径，再调 `0x464d60` |
| `0x45fc12` | `201__jmpIfPos` | `201__jmpDec` | 先 `dec` 再判 `> 0`；社区名没体现「自减」这一半，而自减才是它的主要作用 |
| `0x45fd68` | `112__add` | `112__add3` | 三操作数 `x = a + b`，与 `102__iadd`（`a += b`）不是一回事，现名易混 |
| `0x460169` | `102__iadd` | `102__add` | 同上；`i` 前缀在 thpages 体系里表示「整型」，而这里 102/103 是整/浮对，不是 in-place 标记 |
| `0x4607ce` 等 12 条 | `202__jmpEq` … `213__jmpGeqf` | 保留 | 一手逐条核过，语义全对（含 NaN 行为） |

> ⚠️ `112`–`121` 与 `102`–`111` 的社区 Ghidra label 前缀（`add` vs `iadd`）与 thpages 的
> `add` / `add3` **正好相反**，两套都在仓里出现会长期误导。建议统一到 thpages 体系
> （`102 add` / `112 add3`），因为 thanm 生成的反汇编文本用的就是那套。

### 函数改名

| 地址 | 现名 | 建议名 | 依据 |
| --- | --- | --- | --- |
| `0x488790` | `FUN_00488790` | `libm_sse2_tan` | 与 `libm_sse2_sin/cos` 同套 `32/π` 扇区归约表 + 一条取倒数 `divsd` |
| `0x487af0` | `FUN_00487af0` | `libm_sse2_acos` | 常量池 = asin Taylor 系数 `1/6, 3/40, 15/336, 105/3456, …`；3 条 `sqrtsd`；同时有 π/2 与 π |
| `0x488010` | `FUN_00488010` | `libm_sse2_atan` | `0x4882a6` 判 `abs(x)==+inf` 返回 `±π/2`；`-1.0/x` 归约；无 `sqrtsd` |
| `0x464930` | `cartesian_from_polar_464930` | `math_polar_to_xy_2ptr` | 输出写**两个**独立 `float*`（`ECX`/`EDX`），只服务 `130` |
| `0x464d60` | `cartesian_from_polar_464d60` | `math_polar_to_xy_vec2` | 输出写**同一个** `float[2]`（`ECX`），8 处调用方 |
| `0x402d30` | `math_wrap_angle_402d30` | `math_wrap_angle_max34steps` | 名字要体现「最多 34 次 ±2π 就放弃」（守卫比的是自增**前**的计数器），否则移植会写成无界 `fmod`；泄漏阈值 `abs(θ) ≈ 216.77` = 69π |

### 数据符号

| 地址 | 建议名 | 依据 |
| --- | --- | --- |
| `0x462cb8` | `ANM_VM_JUMPTABLE_LO`（`void*[53]`） | 已在 R2 批次登记；本批复核确认覆盖 opcode `-1..213` |
| `0x462d8c` | `ANM_VM_JUMPINDEX_LO`（`u8[215]`） | 同上；下标 = opcode + 1，`0x45fbe3` 的上界是 `0xd6` |
| `0x45f8fc` / `0x45f91c` | `ANM_INTVARPTR_JUMPTABLE`（`void*[8]`）/ `ANM_INTVARPTR_JUMPINDEX`（`u8[30]`） | `get_int_var_ptr` `0x45f890` 的两级表，基 10000 |
| `0x45f830` / `0x45f870` | `ANM_FLOATVARPTR_JUMPTABLE`（`void*[16]`）/ `ANM_FLOATVARPTR_JUMPINDEX`（`u8[32]`） | `get_float_var_ptr` `0x45f780` 的两级表，基 10004 |
| `0x45f710` / `0x45f758` | `ANM_INTVAR_JUMPTABLE`（`void*[18]`）/ `ANM_INTVAR_JUMPINDEX`（`u8[36]`） | `get_int_var` `0x45f610` 的两级表，基 10000 |
| `0x4a6d80` | `PRNG_STREAM_B` | ANM 侧 16 个 PRNG 调用点里的 13 个走它（`122`/`123`/`131` 与变量 `10010–10012`/`10022`），另有屏幕震动 / 雾。**⚠️ 别把「不进回放」写进符号名**——ANM 变量 `10030–10032` 走的是 Stream A，见 `03-math-jumps.md` §7.4 |
| `0x4a6d88` | `PRNG_STREAM_A` | ECL / 弹幕 / 掉落 走它；ANM 侧变量 `10030–10032` 也走它。（🟡：「回放关键」这层含义来自调用方分布，非直接标记） |
| `0x494588` | `F_PI`（`3.1415927f`） | `0x402d30` 的上界、`0x46073d` 把 `[-1,1)` 乘成角度 |
| `0x4945b8` | `F_TWO_PI`（`6.2831855f`） | `0x402d52` / `0x402d72` 的步长 |
| `0x494734` | `F_NEG_PI`（`-3.1415927f`） | `0x402d61` 的下界 |
| `0x494720` | `F_NEG_ONE`（`-1.0f`） | 46 条 handler 收尾前都会把它搬回 `XMM1`；是循环不变量，**不是语义的一部分** |
| `0x494ad0` | `D_PI_OVER_2`（`double`，`0x494ad8` = 负值） | `libm_sse2_atan` 对 `±inf` 的返回值 |

### 注释（`set_comment`）

| 地址 | 注释 |
| --- | --- |
| `0x45fc0d` | 200 jmp：instr_offset 直接 = arg0，即 **script 内字节偏移**（非指令下标）；857/857 真实跳转落在指令边界 |
| `0x45fbff` | 200 jmp：Timer::set_value(script_time, arg1) —— 时间由参数覆盖，与目标指令的 time 标签无关 |
| `0x45fc27` | 201 jmpDec：先 dec 计数器再判 >0；若 param_mask bit0 未置位，这条会改写指令流里的立即数 |
| `0x460b88` | 202–213 共用尾巴：Timer::set_value(script_time, arg3) + instr_offset = arg2；此处不查 param_mask ⇒ dest/t 不可为变量 |
| `0x460324` | 108 div：cdq+idiv，**无零除守卫** —— b==0 或 INT_MIN/-1 直接 #DE |
| `0x4600bb` | 120 mod3：同 idiv，取 EDX 余数；余数符号随被除数 |
| `0x460472` | 122 rand：唯一做了零除守卫的一条（n==0 → 0）；div 是**无符号**的 |
| `0x46075e` | 131：半径 = lo + randf_signed()*(hi-lo)，randf_signed ∈ [-1,1) ⇒ 区间不是 [lo,hi]，社区文档在此有误 |
| `0x402d50` | 角度归一循环：守卫 cmp 的是自增**前**的计数器 ⇒ 上限 34 步（两个循环共用计数器）；abs(x) > 69π≈216.77 会原样漏出 |
| `0x45f4fe` | ★ get_float_var 变量 10031/10032/10030 走 **Stream A（`0x4a6d88`，回放流）**，与紧邻的 10011/10012/10010（Stream B）是同公式镜像 |
| `0x45f8f6` | get_int_var_ptr 默认分支：把入参指针原样退回 = 指令流里的那 4 字节（自修改字节码入口） |
| `0x45f82a` | get_float_var_ptr 默认分支：同上 |
| `0x488540` | libm_sse2_sin 的扇区偏置 `0x1c7600`；cos（`0x488380`）是 `0x1c7610`，差 16 个 π/32 扇区 = π/2 |
