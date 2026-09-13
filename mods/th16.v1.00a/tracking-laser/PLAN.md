# test-laser:追踪 + 爆发式激光(TH16 自机扩展实验)
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> 目标(用户设定):做一个**追踪+脉冲激光**自机弹——每隔一段时间发射一次,**锁定一个敌人**,
> 造成**短暂的脉冲伤害**,然后**消失**。
> 版本:TH16 v1.00a(th16.exe,imagebase `0x400000`)。日期 2026-06-11。
> 性质:**档 1(打 exe 补丁)实验**——纯改 .sht 做不到(见下),需 thcrap code cave。

## 0. 为什么纯 .sht(档 0)做不到

`func_on_tick` 一个 shooter 只能选**一个**索引。而:
- **激光(tick idx2)方向是纯几何**(按 .sht 角 / 聚焦竖直),**完全不读敌人**(见 `engine/sht/th16/03` §6.7)。
- **追踪(tick idx1)**会 `find_nearest_enemy`+`atan2` 瞄敌,但驱动的是**普通子弹,不是激光束**。

没有任何一个已编译 tick 同时干"激光束几何 + 瞄敌"。所以"会追踪的激光"在原版行为表里不存在,
**必须自己写一个新 tick 函数并注入**。

## 1. 方案:写新 tick = idx2(激光)+ idx1(瞄敌)+ 寿命爆发

新函数 = **克隆 idx2 激光 tick**,只做两处外科手术:
1. **换角度源**:把 idx2 里"聚焦→竖直 / 非聚焦→.sht 角"那段,换成"`find_nearest_enemy`→`atan2`→目标角"
   (照搬 idx1 的瞄敌,见 `engine/sht/th16/03` §3 词汇 + 本目录 `tick_tracking_burst_laser.c`)。
2. **加爆发寿命**:函数开头判存活帧 `self+0x10 > BURST_FRAMES` → 置 `self+0x8c=2`(走 idx2 自带的
   "激光熄灭"收尾)→ 本束消失。`fire_rate` 控制重发节奏 = 脉冲。

这样激光的**渲染 / 伤害 / option 锚定 / 开关逻辑全部继承 idx2**,我们只改"瞄哪"和"活多久"。

## 2. 落地:占用 tick idx4 槽(原版没人用)

- **决策**:不新增表项,直接**重指 tick 表的 idx4 槽**(`0x4919b0`,现 = `0x004470f0` lock-dash,
  原版零使用)→ 指向我们的 cave 函数。`engine/sht/th16/03` §4 实测 tick∈{0,1,2,5},idx4 安全。
- **.sht 侧已就绪**:我们之前生成的 `local/th16.v1.00a/pl02_lockdash_*.sht` 已经把子机 tick 填成 **4**。
  ⚠️ 但它们用的是 **init=3**;激光需要 **init=2**(清蓄力 +0xa0 + 建 +0xb0 激光池对象 + SFX)。
  → **需重生成 pl02:`(init=2, tick=4, hit=0)`**(见 §4 待办)。
- hit:用户要"hit 效果就是伤害"。激光伤害走 idx2 的 **+0xb0 激光池对象**(本身对接触敌人持续掉血),
  故 **func_on_hit 设 0** 即可(不挂额外命中特效);若想要"束体截断到接触点"再用 hit2(`0x446870`)。

## 3. 开放 RE / 风险

### ✅ 已解决(2026-06-11,见 `engine/sht/th16/08`)—— 两个致命项都拆了
1. ~~❓ 激光伤害怎么施加~~ → ✅ **伤害走独立管线,敌人侧自动处理**:spawn 给每发弹建 `+0xb0` 伤害源
   (dmg = .sht `dmg` 字段 +0x02),tick_bullets 每帧拷弹位置进去,敌人 `enm_compute_damage_sources`
   遍历重叠源累加扣血(封顶 = .sht `max_dmg` header+0x28)。**cave 函数不用写任何伤害代码。**
2. ~~❓ +0xb0 谁创建~~ → ✅ **`PlayerBullet__create`(spawn `0x444e10`)给每发弹都建**(`sub_444b20`),
   dmg 自动 = .sht dmg 字段。我们的弹是正常 .sht 发的 → 自动有伤害源。**连 init=2 都不是伤害的必要条件。**

→ **重大去风险**:追踪激光只要(a)正常 .sht 发射、(b)cave 保持 `slot+0x48` 位置更新、(c)活到
   BURST_FRAMES 再结束,**伤害自动生效**。甚至"束形"都不是掉血必要条件(点在敌人身上的伤害源就掉血)。

