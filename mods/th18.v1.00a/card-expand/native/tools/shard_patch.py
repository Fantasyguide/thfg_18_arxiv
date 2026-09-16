#!/usr/bin/env python3
"""shard_patch.py —— **分片实验装置**：把同一个 patch 的站点声明拆到两个 archive 里。

## 要回答的问题（决定 TH12 / TH08 / TH09 的架构）

「thcrap 的站点（断点跳板）预算」到底是**每个 patch 各算一份**，还是**整个进程共享一份**？
  · 若**每 patch 一份** ⇒ 分身几乎免费：把站点声明拆到 N 个 archive，config 里全挂即可（名额上限 × N）；
  · 若**每进程一份** ⇒ 上限是硬的，只能「合并站点 / 一个入场时刻兼做几件事」。

## 怎么做（判读表）

1. 仓库侧生成分片：`python3 native/tools/shard_patch.py`（写出 `patch-shard/`）；
2. 发布到 modkit：`MODKIT_DIR=<modkit> python3 tools/deploy.py --shard`（它会把 A 档写成原来的
   `th18_card_expand_255`，并**新建** `th18_card_expand_sites2` 这个 archive）；
3. 在启动器里把 `th18_card_expand_sites2` 也勾上（等价于往 `mods/enabled.json` 的 `patches` 里加这个 id）；
4. 进游戏，照常打枪/收点，跑一局。

| 结果 | 结论 | 下一步 |
| --- | --- | --- |
| **不崩** | 预算 = **每 patch** | TH12/TH08/TH09 走「多 archive 分片」，名额基本不再是约束；把站点按主题分档（例如按卡/按子系统） |
| **崩** | 预算 = **每进程** | 上限是硬的 ⇒ 按 `METHOD.md` 铁律「站点数预算」做**合并**（并入每帧家务 / 合并相邻站点 / 观察者钩子只在 patch-test 里） |

## ⚠ 判读前提（两条，缺一条实验就无效）

1. **unsplit 的总数必须落在「已知会崩」的区间**：只有「单档会崩、分片后不崩」才说明预算与 patch 数量有关。
   实测已知：单档 **22 条安全 / 24 条崩**（口径 = 本 patch 的 `breakpoints` 条数）。所以实验要用**总数 ≥24** 的
   站点集去分片；如果拿一个本来就安全的总数（例如现在的 20 条）去分，两边都不崩，什么也证明不了。
2. **每一条被声明的站点都必须有「已导出的 handler」**：thcrap 在 `GetProcAddress` 拿不到 `BP_<名字>` 时
   会**跳过**那条断点 ⇒ 它既不生效、**也不占跳板池名额** ⇒ 拿它凑数等于白凑。
   本项目里这件事由 `tools/gen_def.py` 保证（`.def` 由声明集生成）⇒ **先加回站点、再跑 gen_def、再分片**。

`binhacks`/`codecaves` **只在 A 档**（它们是同一份实验装置的一部分，不该跟着分）。
"""
import argparse
import json
import os
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
MOD = os.path.dirname(NATIVE)
SRC = os.path.join(MOD, "patch", "th18.v1.00a.js")
OUT = os.path.join(MOD, "patch-shard")
PKG_A = "th18_card_expand_255"
PKG_B = "th18_card_expand_sites2"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--split", type=int, default=0,
                    help="A 档保留多少条 breakpoints（默认对半）")
    a = ap.parse_args()

    d = json.load(open(SRC, encoding="utf-8"))
    bps = list(d.get("breakpoints", {}).items())
    n = a.split or (len(bps) + 1) // 2
    bp_a, bp_b = bps[:n], bps[n:]

    a_dir = os.path.join(OUT, PKG_A)
    b_dir = os.path.join(OUT, PKG_B)
    for p in (a_dir, b_dir):
        os.makedirs(p, exist_ok=True)

    ja = {"codecaves": d.get("codecaves", {}), "binhacks": d.get("binhacks", {}),
          "breakpoints": dict(bp_a)}
    json.dump(ja, open(os.path.join(a_dir, "th18.v1.00a.js"), "w", encoding="utf-8"),
              ensure_ascii=False, indent=1)
    jb = {"breakpoints": dict(bp_b)}
    json.dump(jb, open(os.path.join(b_dir, "th18.v1.00a.js"), "w", encoding="utf-8"),
              ensure_ascii=False, indent=1)
    json.dump({"id": PKG_B, "title": "TH18 卡表扩容 —— 分片实验（站点声明第二档）",
               "dependencies": ["nmlgc/base_tsa"], "update": False},
              open(os.path.join(b_dir, "patch.js"), "w", encoding="utf-8"),
              ensure_ascii=False, indent=2)
    json.dump({}, open(os.path.join(b_dir, "files.js"), "w", encoding="utf-8"))

    print("分片写出 → %s" % OUT)
    print("  %-26s breakpoints %2d 条 + binhacks %d + codecaves %d"
          % (PKG_A, len(bp_a), len(ja["binhacks"]), len(ja["codecaves"])))
    print("  %-26s breakpoints %2d 条（只声明，不搬 binhack/codecave）" % (PKG_B, len(bp_b)))
    print("判读：挂上第二档后**不崩** ⇒ 预算=每 patch（分片可用）；**崩** ⇒ 预算=每进程（必须合并站点）。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
