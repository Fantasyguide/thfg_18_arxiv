# ANM VM 骨架 — TH16 一手（结构 / 每帧驱动 / 时间 / interrupt / 终止）

> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：一手反汇编 `AnmVm::run` `0x45f980`（3223 条指令）及其上下游；分发表 / 常量表 **直接解 PE 节表读字节**
> （不经 Ghidra 渲染）；`local/th16.v1.00a/dat/` 的 54 个真实 `.anm`（1929 script / 43268 指令）做统计对账。
> **可信度**：控制流与时间语义全部 ✅（每条给得出机器指令地址）；字段图 ✅ 62 条 / 🟡 9 条；flag 位含义 ✅ 11 位 / ❓ 其余。
> **衔接**：指令头布局、`param_mask`、script 表结构见 [`01-format.md`](01-format.md)，本文不重复。
> **不含版权字节**：只写偏移、控制流与语义。

## 0. 一句话结论

TH16 的 ANM VM 是一台 **「时间闸门 + 字节码顺序执行」的协程**：每帧 `AnmVm::run` 被调一次，
它在 `0x45fba0` 的循环里**连续执行所有 `ins.time <= vm.time` 的指令**，直到撞上一条时间未到的指令
（或 `stop` / `delete` / `static`）才让出；让出后跑一遍「帧尾」（速度积分、插值器、几何），
最后**把脚本时间加 `GAME_SPEED`**（正常 = 1.0）。
`interrupt` 不是消息队列，而是**从脚本开头线性扫一遍找 `ins 5 <n>` 标签**，命中就把时间直接设成标签的 `time` 并跳过去。

---

## 1. 怎么找到主循环的

**① 发现**：`th-re-data` 给了两条 label —— `0x45fb9a end_loop_iter` / `0x45fba0 begin_loop_iter`（社区单源），
以及函数名 `AnmVm::run` `0x45f980`。
**② 推测**：`0x45fba0` 是取指 + 分发的循环头。
**③ 验证**：反汇编 `0x45fba0`–`0x45fbf5`，六条指令就是标准的解释器循环：取脚本基址 → 加 `vm+0x28`
→ 比较 `ins.time` 与 `vm+0x544` → 按 `opcode` 查两级表跳转。全部 137 条 case label 的落点都在
`0x45fbfc`–`0x462c92` 之间，且每条 case 都以 `ADD dword ptr [EBX+0x28], ins.size` + `JMP 0x45fb9a` 收尾。
**④ 结论**：`AnmVm::run` = 整台 VM（取指 / 分发 / 时间 / interrupt / 终止全在一个函数里），
`EBX` = `zAnmVm*`，`EDI` = 当前指令指针，`ESI` = 常量 `0x100`（给 `param_mask` bit8 用，见 `01-format.md` §5.2）✅。
**⑤ 证据**：`0x45fba0`（循环头）、`0x45fbc2`（时间闸门）、`0x45fbf5`（跳表）、`0x4629c8`（默认 = 跳过）、
`0x462a06`（帧尾）、`0x462c92`（尾声）。

**取脚本基址**（`0x45fba0`–`0x45fbbb`，与 `01-format.md` §8 的 `AnmLoaded` 字段图吻合）：

```
script_base(vm) = *(u8**)( *(u32*)( *(u32*)(ANM_MANAGER_PTR + 0x184f4f0 + vm.anm_slot*4) + 0x120 )
                           + vm.script_index*4 )
ins             = script_base(vm) + vm.instr_offset
```

`ANM_MANAGER_PTR` = `0x4c0f48`；`+0x184f4f0` 是 31 个 anm 槽的数组；`AnmLoaded+0x120` 是脚本指针数组。

---

## 2. ★ 一帧发生什么 —— `AnmVm::run` 的完整伪代码

下面每一行都标了对应机器指令地址。**这是给播放器照抄的那份。**

