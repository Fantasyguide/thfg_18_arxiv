#!/usr/bin/env python3
"""contract_check.py —— **构建期**预检 SDK 的「JSON ↔ 行为」两条契约（省得等实跑才发现整局新卡被禁用）。

## 为什么要有它（2026-09-19 一次真实事故）

那天为了让十二张系统卡（106..117）在选卡界面连成一条，有人把 `113/116/117` 的
`cards.js` `"category"` 由 `0` 改成 `2` —— 这三张是**主动卡**（`CE_CARD(...)` 里有 `active_recharge`），
于是实跑时自检门当场：

```
FAIL: sdk: card 113 violates JSON/behavior contract (reason 2)
mitigation: allocator bound restored to retail (56) — new ids disabled
```

⇒ **整局新卡全部禁用**，玩家侧表现是「编成里某一格变成虚无」。两条契约本来就有主机单测
（`tests/test_sdk_core.c` 的 `ce_sdk_contract_check` / `ce_sdk_bind_check`），但它们喂的是**构造数据** ——
**真数据（`cards/*.c` ↔ `cards.js`）从来没在构建期对过账**。本工具补这一刀。

## 判据（与运行时 `sdk.c` 的两处一模一样）

1. **契约（`ce_sdk_contract_check`）**：`category == 0` ⟺ 这张卡有主动行为（`active_recharge != 0`）。
   - `category == 0` 但**没有**主动行为 ⇒ reason 1（`ACTIVE_BEHAVIOR_REQUIRED`）；
   - **有**主动行为但 `category != 0` ⇒ reason 2（`ACTIVE_CATEGORY_REQUIRED`）。
   ⇒ 想改显示分组，**别动 `category`**（那是给引擎/契约看的）；要连成一条改 `menu.c` 的显示分组
   （`CE_SYS_CARD_LO..HI`，见 `cards_def.h`）。
2. **绑定（`ce_sdk_bind_check`）**：`cards/*.c` 里有 `CE_CARD(id, …)` 的 id **必须**在 `cards.js` 里登记
   （反方向允许：JSON 登记了但 C 里没有行为 = 基类空卡，开发期正常）。

    python3 tools/contract_check.py        # 进 make check
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))       # native/tools
NATIVE = os.path.dirname(HERE)
MOD = os.path.dirname(NATIVE)
CARDS_DIR = os.path.join(NATIVE, "cards")
CARDS_JS = os.path.join(MOD, "patch", "th18", "cards.js")


def behavior_cards():
    """→ {id: (file, 是否主动)}。扫 `CE_CARD(<id>, …)` 到配对的 `);`。"""
    out = {}
    for fn in sorted(os.listdir(CARDS_DIR)):
        if not fn.endswith(".c"):
            continue
        path = os.path.join(CARDS_DIR, fn)
        text = open(path, encoding="utf-8", errors="replace").read()
        # 去掉注释再找（避免注释里出现的 CE_CARD(...) 被当真）
        code = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        code = re.sub(r"//[^\n]*", "", code)
        for m in re.finditer(r"\bCE_CARD\s*\(\s*(\d+)\s*,", code):
            cid = int(m.group(1))
            depth, i = 1, code.find("(", m.start())
            i += 1
            while i < len(code) and depth:
                if code[i] == "(":
                    depth += 1
                elif code[i] == ")":
                    depth -= 1
                i += 1
            body = code[m.end():i]
            m2 = re.search(r"active_recharge\s*=\s*([^,)]+)", body)
            val = m2.group(1).strip() if m2 else None
            out[cid] = (fn, bool(val) and val not in ("0", "0u", "0U"))
    return out


def main():
    if not os.path.isfile(CARDS_JS):
        print("contract_check: 找不到 %s" % CARDS_JS)
        return 2
    cards = json.load(open(CARDS_JS, encoding="utf-8"))
    js = {int(k): v.get("category", 2) for k, v in cards.items()}
    beh = behavior_cards()
    bad = []

    for cid, (fn, active) in sorted(beh.items()):
        if cid not in js:
            bad.append("卡 %d（%s）有行为但 `cards.js` 没登记 ⇒ 运行时 `ce_sdk_bind_check` 会 FAIL、"
                       "**整局新卡禁用**" % (cid, fn))
            continue
        cat = js[cid]
        if cat == 0 and not active:
            bad.append("卡 %d（%s）`category` = 0（主动）但行为里没有 `active_recharge` ⇒ "
                       "契约 reason 1（`ACTIVE_BEHAVIOR_REQUIRED`）" % (cid, fn))
        if active and cat != 0:
            bad.append("卡 %d（%s）有主动行为但 `cards.js` 的 `category` = %d ≠ 0 ⇒ "
                       "契约 reason 2（`ACTIVE_CATEGORY_REQUIRED`）；要调显示顺序请改 `menu.c` 的显示分组"
                       % (cid, fn, cat))

    if bad:
        print("❌ JSON/行为契约预检不过（%d 条）—— 实跑会 `FAIL: sdk: …` 并**禁用全部新卡**：" % len(bad))
        for b in bad:
            print("   · " + b)
        return 1
    print("✅ JSON/行为契约预检通过：%d 张有行为的卡，category 与主动行为一致；无「有行为未登记」"
          % len(beh))
    return 0


if __name__ == "__main__":
    sys.exit(main())
