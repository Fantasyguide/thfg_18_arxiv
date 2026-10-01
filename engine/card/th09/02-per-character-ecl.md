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
  **`ecl` 键**就是 TH09 的完整指令名（`54__anm_set_main` / `92…` / `111__bullet_effects` /
  `116__laser_index` / `127__boss_set` / `128__spellcard_effect` / `131__enemy_life_set` …）
  ⇒ **可以照它生成一份 `th09.eclm`**，之后反编译就有名字了。

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

## 4. 🟡 未钉死：`plNN` 号 ↔ 哪个角色

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