```c
// AnmVm::run(vm) -> int      th16:0x45f980   __fastcall(ECX = zAnmVm*)
// 返回 1 = 请销毁这个 VM；返回 0 = 继续存活
int AnmVm_run(vm) {
    saved_speed = GAME_SPEED;                        // 0x45f99a  全局 float @0x4a5788，默认 1.0
    if (vm.flags_hi & 0x200) GAME_SPEED = 1.0;       // 0x45f9ad  “本 VM 不吃减速”

    // ── ① 减速（子弹时间）：整棵树共用「根 VM 的 slowdown」 ────────── 0x45f9c3
    root = vm.root_vm;                                            // +0x5ac
    f = (root && !(vm.flags_hi & 0x10000)) ? get_slowdown_factor(root)   // 0x406340
                                           : vm.slowdown;               // +0x5b4
    if (f > 0.0)                                                  // 0x45f9ed
        GAME_SPEED = max(0.0, GAME_SPEED - f * GAME_SPEED);       // 0x45fa16..0x45fa2e

    // ── ② on_tick 回调，可以整帧抢走 ─────────────────────────────── 0x45fa38
    if (vm.on_tick_idx)                                           // +0x5c4
        if (ANM_ON_TICK_CALLABLES[vm.on_tick_idx](vm)) return RET(1);   // 表 @0x4919e8

    // ── ③ 两道早退闸门 ─────────────────────────────────────────────
    if (vm.instr_offset < 0)     return RET(0);      // 0x45fa55  脚本已终止（delete/static 留下的 -1）
    if (vm.flags_lo & 0x100000)  return RET(0);      // 0x45fa5f  冻结

    Timer_increment(&vm.timer2);                     // 0x45fa70  第二计时器 +0x554，无条件推进

    // ── ④ 取指前先处理挂起的 interrupt ─────────────────────────────
    if (vm.pending_interrupt != 0) goto DO_INTERRUPT;             // 0x45fa7b  +0x490
    if ((vm.flags_hi & 0xc000) == 0x4000 &&                       // 0x45fa91  “可暂停”类
        GAME_THREAD_PTR && (GAME_THREAD_PTR[0x88] & 2))
        return RET(0);                                            // 0x45fabb  暂停期不跑脚本

    // ── ⑤ 分发循环：把这一帧该跑的指令一口气跑完 ────────────────────
DISPATCH:                                                         // 0x45fba0
    for (;;) {
        ins = script_base(vm) + vm.instr_offset;                  // 0x45fba0..0x45fbbb
        if ((s16)ins.time > vm.timer.current) goto FRAME_TAIL;    // 0x45fbc2  ★ 时间闸门（有符号）
        switch ((s16)ins.opcode) {                                // 0x45fbce  两级跳表，见 §7
            /* 绝大多数 handler： …副作用…;  vm.instr_offset += ins.size;  continue; */
            case -1: case 1:  vm.flags_lo &= ~1; vm.instr_offset = -1; return RET(1); // 0x462bf0
            case 2:           vm.instr_offset = -1;                return RET(0);     // 0x462c08
            case 4:           vm.flags_lo &= ~1;   /* 落入 case 3 */                  // 0x4629dc
            case 3:           if (vm.pending_interrupt) goto DO_INTERRUPT;            // 0x4629e3
                              vm.flags_lo |= 0x4000;
                              Timer_decrement(&vm.timer); goto FRAME_TAIL;            // 0x4629fa
            case 6:           n = arg0;                                               // 0x45fc65
                              vm.timer.previous = vm.timer.current;
                              vm.timer.current_f -= (float)n;
                              vm.timer.current = (int)vm.timer.current_f;   // 截断向零
                              vm.instr_offset += ins.size; continue;
            case 7:           Timer_copy(&vm.timer, vm.saved_time);                   // 0x45fcc1
                              vm.instr_offset = vm.saved_instr_offset; continue;
            default:          vm.instr_offset += ins.size; continue;                  // 0x4629c8
        }
    }

    // ── ⑥ 帧尾：让出之后仍然每帧要做的事 ────────────────────────────
FRAME_TAIL:                                                       // 0x462a06
    if (vm.flags_hi & 0x1000000) apply_velocities(vm);            // 0x462a12 → 0x4630f0
    if (vm.flags_lo & 0x8000) {                                   // 0x462a19
        vm.entity_pos.x += GLOBAL_SCROLL_DX;                      // 0x4c1790 / 94 / 98
        vm.entity_pos.y += GLOBAL_SCROLL_DY;
        vm.entity_pos.z += GLOBAL_SCROLL_DZ;
    }
    if (vm.flags_hi & 0x2000) recompute_uv_quad(vm);              // 0x462a7d → 0x468c00 → +0x3a8..0x3c4
    step_interpolators(vm);                                       // 0x462bc1 → 0x463b30
    update_geometry_for_render_mode(vm);                          // 0x462bc8 → 0x4632f0
    if (cb = ANM_ON_WAIT_CALLABLES[vm.on_wait_idx])               // 0x462bcd  +0x5c0，表 @0x4c0f44
        if (cb(vm)) return RET(1);
    Timer_increment(&vm.timer);                                   // 0x462c11 内联  ★ 脚本时间前进
    return RET(0);

    // RET(x) ≡ { GAME_SPEED = saved_speed; return x; }           // 0x462c99 / 0x462ca4
}
```

**为什么这份伪代码可以照抄**：`0x45fba0`–`0x45fbf5` 是循环头，
`0x4629c8` 是「默认 = `instr_offset += size` 后 `JMP 0x45fb9a`」，
而 `0x45fb9a` 只是 `MOV ESI,0x100; NOP` 后落进 `0x45fba0` —— 即 `end_loop_iter` → `begin_loop_iter`
就是一次循环迭代的接缝。133 个 handler 的收尾全部走这两条路之一（唯一例外是 §6 的五个终止 / 让出分支）。

### 2.1 `apply_velocities`（`0x4630f0`）：真正的「一帧推进多少」

```c
// 只有 flags_hi & 0x1000000 时才跑
rotation.xyz += angular_velocity.xyz * GAME_SPEED;   // +0x38 += +0x44，各自 wrap 进 (-π, π]，最多 33 次
scale.y      += scale_growth.y * GAME_SPEED;         // +0x54 += +0x64
scale.x      += scale_growth.x * GAME_SPEED;         // +0x50 += +0x60
uv_scroll_pos+= uv_scroll_vel * GAME_SPEED;          // +0x78 += +0x3c8
```

**`GAME_SPEED` 是全 VM 共享的全局 float** `0x4a5788`（默认 `1.0`），
`AnmVm::run` 在进入时按本 VM 的 slowdown **临时改写它**、退出时恢复（`0x45f99a` 存 / `0x462ca4` 复）✅。
所以「速度」有两个身位：**位移/旋转类是乘 `GAME_SPEED`**，**脚本时间是加 `GAME_SPEED`**（§4）。

---

## 3. `zAnmVm` 字段图（0x5fc 字节）

命名沿用 ExpHP `th-re-data` 的 `zAnmVmPrefix`（`0x000`–`0x537`）/ `zAnmVmSuffix`（`0x538`–`0x5fb`），
**但下表每条 ✅ 都指得到一条 TH16 上读写该偏移的机器指令**；🟡 = 只有社区名 / 只有 `AnmVm::wipe` 的初始化，没有语义证据。

