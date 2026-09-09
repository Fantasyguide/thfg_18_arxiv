# 逆向工程发现 04:TH16 自机射击运行时架构(子弹行为控制链)
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> 方法:Ghidra(ghidra-re MCP)+ objdump(裸码)反汇编**用户自有 th16.exe**。日期 2026-06-08。
> 承接 `03-*.md`(func_* 跳转表):03 解决"索引→哪个函数",本篇解决"这些函数/变量在引擎里**怎么被
> 调用、住在哪**"。分级 ✅高 / 🟡中。**仅 TH16。**

## 0. 一图流:每帧子弹行为控制链 ✅

```
游戏主循环
  └─ player 任务入口 0x443720 (蹦床) ─► FUN_00442560  player 每帧更新(状态机 @+0x165a8)
        ├─ case1 存活 ─► FUN_00441cf0  输入/移动(读 DAT_004a52c8 按键→9 向 +0x2c780;移速取自 SHT)
        │                   ├─ FUN_00442380(player, player+0x660, 4)   ← shot 组 A 每帧更新
        │                   └─ FUN_00442380(player, player+0x9f0, 8)   ← shot 组 B(option) 每帧更新
        │                         └─ 遍历槽(stride 0xe4):若 slot.func_on_tick@+0xdc≠0 则 (*它)()  ★每帧行为派发
        └─ 渲染任务 0x443730 ─► 同步到图形管理器 DAT_004c0f48
碰撞时(异步回调) ─► FUN_00445d40  this+0x88→槽,槽+0xac 打包索引反推 shooter → 调 shooter+0x34   ★命中派发
```

## 1. 三个 func_* 派发点 ✅

| 回调 | 何时/谁派发 | 怎么取到函数 |
| --- | --- | --- |
| `func_on_init` | 开火创建 shot 时 | 解析时存入 shooter+0x28;spawn 时调 |
| **`func_on_tick`** | **每帧**,`FUN_00442380` `0x442380` | **已拷入运行时槽 slot+0xdc**,非空则 `(*slot+0xdc)()` |
| **`func_on_hit`** | 命中敌人(碰撞回调 `playershot_hit_dispatch` `0x445d40`,注册式无直接 xref) | `this+0x88`→选槽,槽 **+0xac** 打包索引现场反推 shooter,调 **shooter+0x34** |
| `_old_on_draw` | (TH16 不调,表全 null) | — |

> 注意两种取法并存:**tick 把解析后的指针拷进运行时槽(slot+0xdc)**;**hit 不拷,靠 slot+0xac 回链 SHT
> 现场反推**(`this+0x88`→槽→槽+0xac)。slot+0xac 编码:低字节=shooterset 内 shooter 序号,>>8=set/option,
> **掩码 0xf0000**=主/副 .sht(审计修正:非 bit 0x10000)。

## 2. 对象模型 ✅/🟡

### player 对象(全局 `DAT_004a6ef8`,`operator_new(0x2c828)`@`FUN_00441c60`)
- `+0x610/+0x614/+0x618` 自机 x/y/z(float);`+0x61c/+0x620` 定点坐标;`+0x2c780` 移动方向(0..8);
- `+0x2c788 / +0x2c78c` **主 / 副 .sht 解析后基址**(=各 on_tick 里 `DAT_004a6ef8+0x2c788` 的来源);
- `+0x16650/54/58/5c` 移速(focus/unfocus,源自 SHT move_*);`+0x165a8` 主状态机;
- **shot/option 槽**:两组 `+0x660`(×4)、`+0x9f0`(×8),**stride 0xe4**;槽内 `+0x00`=active、`+0xdc`=
  func_on_tick 指针、`+0xac`=shooter 回链打包索引、`+0x48/4c`=坐标、`+0x60`=角、`+0xa0`=蓄力(激光长);
- **子弹/效果池** `+0xd080`,**stride 0x94**,×256(`FUN_00442560` 里那个 256 次循环更新它);
- `+0x1110` 起另一组 256×0xc0 槽(`player_shot_init` 末尾初始化)。

