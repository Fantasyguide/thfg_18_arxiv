#!/usr/bin/env python3
"""gen_def.py —— 从「本 patch 实际声明的断点」反推 th18_card_expand.def（导出表）。

为什么要生成而不是手写：`sites.py check` 的双向对账（patch 声明 ↔ def 导出）今天抓到过两类事故
（2026-09-17）：
  · patch 声明了断点而 def 没导出 ⇒ thcrap `GetProcAddress` 拿不到处理函数，**游戏里静默不生效**；
  · def 导出了而 patch 没声明（僵尸导出）⇒ 启动器报 `[!] dll 导出了 … 但没有已选 patch 声明断点`。
两种都是「改了站点表却忘了同步另一头」。**让 def 由声明集生成，这类不一致就不可能出现。**

输入 = `sites.py` 的 `emitted_breakpoint_names()`（与 `gen` 写 patch 的**同一条代码路径**），
所以 def 与 patch 永远一致；`.text` 的定位是 PE 节表一手值（th18 v1.00a：RVA 0x1000 / 大小 0xab7ca / 文件 0x400）。

用法：python3 tools/gen_def.py            # 写出 ../th18_card_expand.def
      python3 tools/gen_def.py --check     # 只对账，不写（不一致则退出码 1）
"""
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
sys.path.insert(0, NATIVE)
import sites as S                                            # noqa: E402

TEXT_OFF, TEXT_VA, TEXT_SIZE = 0x400, 0x401000, 0xab7ca       # 一手：PE 节表


def declared_names():
    with open(S.EXE, "rb") as f:
        text = f.read()[TEXT_OFF:TEXT_OFF + TEXT_SIZE]
    return S.emitted_breakpoint_names(text, TEXT_VA)


def render(names):
    out = ["LIBRARY th18_card_expand.dll", "EXPORTS", "\tthcrap_plugin_init\t@1"]
    for i, n in enumerate(sorted(names), start=2):   # 排序 ⇒ 输出确定（否则 --check 每次都“不一致”）
        out.append("\tBP_%s\t@%d" % (n, i))
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="只对账")
    a = ap.parse_args()
    names = declared_names()
    want = render(names)
    path = os.path.join(NATIVE, "th18_card_expand.def")
    have = open(path, encoding="utf-8").read() if os.path.exists(path) else ""
    if a.check:
        # ★ 2026-10-11 修：`.def` 的 `;` 是**注释到行尾**（MS 格式）⇒ 先把注释剥掉再抓 `BP_*`，
        #   否则**注释掉的**导出（例：`; BP_ce_input_synth  @12`，2026-09-13 拆线留下的历史注）
        #   会被算成「def 有而 patch 没有的僵尸导出」，把 deploy.py 的发布闸**永久**卡红。
        #   判据 = **集合相等**（文件里允许有人类注释与不同排序；`gen_def.py` 无参运行会重写成规范形）。
        code = "\n".join(l.split(";", 1)[0] for l in have.splitlines())
        got = set(re.findall(r"BP_([a-z0-9_]+)", code))
        if have.strip() == want.strip() or got == set(names):
            print("gen_def: ✅ def 与 patch 声明一致（%d 个导出 = 1 + %d 断点）" % (len(names) + 1, len(names)))
            return 0
        print("gen_def: ❌ 不一致 —— 声明 %d 条 / def %d 条" % (len(names), len(got)))
        print("    patch 有 def 没有：", sorted(set(names) - got) or "无")
        print("    def 有 patch 没有：", sorted(got - set(names)) or "无")
        return 1
    with open(path, "w", encoding="utf-8") as f:
        f.write(want)
    print("gen_def: 写出 %s（%d 个导出 = 1 + %d 断点）" % (path, len(names) + 1, len(names)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
