{
 "breakpoints": {
  "ce_item_commit": {
   "addr": "0x44658a",
   "cavesize": 6,
   "expected": "f30f10166a25",
   "title": "行为 SDK → BP_ce_item_commit：共享收尾起点：movss xmm2,[esi]（4 字节）+ push 0x25（2 字节），本道具分值已按 type 计入 SCORE、公共收尾（清 state、接 commit tail）之前；各收集路线（线/半径）在此汇聚。两条都无相对寻址。沿卡链表调 on_item_commit()"
  },
  "ce_shot_damage": {
   "addr": "0x45f2a2",
   "cavesize": 7,
   "expected": "b867666666f7ee",
   "title": "行为 SDK → BP_ce_shot_damage：enm_compute_damage_sources 0x45f0f0 里可计分命中路径：mov eax,0xee666667 + imul esi（两条，都无相对寻址；0x45f2dc 因 guard-fail 路 0x45f14e jmp 0x45f2de 落在窗内而**被否**，此窗已对齐 objdump 确认无窗内跳转目标）。esi = 本帧本敌伤害（钳 cap 后 ≥1），ebx = 帧基 → pos 在 [ebx+8]（敌世界坐标 x,y）、param_6 在 [ebx+0x1c]。沿卡链表调 on_shot_damage(card, dmg, pos)"
  },
  "ce_bomb_gate": {
   "addr": "0x45c048",
   "cavesize": 7,
   "expected": "f60534a44c0002",
   "title": "行为 SDK → BP_ce_bomb_gate：Player__on_tick__body 0x45be90 的普通炸弹分支：test BYTE PTR ds:0x4ca434,0x2（7 字节，输入上升沿 bit1）。handler 沿卡链表问 on_bomb_gate()；任一卡返回 1 时用 retaddr 跳到 0x45c056 跳过 call do_bomb 0x45c051。只在咆哮 ready/hypering 的卡返回 1；其余帧/无卡时放行原指令（真实炸弹不受影响）。"
  },
  "ce_enemy_death": {
   "addr": "0x430240",
   "cavesize": 6,
   "expected": "558bec83ec14",
   "title": "行为 SDK → BP_ce_enemy_death：FUN_00430240（SDK 名 Enemy__die）入口：push ebp/mov ebp,esp/sub esp,0x14，无相对寻址。thiscall，ecx = zEnemy；**引擎撒主掉落之前**（0x430493 才是撒、0x43049f 才通知卡）、链表仍挂着，恒定 mov eax,1 返回 ⇒ 一敌一生正好一次。handler 用 regs->retaddr 分死因（0x42f3b7 = 打死主路 / 0x42ee90 = 炸弹秒杀 / 0x430f59 = ECL die / killAll 三兄弟），沿卡链表调 on_enemy_death(&ev)。详见 docs/superpowers/specs/2026-09-12-shared-enemy-death-event-design.md"
  },
  "ce_poc_gate_a": {
   "addr": "0x445bb9",
   "cavesize": 6,
   "expected": "0f87a4030000",
   "title": "行为 SDK → BP_ce_poc_gate_a：ItemManager__on_tick__body 的『过线 ⇒ 道具无条件真空』分支 A：`445baa movss xmm0,[esi+0x47998]`（收点线）→ `445bb2 comiss xmm0,[esi+0x624]`（自机 y）→ **本站点 `ja 0x445f63`**（0x445f63 = `mov [item+0xc74],3` 真空态）。6 字节、无相对寻址。handler 读声明层 `ce_full_p_gate_active()`：开着 ⇒ **返回 0（不执行这条 ja）**= 条件不成立 = 落空到 0x445bbf（道具继续下落）。一手依据 = TH06 `ItemManager.cpp` 的道具 tick`state==1 || (128 <= currentPower && player.y < 128.0f)` ⇒ 『满 P + 在线上』才是过线收点的充要条件。"
  },
  "ce_poc_gate_b": {
   "addr": "0x446003",
   "cavesize": 6,
   "expected": "0f875affffff",
   "title": "行为 SDK → BP_ce_poc_gate_b：同上，**第二条** `ja 0x445f63`（向后跳；0x446003 处、6 字节）。落空到 0x446009。两条都要挂：只挂一条 = 另一条支路照旧把道具拉进真空态（静默半失效）。"
  },
  "ce_player_die": {
   "addr": "0x45d3a0",
   "cavesize": 6,
   "expected": "558bec83ec14",
   "title": "行为 SDK → BP_ce_player_die：Player__die 0x45d3a0 入口：push ebp/mov ebp,esp/sub esp,0x14（6 字节，无反/相对寻址、无窗内跳转目标）。thiscall，ecx = zPlayer；全 exe 只有 3 处 call（0x45cd27/0x45ce81/0x45d060，全在中弹判定链上）、0 处 jmp、0 处数据表引用 ⇒ 一个站点覆盖子弹/敌本体/三种激光**全部**命中路径。handler 读声明层 ce_no_hit_active()：开着就 **返回 0 并把 regs->retaddr 改成 0x45d584**（该函数唯一的 c3；此刻栈上只有调用者返回地址 ⇒ 栈干净），等于整段 die 体不执行= 一手 TH13 的「撞了但不算撞弹」（弹照样被吃掉、擦弹不受影响）。⚠ 尾部的卡虚表 +0x10 救援广播也一并被跳过 ⇒ 要靠本闸门自救的那张卡必须先撤声明再调 die。一手链见 engine/player/th18/03-player-hit-gate.md"
  },
  "ce_shot_set_index": {
   "addr": "0x45ebfb",
   "cavesize": 7,
   "expected": "8bbc97e0000000",
   "title": "行为 SDK → BP_ce_shot_set_index：Player__tick_shooting_state 0x45ea00 查主炮 shooterset 前的 mov edi,[edi+edx*4+0xe0]（7 字节，无相对寻址）。edx = 本帧 fire-time set index（0..9 主炮；eax = 火力档），ebp-8 暂存同一 set index，随后 0x45ec69 会打包进 bullet+0xac。\nhandler 沿卡链表问 on_shot_set_index(card, eax)：返回 >=0 时改写 edx 与 [ebp-8] 为咆哮组 base+power，放行原指令；不咆哮/无卡时返回 -1，原版主炮 0..9 一个字节都不动。"
  },
  "ce_repopulate_native_option": {
   "addr": "0x45d610",
   "cavesize": 7,
   "expected": "538981dc710400",
   "title": "行为 SDK → BP_ce_repopulate_native_option：Player__repopulate_options_and_notify_cards 0x45d5e0 里**算完火力档之后那一条**：`push ebx` + `mov [ecx+0x471dc],eax`（7 字节；该 mov 的 imm32 = 0x471dc，**不是相对寻址**）。此刻 ecx = &zPlayer.inner（thiscall this = PLAYER+0x620；一手 0x45d5fc）、eax = `CURRENT_POWER / POWER_PER_LEVEL`（一手 0x45d604–0x45d60a 的 idiv）。handler 把 eax 改成 0 后**放行原字节**：后面两段（满火力形子机 VM 的建/清、普通形 4 格的建/清）读到的档位都是 0 ⇒ 不建新子机、并走引擎自己的「清空 4 格」那条路；handler 另外自己 interrupt 掉4 格 × 2 支（+0xb0 普通形 / +0xb4 满火力形，照 0x45d823 那段同款写法）。只在 `ce_native_option_hidden(当前机体)` 为真时动手 ⇒ 其余机体 / 没装卡时逐字节不变。"
  },
  "ce_native_option_muzzle": {
   "addr": "0x45e930",
   "cavesize": 5,
   "expected": "558bec5153",
   "title": "行为 SDK → BP_ce_native_option_muzzle：Player__shoot_one_bullet 0x45e930 的**函数入口**（5 字节 = push ebp / mov ebp,esp / push ecx / push ebx，恰好在指令边界上；无相对寻址、窗内无跳转靶。⚠ 取 6 字节会**从 `mov ebx,[ebp+8]` 中间切开**（那条是 3 字节、落在 0x45e935）⇒ cave 会回放半条指令，必须取 5 或 8）。thiscall ⇒ ecx = zPlayer*；栈（低→高）= 返回地址、packed id、timer、pos*、**option*（膛口）**（调用方压栈序一手：0x45ec5f push 膛口 / 0x45ec68 push pos / 0x45ec71 push timer / 0x45ec72 push id）。handler 读第 5 个栈参：落在**原生子机数组区间** `[PLAYER+0x670, PLAYER+0xa30)` 时**整发跳过**（`regs->retaddr = [esp]` + `regs->esp += 0x14` = 等价一条 `ret 0x10`；thcrap 的 esp 机制见 breakpoint.cpp 的 esp_diff；返回值调用方不检查，一手 0x45ec78）。卡申请的池 = `PLAYER+0xa30` 起（一手 0x40a7a4 `add edi,0xa30`）⇒ 区间不相交，117 / 116 / 108 的卡子机零影响。"
  },
  "ce_option_shot_gate": {
   "addr": "0x40a9c3",
   "cavesize": 6,
   "expected": "8b550883ec0c",
   "title": "行为 SDK → BP_ce_option_shot_gate：Player__tick_shooters_for_ability_card 0x40a9c0 的序言后 `mov edx,[ebp+8]; sub esp,0xc`（两条完整指令，6 字节，无相对寻址 / 无窗内跳转目标）。选 6 字节是因为 thcrap 的 sourcecave 下限是 5 字节：上一版只取 3 字节的 `mov edx,[ebp+8]`，thcrap 直接以 `cavesize too small` 拒绝装载，SDK 门回读失败后触发 `restore_alloc_bound` 降级路径 —— 存档/编成里已有新 id 时会踩出「卡 id 变垃圾 / HUD 槽消失」。**不能用 `0x40a9cb` 的 6 字节 `je`**：thcrap 的 cave_fix 不重定位 `0f 84 rel32`，复制进 cave 后会跳飞。handler 返回 1 = 执行完整两条原指令；返回 0 且 retaddr=0x40aa9e 才跳过本组（`mov esp,ebp` 重设栈顶）。⚠ 当前 C 侧 `CE_OPTION_SHOT_GATE_ENABLE` 默认 0（不启用），只为让门回读通过并保留实现。"
  },
  "ce_bullet_damage_final": {
   "addr": "0x45e7ff",
   "cavesize": 9,
   "expected": "ff77088b0d5cf65100",
   "title": "行为 SDK → BP_ce_bullet_damage_final：PlayerBullet__create 0x45e320 的**卡链遍历出口**：0x45e7dc mov eax,[0x4cf298]（ABILITY_MGR）→ 0x45e7e4 mov esi,[eax+0x1c]（卡链头）→ 0x45e7e9 je 0x45e7ff（**没卡也跳这里**）→ 0x45e7f0 mov ecx,[esi] / 0x45e7f2 push edi（bullet）/ 0x45e7f5 call [eax+0x28] / 0x45e7f8 mov esi,[esi+4] / 0x45e7fb test esi,esi / 0x45e7fd jne 0x45e7f0 —— **五家（含原生 25 Saki 0x40c310 / 54 Momoyo 0x411010）全跑完**。覆盖字节 = `ff 77 08`（push [edi+8]）+ `8b 0d 5c f6 51 00`（mov ecx,[0x51f65c] = ANM_MANAGER_PTR，下一条 0x45e808 call 0x488b40 = AnmManager__get_vm_with_id），两条都无相对寻址；窗内无跳转靶（0x45e7e9 的 je 落在**站点本身** ⇒ 跳到站点 = 走断点，合法）；EDI 此刻仍是 bullet（0x45e837 才 push [edi+0x9c] 把它交给伤害源 0x45dfa0），ESI/EDX 已换用 ⇒ **只信 EDI**。\n为什么在这里：我们的倍率原来落在**同槽** `+0x28` 的桩里，而原生 25/54 是**从 shooter 重算**派 ⇒ 谁在链里靠后谁说了算，我们的 ×1.25 会被**静默抹掉**（卡链顺序 = 获得/存档顺序，不可控）。搬到遍历出口 = 永远乘在**别人的最终值**上（语义 = 乘法叠加，×1.25 × 1.4 = ×1.75，不是覆盖）。\n★ **不重复施加**：本站点是**唯一**落笔点 —— `CE_CARD` 的 `+0x28` 桩不再缩放（普查：全 exe `call [reg+0x28]` 8 处，只有 0x45e7f5 在卡链上，其余 7 处是别的对象类型）⇒ 不需要「本帧是否已施加」的标记，也就没有标记失效这一档。详见 BP30-IMPLEMENTATION.md §1"
  },
  "ce_death_result": {
   "addr": "0x45c127",
   "cavesize": 6,
   "expected": "85ff8b7c2414",
   "title": "行为 SDK → BP_ce_death_result：Player__on_tick__body 0x45be90 的 case 4（决死窗口走完那一帧）里**唯一的那次判定**：0x45c106 mov eax,[0x4cf298] → 0x45c10b mov esi,[eax+0x1c] → 0x45c110 je 0x45c133（没卡 ⇒ 没人取消）；0x45c112 xor edi,edi（acc=0）→ 0x45c116 push edi（当参数）→ 0x45c11c call [eax+0xc] → 0x45c121 or edi,eax → 0x45c123/0x45c125 循环把**五家全跑完** → **0x45c127 test edi,edi（★ 本站点）** → 0x45c129 mov edi,[esp+0x14]（还原 EDI = player）→ 0x45c12d jne 0x45c3d1（acc≠0 ⇒ 取消死亡）→ 0x45c135 call 0x45d090（acc=0 ⇒ 扣命）。handler 读 **EDI = acc**：1 = 这次死亡已被取消、0 = 会扣命，写进 sdk_core 的**死亡结果闩锁**（只读 getter 供卡侧在 `+0x0c`/`+0x14`/记账回调里查）。\n窗口 = `85 ff`（test edi,edi）+ `8b 7c 24 14`（mov edi,[esp+0x14]）共 **6 字节**：两条都无相对寻址、窗内无跳转靶（0x45c110 的 je 落 0x45c133、0x45c12d 的 jne 落窗后），且**两条指令在两条臂上都要执行**（直线码、分支在窗后）⇒ **一个站点就看得到两种结果**。\n★ 为什么不是 0x45c3d1（取消臂）/ 0x45c135（扣命臂）：0x45c3d1 是 case 3 与 case 4 的**共用尾**（一手不止字节：Ghidra 反编译把这个尾部直接标成 `LAB_0045c3d1`，`case 3` 与 `case 4` 都 `goto` 它；汇编侧 0x45c0db 的 je / 0x45c0ef 的 jmp 也跳它）⇒ 会把非死亡帧记成「取消」；0x45c135 是 `call rel32`（窗内相对寻址，纪律禁止）。\n★ `[esp+0x14]` 在 cave 里读的是**同一个栈**：一手 thcrap/src/bp_entry.asm（pusha/pushf → handler → popf/popa → 回放原指令 → jmp 站点+len ⇒ cave 里 ESP = 站点处 ESP），与已实跑的 `ce_option_shot_gate`（窗内含 `sub esp,0xc`）、`ce_shop_bought`（窗内三条 push）同机制。\n本站点**只记事实、不改控制流**：返回 BP_EXEC_ORIGINAL，原两条指令原样回放，`test` 设的 ZF 正好给 0x45c12d 用。"
  },
  "ce_chain_node_guard": {
   "addr": "0x401357",
   "cavesize": 7,
   "expected": "803dbe17520000",
   "title": "行为 SDK → BP_ce_chain_node_guard：UpdateFuncRegistry__run_all_on_tick 0x4012e0 的**派发**上：0x40131f mov edi,[ecx]（更新链节点）→ 0x401323 cmp [edi+8],0 / je（没 on_tick 就跳过）→ 0x401329 test byte [edi+4],2（暂停位）→ 0x40134f mov ecx,[edi+0x24]（this）→ **本站点 0x401352 mov eax,[edi+8]** → 0x401355 call eax（下一条 0x401357）。窗口 = `8b 47 08` + `ff d0`：两条都无相对寻址、窗内无跳转靶（分派在 0x401373 之后）。\n节点 = `UpdateFunc__operator_new` 0x401530 的 0x28 字节对象（布局一手见 sdk.c）：+0x00 排序键 / +0x04 flags / +0x08 **on_tick** / +0x0c 注册期回调 / +0x10 第二回调 / +0x14 链格（自指；+0x18 next、+0x1c prev，register__on_tick 0x401180 按 +0x00 升序插入）/ +0x24 **this**。注册者 = Supervisor/AbilityManager/ItemManager/EnemyManager/BulletManager/EffectManager/Player/Stage/AsciiManager/Bomb*（运行期动态）等；摘链者 = 各管理器自己的析构（0x4015a0 的调用者）。\n崩溃签名（crashdumps 5 次全同）：0xC0000005 **写 0x1** + Eax=1 + EIP 在堆（无 DEP ⇒ 堆数据当指令执行）⇒ `[edi+8]` 悬垂。触发器（用户实测）= 射击中 + 上线收点那一刻。\nhandler 三件事：① 首见每个节点打一行「链地图」（node/order/flags/on_tick/this/vt + 各自模块名）；② 已知节点的 +8/+0x24 变了就打 ★ CHANGED（启动期一次分配、正常永不改变 ⇒ 直接抓到改写者）；③ on_tick 不是可执行地址（VirtualQuery 判页保护）⇒ **跳过这次 call**：eax=1、retaddr=0x401357 = 跳转表 0x4013f8[1]=0x4013b7 的纯空转路径（不碰 edi、不摘链；[0]=0x401397 会去 0x4015a0 摘链，对垃圾节点更危险）。返回 0 = 不回放 cave 里的两条原指令（栈上没有 call 的压栈 ⇒ 与「调用后」同形）。"
  },
  "ce_shop_bought": {
   "addr": "0x4183ea",
   "cavesize": 12,
   "expected": "6a066a006a068d8f28020000",
   "title": "商店走两遍 → BP_ce_shop_bought：AbilityShop__on_tick 成交分支（状态已置 5）：push 6 / push 0 / push 6 / lea ecx,[edi+0x228]，无相对寻址；只记「本次进店成交」"
  },
  "ce_shop_reopen": {
   "addr": "0x443b05",
   "cavesize": 5,
   "expected": "a900000200",
   "title": "商店走两遍 → BP_ce_shop_reopen：GameThread__on_tick：test eax,0x20000（eax = GameThread+0xb0，esi = this）；店刚关且成交过且有名额 → eax |= 0x20000 再开一家"
  },
  "ce_snd_gate": {
   "addr": "0x476410",
   "cavesize": 5,
   "expected": "558bec6aff",
   "title": "音效表门：SoundManager::init 入口 → BP_ce_snd_gate（填语音 blob 与新行配置 + I1/I2 自检，然后放行让引擎建 buffer）"
  }
 }
}