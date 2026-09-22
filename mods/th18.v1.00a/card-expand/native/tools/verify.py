#!/usr/bin/env python3
"""原语验证批次（一键可跑）—— 部署 → 操作单 → 日志判读。

目标（用户 2026-10-01 口径）里每个原语的**第 ④ 件**：一个能发布、有判读说明的验证批次。
⚠ 发布版**不允许调试键**（用户 2026-09-13 口径：`ANIMAL_DEBUG_KEY 0`）⇒ 本批次不是"按键触发"，
而是「装哪张卡 + 怎么操作 + 日志该出现哪几行」的可判读清单；`--check` 会把日志**本次启动段**
（分界线 `=== th18_card_expand <时间> ===`）逐条比对。

用法
----
    python3 native/tools/verify.py --list
    python3 native/tools/verify.py storm-chain                 # 只打印操作单
    python3 native/tools/verify.py storm-chain --deploy        # 先走 tools/deploy.py（唯一发布路径）再打印
    python3 native/tools/verify.py storm-chain --check         # 读日志判读（用户实跑之后跑这条）
    python3 native/tools/verify.py all --check

路径：本脚本**不写死任何机器路径**。modkit 根走 `--modkit` / `MODKIT_DIR`；
日志走 `--log` / `CE_LOG`，缺省 = `<modkit>/game/th18_card_expand.log`。

退出码：`--check` 下，任一"必需"判据缺失 ⇒ 1（可直接进 CI / 脚本链）。
"""
import argparse
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LOG_NAME = "th18_card_expand.log"
BOOT_RE = re.compile(r"^=== th18_card_expand .*===$")