### ✅ ABI/地址已补(2026-06-11,反 `0x445ee0` + list_names)
3. ~~find_nearest_enemy ABI~~ → ✅ `find_nearest_enemy(out,&refpos)` cdecl,**半径 XMM3 = `[0x494680]` = 256.0**,调用后 `*out`=句柄。
4. ~~地址~~ → ✅ 全部:`find_nearest 0x425240`、`is_enemy_alive 0x41a980`、`handle_to_enemy 0x41b540`(__fastcall ECX=&句柄)、
   `crt_atan2 0x487aaa`(FPU)、`math_add_normalize_angle 0x4052e0`、`get_vm_with_id 0x46efa0`、`anm_unload 0x46f1c0`。

### 🟡 仍待游戏内验证(非阻塞)
5. **脉冲节奏**:cave 用 `slot+0x10 > BURST_FRAMES` 自我了结(清源/卸 anm/释放槽)+ fire_rate 重发 → "脉冲";手感待验。
6. 手写 asm(`tick_tracking_burst_starter.asm`)需汇编后比对 + 游戏内验证(崩溃/坐标/FPU 栈平衡)。

## 4. 待办清单

- [x] ~~反伤害施加路径 + +0xb0 创建点~~ → ✅ `engine/sht/th16/08`(伤害自动,免写代码)。
- [x] ~~补 ABI/地址~~ → ✅ §3(半径 256、全部函数地址)。
- [x] ~~生成 .sht~~ → ✅ `local/th16.v1.00a/pl02_tracklaser.sht`(子机 init=3/tick=4/hit=0,dmg=30;主弹保留直线)。
- [x] ~~写起步版 cave~~ → ✅ `tick_tracking_burst_starter.c` + `.asm`(简化版:追踪+脉冲+伤害,无束体)。
- [x] ~~对抗审计 cave/管线~~ → ✅ 见 `NOTES.md`:抓到并修了 BLOCKER(stdcall 误当 cdecl 多 add esp);hit=0 证为安全;链条补实。
- [ ] **汇编 cave**:`nasm -f bin tick_tracking_burst_starter.asm -o cave.bin` → hexdump 贴进 thcrap codecave。
- [ ] **打 thcrap**:codecave + 重指 `0x4919b0`(`thcrap_patch.md`)。
- [ ] **游戏内验证**:子机弹会不会瞄敌?约 24 帧后消失(脉冲)?敌人掉血?崩不崩?(回填 NOTES)
- [ ] 起步版跑通后:加 idx2 束体渲染做成真激光(完整版 `tick_tracking_burst_laser.c`,需补 init=2 + get_vm 调用)。

> 简化版起步建议:第一版**先不追求束形**——cave 只做"瞄敌 + 飞向/贴住敌人 + BURST_FRAMES 后结束",
> 伤害靠 spawn 自带的点伤害源自动生效(engine/sht/th16/08 §4.3)。跑通"会追踪+有伤+脉冲"后,再加 idx2 束体渲染。

## 5. 经验总结(到目前为止)

- **TH16 自机弹 = 硬编码函数表,不是脚本**:这决定了"扩展"分两档——档 0 重组/解锁已编译行为(免改 exe、
  可分发);档 1 才能加**新逻辑**(code cave + 重指表槽)。详见 `engine/sht/th16/03`、`engine/sht/th16/07`。
- **"独立索引"是档 1 的福利**:init/tick/hit 各自独立查表 + **解析器对索引无边界检查** →
  重指一个原版没用的槽(如 tick idx4)就能让现成 .sht 触发我们的新函数,不必动 .sht 结构。
- **新行为别从零写,焊现成件**:寻的(瞄敌)、激光(束几何/蓄力)、锁定冲刺(锁敌)的原子函数都已定位
  命名(`find_nearest_enemy 0x425240`、`atan2 0x487aaa`、`cartesian_from_polar_4476b0`、idx2 蓄力段)。
  新 tick 本质是"取两段已验证逻辑拼接"。
- **纪律**:伤害路径没反掉之前,这个实验**不能宣称可行**——能瞄、能画束 ≠ 能造成伤害。先证伪"有伤无伤"。
- **slot 选择教训**:重指 idx4 会**牺牲 lock-dash**(那也是实验产物,无所谓);若想两者并存,应改用表尾
  null 空位(tick `0x4919b8`)新增 idx6,但那是写 .rdata,thcrap binhack 可做,布局更复杂。
