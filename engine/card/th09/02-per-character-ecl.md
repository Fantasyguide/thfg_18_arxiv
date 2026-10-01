# 02 — TH09 的 **16 份 `plNN.ecl`** = 「被送到对面的那个 boss」= 每角色一份的符卡本体

> **版本**：TH09 v1.50a（`local/th09.v1.50a/th09.exe` / `th09.dat`，imagebase `0x400000`）。
> **方法**：一手 —— `thdat -x9` 抽 `th09.dat` 的 `plNN.ecl` ＋ `thecl -d9` 反编译
> ＋ ExpHP `local/vendor/th-re-data/data/th09.v1.50a/labels.json` 的 **ECL opcode 名表**。
> **可信度**：文件清单与「每角色一份」✅（`thdat -l9` 直接列出 16 个）；
> 「它就是被送过去的那只 boss」✅（与 `engine/versus/th09/01-versus-loop.md` §2.3 的 A 链一致）；
> **`plNN` 号 ↔ 角色** 🟡（**未钉死**，见 §4）。2026-10-01。

## 1. 发现：资源层是**按角色**分的，而且多一份 `ecl`

`thdat -l9 th09.dat` 里每个角色有 **6 个文件**（与 `engine/versus/th09/01-versus-loop.md` §2.4
的 `sht` 表 `0x4a1238` 完全对上：**16 角色 × 6 指针**）：

```
plNN.sht   plNN.anm   plNNb.anm   plNN.msg   plNN_match.msg   ★ plNN.ecl
```

⇒ 实测清单（16 份，`thdat -l9 th09.dat | grep '\.ecl'`）：

```
enemy.ecl 5296
pl00.ecl 3480   pl01.ecl 5556   pl02.ecl 6120   pl03.ecl 5316
pl04.ecl 3756   pl05.ecl 3088   pl06.ecl 5576   pl07.ecl 3520
pl08.ecl 4964   pl09.ecl 5188   pl10.ecl 5168   pl11.ecl 5496
pl12.ecl 5112   pl13.ecl 6632   pl14.ecl 5232   pl15.ecl 5460
```

## 2. 这就是「送到对面的 boss」跑的东西

一手链（`engine/versus/th09/01-versus-loop.md` §2.3 的 A 链）：
CA 发动 → `0x403e50(档, 档, 0, 名字)` → `0x4a00c0[档]` 的载体 → `0x440c80` →
**`0x40f1d0(对手 EM, type, pos, …)`** = 在**对手的 `EnemyManager` 里生成一只敌机**；
charge 等级 `min(等级+1, 3)`，**等级 3 = 终点（boss 档）**。

⇒ **那只敌机 = 你（自机）的 boss 态副本**，它跑的就是 **`plNN.ecl`**（`NN` = 你的角色号）。
**所以「C4 的符卡」在一手里就是这一份 ECL 发的东西。**

## 3. 反编译是通的（`thecl` + ExpHP 的 opcode 名表）

`local/vendor/thtk/build/{thdat,thecl}` 现编即用（`thtk` 支持 th09，只是 `tooling/thtk/unpack.py`
的 `GAMES` 表没列 th09）。

- 抽：`thdat -x9 local/th09.v1.50a/th09.dat pl00.ecl … -C /tmp/th09all`
- 反编译：`thecl -d9 /tmp/th09all/pl00.ecl`（**没有 `th09.eclm`** —— 仓库只有 th08/th11–18）
- ★ **opcode 名表在 ExpHP 那边**：`local/vendor/th-re-data/data/th09.v1.50a/labels.json` 的
  **`ecl` 键**就是 TH09 的指令名（`54__anm_set_main` / `111__bullet_effects` /
  `116__laser_index` / `127__boss_set` / `128__spellcard_effect` / `131__enemy_life_set` …）
- ★★ **现在有生成器了**：`tooling/eclmap/gen_eclmap.py`（**新增**）把上面那个 `ecl` 键转成 `thecl` 要的
  eclmap，写完是 `local/vendor/eclmap/eclmap/th09.eclm`：

  ```bash
  python3 tooling/eclmap/gen_eclmap.py th09.v1.50a          # → local/vendor/eclmap/eclmap/th09.eclm
  python3 tooling/eclmap/gen_eclmap.py --list               # 哪些版本能生成（th06–th18 全都能）
  thecl -d9 pl00.ecl -m local/vendor/eclmap/eclmap/th09.eclm
  ```

  自检（拿仓库已有的 `th08.eclm` 对）：生成的 `!ins_names` 段 **160 行 vs 仓库那份 159 行**
  （只差一个 `nop`）⇒ **转换是对的**；仓库那几份**多出来的段**
  （`!timeline_ins_names` / `!gvar_names` / `!gvar_types`）ExpHP 这边没有，本生成器不做。
  ⚠ 少数 opcode ExpHP 只给了裸数字 ⇒ 兜底名写成 `op_<n>`（**不能**写 `ins_<n>`：`thecl` 会拒
  「mnemonic can't start with 'ins_'」）。

