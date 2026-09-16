# BP30-IMPLEMENTATION —— 两处新断点（B-1 伤害落笔点 / A-2 死亡结果闩锁）

> **版本**：TH18 v1.00a（`th18.exe`，md5 `9969cac756098c1da05a81de45437a70`，imagebase `0x400000`）。本文裸地址默认属该版本；引别作写 `th18:0x…`。
> **日期**：2026-09-13。**用户口径**（唯一权威）：系统卡与原生卡冲突**按语义叠加** —— 「你说伤害 ×2、我说伤害 ×2，就是 ×4」。
> **来源**：[`CARD-NATIVE-CONFLICT-AUDIT.md`](CARD-NATIVE-CONFLICT-AUDIT.md) 的 **§B-1**（伤害乘法被原生卡重算抹掉）与 **§A-2**（死亡结果闩锁）两条「根治」，用户已批准加断点（导出数因此从 **29 → 31**）。
> **状态**：静态全绿（`sites.py check` 30 个 `BP_*` 双向一致、DLL 31 exports、`dllx87=0`、两站点逐字节对账），**未实跑**（实跑判据见 §4）。
> **两条不越界**：① exe 里的原生代码**一行不动**（只加我们自己的断点）；② **别人的卡（id 58–80）行为一行不动** —— 共享件改动按纪律登记在 [`AUDIT.md`](AUDIT.md) §BE。

---

## 0. 一页总表

| # | 站点 | 长度 | exe 里的字节 | 干什么 | 新导出 | 一句话语义 |
| --- | --- | --- | --- | --- | --- | --- |
| **B-1** | `th18:0x45e7ff` | 9 | `ff 77 08 8b 0d 5c f6 51 00` | 卡链遍历出口：把我们的伤害倍率乘在**别人算完的值**上 | `BP_ce_bullet_damage_final` @30 | **唯一**落笔点 ⇒ 乘法叠加、与卡链顺序无关 |
| **A-2** | `th18:0x45c127` | 6 | `85 ff 8b 7c 24 14` | 五家救援者跑完后的**唯一判定**：把「这次死亡最终有没有被取消」写进闩锁 | `BP_ce_death_result` @31 | 引擎那次判定的**只读复述**（`-1` 未知 / `0` 未取消 / `1` 已取消） |

**为什么正好是这两个地址**：两处都是**唯一**能同时看见「五家都跑完」与「我们要的东西还在寄存器里」的位置 —— 一手论证见 §1.2 / §2.2，纪律体检见 §3。

---

## 1. B-1：伤害倍率的落笔点搬到卡链遍历之后

### 1.1 一手字节与周围控制流（逐条）

站点 = `PlayerBullet__create` `th18:0x45e320` 的**卡链遍历出口**。反汇编（Capstone 线性、地址逐条核过）：

```text
0045e7dc  a198f24c00      mov eax,[0x4cf298]        ; eax = ABILITY_MGR（0x4cf298 = zAbilityManager 指针）
0045e7e1  897708          mov [edi+8],esi           ; bullet+8 = 刚插进世界列表的 VM id（edi = bullet）
0045e7e4  8b701c          mov esi,[eax+0x1c]        ; esi = 卡链头（mgr+0x1c）
0045e7e7  85f6            test esi,esi
0045e7e9  7414            je 0x45e7ff               ; ★ 没卡 ⇒ 直接落到我们的站点
0045e7eb  0f1f440000      nop dword [eax+eax]
0045e7f0  8b0e            mov ecx,[esi]             ; ecx = 卡对象
0045e7f2  57              push edi                  ; ★ 参数 = bullet
0045e7f3  8b01            mov eax,[ecx]
0045e7f5  ff5028          call [eax+0x28]           ; ★ 卡虚表 +0x28 = on_bullet_created（原生 25/54 也在这条链上）
0045e7f8  8b7604          mov esi,[esi+4]           ; 下一个节点
0045e7fb  85f6            test esi,esi
0045e7fd  75f1            jne 0x45e7f0
0045e7ff  ff7708          push [edi+8]              ; ★★ 我们的站点（9 字节从这里起）
0045e802  8b0d5cf65100    mov ecx,[0x51f65c]        ;    ANM_MANAGER_PTR
0045e808  e833a30200      call 0x488b40             ;    AnmManager__get_vm_with_id
...
0045e837  ffb79c000000    push [edi+0x9c]           ; ★ 伤害在这里被交给伤害源（0x45e861 call 0x45dfa0）
```

