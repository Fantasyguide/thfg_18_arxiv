#!/usr/bin/env python3
"""derive_astralup.py —— 兼容入口（**文件名保留**：`make voice` 与既有文档调的是它）。

真正的表与逻辑搬到了同目录的 [`derive_retail_se.py`](derive_retail_se.py) ——
「从 `local/` 现取别作的 `se_*.wav`」现在不止 ASTRALUP 一条
（2026-09-20 起还有 TH12 的 `se_ufoalert` 等，见那张表的 `一手` 列）。
本文件只做转发，行为与旧版一致（含「缺样本就报错停下、不去下载」）。

历史（这一支的由来，别删）：用户 2026-09-16n 定案「TH13 的 `se_astralup.wav` 随 mod 发」——
TH13 的 59 枚 SE 与 TH18 的 71 枚里**同名 58 枚逐字节完全相同**，而 TH13 唯一多出来的那枚
正是 `se_astralup.wav`（灵界槽跨格的升格音，`th13:0x43d574 MOV EDX,0x44`），TH18 的 84 行里
没有它、也无波形替身 ⇒ 走本仓**已有的音效表扩容通道**（`id 0x54..0x73`）。版权口径与做法见
`derive_retail_se.py` 的文件头（逐字节拷、产物 gitignore、源留 `local/`）。
"""
import os
import runpy

HERE = os.path.dirname(os.path.abspath(__file__))
runpy.run_path(os.path.join(HERE, "derive_retail_se.py"), run_name="__main__")
