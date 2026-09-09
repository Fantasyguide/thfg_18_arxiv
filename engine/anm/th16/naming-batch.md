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