`pl00.ecl` 的 `Sub0` 片段（数字 opcode，未套名表）：

```
ins_54(0); ins_80(8); ins_185(1); ins_77(24.0f, 24.0f);
ins_51([10016.0f], -80.0f, 0, Sub0_160);   /* 移动 */
ins_63([10016.0f], 128.0f);
ins_139(6, 1, #ffffffff);
+40:  ins_22(...); ins_10(..., 32); ins_50(...);
      …
      ins_99(2, 2, [10000], 1, [10017.0f], 0.5f, [10016.0f], 0.0f, 514);
      ins_101(2, 15, [10000], 1, [10017.0f], 0.5f, [10016.0f], 0.0f, 642);
      …
```
（`514` / `642` 这类尾参 = **弹的 anm 脚本号** ⇒ 想认角色，见 §4。）

## 4. `plNN` 号 ↔ 角色（🟡 owner 供给，结构自洽）

**owner 2026-10-01 供给的顺序**（原话：「不知道，可能是」——按**目录** 1-based 列）：

| 0-based | 角色 | 0-based | 角色 |
| :-: | --- | :-: | --- |
| `pl00` | 博麗霊夢 | `pl08` | 露娜萨·普莉兹姆利巴 |
| `pl01` | 霧雨魔理沙 | `pl09` | 米斯蒂娅·萝蕾拉 |
| `pl02` | 十六夜咲夜 | `pl10` | 因幡天为 |
| `pl03` | 魂魄妖夢 | `pl11` | 射命丸文 |
| `pl04` | 铃仙·优昙华院·因幡 | `pl12` | 梅蒂欣·梅兰可莉 |
| `pl05` | 琪露诺 | `pl13` | 风见幽香 |
| `pl06` | 莉莉卡·普莉兹姆利巴 | `pl14` | 小野塚小町 |
| `pl07` | 梅露兰·普莉兹姆利巴 | `pl15` | 四季映姬·夜摩仙那度 |

**结构自洽的佐证**（一手，`th09:0x4a0330`，16 项 × `0x1c` =「按角色的 `Characteristics`」）：

```
pl00 灵梦      00 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00  05 0a 03
pl01 魔理沙     00 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00  06 0a 03
pl02 咲夜      00 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00  07 0f 03
…
pl10 因幡天为   0a 05 0a 05  0a 0a 02      ← 从 pl10 起**前四列也非 0**，第 3 列变 0x0a、末列 3→2
…
pl15 映姬      0a 05 0f 05  0a 0a 02
```

⇒ **前 10（`pl00`–`pl09`）与后 6（`pl10`–`pl15`）在表里就是两组**（第 3 列 `0` vs `0x0a`、
末列 `3` vs `2`），与 TH09「**前 10 个常规 / 后 6 个 Boss**」的结构一致、
也与 owner 给的顺序**逐个吻合**。⚠ 仍标 🟡：**没有**从名字串或贴图上直证
（exe 无 Shift-JIS 名、符号表没有角色表、`plNN.msg` 压缩）。
⇒ 想 100% 钉死：抽 `etama.anm`，用 ECL 里的弹脚本号（如 `pl00` 的 `514`/`642`）回查贴图。

### 4.1 原「未钉死」段的取证记录（保留）



- **不在 exe 里**：全 exe 扫 Shift-JIS 找不到「霊夢 / 魔理沙 / 咲夜」。
- **不在符号里**：`th-re-data` 的 `statics/funcs/labels` 都没有角色表。
- **`.msg` 是压缩的**：`plNN.msg` 按 Shift-JIS 直读出来是垃圾。
- **ECL 里没有激光 opcode**：16 份都扫了 `116/117/118/167/119/120/121/154/170/171/172`
  ⇒ **全 0** ⇒ 「魔理沙 = 激光」那条**不在这份 ECL 里**（激光应该是**自机自己的 shot**，
  即 `plNN.sht`，不是送过去那只 boss 的 ECL）。

**可行的钉法（下一步）**：把 `pl00–pl15` 的 `etama.anm` 抽出来，
用 ECL 里那些**弹脚本号**（如 `514`/`642`）回查**贴图名** ⇒ 认角色
（灵梦的阴阳玉 / 咲夜的飞刀 在贴图上一眼可辨）。

## 5. 对 109 的意义

| 结论 | 怎么用 |
| --- | --- |
| 「C4 的符卡」= `plNN.ecl` 发的东西 | 要复刻 C4 的**华丽弹幕**，就该**照 `plNN.ecl` 的弹型/节奏**（per 角色一份） |
| 「魔理沙 = 激光 / 灵梦 = 大阴阳玉」 | 这是**自机自己的攻击风格**（C3 那一档），不在送的 boss 的 ECL 里；落点是 `plNN.sht` 或角色 CA 的视觉 |
| 反编译链已通 | `thdat -x9` + `thecl -d9`（+ 用 ExpHP 的 `labels.json` 生成 `th09.eclm`）⇒ 每个角色的符卡都能逐条读 |