### 3.1 控制与取指

| 偏移 | 类型 | 名 | 含义 | 可信度 | 一手证据 |
| --- | --- | --- | --- | :---: | --- |
| `+0x00` | zTimer | `saved_time` | interrupt 前的脚本时间快照 | ✅ | 写 `0x45fb51`；读 `0x45fcc1` |
| `+0x14` | s32 | `saved_instr_offset` | interrupt 前的指令偏移快照 | ✅ | 写 `0x45fb7d`；读 `0x45fcdd` |
| `+0x18` | s32 | `layer` | 渲染层；管理器按它分桶 | ✅ | `0x406d86`；`0x46e340` |
| `+0x1c` | s32 | `anm_slot` | AnmLoaded 槽号（0–30） | ✅ | `0x45fba0` |
| `+0x20` | s32 | `sprite_index` | 当前 sprite 的全局序号 | ✅ | `0x45edd1` |
| `+0x24` | s32 | `script_index` | 脚本**序号**（不是文件里的 id） | ✅ | `0x45fbaf` |
| `+0x28` | s32 | `instr_offset` | 脚本内字节偏移；`< 0` = 已终止 | ✅ | `0x45fa55`、`0x45fb97` |
| `+0x490` | s32 | `pending_interrupt` | 挂起的 interrupt 号；`0` = 无 | ✅ | `0x45fa7b`、`0x46f0e3` |
| `+0x494` | s32 | 上次 `texSelect` 时的脚本时间 | 只写不读 | ✅ | `0x460c34` |
| `+0x530` | u32 | `flags_lo` | 见 §3.5 | ✅ | `0x45fa5f` 等 70 处 |
| `+0x534` | u32 | `flags_hi` | 见 §3.5 | ✅ | `0x45f9ad` 等 31 处 |
| `+0x538` | u32 | `id` | 句柄（`AnmManager::get_vm_with_id` 用） | 🟡 | `0x46efa0`（未逐条核） |
| `+0x53c` | u32 | `fast_id` | 池内 VM 的代号；`wipe` 保留 | ✅ | `0x46f600` |
| `+0x540` | zTimer | **`script_time`** | 时间闸门比的就是它 | ✅ | `0x45fbc2`、`0x462c11` |
| `+0x554` | zTimer | 第二计时器 | `run` 开头无条件 `+1`；VM 用不到它 | ✅ | `0x45fa70` |

### 3.2 `zTimer`（20 字节）—— VM 时间的载体

| 偏移 | 类型 | 名 | 含义 | 可信度 | 一手证据 |
| --- | --- | --- | --- | :---: | --- |
| `+0x00` | s32 | `previous` | 上一帧的 `current` | ✅ | `0x4061aa` |
| `+0x04` | s32 | `current` | 整数时间；`(int)current_f`，**截断向零** | ✅ | `0x4061dc` |
| `+0x08` | f32 | `current_f` | 浮点时间，真正累加的那个 | ✅ | `0x4061d7` |
| `+0x0c` | s32 | 速度源下标 | 索引 `0x490eb0`；**`increment` 里 ≥1 一律改回 0** | ✅ | `0x406190`、`0x40d490` |
| `+0x10` | u32 | `control` | bit0 = 已初始化 | ✅ | `0x406493` |

三个操作（全部 `__thiscall`）：

- `Timer::increment` `0x406190` —— `prev = cur`；取 `p = *(float**)0x490eb0[idx]`；
  若 `p` 非空且 `*p <= 0.99 || *p >= 1.01` 则 `cur_f += *p`、`cur = (int)cur_f`，否则 `cur += 1`、`cur_f += 1.0`。
- `Timer::set_value` `0x406490` —— `cur = v; prev = v - 1; cur_f = (float)v`（未初始化时先填哨兵 `0xfff0bdc1`）。
- `Timer::decrement` `0x40d490` —— 与 `increment` 对称，`cur_f -= 速度`。

**速度源表 `0x490eb0` 的第 0 项直接读出来是 `0x4a5788` = `GAME_SPEED`**（PE 手工解字节：`88 57 4a 00`）✅。
`increment` / `decrement` 都会把非 0 的下标强行改回 0，所以 **TH16 里所有 zTimer 都以 `GAME_SPEED` 为步长**，
表里另外两项（`0x4253f0` / `0x425410`）在这条路上是死代码 ✅。

### 3.3 变换与外观