# ---------------------------------------------------------------- 原语台账
# key: 命令行用的名字；card: 卡号；impl: 实现文件；pre/steps: 操作单；
# expect: [(正则, 说明, 是否必需)]；fail: [(正则, 说明)]（命中即 FAIL）
PRIMS = {
    "familiar": dict(
        title="创建使魔（TH08 / 卡 108 幽冥组）",
        card=108, impl="cards/youkai.c",
        pre="装 108（Th08 系统卡），进关卡并吃到有 option 的 power",
        steps=[
            "Shift 低速移动，看使魔是否跟随（原生 option 位）",
            "被弹 / 放弹，确认使魔随 power 变化出现与撤回",
            "通关或删卡，看收尾是否干净（run done 那行）",
        ],
        expect=[
            (r"youkai: (run done|ledger) - \d+ familiars spawned|youkai: ledger - \d+ spawned",
              "使魔**创建过**（本局计数汇总；`ledger` = 108 补的定期账，见计划 §6.2）", True),
            (r"youkai: withdrew \d+ familiar\(s\)", "使魔按规则**撤回**（不是凭空消失）", False),
            # ★ 2026-09-22（P1）：跟随 VM 表的**自愈回收**（正常情况不该出现 —— 出现就是还有漏收路径）
            #   与「几何自述」（P2：把写进 VM 的锚点/条/数字位置打出来，与屏上所见对照即可定坐标空间）。
            (r"youkai: familiar follow-VM table self-healed",
             "跟随 VM 表自愈回收过（正常为 0；见 AUDIT §BE 详录 #56 第 1 节）", False),
            (r"youkai: gauge HUD geometry \(surface px",
             "人妖槽 HUD 的几何自述（P2：用来定「写进去的数 = 屏上的像素」）", False),
            # ★★★ 2026-09-22 夜（#23）：**本体**的 Shift 光环（TH08 `etama.anm` script54）起来了 ——
            #   实跑判据 = 按 Shift 进低速后**自机身上**应当出现那圈淡红火花云（固定色，见 TUNING §2.0zd）。
            (r"youkai: body Shift aura ON",
             "本体的 Shift 光环起来了（TH08 `Player::move` 0x44b194 → etama script54）", False),
            # ★ 2026-09-22（#26 风味）：决死窗口里全屏敌弹变红（默认开；开关见 TUNING §2.0zc）
            (r"youkai: deathbomb window - tinted \d+ live enemy bullet\(s\) red",
             "决死窗口染红生效（风味选项；退出窗口当帧还原）", False),
            # ★ 2026-09-22 夜（P-A 夜点）：开局那一行 —— 夜点初值按难度四档（一手 th08:0x43af1b 的 switch）
            (r"youkai: night point init = \d+ \(difficulty \d",
             "夜点初值已按难度起（蓝点最大分 = 夜点；每 2 刻符 +10）", False),
            # ★ 2026-09-22 夜（P-B 逢魔）：ledger 行里的逢魔位（1 = 极人 OR 极妖；加分 100/帧、对话中不涨）
            (r"fengma=[01] fscore=\d+",
             "逢魔状态与自动加分在账上可见（一手 100 分/帧、一档）", False),
            # ★★★ 2026-09-22 夜（P-F 保真缺口 · 擦弹的**颗数**）：一手是**每擦到一颗弹给 1 / 2 / 3 颗**
            #   —— `th08:0x44aac9` 必给 + `:0x44aaef`（弹路径 **且** 符卡进行中）+ `:0x44ab0f`（不是「妖単独」）。
            #   本卡映射下只出现 `n=1`（平擦）/ `n=3`（符卡中）；`n=2` 那一档 = 「妖単独」机体，TH18 无对应机体。
            #   逐指令见 `youkai_core.h` §3.11c；登记 `TUNING.md` §2.0zs / `AUDIT.md` §BE 详录 #73。
            # ★ 2026-09-23（统筹拍「三条场上门按一手接上 + 一行回退」）：`gate=` 直接给出**是哪道门挡的**
            #   —— ok 全过 / boss 场上没 boss / gauge 槽没到 +8000 / shottype 人単独非妖夢 / off 开关关掉；
            #   `n=0` = 被门挡（一手：门没过 ⇒ 这一颗弹一颗都不给）。开关 = `YK_GRAZE_FIELD_GATES`（默认 1）。
            (r"youkai: graze orbs n=[0-3] x\d+ bullet\(s\) gate=(ok|boss|gauge|shottype|off)",
             "擦弹给永点的**颗数**与**为什么没给**都在这一行（一手三道门 + 符卡门 + 机体门）", False),
            # ★ 同批：跳帧护栏（累计擦弹计数在一帧里跳变 ⇒ 夹到池容量）；正常应当一次都不出现
            (r"youkai: graze resync clamped",
             "擦弹计数在一帧里跳变被护栏夹掉（那一帧确实跳了帧；一手没有单帧上限）", False),
            # ★ 2026-09-22 夜（P-E 单人反向封顶）：反侧真的被夹在 ±2000（= 20%）时的一行
            (r"youkai: solo reverse cap engaged - the opposite side stops at -?\d+",
             "单人反向封顶生效（反侧到不了逢魔；一手 th08:0x44daa2 / :0x44dacd）", False),
            # ★ 2026-09-22 夜（P-D）：击破自家使魔 ⇒ −gauge/12 + 掉 1 颗 + 随后 50 帧收刻符不给槽
            #   （一手 th08:0x42b216 除数 0xc / th08:0x42b29b / th08:0x42b240）
            (r"youkai: familiar enemy_id \d+ killed",
             "击破自家使魔（P-D 三条都在这一支；实跑看 50 帧内 gauge 不动）", False),
            # ★ 2026-09-22 夜（P-C）：推进口径的两行（斜坡 / 放符卡期间 ±26）+ 死亡清零
            (r"youkai: gauge advance = continuous-fire time ramp",
             "推进 = 时间斜坡（一手 `th08:0x44be67`；不是恒定 ±26）", False),
            (r"youkai: gauge advance = BOMB/spell period at [+-]\d+/frame",
             "放符卡/炸弹期间的推进（一手 `shotActive` 的真身）", False),
            (r"youkai: gauge reset to 0 on death",
             "死亡时槽归零（一手 `th08:0x44ac09`）", False),
            # ★ 2026-09-22 夜（#27 弹字）：两支刻符弹字是**纯观感**，正常没有日志；
            #   实跑判据 = 收刻符时在那颗刻符上闪出分值（60 帧上浮）、击破使魔时闪「+1」（90 帧不动）。
            #   （池满/起不出来的诊断走 ledger 的计数，不单独刷屏。）
        ],
        fail=[(r"youkai: familiar spawn FAILED", "创建失败（池满 / 坐标非法）"),
              (r"youkai: familiar follow-VM table full", "跟随 VM 表满（画面会少使魔）"),
              # ★ 2026-09-22 夜（owner 二次订正）：**切换那一下的短命魔法阵环**（TH08 `etama.anm` script48）
              #   起不出来（观感项；现在它是「闪一圈就自删」，不再常驻在使魔身上）
              (r"youkai: switch ring VM spawn FAILED", "切换环没装上（Shift 那一下少闪一圈）"),
              # ★ 2026-09-22 夜（#23）：**本体**光环起不出来（画面少了那一圈，机制不受影响）
              (r"youkai: body Shift aura VM spawn FAILED", "本体 Shift 光环没装上（自机身上少一圈）")],
    ),
    "emitter-close": dict(
        title="关闭发弹口（TH08 / 卡 108）",
        card=108, impl="cards/youkai.c",
        pre="装 108，找一只有**多个发弹口**的敌机（妖精群 / boss 部件）",
        steps=[
            "用使魔弹（或自机弹）把某个发弹口打掉",
            "确认该口不再发弹，直到 ECL 重新武装它",
        ],
        expect=[
            (r"youkai: emitter gate ACTIVE - slot \d+ of enemy_id \d+", "发弹口被**闸住**", True),
            (r"youkai: familiar enemy_id \d+ killed", "闸与击破记在同一只敌机上（因果可读）", False),
        ],
        fail=[],
    ),
    "z-intercept": dict(
        title="拦截 Z（TH09 / 卡 109 花映塚）",
        card=109, impl="cards/pofv.c",
        pre="装 109，进关卡（手上有 Z 点射与蓄力两种行为）",
        steps=[
            "**短按** Z：应照常出一发点射（原版手感不变）",
            "**长按** Z 直到蓄到 C1：本帧应跳过原生行循环（不出点射），改走蓄力",
            "松开 Z：应放出蓄力攻击（CA）",
        ],
        expect=[
            (r"pofv: ctor \(T = ", "卡已装载（T = 白弹半径阈值）", True),
            (r"pofv: CA L\d+ r=\d+ cancelled \d+ bullets", "蓄力攻击释放（= 拦 Z 成功后才可能有）", True),
        ],
        fail=[],
    ),
    "charge": dict(
        title="蓄力（TH09 / 卡 109）",
        card=109, impl="cards/pofv_core.c",
        pre="装 109；气槽随消弹 / 击破上涨",
        steps=[
            "长按 Z 蓄力，观察气槽与档位（C1..C4）",
            "到档后松开，看 CA 半径与连爆档是否随之变化",
            "SP 跨 10 万 / 30 万 / 50 万：看 boss attack 占位日志",
        ],
        expect=[
            (r"pofv: CA L\d+ r=\d+", "蓄力攻击按档位放出（r = 半径档）", True),
            (r"pofv: SP \d+ crossed stage \d+ -> boss attack", "SP 跨档触发（蓄力链的收益端）", False),
        ],
        fail=[],
    ),
    "storm-chain": dict(
        title="暴风连锁（TH09 / 卡 109）",
        card=109, impl="cards/pofv.c",
        pre="装 109，找一片**密集小怪**（妖精群）",
        steps=[
            "一次打爆其中一只 ⇒ 出暴风（蓝圈）",
            "观察暴风是否**引爆范围内其他小怪**、并消掉圈内小弹",
            "看日志里 chain 是否从 1 涨到 2 以上（= 经真小怪传播）",
        ],
        expect=[
            (r"pofv: blast chain (\d+) r (\d+) cancelled \d+ white", "出暴风并消白弹", True),
        ],
        fail=[(r"pofv: blast queue full", "爆风队列被打满（连锁太快，会丢暴风）")],
    ),
    "spirit": dict(
        title="创建灵魂（TH09 / 卡 109）",
        card=109, impl="cards/pofv.c",
        pre="装 109，正常打小怪（击破 = 掉一颗灵魂，进卡自己的池）",
        steps=[
            "击破若干小怪 ⇒ 场上应出现灵魂（自建池）",
            "Shift 固灵（低速）展开吸灵领域，把灵魂吸掉",
            "看收尾行 `pofv: run done - N spirits spawned…`（过关 / 删卡 / 重开都会打）",
        ],
        expect=[
            (r"pofv: run done - [1-9]\d* spirits spawned", "灵魂账：**击破 ⇒ 掉灵魂**真的发生过（spawned ≥ 1）", True),
            (r"pofv: guzei activated \d+ spirits", "固灵吸走 N 颗（旁证：池里确实有货）", False),
        ],
        fail=[(r"pofv: spirit pool full", "灵魂池满（32）⇒ 后面的灵魂被丢")],
    ),
    "spawn-enemy": dict(
        title="创建敌人（TH12 / 卡 112 星莲船）",
        card=112, impl="cards/ufo.c",
        pre="装 112，进关卡打出**彩虹令牌**（UFO 产物）",
        steps=[
            "击破敌机攒令牌 → 令牌落地 / 被吸",
            "令牌到点后应**生成一只真敌人载体**（能被自机弹打、能掉东西）",
        ],
        expect=[
            (r"ufo: ce_spawn_enemy\(", "真敌人载体被创建（`th12:0x412990` 路线）", True),
        ],
        fail=[(r"ufo: break - the returned token could NOT be spawned", "载体创建失败（池满 / anm 未加载）"),
              (r"ufo: body follow-VM spawn FAILED", "跟随 VM 没装上（令牌会看不见）")],
    ),
    "collect": dict(
        title="收点（TH12 / 卡 112）",
        card=112, impl="cards/ufo.c",
        pre="装 112，打出令牌并让 UFO 吃掉 / 爆开",
        steps=[
            "喂满一个 UFO（P 点 / 金钱）",
            "爆开吸收益：P、金钱、bounty 分",
            "收尾看 finish 行里的汇总（frame 数 = 整条链花了多久）",
        ],
        expect=[
            (r"ufo: BREAK color \d+ - \d+ P \+ \d+ money absorbed", "爆开吸收益（= 收点走到）", True),
            (r"ufo: finish \(", "收尾汇总（证明整局链路干净）", False),
        ],
        fail=[(r"ufo: bounty item type \d+ skipped", "掉点道具类型没落地（收益会少）"),
              (r"ufo: fill full \(color \d+\)", "填充满了却没东西吐出来")],
    ),
}


