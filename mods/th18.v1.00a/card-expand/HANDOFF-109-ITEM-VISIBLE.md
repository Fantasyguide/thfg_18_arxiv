# HANDOFF-109-ITEM-VISIBLE —— 灵力道具「生成/存活/位置全对，却看不见」的定案与两分钟实验

> **状态**：**诊断交接**（不改玩法、只加日志/换常量，均可回退）。
> **为何单独成文**：`native/cards/pofv.c` 此刻**有并行改动**在跑 ⇒ 本件**不碰它**，写成可粘贴说明 + 实验方案，交给在改那份文件的人落笔。
> **一手出处**：本仓既有研究（**不要重新挖**）：`engine/versus/th19/06-aura.md`（TH19 除霊オーラ / 霊力アイテム / 各角色 C1–C4）、`engine/item/th15/01-tick-shake-tint-and-graze-item.md`（道具 tick / 出生门）、`engine/card/th18/*`（原生卡侧）。

## 1. 结论：问题 100% 在「画」

用 13:26 那局的原始日志行（逐字）：

```
pofv: [ITEM]  active=6 vm=6 spawn_ok=86 spawn_fail=0 | cancel=0 kill=0 aura=6 reigeki=0
pofv: [ITEMP] item=(89,395)  player=(94,400)   age=26..26   vm=147849002 alive=1 spr=757 | poc_y=128 pl_y=400
pofv: [ITEMP] item=(-6,330)  player=(-19,361)  age=11..11   vm=270466471 alive=1 spr=757 | poc_y=128 pl_y=361
pofv: [ITEMP] item=(17,422)  player=(-115,333) age=256..256 vm=445808069 alive=1 spr=757 | poc_y=128 pl_y=333
pofv: [ITEMP] item=(-42,316) player=(40,376)   age=227..227 vm=612925279 alive=1 spr=757 | poc_y=128 pl_y=376
pofv: [ITEMP] item=(0,182)   player=(0,358)    age=50..50   vm=831651305 alive=1 spr=757 | poc_y=128 pl_y=358
```

逐条排除：

| 假设 | 日志判据 | 结论 |
| --- | --- | --- |
| 没生成 | `spawn_ok=86`、`spawn_fail=0` | ✗ 排除 |
| 没 VM | `vm=6`（`vm == active`） | ✗ 排除 |
| VM 死了 | `alive=1` | ✗ 排除 |
| 贴图不对 | `spr=757`（= `TH19_ITEM_KIND9` 蓝点，14×14） | ✗ 排除 |
| 瞬间被吃 | `age` 到 **50 / 227 / 256** 仍在 | ✗ 排除 |
| 坐标跑到天边 | `item≈player`（相距几像素到几十像素） | ✗ 排除 |
| POC 收走 | `pl_y=400 > poc_y=128` | ✗ 排除 |

⇒ ★ **只剩"被画在哪儿 / 画成什么样"** ✓。owner 的实跑描述「只在奇怪的边边角角偶尔看到一颗」= **"被画到了别的坐标空间"** 的典型症状。

★ **另一条强证据（分角色）**：owner 实测「换**魔理沙**每颗弹的道具都看得见，**灵梦**看不见」。
- 魔理沙 = `kind 1` = `POFV_AURA_SHAPE_BAND`（竖带）⇒ 命中点分布在**离自机很远的弹**上；
- 灵梦 = `kind 0` = `POFV_AURA_SHAPE_CIRCLE`，圆的大小 = `s_aura_r` ⇒ 无蓄力时几乎为 0 ⇒ 只命中**贴脸**的弹。
- ⇒ 两者都"看得见与否"与**命中点离自机多远**相关 ⇒ 与"画的位置/空间"这条结论**一致**。

## 2. 三行诊断：把 VM 自己眼里的字段打出来

`[ITEMP]` 现在打的是**我们写进去的值**，不是 **VM 里实际生效的值**。补这一段（**宏都现成**，`engine.h` / `sdk.h`）：

```c
{   /* 临时诊断：VM 自己眼里的 pos/scale/alpha/sprite */
    uint8_t *v = ce_anm_get_vm((uint32_t)first_vm);
    const float *vp  = (const float *)(v + CE_ANM_VM_POS);   /* 实体坐标（ce_anm_set_pos 写的）*/
    const float *vp2 = (const float *)(v + 0x30);            /* ExpHP prefix pos（绘制点）*/
    float    sc  = *(const float *)(v + CE_ANM_VM_SCALE);
    uint32_t c1  = *(const uint32_t *)(v + CE_ANM_VM_COLOR1);
    int      spr = *(const int32_t *)(v + CE_ANM_VM_SPRITE_ID);
    ce_log("pofv: [ITEMVM] set=(%.0f,%.0f) vm_ent=(%.0f,%.0f) vm_draw=(%.0f,%.0f) scale=%.3f c1=%08x spr=%d",
           (double)ix, (double)iy, (double)vp[0], (double)vp[1],
           (double)vp2[0], (double)vp2[1], (double)sc, c1, spr);
}
```

读法（**三选一，一眼分家**）：

| 现象 | 病因 | 落点 |
| --- | --- | --- |
| `vm_ent ≈ set`，但 **`scale≈0`** 或 **`c1` 的 α≈0** | ★ **脚本/参数**：`423_pofv109_power_item.anm.txt` 起手 `scale(0,0)` + `alpha(96)` ⇒ 7 px、37% | 在闸上改：按一手核 TH19 谁给 base scale |
| ★ **`vm_draw ≠ vm_ent`**（绘制点还在 0,0 或别的值） | ★★ **写了错的字段/空间** ⇒ 道具被画到 (0,0) 那种角落 | 修 `power_vm_sync` 的写点 |
| 两者都对、scale/α 也对，却仍看不见 | ★ **那台 VM 的层/列表**与暴风不同 | 把 spawn 层从 **13** 换成暴风的 **6** 试一发 |

## 3. 一分钟 A/B（不改玩法、可回退）

在 `power_vm_sync` 里**临时**把 script 换成**已经看得见的**那支（暴风 **396** 或火花 **431**）：

- 换成 396/431 后**看得见** ⇒ ★ 病灶在 **脚本 423**（`scale` / `alpha` / `pos` 那几行）；
- 换成 396/431 后**还是看不见**（或只在角落）⇒ ★ 病灶在 **位置/列表**（层 13 ⇄ 6，或"实体坐标 vs 绘制点"两个字段）。

以上两步**都只加日志/换常量**，全部可回退。

## 4. 纪律与并行约定

- ★ **本件不修改 `pofv.c`**（交给正在改那份文件的人落笔）；
- ★ 落笔仍按本仓规矩：一笔一提交、`make` / `make check` / `check-docs` **三闸全绿才提交**；
- ★ 本检出落后 `origin/main` 200+ 提交 ⇒ 合入共享线前需先 rebase 并重跑闸（见 `TUNING.md` 的订正条目 `1372a71b`）。