| 偏移 | 类型 | 名 | 可信度 | 一手证据 |
| --- | --- | --- | :---: | --- |
| `+0x2c` | f32[3] | `pos` | ✅ | `0x46162f`（`400 move`）、`0x463b30` |
| `+0x38` | f32[3] | `rotation` | ✅ | `0x4613bf`、`0x4630f0` |
| `+0x44` | f32[3] | `angular_velocity` | ✅ | 写 `0x4614a2`；积分 `0x4630f0` |
| `+0x50` | f32[2] | `scale` | ✅ | `0x46101f`（`402 scale`） |
| `+0x58` | f32[2] | `scale_2`（op 434 的目标） | ✅ | `0x461094` |
| `+0x60` | f32[2] | `scale_growth` | ✅ | 写 `0x461513`；积分 `0x4630f0` |
| `+0x68` | f32[2] | `uv_scale` | ✅ | `0x461109` |
| `+0x70` | f32[2] | `sprite_size` | ✅ | `0x45ee36`（`set_sprite` 填） |
| `+0x78` | f32[2] | `uv_scroll_pos` | ✅ | 积分 `0x4630f0` |
| `+0x80` | f32[2] | `anchor_offset` | ✅ | `0x4616ef`（`421 anchor`） |
| `+0x3a8` | f32[2][4] | 当前 sprite 的 UV 四角 | ✅ | 写 `0x45ede8`、`0x462abc`；读 `0x462573` |
| `+0x3c8` | f32[2] | `uv_scroll_vel` | ✅ | 写 `0x461815`；读 `0x4630f0` |
| `+0x3d0` | D3DMATRIX | `wipe` 置单位阵；`0x410`/`0x450` 另两块只清零 | 🟡 | `0x4090f0` |
| `+0x4e0` | f32[3] | `pos_2`（`flags_lo & 0x400` 时位置写这里） | ✅ | `0x4616bc`、`0x463b30` |
| `+0x520` | D3DCOLOR | `color_1`：`+0x520`=B `+0x521`=G `+0x522`=R `+0x523`=A | ✅ | `0x4611c1`/`0x4611dc`/`0x4611fa`/`0x46117d` |
| `+0x524` | D3DCOLOR | `color_2`，同序 | ✅ | `0x461241`–`0x4612be` |
| `+0x528` | D3DCOLOR | 混合后的继承色 | 🟡 | 仅社区名 |
| `+0x5e0` | f32[3] | `entity_pos`（宿主实体坐标）；**`wipe` 保留不清** | ✅ | `0x4090f0`、`0x462a25` |

**默认值来自 `AnmVm::wipe` `0x4090f0`**：整块 `memset 0`（保留 `+0x18` `+0x53c` `+0x5e0..0x5eb`），
然后 `scale = scale_2 = uv_scale = (1,1)`、`color_1 = 0xffffffff`、`+0x3d0` 单位阵、
`flags_lo` 的低 16 位 `= 7`、`flags_hi = (flags_hi & ~0x8000) | 0x4000`、
`+0x4d4 = 1.0`、`+0x4d8 = π`、`+0x4dc = 0x10000`、两个 zTimer 归零、四个链表节点自指 ✅。

### 3.4 插值器（12 组）与脚本变量

每组插值器有一个「剩余帧」闸门 dword，非 0 才在帧尾 step。基址与闸门全部读自 `AnmVm::step_interpolators` `0x463b30` ✅：

| 目标字段 | 插值器基址 | 闸门 | step 后置的脏位 |
| --- | --- | --- | --- |
| `pos` `+0x2c`（或 `+0x4e0`） | `+0x8c` | `+0xdc` | — |
| `color_1` RGB | `+0xe4` | `+0x134` | — |
| `color_1` A | `+0x13c` | `+0x164` | — |
| `rotation` | `+0x16c` | `+0x1bc` | `flags_lo |= 4` |
| `rotation.z`（2D） | `+0x1c4` | `+0x1ec` | `flags_lo |= 4` |
| `scale` | `+0x1f4` | `+0x230` | `flags_lo |= 8` |
| `scale_2`（op 434） | `+0x238` | `+0x274` | `flags_lo |= 8` |
| `uv_scale` | `+0x27c` | `+0x2b8` | `flags_lo |= 0x10` |
| `color_2` RGB | `+0x2c0` | `+0x310` | — |
| `color_2` A | `+0x318` | `+0x340` | — |
| `uv_scroll_vel.x` | `+0x348` | `+0x370` | — |
| `uv_scroll_vel.y` | `+0x378` | `+0x3a0` | — |

脚本变量（`AnmVm::get_int_var` `0x45f610` 的 switch 逐条读出，✅）：

| 变量号 | 字段 | 备注 |
| --- | --- | --- |
| 10000–10003 | `+0x4a0` s32[4] | 整型 I0–I3 |
| 10004–10007 | `+0x4b0` f32[4] | 浮点 F0–F3 |
| 10008 / 10009 | `+0x4cc` / `+0x4d0` | s32 |
| 10022 | `rand() % *(s32*)(+0x4dc)` | 用 `REPLAY_UNSAFE_RNG` |
| 10027 / 10028 / 10029 | `+0x4d4` / `+0x4d8` / `+0x4dc` | 1.0 / π / `0x10000` |
| 10033–10035 | `+0x4c0` f32[3] | 旋转相关 |

浮点侧 `AnmVm::get_float_var` `0x45f2d0` 多出 10026/10027/10028（`UNSAFE` 随机）与
10030/10031/10032（`REPLAY_SAFE_RNG` 随机）两组 ✅。

### 3.5 树、链表、回调下标