### 子系统管理器(全局)
| 全局 | 作用 |
| --- | --- |
| `DAT_004a6ef8` | **player 对象**(内含上述全部数组/SHT) |
| `DAT_004a6dc0` | **敌人管理器**(`+0x180` 敌人链表头,`+0x18c` 计数,`+0x15c` 遍历游标) |
| `DAT_004c0f48` | **图形/效果对象管理器**;`FUN_0046efa0(mgr, handle)` 句柄→对象,`FUN_0046f0b0(handle,…)` 释放 |
| `DAT_004a6db8` | 渲染条目分配源(`FUN_00406380(...,&out,...)` 分配) |
| `DAT_004a52c8` | 当前帧**按键位**(bit0x10/20=方向,0x40=低速/focus 等) |

## 3. 引擎跳转表清单(目前已知 5 张)✅地址

| 表址 | 用途 | 索引来源 |
| --- | --- | --- |
| `0x4919c0` / `0x4919a0` / `0x4a6f04` / `0x491980` | 自机 func_on_init/tick/draw/hit(见 03) | SHT shooter 字段 |
| **`0x491b0c`** | ~~效果/敌弹行为派发~~ → ANM VM 的事件回调表(🟡 `on_switch` → ✅ `on_interrupt`,见下) | ANM VM `+0x5d0` |

> ❌ **更正(2026-06-09,th-re-data + dump + 反编译,见 `../engine/ecl/th16/03-thredata-crosscheck.md` §4c)**:`0x491b0c` **不是**"敌弹/效果子系统入口",而是 **ANM(动画)VM 的 on-switch 回调表**:`void*[4]`={null,`0x407900`,`0x405f20`,`0x406920`}=`AnmVm::on_switch__1/2/3`(操作 AnmVm 顶点数组 `+0x5b8`、设渲染/混合模式)。属一整族 ANM 事件表(`0x491b0c`..`0x491b58`:switch/sprite_set/draw/copy/delete)。`FUN_0044f810`/`FUN_0044c8c0` 是 ANM 管理器(`DAT_004c0f48`)的每帧渲染更新器,不是敌弹派发器。**敌弹行为请走 `../engine/bullet/th16/`(弹运动 VM `0x413860`),别再顺这表挖敌弹。** 归口 `../engine/anm/th16/`。

> ⚠️ **再更正(2026-09-09)**:上面把该表叫 `on-switch` 这一步**下调为 🟡**。
> 全 exe 32 个引用点的形态是 `ECX = vm; EDX = <中断号>; CALL [0x491b0c + vm[+0x5d0]*4]`,
> 随后 `vm[+0x490] = 同一个中断号`(`vm+0x490` = pending_interrupt),**即 on_interrupt** ✅。
> 证据与 VM 侧语义见 `engine/anm/th16/02-vm-core.md` §3.5 / §5。
> **本节关于"不是敌弹派发器"的结论不受影响**,只是那张表的名字换了。

## 4. 常用辅助函数(已实证,复用)✅/🟡
`FUN_0046efa0(mgr,handle)` 句柄→对象 | `FUN_0046f0b0(handle,mode)` 释放/改状态 | `FUN_00406380` 分配
渲染对象 | `FUN_0040e5c0` 注册渲染条目 | `FUN_00442380` shot 每帧派发 | `FUN_00445d40` 命中派发 |
`FUN_00445e20` 共享发射收尾 | `find_nearest_enemy` `0x425240` | `atan2` `0x487aaa`(见 03 词汇表)。

## 5. 下一步
1. ~~顺 `0x491b0c` 表挖敌弹/效果行为~~ **作废**:`0x491b0c` = ANM on-switch 表(见上更正);`FUN_0044f810`/`FUN_0044c8c0` 是 ANM 渲染更新器。敌弹行为已在 `../engine/bullet/th16/`,ANM 在 `../engine/anm/th16/`。
2. 补 `func_on_init` 的 spawn 派发点(开火读 fire_rate/start_delay 计时 → 建槽 → 调 init)。
3. 把 slot(0xe4)与池(0x94)结构字段补全,回填 `docs/` 作 IDE 运行时语义参考。
4. 用同套锚点(player_shot_init→FUN_00442560→FUN_00442380→func 表)在 TH18/19 验证架构是否通用。
