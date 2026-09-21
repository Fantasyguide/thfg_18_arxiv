#!/usr/bin/env python3
"""anm_hud_alpha_check.py —— ★ 「HUD 压暗的**过渡帧**」不变式（2026-09-15i，`make check` 里跑）。

## 它替掉的是什么

116（季节槽）与 113（灵界指示槽）的压暗/淡回**过渡时长**原本住在 anm 脚本里：

    interruptLabel(5): alphaTime(<帧数>, 0, <终点>)      ← 压暗
    interruptLabel(4): alphaTime(<帧数>, 0, 255)         ← 淡回

用户 2026-09-15 口径「**过渡帧得根治**」⇒ 两边都改成**脚本侧统一**的
`alphaTime(4, 0, 32)` / `alphaTime(4, 0, 255)`。

★★ 2026-09-21：owner 拍定「20 帧 / 50% 是 UI ⇒ 得统一成一个值」⇒ 终点与帧数**又换了一档**，
现在脚本侧的统一写法 = **`alphaTime(20, 0, 128)`（压暗，终点 128 = 50% 透明）/ `alphaTime(20, 0, 255)`（淡回）**，
与共享件 `cards/hud_dim.{h,c}` 的 `HUD_DIM_ALPHA_DIM`(128) / `HUD_DIM_FRAMES`(20) **同终点同帧数**
（一手：TH15 `front.anm` script241 的 `alphaTime(20, 0, 0)`，见
`engine/chapter/th15/02-chapter-panel-geometry-and-fades.md` §3）。

改之前，116 那两支（脚本 126/127）靠 `cards/season.c` 里一段
**`min(脚本 α, 32)` 的上限钳制**兜住脚本里的 64 —— 那是"同一件事两套实现"，
脚本改对之后**已删**（`AUDIT.md` §DD-9）。这个脚本就是那层代码的**替身**：
把「脚本必须停在统一值」做成机器判据，谁改回去 `make check` 立刻红。

## 判据（确定性；只读）

对下面 `FILES` 里每个脚本：
  ① `interruptLabel(5);` 的下一条**可执行**行必须是 `alphaTime(20, 0, 128);`
  ② `interruptLabel(4);` 的下一条**可执行**行必须是 `alphaTime(20, 0, 255);`
  ③ 这两条标签各**恰好一条**（多一条 = 有人加了第二条路，判据覆盖不到 ⇒ 也算失败）
（`//` 注释行跳过、行尾 `//` 注释也剥掉；`offsetNNN:` 之类的标签行不算可执行行。）

## 为什么只查这 9 支

它们的 alpha **只由脚本管**（C 侧不写这几支的 color）⇒ 脚本的 `alphaTime` 就是玩家看到的过渡。
116 的另外四支（外框 123 ×4 / 进度 124 / 背景 125）与 110/111 的组合 UI 是**C 每帧写 alpha**
（`s->hud.alpha`，±51/帧）⇒ 脚本里的 `alphaTime` 只是初始态，不在这里判。
⚠ 117 的就绪条（157/158）也带 label 4/5 的老写法，但卡侧**从不发 4/5**（只发 1 = 收条）
⇒ 不在本判据内（要动它得先改 `cards/animal.c` 的调用，属于另一件事）。

退出码：0 = 全部一致；1 = 有不一致（打印逐条对照）。
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
SCRIPTS = os.path.join(os.path.dirname(NATIVE), "assets", "ability", "scripts")

# 压暗 → `alphaTime(20, 0, 128)`、淡回 → `alphaTime(20, 0, 255)`（`TUNING.md` §7.1 的统一动态；
# ★ 2026-09-21 起：20 帧线性 / 终点 128（50% 透明），与共享件 `HUD_DIM_FRAMES` / `HUD_DIM_ALPHA_DIM` 同值）
LABELS = {5: "alphaTime(20, 0, 128);", 4: "alphaTime(20, 0, 255);"}

FILES = [
    # 116 天空璋：季节字（126）/ 可解放指示条（127）—— 这两支的 alpha 归脚本
    "126_season_gauge_glyph.anm.txt",
    "127_season_gauge_strip.anm.txt",
    # 113 神灵庙：指示槽四支（175 外框 A / 176 总条 / 177 外框 B / 178 游标）
    #   —— `cards/trance.c` 对这四支一起发 interrupt 5/4 ⇒ 四支都得是统一值
    "175_trance_gauge_frame.anm.txt",
    "176_trance_gauge_fill.anm.txt",
    "177_trance_gauge_frame_b.anm.txt",
    "178_trance_gauge_mark.anm.txt",
]

# 116 的 123/124/125 也一起查「有没有 4/5 标签」——它们的 alpha 虽由 C 写，
# 但标签值若被改乱，就说明有人又去动那套统一口径了（只提示，不当失败）
ADVISORY = [
    "123_season_gauge_frame.anm.txt",
    "124_season_gauge_grad.anm.txt",
    "125_season_gauge_solid.anm.txt",
]

fail = []
n_check = 0


def check(cond, msg):
    global n_check
    n_check += 1
    if not cond:
        fail.append(msg)


def executable_lines(text):
    """去掉 `//` 注释行，产出 (行号, 原文) —— 只看可执行行。"""
    out = []
    for i, raw in enumerate(text.splitlines(), 1):
        s = raw.strip()
        if not s or s.startswith("//"):
            continue
        out.append((i, raw))
    return out


def code_of(raw):
    """剥掉行尾 `//` 注释（脚本里常用它写一手留档）。"""
    return re.split(r"//", raw, 1)[0].strip()


def next_exec(lines, idx):
    """`lines` 里 idx 之后第一条可执行行（跳过 `offsetNNN:` 这类标签行）。"""
    for j in range(idx + 1, len(lines)):
        s = lines[j][1].strip()
        if re.match(r"^[A-Za-z_][A-Za-z0-9_]*:$", s):      # `offset124:` / `+20: // 20`
            continue
        return lines[j]
    return None


def main():
    for name in FILES:
        path = os.path.join(SCRIPTS, name)
        if not os.path.exists(path):
            check(False, "%s：文件不存在（脚本被改名 / 删了？）" % name)
            continue
        with open(path, encoding="utf-8") as f:
            lines = executable_lines(f.read())
        for label, want in sorted(LABELS.items()):
            hits = [i for i, (_, raw) in enumerate(lines)
                    if re.search(r"interruptLabel\(%d\);" % label, raw)]
            check(len(hits) == 1,
                  "%s：interruptLabel(%d) 应恰好 1 处，实际 %d 处" % (name, label, len(hits)))
            if len(hits) != 1:
                continue
            nxt = next_exec(lines, hits[0])
            got = code_of(nxt[1]) if nxt else "<文件结束>"
            check(got == want,
                  "%s:%d interruptLabel(%d) 的下一条是 `%s`，应为 `%s`"
                  % (name, lines[hits[0]][0], label, got, want))

    for name in ADVISORY:
        path = os.path.join(SCRIPTS, name)
        if not os.path.exists(path):
            continue
        with open(path, encoding="utf-8") as f:
            text = f.read()
        for label, want in sorted(LABELS.items()):
            for m in re.finditer(r"interruptLabel\(%d\);\s*\n\s*(alphaTime\([^)]*\);)" % label, text):
                if m.group(1) != want:
                    print("  note %s：interruptLabel(%d) 的 alphaTime 是 `%s`（C 每帧写 alpha ⇒ 只是初始态，"
                          "不判失败；与统一值 `%s` 不同）" % (name, label, m.group(1), want))

    if fail:
        print("anm_hud_alpha_check: %d/%d 项不一致" % (len(fail), n_check))
        for m in fail:
            print("  FAIL " + m)
        print("  ⇒ 压暗/淡回的过渡（脚本侧）必须停在统一值：压暗 `alphaTime(20, 0, 128)`、"
              "淡回 `alphaTime(20, 0, 255)`（`TUNING.md` §7.1 / `AUDIT.md` §DD-9）")
        return 1
    print("anm_hud_alpha_check: %d 项一致 ✅（压暗 20 帧→128 / 淡回 20 帧→255；%d 个脚本）"
          % (n_check, len(FILES)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