**站点纪律四问**（脚本：`local/_scratch/scan_bp_window.py 0x45e7ff:9`，只读、不占 Ghidra 锁）：

| 问 | 答 |
| --- | --- |
| ① 长度 ≥ 5（thcrap 下限，上限 27）| ✅ 9 字节：`push [edi+8]`（3）+ `mov ecx,[0x51f65c]`（6）|
| ② 窗内相对寻址 | ✅ 无（一条 `push imm` + 一条绝对寻址 `mov`）|
| ③ 窗内跳转靶（严格落在 (va, va+len)）| ✅ 无。`0x45e7e9` 的 `je` 落在**站点本身** `0x45e7ff` —— 落在站点 = 走断点（handler → cave → 原指令 → 回站点+9），合法 |
| ④ 窗口与指令边界对齐 | ✅ 两条完整指令，不切断任何一条 |

**EDI = bullet 的两处独立印证**：`0x45e7e1 mov [edi+8],esi`（把 VM id 写进 bullet）与 `0x45e837 push [edi+0x9c]`（把伤害交给伤害源）。
ESI/EDX 此刻已被引擎换用（`esi` = 卡链节点游标）⇒ handler **只读 EDI**。

### 1.2 为什么必须搬到遍历之后（这就是 B-1 的病根）

我们的伤害层（`AUDIT` §AA）原来是**乘在不重算的值上**：落笔在 `+0x28` 的桩里，而原生两张装备卡
**与我们同槽、且是从 shooter 基值重算**派：

| 原生卡 | 函数 | 槽 | 干了什么 |
| --- | --- | --- | --- |
| **54 MUKADE（Momoyo）** | `th18:0x411010` | `+0x28` | `bullet+0x9c = min(1.8, 1 + 击杀/N) × shooter+0x02`（重算）|
| **25 弱肉強食（Saki）** | `th18:0x40c310` | `+0x28` | `shot type7` 时 `bullet+0x9c = shooter+0x02 × 1.4`（重算）|

⇒ 卡链顺序（= 获得/存档顺序，玩家不可控）决定谁被抹掉：它们在**前** ⇒ `×1.25 × 1.4 = ×1.75` ✅；
在**后** ⇒ 它们从基值重算，我们那条 ×1.25 **整条消失**（不报错、不崩）。

搬到遍历出口之后：**五家全跑完才轮到我方**，我们永远乘在**别人的最终值**上 ⇒ 与链序无关的乘法叠加。
数值算例（`tests/test_sdk_core.c` 的 `test_native_conflict_fixes` 钉住）：Saki 重算出的 **140** × 我们 1250‰ = **175**（= ×1.75）。

### 1.3 「不重复施加」怎么保证 —— 选**取代**而不是「标记去重」

同一帧我们的 `+0x28` 桩原来也会乘一次 ⇒ 如果只是「再落一次」，就变成 ×1.25² 。
两种做法里本实现选**取代**：

| 做法 | 怎么保证只乘一次 | 代价 |
| --- | --- | --- |
| ★ **取代**（本实现）| 落笔点**唯一**：`CE_CARD` 的 `+0x28` 桩不再缩放（`sdk.h` 的那一行已删），全 exe 只有本站点写 `bullet+0x9c` | 站点没挂上会直接 FAIL（见下），不会「静默少乘一次」|
| 标记去重 | 记录 (bullet, 写进去的值)，站点处比对：相等 ⇒ 跳过、不等 ⇒ 再乘 | 需要一份跨调用状态 ⇒ 多一类失效模式（过期 / 漏清 / 链内嵌套创建弹时被覆盖）|

