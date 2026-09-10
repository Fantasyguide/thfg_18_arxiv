# TH06NC — New Classic

本页是版本入口；语义结论放在 `engine/`。样本是 Steam New Classic 启动版本，当前没有可读版本资源，使用以下锚点识别：

| 项目 | 值 |
| --- | --- |
| PE | x86-64，imagebase `0x140000000` |
| SHA-256 | `07850c8c6e469c0e82c13423e6d0d096a88d693455bdacacbb44c0aa3bcce473` |
| MD5 | `b0cc28ec904e9efce2439ad857c938b5` |
| Ghidra | `local/th06nc/ghidra_projects/th06nc.exe.*`（本地忽略） |
| HTML | `127.0.0.1:6092`（本地生成，不入库） |

## 当前覆盖

- Ghidra 共 2816 个函数；**已命名 30 个**（ECL VM 与子系统 19、菜单/输入/存档 6、ANM 5），另有 120 个 ECL opcode handler 标签。
- ECL：已确认 TH06 风格记录、敌机解释器、时间线；新增主 opcode `200/201` 与时间线 `13/14` 见 [ECL 记录](../../engine/ecl/th06nc/01-th06-baseline-and-nc-deltas.md)。
- ANM：125 个文件均可解码；**VM 已定位**（`0x1400069b0`），指令集 0–35，新增 opcode 32/33/34/35 = 锚点设置，见 [ANM VM 与锚点 opcode](../../engine/anm/th06nc/01-vm-and-anchor-opcodes.md)。
- STD：7 个文件已解包，结构级语义未开始。
- SHT：尚未定位运行时入口。
- 存档/解锁：`score.dat` 读写已见；**全开秘籍已一手反出**——口令 `classiclager` 与一条 XInput 手柄序列，挂在 Lunatic 成绩页，见 [解锁秘籍](../../engine/menu/th06nc/02-classiclager-unlock-cheat.md)（调查过程见 [01](../../engine/menu/th06nc/01-result-screen-unlock-cheat-recon.md)）。

## 对外发布

符号成果单向发布到 [`Renko6626/th06nc-re-data`](https://github.com/Renko6626/th06nc-re-data)（CC0）：

```sh
python3 tooling/publish_th06nc_re_data.py local/vendor/th06nc-re-data
python3 tooling/publish_th06nc_re_data.py local/vendor/th06nc-re-data --check   # 只比对
```

⚠️ **唯一源是 `games/th06nc/symbols.json`**，公开仓库只接收不回流。
发布器会滤掉 Ghidra 的 IAT 桩注释，并且 mapfile 只输出我们自己查出来的 opcode
（上游 mapfile 标注 personal use，不能抄）。

## 复现入口

```sh
source tooling/env.sh
python tooling/ghidra/apply_th06nc_ecl.py --dry-run
python tooling/ghidra/apply_th06nc_menu.py --dry-run
python tooling/ghidra/apply_th06nc_anm.py --dry-run
python3 tooling/anm0_scan.py local/th06nc/extracted
python tooling/ghidra/symbols.py status th06nc
python tooling/ghidra/export_html.py th06nc
tooling/ghidra/serve.sh th06nc 6092
```

TH06 社区源码参考服务（只读）：

```sh
python3 -m http.server 6093 --bind 127.0.0.1 --directory local/vendor/th06-re/src/th06_102h.exe
```

不要把 exe、DAT、导出纹理或 HTML 数据库提交到仓库；它们均是本地研究输入。