| 偏移 | 类型 | 名 | 可信度 | 一手证据 |
| --- | --- | --- | :---: | --- |
| `+0x568` | node16 | 全局（world / ui）链表节点 | ✅ | `0x4090f0`、`0x46e340` |
| `+0x578` | node16 | 作为子节点挂在父的子链上 | ✅ | `0x46ed60` 尾部 |
| `+0x588` | node16 | **子链表头**；`+0x58c` = 第一个子 | ✅ | `0x46e660`、`0x46f0e9` |
| `+0x598` | node16 | 待销毁链节点 | ✅ | `0x46e660` |
| `+0x5a8` | zAnmVm* | 同层链表 next（渲染用） | ✅ | `0x46e340` |
| `+0x5ac` | zAnmVm* | **根 VM**（顶层祖先） | ✅ | `0x406340`、`0x46ed60` |
| `+0x5b0` | zAnmVm* | **父 VM** | ✅ | `0x46ed60`、`0x4632f0` |
| `+0x5b4` | f32 | `slowdown` | ✅ | `0x45f9e2`、`0x406368` |
| `+0x5b8` | void* | 顶点缓冲 | 🟡 | `0x4632f0` |
| `+0x5c0` | s32 | `on_wait` 回调下标；表 `0x4c0f44` | ✅ | `0x462bcd`（见 §9 的坑） |
| `+0x5c4` | s32 | `on_tick` 回调下标；表 `0x4919e8` | ✅ | `0x45fa38` |
| `+0x5d0` | s32 | **`on_interrupt`** 回调下标；表 `0x491b0c` | ✅ | `0x46f0d6`、`0x46f15b` |
| `+0x5dc` | s32 | `on_sprite_set` 回调下标；表 `0x491b1c` | ✅ | `0x460bd8` |

`node16` = `{zAnmVm* entry; node* next; node* prev; node* tail}`（`zAnmVmList`，16 字节）。

`ANM_ON_TICK_CALLABLES` `0x4919e8` 逐字读出为 `{0, 0x407330, 0x405700, 0x406690, 0x46a0b0, 0}` ✅
（PE 手工解字节）；下标 4 由 `0x469e20` 在 `302 renderMode` 选到模式 10 时写入 ✅（`0x46240b`）。

> ⚠️ **更正一条本仓旧记录**：[`README.md`](README.md) §B 把 `0x491b0c` 记作 `ANM_ON_SWITCH_FUNCS`（🟡，
> 来自做弹幕时顺带掀开的口子）。一手看 `0x46f0b0`/`0x46f130`：这张表是**带着 interrupt 号被调用的**
> （`MOV EDX, 中断号; MOV ECX, vm; CALL [0x491b0c + vm[+0x5d0]*4]`），
> 在这两个调用点它就是 **on_interrupt** ✅。旧结论下调为 🟡。

### 3.6 flag 位（只列一手坐实的）

`flags_lo` = `vm+0x530`：

| 位 | 含义 | 可信度 | 一手证据 |
| :---: | --- | :---: | --- |
| `0x1` | 可见 / 可绘制 | ✅ | `300 texSelect` 置位 `0x460ba9`；`1 delete` `0x462bf0` 与 `4 stopHide` `0x4629dc` 清位 |
| `0x2` | 由 op 316 置、op 317 清 | 🟡 | `0x461370` / `0x46138b`（语义未定） |
| `0x4` | 旋转脏位 | ✅ | `0x4613f4`、`0x4630f0`、`0x463b30` |
| `0x8` | 缩放脏位 | ✅ | `0x461036`、`0x463b30` |
| `0x10` | UV 缩放脏位 | ✅ | `0x461120`、`0x463b30` |
| `0x400` | 位置改写 `+0x4e0` 而非 `+0x2c` | ✅ | `0x46159c`、`0x463b30` |
| `0x4000` | 已 `stop` | ✅ | 置 `0x4629f0`；interrupt 命中时清 `0x45fb21` |
| `0x8000` | 帧尾把全局滚动量加进 `entity_pos` | ✅ | `0x462a19` |
| `0x100000` | 冻结：`run` 立刻返回 0 | ✅ | `0x45fa5f` |
| bit 25–29 | 渲染模式（5 位） | ✅ | `302 renderMode` 写 `0x4623df`；`0x4632f0` 读 `>>0x19 & 0x1f` |

`flags_hi` = `vm+0x534`：

| 位 | 含义 | 可信度 | 一手证据 |
| :---: | --- | :---: | --- |
| `0x20` / `0x40` | 请求销毁 / 已销毁（管理器用） | ✅ | `0x46e340`、`0x46e660` |
| `0x200` | 本 VM 强制 `GAME_SPEED = 1.0` | ✅ | `0x45f9ad` |
| `0x400` | 由管理器托管（`create_managed_*` 置） | ✅ | `0x46eea0`、`0x46ed60` |
| `0x2000` | 帧尾重算 sprite 的 UV 四角 | ✅ | `0x462a6d` |
| `0x4000` / `0x8000` | 暂停类：仅 `0x4000` 时受游戏暂停影响 | ✅ | `0x45fa91`；`wipe` 默认置 `0x4000` |
| `0x10000` | 不向父/根继承（减速、缩放） | ✅ | `0x45f9cd`、`0x406350`、`0x4632f0` |
| `0x40000` / `0x80000` | 由 layer 区间派生（3–19 / 20–23） | ✅ | `0x406d93`、`0x406db5` |
| `0x1000000` | 启用每帧速度积分 | ✅ | `0x462a06`、`0x461490` |
| `0x2000000` | 创建时从父继承的一位 | 🟡 | `0x46eec6`、`0x46ed9a`（语义未定） |

---

## 4. 时间语义：`time` 标签与指令执行的关系

**① 发现**：`0x45fbbe`–`0x45fbc8` 三条指令 `MOVSX EAX, word[EDI+4]` / `CMP EAX,[EBX+0x544]` / `JG 帧尾`
（PE 字节 `0f bf 47 04  3b 83 44 05 00 00  0f 8f …`，手工解码复核过）。
**② 推测**：`ins.time` 是「到点才执行」的门槛。
**③ 验证**：闸门用的是 `JG`（有符号大于）→ **条件是 `ins.time <= vm.time` 就执行**；
不满足就整条循环退出去跑帧尾。循环内没有任何计数上限。
**④ 结论**（✅ TH16）：