**「只有这一个落笔点」不是推断，是普查**：全 `.text` 扫 `call [reg+0x28]` 共 **8 处**，
只有 `0x45e7f5` 在**卡链**上（它前面就是 `mov eax,[0x4cf298]` + `mov esi,[eax+0x1c]`）；
其余 7 处（`0x435789` / `0x43e30f` / `0x448ffe` / `0x4490b3` / `0x449be9` / `0x45113a` / `0x474064`）
分别是别的对象类型（敌人弹 / 别的管理器）的虚表调用 —— 逐处读过指令上下文（`scan_bp_window.py` 的普查段）。
⇒ 卡链的 `+0x28` 广播**只有这一处**，所以搬家和普查是同一件事的两面。

⚠ **反向纪律**：`sdk.h` 的 `CE_CARD` 里那一行删掉后**别再手动加回来**（注释已写在原地）——
加回来就是同帧施加两次。诊断上怎么认：伤害倍率对不上 ×1.25² 的样子（175 → 218）。

### 1.4 去处（改了哪五个文件）

| 文件 | 改了什么 |
| --- | --- |
| `native/sites.py` | `SDK_SITES` 加 `("ce_bullet_damage_final", 0x45e7ff, 9, "ff77088b0d5cf65100", …)`（含一手字节、跳转靶、为什么在这里）；`emit_header()` 加 `CE_BP_BULLET_DAMAGE_FINAL_RVA` |
| `native/sites_gen.h` | 生成物：`#define CE_BP_BULLET_DAMAGE_FINAL_RVA 0x05e7ff` |
| `native/th18_card_expand.def` | `BP_ce_bullet_damage_final @30` |
| `native/Makefile` | `dllverify` 的导出行数 `29 → 31` 与 echo 文案（31 = 30 个 `BP_*` + `thcrap_plugin_init`，见 §5.3 第 6 条）|
| `native/sdk.c` / `sdk.h` | 新 handler `BP_ce_bullet_damage_final`；`ce_sdk_scale_bullet_damage(id, bullet)` → `ce_sdk_bullet_damage_final(bullet)`（不再需要「落笔人」判断：写的人只有 SDK）；`CE_CARD` 的桩里删掉那次调用；`ce_sdk_setup` 的断点自检表补进本条（不挂 = FAIL）|
| `patch/th18.v1.00a.js` | 生成物：`breakpoints.ce_bullet_damage_final = { addr "0x45e7ff", cavesize 9, expected "ff77088b0d5cf65100" }` |

### 1.5 回滚方法（三步，任意一步都能单独回）

1. **只想关掉效果**：把 `sdk.c` 里 `BP_ce_bullet_damage_final` 的 handler 体改成直接 `return BP_EXEC_ORIGINAL;`
   —— 伤害回到「引擎给什么就是什么」（其余一切不变，断点仍在、自检仍过）。
2. **回滚整条站点**：删 `sites.py` 的 `SDK_SITES` 那一项与 `emit_header` 的 `#define`、删 `.def` 的 `@30`、
   `Makefile` 的 `31 → 29`、`sdk.c` 自检表那一行、然后把 `sdk.h` 的 `ce_sdk_scale_bullet_damage(ID, b);`
   **加回** `CE_CARD` 的 `+0x28` 桩（并把 `ce_sdk_bullet_damage_final` 换回旧名/旧签名）。
   ⚠ **这两半必须一起回**：只回一半会出现「两个落笔点」（×1.25²）或「一个都不落」（倍率失效）。
3. **回滚到「本轮之前」的整棵状态**：`git checkout -- native/{sites.py,sdk.c,sdk.h,sites_gen.h,th18_card_expand.def,Makefile} patch/th18.v1.00a.js`
   （本轮改动未提交 ⇒ 可直接丢弃；⚠ 工作树里还有别的会话的在飞改动，别用 `git checkout .`）。

---

## 2. A-2：死亡结果闩锁（引擎判完五家之后广播结果）

### 2.1 一手字节与周围控制流（逐条）