# ---------------------------------------------------------------- 前置行（判"未触发 vs 失效"）
# 判据没中时，看**这条路的迹象**在不在：
#   有迹象 ⇒ 前置齐、判据没中 ⇒ **失效**（做了但没效果）
#   没迹象 ⇒ **未触发**（本局压根没走到那条路：没装那张卡 / 没做那个动作 / 没跑到收尾）
# 这些正则都取自已实的卡日志前缀（一手），不引入新假设。
PRE = {
    "familiar":      [r"youkai:", r"native option"],
    "emitter-close": [r"youkai:", r"emitter"],
    "z-intercept":   [r"pofv:"],
    "charge":        [r"pofv:"],
    "storm-chain":   [r"pofv:"],
    "spirit":        [r"pofv:"],
    "spawn-enemy":   [r"ufo:"],
    "collect":       [r"ufo:"],
}


# 判据的"为什么可能没中"说明（写进报告，避免把"未触发"读成失败）
NOTES = {
    "familiar": "必需判据要**一局收尾**才打（`run done`）；代打场景需跑完一关或删卡才会出现。",
    "emitter-close": "需要**打掉某只敌机的发弹口**（gate 才会出声）；场景要跑到敌机群。",
    "spawn-enemy": "需要**令牌走到 `ce_spawn_enemy`**（112 的令牌路径被触发）。",
    "collect": "需要**令牌落地/爆开**。",
    "spirit": "灵魂账在三处打印（过关/删卡/重开）—— 有值才出声。",
}