- **同一个 `time` 标签下的所有指令，在同一帧内一次跑完**——因为闸门是 `<=`，
  而每条 handler 只把 `instr_offset` 往前推，不推时间。
- **时间没到的指令不是「跳过」而是「挡住」**：VM 停在它上面，下一帧重新比一次。
- **`time` 是有符号的**：真实数据里 43268 条指令有 **3540 条 `time < 0`**（模板预跑段，见 §8）。
- **脚本时间每帧 `+= GAME_SPEED`**（`0x462c11` 内联的 `Timer::increment`）。
  正常 `GAME_SPEED == 1.0` 走整数快路径 `cur += 1`；被减速时走浮点路径 `cur_f += GAME_SPEED; cur = (int)cur_f`。
- **`6 wait n` 不是「设定目标」而是「把时间往回拨 n」**：`cur_f -= n`（`0x45fc97`）。
  于是后面那些 `time` 更大的指令自然被闸门挡住 n 帧。这跟「设一个 wake-up 时刻」等价但实现相反。
- **没有任何防死循环保护**：`200 jmp` 跳回同一 `time` 的指令会让 `0x45fba0` 死转。

**⑤ 证据**：`0x45fbbe`、`0x45fbc2`、`0x45fbc8`、`0x462c11`–`0x462c8a`、`0x45fc65`–`0x45fcbc`、
`0x406190`、`0x490eb0`（速度源表）、`0x4a5788`（`GAME_SPEED`）。

---

## 5. interrupt 机制

**触发**：`AnmManager::interrupt_tree(anm_id, n)` `0x46f0b0` —— `get_vm_with_id` `0x46efa0` 找到 VM，
先调 `on_interrupt` 回调（`0x491b0c[vm+0x5d0]`），再 `vm.pending_interrupt = n`（`0x46f0e3`），
然后**对直接子节点（`vm+0x58c` 链）各做一遍同样的事**。
`AnmManager::interrupt_tree_and_run` `0x46f130` 多做一步：每设完一个就立刻 `AnmVm::run` 一次。

> ⚠️ **名字里的 "tree" 名不副实**：`0x46f0e9`–`0x46f118` 只遍历一层子节点，**不递归到孙节点** ✅。

**服务时点**：`0x45fa7b` —— 在计时器推进之后、取指之前；另外 `3 stop` 的 handler 也再查一次（`0x4629e3`）。

**查找与跳转**（`0x45fac6`–`0x45fb97`，全部一手）：

```c
DO_INTERRUPT:
    want = vm.pending_interrupt;
    ins  = script_base(vm);          // ★ 从脚本开头扫，不是从当前位置        0x45fae8
    off = 0; fallback = NULL; fallback_off = 0;
    while (!(ins.opcode == 5 && ins.args[0] == want) && ins.opcode != -1) {   // 0x45faf1
        if (ins.opcode == 5 && ins.args[0] == -1) {                           // 0x45fb05
            fallback = ins; fallback_off = off;      // 记住 default 标签
        }
        off += ins.size;  ins += ins.size;                                    // 0x45fb17
    }
    vm.flags_lo &= ~0x4000;  vm.pending_interrupt = 0;                        // 0x45fb21
    if (ins.opcode != 5) {                      // 扫到 -1 都没命中           0x45fb35
        if (!fallback) { Timer_decrement(&vm.timer); goto FRAME_TAIL; }       // 0x4629fa
        ins = fallback; off = fallback_off;
    }
    vm.saved_time         = vm.timer;           // Timer::copy(vm+0x00, …)    0x45fb6b
    vm.saved_instr_offset = vm.instr_offset;    // +0x14                      0x45fb7d
    off += ins.size;                            // 跳过 case 指令本身          0x45fb84
    Timer_set_value(&vm.timer, (s16)ins.time);  // ★ 时间直接设成标签的 time   0x45fb8b
    vm.flags_lo |= 1;                           // 重新可见                    0x45fb90
    vm.instr_offset = off;
    goto DISPATCH;
```

**结论**（✅ TH16）：

1. **标签就是 `ins 5 <n>`**，本身是 no-op（opcode 5 在跳表里落到 default `0x4629c8`，执行时只推进指针）。
   真实数据里 opcode 5 出现 **4009 次**，是最常见的指令之一。
2. **查找是线性全扫**，从脚本开头到 `opcode == -1` 为止；`ins 5 -1` 是 **default 分支**，
   只有在没有精确匹配时才用。命中精确匹配就立刻停，所以**同号标签只有第一个生效**。
3. **触发后时间被改写**：`Timer::set_value(标签的 time)` —— 不是清零、不是保持，
   而是**跳到那个标签所在的时间刻度**，`previous = time - 1`、`current_f = (float)time`。
4. **`7 caseReturn` 是配套的返回**：`Timer::copy(vm.timer, saved_time)` + `instr_offset = saved_instr_offset`
   （`0x45fcc1`–`0x45fce3`）。快照只有一层，**interrupt 不可重入嵌套** ✅。
5. `pending_interrupt == 0` 表示「无」，所以 **interrupt 号 0 无法投递** ✅（`0x45fa7b` 的 `CMP …,0x0`）。

---

## 6. 终止 / 让出语义（五个分支）

跳表一手解码（§7）给出的落点，与真实数据里的出现次数：