站点 = `Player__on_tick__body` `th18:0x45be90` 的 **case 4**（决死窗口走完那一帧）里**唯一那次判定**：

```text
0045c0f4  8b8738060000    mov eax,[edi+0x638]       ; case 4：决死窗口计时器
0045c0fa  3b8708790400    cmp eax,[edi+0x47908]     ; 比窗口帧数（原生 8；原生 24 Tewi 改成 15）
0045c100  0f8ca3010000    jl 0x45c2a9               ; 窗口没走完 ⇒ 走 B 键那条路（本帧不判）
0045c106  a198f24c00      mov eax,[0x4cf298]        ; ABILITY_MGR
0045c10b  8b701c          mov esi,[eax+0x1c]        ; 卡链头
0045c10e  85f6            test esi,esi
0045c110  7421            je 0x45c133               ; ★ 没卡 ⇒ 直接去扣命臂（本帧没有「取消」可言）
0045c112  33ff            xor edi,edi               ; acc = 0
0045c114  8b0e            mov ecx,[esi]             ; 卡对象
0045c116  57              push edi                  ; ★ 把「到目前为止的 acc」当**参数**
0045c117  8b01            mov eax,[ecx]
0045c119  8b400c          mov eax,[eax+0xc]         ; 卡虚表 +0x0c = 死亡救援
0045c11c  ffd0            call eax
0045c11e  8b7604          mov esi,[esi+4]
0045c121  0bf8            or edi,eax                ; 累计
0045c123  85f6            test esi,esi
0045c125  75ed            jne 0x45c114              ; ★ 五家全跑完
0045c127  85ff            test edi,edi              ; ★★ 我们的站点（6 字节从这里起）= 引擎**唯一**的判定
0045c129  8b7c2414        mov edi,[esp+0x14]        ;    还原 EDI = player
0045c12d  0f859e020000    jne 0x45c3d1              ; acc≠0 ⇒ 取消死亡（跳共用尾）
0045c133  8bcf            mov ecx,edi
0045c135  e8560f0000      call 0x45d090             ; ★ acc==0 ⇒ 扣命（Player__commit_death_and_enter_state2）
```

**站点纪律四问**（`local/_scratch/scan_bp_window.py 0x45c127:6`）：

| 问 | 答 |
| --- | --- |
| ① 长度 | ✅ 6 字节：`test edi,edi`（2）+ `mov edi,[esp+0x14]`（4）|
| ② 窗内相对寻址 | ✅ 无（真分支 `jne 0x45c12d` 在**窗后**，不在窗内）|
| ③ 窗内跳转靶 | ✅ 无。`0x45c110` 的 `je` 落 `0x45c133`、`0x45c12d` 的 `jne` 落 `0x45c3d1`，都在窗外 |
| ④ 指令边界 | ✅ 两条完整指令 |
| ★ 两条臂都执行？ | ✅ **是**：`test`/`mov` 是直线码，**取消**与**扣命**两条路都得先过它 ⇒ **一个站点看得到两种结果** |

**EDI = acc**：循环里 `or edi,eax` 累加，`0x45c129` 才把 EDI 换回 player ⇒ 站点处 `regs->edi` 就是 acc。

### 2.2 为什么不是 `0x45c3d1` / `0x45c135`（两个看着更自然的候选）

| 候选 | 为什么否 |
| --- | --- |
| `th18:0x45c3d1`（取消臂的落点）| 它是 **case 3 与 case 4 共用的尾**：一手不止字节 —— Ghidra 反编译把这个尾部直接标成 `LAB_0045c3d1`，`case 3` 与 `case 4` 都 `goto` 它；汇编侧 `0x45c0db` 的 `je` 与 `0x45c0ef` 的 `jmp` 也跳它 ⇒ 会把**非死亡帧**记成「死亡被取消」|
| `th18:0x45c135`（扣命臂的 `call`）| 5 字节的 `call rel32` = 窗内**相对寻址**（纪律禁止）；且它只看得见「没被取消」这一半 |
| `th18:0x45c10e`（`test esi,esi; je`）| 含 `74 21`（rel8）⇒ 窗内相对寻址 |

