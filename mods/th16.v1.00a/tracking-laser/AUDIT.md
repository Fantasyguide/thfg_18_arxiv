# test-laser:对抗审计记录 + 经验
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。


## 对抗审计(2026-06-11,两个独立 agent,默认"我们写错了"去 th16.exe 证伪)

### Agent A — cave asm / ABI 审计 → 抓到 1 个 BLOCKER(已修)
- **★ BLOCKER(已修):`find_nearest_enemy`/`is_enemy_alive`/`anm_unload` 是 STDCALL(callee 清栈,
  分别 `RET 8`/`RET 4`/`RET 4`),不是 cdecl。** 原 asm 在调用后还 `add esp` → ESP 抬高 →
  `pop esi/edi` 读到栈帧里的局部(非真正保存的 esi/edi)→ **esi/edi 被破坏 → tick_bullets 崩**
  (它跨调用持有 ESI=弹槽、EDI=PLAYER_PTR)。一手铁证:寻的 tick `0x445ee0` 调这三个**都没有 add esp**。
  → **修复**:删掉 `tick_tracking_burst_starter.asm` 里 3 条 `add esp`;`.c` extern 标 `__stdcall`。
- 其余全部核对**正确**:slot/enemy 偏移、atan2 的 FPU 压栈顺序(dy 先 dx 后)与角度方向、
  伤害源对象地址算法(`link*0x94 + PLAYER + 0xd080`,无 +1)、self-free 后返回非 0 安全(tick_bullets
  对非 0 返回直接跳到循环推进、不再碰该槽 → 无 UAF)、栈帧布局、`__fastcall` 普通 `ret`。

### Agent B — 伤害管线 / 集成审计 → 核心成立,1 个待验已解
- 管线 Claim 1–5 **全部一手 CONFIRMED**,并**补实了 engine/sht/th16/08 §5 的 🟡**:
  `EnemyData__step_logic @0x41c71c` 确实拿 `enm_compute` 返回值调 `receive_damage` 扣血。
- idx4 重指**全部细节坐实**:`sht_func_tick_table @0x4919a0`,idx4 = `0x4919b0` → `0x4470f0`;
  原字节 `f0 70 44 00`(`expected` 正确);解析器 `0x443790` **无边界检查**;派发经 shooter+0x2c。
- init=3(`0x4470e0`)确认清 `+0x90` 目标槽 + flag&0x3c,正是 tick 前置,安全。
- tick idx4 原版确不用(重指只牺牲 lock-dash 实验产物);**注**:Agent 指出原版 tick 其实也用 idx3(加速),
  我们 PLAN 早先写的 "tick∈{0,1,2,5}" 漏了 idx3,但与 idx4 安全无关。
- **★ 待验项已解**:`func_on_hit=0` 空指针安全 —— 命中派发 `playershot_hit_dispatch @0x445d40`
  **明确 `if(*(shooter+0x34)!=0) call; else playershot_launch_shared`**,hit=0 走 else(消弹),**不会 call 0**。✅
- max_dmg(header+0x28)=**60**;配 dmg=30 多源叠加,单敌每帧≤60,适中不弱。

## 结论:修掉 add esp 后,起步版设计是**可落地的**
- 管线/集成/ABI/偏移全部一手核对;唯一真 BUG(add esp)已修;两个"待验"(hit=0、双重释放)都证为安全。
- 仍属"未在游戏里跑过":手写 asm 汇编后仍需 (a) 比对反汇编 (b) 游戏内验证崩溃/坐标/FPU 栈平衡。

## thcrap 挂载核实(2026-06-11,对 thcrap 一手源码 + ExpHP 真实 th16.v1.00a 补丁)

- **★ 抓到并修了一个真 BUG**:重指数据指针表槽要写**绝对地址**,thcrap code 串里 `[expr]`=**相对**(rel32)、
  `<expr>`=**绝对**(`thcrap/src/binhack.cpp` L869–879 定论)。原 patch 写 `[codecave:…]`(相对)→ 表槽里
  是个相对偏移当指针 → 跳错地址崩。**已改 `<codecave:tracking_burst_laser>`。**
- 其余对照 ExpHP `bullet_cap`/`anm_leak` 的 `th16.v1.00a.js` 实补丁核实:
  - 文件结构 = patch 根下 `patch.js` + `files.js`(CRC32 清单)+ 逐版本 `th16.v1.00a.js`(我原写 `bin/th16.exe.js` **错**,已改)。
  - 顶层 `codecaves`/`binhacks`/`options`;binhack = `{addr, expected, code}`(key 即标识,`title` 可省);
    codecave 值**直接是 hex 串**(也可 `{code,size,access,...}` 对象;默认 access=execute-read-write)。
  - `expected` = 小端 hex 字节串(我们 `f0704400` 对);thcrap 校验原字节,改版不匹配则跳过。
  - binhack 可写**任意地址含 .rdata**(apply 时设页可写);ExpHP 的 `anm_leak` 还 hook 了我们也用的 `0x46efa0`。
- **注入机制**(`thcrap/docs/2_files.md`):loader 挂起启动→改入口 2 字节死循环→跑到入口→进程内分配可执行内存
  + CreateThread `LoadLibrary thcrap.dll` 跑 init→恢复 = 远程线程 DLL 注入 + 运行时内存补丁;版本经 `base_tsa/versions.js` exe 哈希匹配。
- 结论:**方法靠谱、能被 thcrap 正确挂载**;唯一原创用例(重指数据槽)用 `<…>` 绝对地址即可,ExpHP 虽无同款用例但语法支持。

## 经验
- **手写 cave 最易错的是调用约定**:东方引擎内部函数大量是 **stdcall(callee 清栈)**,照着"cdecl 调用方清栈"
  写必崩。**判据**:看目标函数结尾是 `RET imm`(stdcall)还是 `RET`(cdecl)——或直接看引擎自己怎么调(有没有 add esp)。
- **对抗审计值回票价**:这个 add esp 错误肉眼/单人复核极易漏(逻辑全对、只错栈平衡),靠"默认写错、去二进制证伪"
  的独立 agent 才抓到。涉及手写机器码/ABI 的产出,务必上这一道。
- **空指针分支要查派发点**:func_on_hit=0 是否安全,取决于派发处有没有 null 检查(`0x445d40` 有)——
  不能假设"0=无操作",要看代码。