| opcode | thpages 名 | case 地址 | `instr_offset` | 返回值 | 帧尾跑吗 | 真实数据出现次数 |
| :---: | --- | --- | --- | :---: | :---: | :---: |
| `-1` | （脚本末尾） | `0x462bf0` | ← `-1` | **1** | 否 | 1929（每个 script 一条） |
| 1 | `delete` | `0x462bf0`（同一份代码） | ← `-1` | **1** | 否 | 1558 |
| 2 | `static` | `0x462c08` | ← `-1` | 0 | 否 | 27 |
| 3 | `stop` | `0x4629e3` | 不动 | 0 | **是** | 2398 |
| 4 | `stopHide` | `0x4629dc` | 不动 | 0 | **是** | 56 |

### 6.1 逐条说明

**`opcode -1`（跑到脚本末尾）与 `opcode 1 delete` 是同一段代码** ✅ ——
把 `0x462d8c` 的 215 字节索引表逐字解出来，下标 0（= opcode `-1`）与下标 2（= opcode 1）**指向同一个跳表槽 0**，
槽 0 = `0x462bf0`。这段做三件事：`flags_lo &= ~1`（隐藏）、`instr_offset = -1`、`EAX = 1` 返回。
**返回 1 会被管理器当作「销毁我」**（`0x46e340`：非 0 → `sub_46e660_recursive` `0x46e660` 把整棵子树
打上 `flags_hi |= 0x40`，再 `destroy_possibly_managed_vm`）✅。
→ **父 VM 被 delete，子树一并销毁** ✅。

**「跑到末尾一定会 delete」是可以证死的**：终止标记那条指令的 `time` 字段，
在 54 个文件 1929 个 script 里 **全部 = 0**（`size` 也全 = 0，与 [`01-format.md`](01-format.md) §5.1 一致）。
`0 <= vm.time` 永真（`vm.time` 从 0 起且只增），所以**任何走到末尾的脚本都会执行这条 `-1` 并把自己删掉** ✅。

**`2 static`**：只写 `instr_offset = -1`，返回 0。VM 留在链表里继续被渲染，
但从下一帧起 `0x45fa55` 的早退闸门会命中 —— **连计时器、插值器、速度积分都不再跑**，彻底冻结 ✅。

**`3 stop`**：`flags_lo |= 0x4000`，然后 `Timer::decrement(&vm.timer)`（`0x4629fa` → `0x40d490`）
再走完整个帧尾，帧尾末尾又 `Timer::increment`。**一减一加净变化 0 ⇒ 脚本时间冻结**，
而 `instr_offset` 不动 ⇒ 下一帧再次执行这条 `stop`，形成稳定的自锁 ✅。
但**插值器、速度积分、UV 重算、回调照跑**，VM 依旧可见。
`stop` 还会在进入自锁前再查一次 `pending_interrupt`（`0x4629e3`）—— 所以被 `stop` 的 VM 可以被 interrupt 唤醒。

**`4 stopHide`**：先 `flags_lo &= ~1`（隐藏），再落进 `3 stop` 的代码 ✅（`0x4629dc` 与 `0x4629e3` 相邻直落）。

**`0 nop`**：在 `0x462d8c` 的索引表里指向跳表槽 52 = `0x4629c8` = **默认分支 = 只推进指针** ✅。
真实数据里出现 221 次。

### 6.2 与 thpages / 与本任务 brief 的出入

本任务 brief 里写的是「`0 delete` / `15 static`」。**一手不支持这个说法**：
TH16 的 opcode 0 是 no-op，opcode 15 根本没有 handler（落默认分支），
`delete` = 1、`static` = 2。thpages 的 `v8.anmm`（`local/vendor/thpages/static/mapfile/v8.anmm`，社区单源）
写的是 `0 nop / 1 delete / 2 static / 3 stop / 4 stopHide / 5 interruptLabel / 6 wait / 7 caseReturn`，
**与一手完全一致** ✅。brief 那句按错处理。

---

## 7. 指令分发：三段式，共 134 个已实现 opcode

`0x45fbce`–`0x45fbf5` 与 `0x460c46`–`0x460c5d`，两级「字节索引表 + 跳表」：

| 范围 | 判据 | 索引表（字节） | 跳表（dword） | 目标数 |
| --- | --- | --- | --- | :---: |
| `op <= 299` | `INC EAX; CMP EAX,0xd6; JA 默认` | `0x462d8c`，215 项 | `0x462cb8`，53 项 | 53 |
| `op == 300` | `JZ 0x460ba9` 直接内联 | — | — | 1 |
| `301 <= op <= 613` | `SUB EAX,0x12d; CMP EAX,0x138; JA 默认` | `0x462fa8`，313 项 | `0x462e64`，81 项 | 81 |
| 其余 | 落 `0x4629c8` | — | — | 默认 = 跳过 |

**表是直接从 PE 里解字节读出来的**（不经 Ghidra），据此列出 **134 个真正有 handler 的 opcode** ✅：

```
-1, 1..4, 6, 7, 100..131, 200..213, 300..317, 400..438, 500..509, 600..613
```

洞：`0`、`5`、`132..199`、`214..299`、`318..399`、`439..499`、`510..599`、`614+` 全部走默认分支（= `nop`）。
**opcode 语义归下一份文档**，本文只定死分发骨架。

---

## 8. 实例化：模板 → 活的 VM

- **模板**：`AnmLoaded+0x10c` 是 `n_scripts` 个 `0x5fc` 字节的 VM 模板，
  由 `0x46d1c0` 在解析完最后一个 entry 时逐个 `AnmVm::wipe` + `AnmVm::run` 预跑生成
  （见 [`01-format.md`](01-format.md) §8）。所以模板里**已经执行完 `time <= 0` 的那一段**。