### 2.3 语义与它**不**做什么

- 语义（就是引擎那次判定的复述）：`CE_DEATH_RESULT_UNKNOWN`（`-1`）= 还没发生过判定；
  `0` = 这次死亡**没有被取消**（会/已经扣命）；`1` = **已被取消**。
- **只提供原语 + 诊断日志**：`sdk_core.{c,h}` 的 `ce_death_result() / ce_death_cancelled() / ce_death_result_seq()`，
  **没有任何一张卡被改去读它**（别人的卡 69/78 一个字节没动 —— 见 `AUDIT.md` §BE 的「只登记」行）。
- **不改控制流**：handler 返回 `BP_EXEC_ORIGINAL`（回放原两条指令 ⇒ `test` 设的 ZF 正好给 `0x45c12d` 用），
  既不写内存、也不碰寄存器、不改 `retaddr`。
- **复位点**（这是「一次死亡一份结果」的关键）：`BP_ce_player_die`（`th18:0x45d3a0` 入口 = 一次死亡开始）
  → `ce_death_result_reset()`，另在 `ce_sdk_setup` 复位一次（防热重载/异常退出把上一次进程的 `1` 带进来）。
  复位**在闸门检查之前**：闸门开着时这次「死亡」根本没发生 ⇒ 结果保持 UNKNOWN（正确）。
- ⚠ **没卡的空 deck 读不到**：`0x45c110 je 0x45c133` 会绕开站点 ⇒ 闩锁停在 UNKNOWN。这是**对的**
  （没有任何卡能读它），但别把 UNKNOWN 当 `0` 用 —— 这也是 `ce_death_cancelled()` 只在 `== 1` 时返回 1 的原因。

### 2.4 去处 / 回滚

| 文件 | 改了什么 |
| --- | --- |
| `native/sites.py` | `SDK_SITES` 加 `ce_death_result`（含一手字节与「为什么不是另外两个候选」）；`emit_header()` 加 `CE_BP_DEATH_RESULT_RVA` |
| `native/sites_gen.h` / `th18_card_expand.def` / `Makefile` / `patch/th18.v1.00a.js` | 同 §1.4（`@31`、`CE_BP_DEATH_RESULT_RVA 0x05c127`、导出行数、断点声明）|
| `native/sdk_core.{c,h}` | 结果闩锁（`-1/0/1`）+ 三个只读入口；头里带一手依据与「读到 UNKNOWN 不许当 0」的告警 |
| `native/sdk.c` | 新 handler `BP_ce_death_result`（限流日志）；`BP_ce_player_die` 里加一行 `ce_death_result_reset()`；`ce_sdk_setup` 双复位 + 自检表补进本条 |
| `native/tests/test_sdk_core.c` | `test_native_conflict_fixes()`：B-1 的算式（140 × 1250‰ = 175、施加两次 = 218）+ A-2 的三态与复位 |

**回滚**：① 只想静音 ⇒ 把 handler 体改成 `return BP_EXEC_ORIGINAL;`（闩锁永远停在 UNKNOWN，**没有任何卡依赖它**）；
② 整条站点 ⇒ 按 §1.5 第 2 步的同一套清单反向做（`sites.py` / `emit_header` / `.def` / `Makefile` / `sdk.c` /
`sdk_core.{c,h}` / `tests`）；因为目前**没有卡读它**，回滚这一步是纯删除，不需要回归任何卡。

---

## 3. 站点纪律与工具（可复跑）

```bash
python3 local/_scratch/scan_bp_window.py 0x45e7ff:9 0x45c127:6      # 四问 + call [reg+0x28]/[reg+0xc]/[reg+0x14] 普查
cd mods/th18.v1.00a/card-expand/native && python3 sites.py check     # 站点 ↔ exe 字节 ↔ def 导出 双向对账
python3 sites.py gen --rows 255 -o ../patch/th18.v1.00a.js          # 重生成 patch 与 sites_gen.h（幂等）
```