def classify(lines, rx_list):
    """→ (分类, 命中数)。分类：'失效' / '未触发' / '无迹象'。"""
    n = sum(1 for rx in rx_list if any(re.search(rx, l) for l in lines))
    if n:
        return "失效（前置迹象在、判据没中 ⇒ 做了但没效果）", n
    return "未触发（本局没有这条路的迹象 ⇒ 没装那张卡 / 没做那个动作 / 没跑到收尾）", 0


def find_log(args):
    if args.log:
        return args.log
    if os.environ.get("CE_LOG"):
        return os.environ["CE_LOG"]
    mk = args.modkit or os.environ.get("MODKIT_DIR", "")
    if mk:
        return os.path.join(mk, "game", LOG_NAME)
    return ""


def boot_section(text):
    """只取**本次启动**那一段（最后一条分界线之后）。"""
    lines = text.splitlines()
    idx = [i for i, l in enumerate(lines) if BOOT_RE.match(l.strip())]
    if not idx:
        return lines, None
    return lines[idx[-1]:], lines[idx[-1]]


def write_report(path, rows, log, boot, env_rows=(), pool_line=""):
    """把判读结果写成 Markdown（一行一原语）—— 交付判据 ③ 的"自动出报告"。"""
    out = ["# TH18 验收判读报告", "",
           "## 装配判据（先看这一段：这一局是不是我们要的那一局）", "",
           "| 判据 | 结果 |", "| --- | --- |"]
    for why, verdict in env_rows:
        out.append("| %s | %s |" % (why, verdict))
    if pool_line:
        out += ["", "池子实测读数：`%s`" % pool_line.strip()]
    out += ["", "## 八原语判据", "",
           "- 日志：`%s`" % log,
           "- 本次启动段：`%s`" % (boot or "(没有分界线，按整份日志判读)"),
           "",
           "| 原语 | 结果 | 分类（判据没中时）| 前置命中 | 反例 |",
           "| --- | --- | --- | --- | --- |"]
    for r in rows:
        out.append("| %s | %s | %s | %s | %s |" % (r["name"], r["verdict"], r["cls"], r["pre"], r["fail"]))
        if r.get("note"):
            out.append("| ↳ %s | | | | |" % r["note"])
    n_bad = sum(1 for r in rows if not r["verdict"].startswith("通过"))
    out += ["", "**合计 %d 个原语：%d 通过 / %d 未过**" % (len(rows), len(rows) - n_bad, n_bad), ""]
    open(path, "w", encoding="utf-8").write("\n".join(out))
    print("\n报告已写：%s" % path)



