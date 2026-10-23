# eclmap（手写层）

> **版本**：本目录按版本命名（`<版本目录名>.eclm`，如 `th19.v1.10c.eclm`）。

`gen_eclmap.py` 只能给 **ExpHP `th-re-data` 有 `labels.json` 的版本**生成 eclmap；
**TH19／TH20 没有**（`labels.json` 最高到 `th185.v1.00a`）⇒ 这些版本的名表只能**手写**，放这里入库。

## 格式

```
!eclmap
!ins_names
522 setSpellName   # 证据: 0x… handler；判据: …
```

* ★ **名字不能以 `ins_` 开头** —— `thecl` 会报 `mnemonic can't start with 'ins_'`。
* ★ **没定的 opcode 不要写** —— `thecl` 自动回落 `ins_NNN`，写了反而会误导。
* `!ins_signatures` 段可选（只影响 `thecl -c` 编译方向；`-d` 转储不需要）。

## 证据纪律

每条形如 `NN name  # 证据: <handler 地址>；<判据>`。判据必须是"读到的东西"
（从哪个偏移取参、写哪个字段、与哪个调用点的形态一致），★ 不接受"像是什么"。

## 自检（改名不改参）

```bash
thecl -d 19 -x -j -m tooling/eclmap/maps/th19.v1.10c.eclm <pl02.ecl> /tmp/a.txt
thecl -d 19 -x -j                                   <pl02.ecl> /tmp/b.txt
# 归一化（把所有 `名字(` 换成 `FN(`）后必须逐字相同
```

★ 2026-10-18 用最小 map 验证过这条管道：归一化后完全一致；同时确认
`local/th19.v1.10c/_ext/ecl/*.txt` 现存版本就是**无 map** 的转储（与 `-m` 版归一化后相同）。