`scan_bp_window.py` 的手法：Capstone 线性反汇编整段 `.text`（204100 条指令，66 处重同步），
收集**全部直接分支靶**，然后回答「长度 / 相对寻址 / 窗内跳转靶 / 指令边界」四问，并顺带普查
`call [reg+0x28]`（卡虚表 `on_bullet_created`）、`call [reg+0xc]`（死亡救援）、`call [reg+0x14]`（真死回调）。
不占 Ghidra 工程锁。

**thcrap 侧的机制一手**（`local/vendor/thcrap/thcrap/src/breakpoint.cpp` + `bp_entry.asm`，本轮为回答
「窗内有 `[esp+0x14]` 会不会读错栈」而读了源码）：

- 站点被改写成 `call bp_entry` + nop 补齐到 `cavesize`（`breakpoint_apply`）；
- `bp_entry`：`pusha; pushf; push esp; push bp_local; call breakpoint_process; add esp,8; add esp,eax; popf; popa; ret`
  ⇒ handler 里的 `regs` 就是那个 `pushad` 帧（`thcrap_bp.h` 的布局注释）；
- `breakpoint_process`：handler 返回非 0 ⇒ `regs->retaddr = cave`（= 回放原指令），返回 0 ⇒ 不回放；
- cave = **原 `cavesize` 字节的拷贝** + `jmp 站点+cavesize`，且 `cave_fix()` **只重定位开头**的 `e8/e9`
  （这就是「窗内不许有别的相对分支」的出处）；回放跑在 **`popa/popf` 之后** ⇒ **cave 里的 ESP 就是站点处的 ESP**。
  已有实跑先例同机制：`ce_option_shot_gate`（窗内含 `sub esp,0xc`）、`ce_shop_bought`（窗内三条 `push`）。
- `cavesize` 合法区间：`[5, 27]`（`BP_SourceCave_Min/Max = CALL_LEN / BP_Offset − CALL_LEN`）—— 我们取 9 与 6。

---

## 4. 实跑怎么验（每条给可观察现象）

| # | 怎么造 | 该看到什么 | 反例（说明坏了）|
| --- | --- | --- | --- |
| **B-1-① 基本叠加** | 卡组装 **113（魔理沙 ×1.25）+ 25 弱肉強食**，打同一个敌人（Saki 只在 `shot type7` 生效 ⇒ 用咲夜系主机且装对应子机）| 伤害 = 引擎基值 × 1.4 × 1.25 = ×1.75；HUD 左上角的 `x1.25` 仍在（它读的是聚合值，与落笔点无关）| 伤害 = 基值 × 1.4（我们的被抹掉）⇒ 站点没生效，查启动日志有无 `FAIL: sdk: breakpoint ce_bullet_damage_final not applied` |
| **B-1-② 顺序无关** | 同一组卡，把 113 与 25 的**获得顺序**换一次（存档/获得顺序 = 卡链顺序），各测一次 | 两次伤害**一样**（×1.75）| 两次不一样 ⇒ 落笔点没搬成功（还是 `+0x28` 的旧行为）|
| **B-1-③ 没加伤害卡时零影响** | 只装 76 古锭刀（平时声明 `1000` = 中性）与不装它，各打一发 | 伤害与不装时**逐点相同**（中性 ⇒ handler 直接 return，一个字节都不写）| 伤害变了 ⇒ handler 的门写错（`agg == CE_DMG_UNIT` 那条）|
| **B-1-④ 没被施加两次** | 同上 113 + 25，把一发弹的实际伤害与「基值 × 1.75」对 | 正好 ×1.75（例：基值 80 ⇒ 140 ⇒ 175）| 出现 ×1.25² 的样子（175 → 218）⇒ `+0x28` 桩里的缩放被加回来了 |
| **A-2-① 被救下** | 装 **113（灵界槽 ≥ 200）**，中弹、等决死窗口走完 | 日志 `death result: CANCELLED (acc=1, five rescuers done, #n) - life kept`；命**不掉**（113 的 `+0x0c` 返回 1）| 打的是 `not cancelled` 却仍然没扣命 ⇒ 站点读错了寄存器（不是 EDI）|
| **A-2-② 没被救下** | 只装 106/111 这类不救援的卡，中弹（或槽 < 200 的 113） | 日志 `death result: not cancelled (acc=0, …) - will be lost (0x45c135 -> commit)`；命 **−1** | 一条日志都没有 ⇒ 站点没挂上/没进 case 4（看启动自检是否 FAIL）|
| **A-2-③ 复位正确** | 连着死两次：第一次让 113 救下、第二次槽 < 200（照常死）| 第二条日志必须是 `not cancelled`（不是 `CANCELLED`）—— 证明 `BP_ce_player_die` 的复位起了作用 | 第二次仍打 `CANCELLED` ⇒ 复位没生效（闩锁粘住了）|
| **A-2-④ 闸门开着时不误记** | 113 声明「中弹无伤」（灵界中）+ 中弹 | **没有** `Player__die` 体、也没有 `death result:` 日志（这次「死亡」没发生 ⇒ 结果停在 UNKNOWN）| 出现 `not cancelled` ⇒ 说明 UNKNOWN 被当成了 0（读法错）|
| **A-2-⑤ 无卡 deck（健全性）** | 空卡组（或全部倒吊人删光）中弹 | 依旧扣命、**无** `death result` 日志（`0x45c110` 绕开站点）—— 不崩、不记错 | 崩 / 卡在中弹动画 ⇒ 站点把控制流改坏了（不该发生：handler 只读）|