# ---------------------------------------------------------------- 环境/装配判据（总验收 §5 的四条）
# 这些**不是**八原语，而是"这一局到底是不是我们要的那一局"：
#   ① 防假绿：`integrity[stage N]: card[i] id=…` 必须列出全部系统卡（少一张卡的绿灯不算数）
#   ② 闸可观测：`pool:` 行（sdk.c 的实测读数）必须在 —— 没有它，站点/池子无从判定
#   ③ 无 SDK 自检失败：`FAIL:`（如"有行为没数据行"，2026-09-17 卡 118 那次就是它）
#   ④ 无降档：`mitigation: allocator bound restored`（新 id 被全体停用 ⇒ 系统卡形同不存在）
ENV_RULES = [
    ("防假绿：12 张系统卡都在编成里", r"integrity\[stage \d+\]: card\[\d+\] id=(10[6-9]|11[0-7])", 12),
    ("闸可观测：pool: 实测读数", r"pool: 可执行页 \d+、跳板格 \d+", 1),
]
ENV_FORBID = [
    ("SDK 自检失败（FAIL:）", r"FAIL: "),
    ("新 id 被降档（mitigation）", r"mitigation: allocator bound restored"),
]


def check_env(lines):
    """→ (rows, rc)。总验收的装配判据（§5 的四条）。"""
    rows, rc = [], 0
    for why, rx, need in ENV_RULES:
        hits = [l for l in lines if re.search(rx, l)]
        uniq = set(re.findall(r"id=(10[6-9]|11[0-7])", "\n".join(hits))) if need > 1 else hits
        n = len(uniq) if need > 1 else len(hits)
        ok = n >= need
        if not ok:
            rc = 1
        rows.append((why, ("✅" if ok else "❌") + " %d/%d" % (n, need)))
    for why, rx in ENV_FORBID:
        hits = [l for l in lines if re.search(rx, l)]
        if hits:
            rc = 1
        rows.append((why, ("❌ 命中 %d 行" % len(hits)) if hits else "✅ 无"))
    return rows, rc