- **克隆**：`AnmLoaded::copy_vm` `0x407b20` —— 先 `AnmVm::wipe_suffix` `0x407a50`，
  再把模板的 **前 `0x14e` dword = `0x538` 字节（只有 prefix）** 整块拷进新 VM，
  最后把 `+0x540` 与 `+0x554` 两个 zTimer 都重置成 `set_value(0)` ✅。
  → **prefix（变换/颜色/指令指针）继承模板，suffix（时间/链表/回调/父子）重来。**
- **分配**：`AnmManager::allocate_vm` `0x46f600` —— 从 `ANM_MANAGER + 0x184f4e0` 的空闲链摘一个
  `zAnmFastVm`（`0x614` 字节 = `zAnmVm` + 空闲链节点 + `is_alive` + `fast_id`），空了才 `operator_new(0x5fc)` ✅。
- **挂上去**：`AnmLoaded::create_managed_root` `0x46eea0` / `create_managed_child` `0x46ed60` ——
  置 `flags_hi |= 0x400`、继承 layer、（child 还要）写 `+0x5b0` 父指针与 `+0x5ac` 根指针，
  **然后立刻 `AnmVm::run` 跑一次**（`0x46eefa` / `0x46edc9`），最后才插进 world / ui 链表 ✅。
  → **播放器要复刻：创建当帧就 tick 一次。**

**谁 tick 它**：两条路，都一手确认 ✅。

1. **托管 VM**：`AnmManager::on_tick_21__tick_world` `0x46e340` 与 `on_tick_09__tick_ui` `0x46e490`
   遍历全局链表（`mgr+0xdc` / `mgr+0xe4`），按 `flags_hi & 0x60` 分流：
   `0x20` → 销毁；`0` → `AnmVm::run`，返回 0 就按 `layer` 挂进渲染桶，返回非 0 就走销毁。
2. **自管 VM**：`AnmVm::run` 全 exe 有 **64 个调用点** —— 弹幕（`0x411e70`）、激光（`0x431fe0` 等）、
   自机（`0x442560`）、道具、菜单各自持有 VM 并手动每帧调。这些 VM 不在管理器链表里，返回值由宿主处置。

---

## 9. 写 JS 播放器时会踩的坑

| 坑 | 说明 |
| --- | --- |
| 时间截断向零 | `cur = (int)cur_f` 是 `CVTTSS2SI`（截零），不是 floor。`6 wait` 能把 `cur_f` 弄成负数，此时两者不同。用 `Math.trunc` |
| 主循环无上限 | `0x45fba0` 没有任何迭代计数保护；自跳脚本会真的卡死。移植时必须自己加护栏 |
| 终止标记 `size == 0` | `-1` 那条的 `size` 恒 0 但实占 8 字节。VM 永远不会越过它，解析器必须当 8 字节（`01-format.md` §5.1） |
| 「跑到末尾」= delete | 不是「停住」。见 §6.1 |
| interrupt 从头扫 | 不是从当前位置扫，也不建索引。同号标签只有第一个生效 |
| `GAME_SPEED` 是全局的 | `AnmVm::run` 临时改写再恢复；父 VM 的 slowdown 通过 `+0x5ac` 根指针影响整棵树 |
| 创建即 tick | `create_managed_*` 在插链表前就跑了一次 `run`；模板本身也已跑完 `time <= 0` 段 |
| `+0x5c0` 的表基址有雷 | `ANM_ON_WAIT_CALLABLES` 基址 `0x4c0f44` 与 `ANM_MANAGER_PTR` `0x4c0f48` 只差一项；见 §10 |

---

## 10. 还没搞清楚的

| 问题 | 状态 |
| --- | :---: |
| `on_wait` 表基址 `0x4c0f44` 的下标 1 正好压在 `ANM_MANAGER_PTR` `0x4c0f48` 上；全 exe 对 `+0x5c0` 只有「写 0」和「从别的 VM 拷贝」，从没写过非 0，所以这条路在 TH16 是死的 | ❓ |
| `GAME_THREAD_PTR` `0x4a6dd4` 的 `+0x88` bit1：全 exe **只有 `0x45fab4` 一处测试**，写它的是 `0x42d7b0` 的整字 `MOV`，语义未坐实（推测「游戏逻辑暂停」） | 🟡 |
| `flags_lo` bit1（op 316 置 / 317 清）、`flags_hi` `0x2000000`（创建时继承）的含义 | ❓ |
| `flags_hi` `0x8000` 从没被任何代码置位（只有 `wipe` 清它），`0xc000` 组合里只用到 `0x4000` 与 `0`，`0x8000`/`0xc000` 是否可达 | ❓ |
| `+0x3d0` / `+0x410` / `+0x450` 三块 `D3DMATRIX` 的分工（`wipe` 只把第一块置单位阵） | 🟡 |
| 渲染模式 5 位（`flags_lo` bit 25–29）的取值枚举与 `0x4632f0` 里各分支的几何 | — 归渲染文档 |
| 134 个 opcode 各自的语义 | — 归下一份文档 |

---

## 关联

- 文件格式（指令头 / script 表 / `param_mask`）：[`01-format.md`](01-format.md)
- 子系统总览：[`../OVERVIEW.md`](../OVERVIEW.md)
- 本作入口：[`README.md`](README.md)
- 想要的 Ghidra 重命名（待 T10 统一落库）：[`naming-batch.md`](naming-batch.md)
- 纪律：[`../../../METHOD.md`](../../../METHOD.md)、[`../../../DOCSTYLE.md`](../../../DOCSTYLE.md)