诊断日志都走 `ce_log`（`rk_log` → 部署日志），两边都是**限流**的：

- **B-1**：自机弹是每发都有的高频路径 ⇒ 只在「真的改了值」时计数（`before != after`），前 3 次 + 之后每 5000 次一条：
  `bullet damage final: 140 -> 175 (agg 1250 permille, bullet 0abc1234, #1, after card chain)`。
  见到 `140 -> 175` 就是「别人的最终值 × 我们的倍率」这一次实锤；**中性时不打**（handler 一个字都不写）。
- **A-2**：前 3 次 + 之后每 20 次一条（`death result: CANCELLED (acc=1, five rescuers done, #n) - life kept`，
  或 `… not cancelled (acc=0, …) - will be lost (0x45c135 -> commit)`）。

数值才是 B-1 的硬判据（日志只用来确认站点真的在跑），A-2 则以日志为主判据（没有别的可观察量）。

---

## 5. 对既有卡的影响面 + 回归 + 未做

### 5.1 共享件影响面（`sdk.h` / `sdk.c` / `sdk_core.*` / `sites.py` / `Makefile` / `.def` 都是共享件）

| 改动 | 对**所有卡**的影响 | 对**别的卡（58–80）**的影响 |
| --- | --- | --- |
| `sdk.h` 的 `CE_CARD`：`+0x28` 桩不再调 `ce_sdk_scale_bullet_damage` | 声明 `bullet_damage_permille` 的卡（61/76/78/113/116）行为**不变**（层还是那个层、倍率还是那个乘积），只是落笔点从链中挪到链尾 ⇒ 与原生卡同装时结果**变正确**（不再被抹） | **只读不写**：它们没声明倍率就完全不受影响（中性 ⇒ handler 直接 return）|
| `sdk.h` / `sdk.c`：`ce_sdk_scale_bullet_damage(id, bullet)` → `ce_sdk_bullet_damage_final(bullet)` | **内部 API**（只在 `CE_CARD` 宏里用过；`cards/` 里零直接调用 —— grep 过）⇒ 没有卡需要改 | 同上 |
| `sdk_core.{c,h}`：新增结果闩锁三个只读入口 + `ce_death_result_reset/note` | 纯新增；没有卡调用 ⇒ 零行为变化 | 零影响（将来 69/78 想修「白记一笔」时可以直接读）|
| `sdk.c`：`BP_ce_player_die` 多一行 `ce_death_result_reset()` | 每个死亡多一次 int 赋值（O(1)），不改任何控制流 | 零影响 |
| `sdk.c`：断点自检表 16 → 18 条 | 启动自检多核对两条「站点首字节是不是 `e8` + nop」⇒ 不挂就 FAIL（**原来会静默失效**）| 零行为影响；真 FAIL 时新卡会按既有门禁被禁用（这是**想要**的：宁可不生效也不静默错）|
| `sites.py` / `Makefile` / `.def` / `patch/*.js` | 断点表 + 导出数（29 → 31） | 所有卡一起走同一份 patch |