def do_check(names, args):
    path = find_log(args)
    if not path or not os.path.exists(path):
        print(f"✗ 找不到日志：{path or '(未给 --log/CE_LOG/MODKIT_DIR)'}")
        return 1
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()
    lines, boot = boot_section(text)
    print(f"日志：{path}")
    print(f"本次启动段：{boot or '(没有分界线，按整份日志判读)'} —— {len(lines)} 行")
    rc = 0
    rows = []
    print("\n=== 装配判据（总验收 §5：这一局是不是我们要的那一局）===")
    env_rows, env_rc = check_env(lines)
    for why, verdict in env_rows:
        print("  %-34s %s" % (why, verdict))
    print("  ⇒ 装配%s" % ("通过" if env_rc == 0 else "**未过**（下面的原语判读仅供参考）"))
    rc |= env_rc
    pool = [l for l in lines if "跳板格" in l]
    if pool:
        print("  池子读数：%s" % pool[-1].strip()[:120])

    for name in names:
        p = PRIMS[name]
        print(f"\n=== {name} —— {p['title']} ===")
        missing = []
        nfail = 0
        for rx, why, need in p["expect"]:
            hits = [l for l in lines if re.search(rx, l)]
            mark = "✓" if hits else ("✗" if need else "·")
            if not hits and need:
                missing.append(rx)
                rc = 1
            print(f"  {mark} {why}")
            print(f"      判据 /{rx}/ —— 命中 {len(hits)} 行")
            for h in hits[:2]:
                print(f"      > {h.strip()[:150]}")
        for rx, why in p["fail"]:
            hits = [l for l in lines if re.search(rx, l)]
            if hits:
                nfail += 1
                print(f"  ⚠ 反例命中：{why}（{len(hits)} 行）")
                for h in hits[:2]:
                    print(f"      > {h.strip()[:150]}")
        row = {"name": f"{name}（{p['title']}）", "verdict": "通过", "cls": "—", "pre": "—", "fail": "—",
               "note": NOTES.get(name, "")}
        nfail = 0
        npre = sum(1 for rx in PRE.get(name, []) if any(re.search(rx, l) for l in lines))
        if missing:
            cls, nhit = classify(lines, PRE.get(name, []))
            row["verdict"] = "未过（缺 %d 条）" % len(missing)
            row["cls"] = cls.split("（")[0]
            row["pre"] = "%d 条" % nhit
            print(f"  ⇒ 缺必需判据：{len(missing)} 条（批次未通过）—— 分类：**{cls}**（前置行命中 {nhit} 条）")
            for rx in missing:
                print(f"      没中的判据 /{rx}/")
        elif nfail:
            row["fail"] = "%d 条" % nfail
            print(f"  ⇒ 必需判据齐，但有 {nfail} 条反例命中 —— 判读时按上面两条一起看")
        else:
            print("  ⇒ 必需判据齐、无反例（本原语通过本次实跑）")
        rows.append(row)
    if getattr(args, "report", ""):
        write_report(args.report, rows, path, boot, env_rows,
                     (pool[-1] if pool else ""))
    return rc


