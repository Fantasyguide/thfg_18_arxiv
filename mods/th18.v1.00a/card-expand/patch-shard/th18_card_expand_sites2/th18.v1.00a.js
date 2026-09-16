{
 "breakpoints": {
  "ce_enemy_drop": {
   "addr": "0x430510",
   "cavesize": 6,
   "expected": "558bec83ec20",
   "title": "行为 SDK → BP_ce_enemy_drop：Enemy__drop_items_and_notify_cards 入口（thiscall，**ecx = zEnemyDrop = enemy+0x623c，不是敌人对象**；一手 0x430499 `lea ecx,[edi+0x623c]`、ECL dropItems 0x42d0c0 同）：push ebp/mov ebp,esp/sub esp,0x20，无相对寻址。在引擎撒附加表**之前**沿卡链表调 on_enemy_drop_pre(counts)，counts = zEnemyDrop+0x04 起的 20 个 int32（type 1..0x13，主掉落 +0x00 已经撒过）；卡循环之后 SDK 施加掉落乘法层 ce_drop_apply()（EVENTS-CONTRACT §2）"
  },
  "ce_enemy_shot_slot": {
   "addr": "0x433ffa",
   "cavesize": 8,
   "expected": "f30f108f44510000",
   "title": "行为 SDK → BP_ce_enemy_shot_slot：敌人 ECL opcode 0x259（etOn、handler 0x433f34）里「发弹三步」的汇聚点：movss xmm1,[edi+0x5144]（8 字节、**窗内无相对寻址**）。edi = &enemy->data（enemy+0x122c，一手 0x430d30 `add ecx,0x122c`）、槽号 i 在 [ebp-0x56c]（0x433f3d 写）、esi = &data + i*0x488（0x433fcf/0x433fd9）。handler 返回非 0 时 regs->retaddr = 0x43402d（= 该 opcode 的收尾 `jmp 0x436417`）⇒ **本帧不发射这一槽**，跳过 et_protect_range 的 set / call 0x427810(__fire) / 复位三步。零内存写、不动敌人生命周期。详见 local/_scratch/research-108-shot-slot-off.md"
  },
  "ce_item_frame": {
   "addr": "0x446ee3",
   "cavesize": 7,
   "expected": "833da4f24c0000",
   "title": "行为 SDK → BP_ce_item_frame：ItemManager tick 0x446ec0 的门卫 CMP（cmp dword [0x4cf2a4],0，**单条 7 字节**）：每个正常游玩帧、道具池 body 前到达一次（菜单/暂停帧跳走不经过）；handler 不读 CMP 结果、不改标志。沿卡链表调 on_item_frame()"
  },
  "ce_item_collect": {
   "addr": "0x44625f",
   "cavesize": 6,
   "expected": "8b97780c0000",
   "title": "行为 SDK → BP_ce_item_collect：collect 分派前一条 mov edx,[edi+0xc78]（edi = 道具槽，**单条 6 字节**）：此时本道具的副作用都没发生（分值未入 SCORE）。handler 从**内存** *(uint32_t*)(edi+0xc78) 读 type（EDX 还没写），不是读寄存器。沿卡链表调 on_item_collect(card, type, item)"
  },
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
  "ce_enemy_death": {
   "addr": "0x430240",
   "cavesize": 6,
   "expected": "558bec83ec14",
   "title": "行为 SDK → BP_ce_enemy_death：FUN_00430240（SDK 名 Enemy__die）入口：push ebp/mov ebp,esp/sub esp,0x14，无相对寻址。thiscall，ecx = zEnemy；**引擎撒主掉落之前**（0x430493 才是撒、0x43049f 才通知卡）、链表仍挂着，恒定 mov eax,1 返回 ⇒ 一敌一生正好一次。handler 用 regs->retaddr 分死因（0x42f3b7 = 打死主路 / 0x42ee90 = 炸弹秒杀 / 0x430f59 = ECL die / killAll 三兄弟），沿卡链表调 on_enemy_death(&ev)。详见 docs/superpowers/specs/2026-09-12-shared-enemy-death-event-design.md"
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