### 5.2 回归（本轮实跑过的门）

```text
python3 sites.py check      → ✅ 30 个 BP_* 双向一致；25 个实例两条不变式全绿；表区间 0 处漏网点
python3 sites.py gen …      → 写出 210 条 binhack + 11 个 codecave（含两条新断点）
make dll dllx87 dllverify   → 编译 0 警告；x87 in our objects: 0；DLL ABI: 31 exports（30 BP_* + thcrap_plugin_init）
objdump -p                  → @29 BP_ce_player_die / @30 BP_ce_bullet_damage_final / @31 BP_ce_death_result
make test-host              → test_cards_def 118/0；test_sdk_core 的新用例 test_native_conflict_fixes 全绿
```

⚠ **诚实登记（本轮工作树是多人同时在写的）**：本轮施工期间，**另一个会话正在同一棵工作树里改计分层**
（`sdk_core.c` 的 `ce_score_*` 一族与 `tests/test_sdk_core.c` 的 `test_score_rework`），它一度让
`make check` 停在 30 条**与本轮无关**的计分断言上（失败行号全在 `test_score_rework` 内，
本轮新增的 `test_native_conflict_fixes` 一条没红）。那次重构落定后已复跑：**`make check` rc=0**
（`test_sdk_core: 700 passed, 0 failed`），本节的五条门与本轮两个站点全绿。
⇒ 若将来再看到类似的「别人那半红」，先把**失败行号落在哪个测试函数**里认清，再决定要不要停手。

### 5.3 没做 / 未验（缺口）

1. **两处都没实跑**（本轮只做静态 + 主机单测）⇒ §4 的判据要按表走一遍。
2. **B-1 只解决了「我们的倍率被抹」**：两张**原生卡之间**的重算冲突（25 在前、54 在后 ⇒ 后者从基值重算、前者的 ×1.4 也会被吞）
   **没有**解决 —— 那是 exe 内部两张原生卡的事，我们能看到的最多是在链尾乘一次。**用户口径要「×1.75」能拿到，
   但「×1.4 × 1.8」这一档拿不到**（除非另做「从 shooter 基值重算 × 各家倍率之积」的更深的层，代价要大得多）。
3. **A-2 的闩锁目前没人读**：本轮只提供原语（用户口径「先只提供原语 + 诊断日志，不改别人卡的行为」）。
   真正用它去修 69/78 的假 miss，是**别人的卡**作者的活（`AUDIT.md` §BE 已登记）。
4. **A-2 不覆盖「没卡」那一帧**（站点被 `0x45c110 je` 绕开）—— 已知且无害（没有卡能读它），但写在这里以免下次误判。
5. **117 的 `+0x28`（妖侧弹伤害按火力重算）现在看到的是「我们乘之前的值」**：语义上我们的倍率变成
   「乘在最终值上」（与 `EVENTS-CONTRACT.md` §2 的「乘作用在最终总数上」一致），但**取整次序与之前可能差 1 点**
   ⇒ 实跑对账时若看到 1 分的差，先想到这一条（`cards/youkai.c` 的 `on_bullet_created`）。
6. 导出数的口径：`Makefile`/`dllverify` 数的是**导出表总条数**（含 `thcrap_plugin_init`），
   `sites.py check` 数的是 **`BP_*` 条数** ⇒ 「29 exports」= 28 个 `BP_*` + 1；本轮加两条 ⇒ **31 exports / 30 个 `BP_*`**。
   报告与门禁里的两个口径都已对齐，别的文档若还写着「29 exports 不变」，以本节为准。