def show(names, args):
    for name in names:
        p = PRIMS[name]
        print(f"\n=== {name} —— {p['title']} ===")
        print(f"  卡：{p['card']}   实现：{p['impl']}")
        print(f"  前置：{p['pre']}")
        print("  操作：")
        for i, s in enumerate(p["steps"], 1):
            print(f"    {i}. {s}")
        print("  期望日志（判读依据）：")
        for rx, why, need in p["expect"]:
            print(f"    {'[必需]' if need else '[旁证]'} {why}")
            print(f"           /{rx}/")
        for rx, why in p["fail"]:
            print(f"    [反例] {why}\n           /{rx}/")
    print("\n用户实跑之后：python3 native/tools/verify.py "
          f"{' '.join(names)} --check")


def main():
    ap = argparse.ArgumentParser(description="原语验证批次（部署 → 操作单 → 日志判读）")
    ap.add_argument("prims", nargs="*", help="原语名（见 --list）；用 all 表示全部")
    ap.add_argument("--list", action="store_true", help="列出原语台账")
    ap.add_argument("--deploy", action="store_true",
                    help="先走 tools/deploy.py 发布（唯一发布路径），再打印操作单")
    ap.add_argument("--check", action="store_true", help="读日志判读（本次启动段）")
    ap.add_argument("--report", default="", help="把判读结果写成 Markdown（一行一原语）")
    ap.add_argument("--log", default="", help="日志路径（缺省 <modkit>/game/" + LOG_NAME + "）")
    ap.add_argument("--modkit", default="", help="modkit 根目录（也可用 MODKIT_DIR）")
    args = ap.parse_args()

    if args.list or (not args.prims and not args.deploy):
        print("原语台账（TH08/09/12 的八个原语）：")
        for k, v in PRIMS.items():
            print(f"  {k:14s} {v['title']}   卡 {v['card']}   {v['impl']}")
        print("\n没实现 / 还没接线的原语不在这张表里（见 engine/_shared/site-budget-and-primitives.md §2）。")
        return 0

    names = list(PRIMS) if args.prims == ["all"] else args.prims
    bad = [n for n in names if n not in PRIMS]
    if bad:
        print(f"✗ 未知原语：{', '.join(bad)}（--list 看台账）")
        return 2

    if args.deploy:
        deploy = os.path.join(HERE, "deploy.py")
        cmd = [sys.executable, deploy]
        if args.modkit:
            cmd += ["--modkit", args.modkit]
        print(f"① 发布：{' '.join(cmd)}")
        r = subprocess.run(cmd)
        if r.returncode != 0:
            print("✗ 发布失败 ⇒ 批次中止（不要让用户拿旧 DLL 实跑）")
            return r.returncode
    if not args.check:
        show(names, args)
        return 0
    rc = do_check(names, args)
    if args.report:
        # 报告需要行数据 ⇒ 交给 do_check 内部处理（这里只兜底：若它没写就再跑一次纯文本）
        pass
    return rc


if __name__ == "__main__":
    sys.exit(main())
