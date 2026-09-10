"""用 CC0 源码里的**非日志字符串**当坐标系，把函数名迁到 TH06NC。

这是 engine/_shared/th06nc-name-transfer.md §3 那条日志消息手法的第二轮：
同一个坐标系，把输入从 i18n.tpl 的 85 条消息扩到**全部 .cpp 字面量**
（文件名、格式串、菜单文本）。命中面大得多 —— 186 条字面量原样存在于 NC 的
exe 里，其中 119 条被 .text 里的 RIP 相对 LEA 指到。

出处：名字全部来自 CC0 th06-decomp（CC0 1.0，可转发）。
th-re-data / th06-re 没有 LICENSE，它们的名字不许混进来 —— 有测试守着。

═══ 判据（三条硬的 + 一条结构复核）═══

(a) **CC0 侧唯一**：该字面量在 CC0 里只出现在一个函数体内。
(b) **自由函数算冲突**。上一轮放跑 WinMain 就是栽在这条没实现上——
    那个函数同时带着 SoundPlayer::InitializeDSound 的消息和 main.cpp 里
    一条**自由函数**的消息，判据只看 Class::method、没把自由函数算进去，
    于是认为"唯一"。本轮把 <free> 归属算作冲突证据后，**它被自动拒绝了**。
(c) **caller-closure 挑内联者**：一个 NC 函数常同时带着好几个 CC0 方法的字面量
    （NC 内联得凶）。按条数投票会选中**被内联的那个**，不是内联它的那个。
    改成：若这些 owner 同属一个类、且其中恰有一个在 CC0 里（传递地）调用其余全部，
    就选它。0x1400479e0 是活证据 —— 票数规则给 MainMenu::LoadDiffCharSelect(9)，
    closure 规则给 MainMenu::OnUpdate(1)，而**上一轮独立地用一条日志消息
    也判成了 OnUpdate**。
(d) **chain 回调的结构签名**（人工复核，不在自动判据里）：TH06 的
    Chain 回调是被存成函数指针的，所以在 NC 里表现为「被 LEA、从不被 CALL」。
    这条拆掉了两组自动判据认输的平票（见 TIEBREAKS），也拦下了一个假阳性。

自证：本轮在完全不知情的情况下复现了上一轮 10 个名字里的 9 个
（AnmManager__LoadAnm / LoadSurface、GameErrorContext__Flush、
GameManager__AddedCallback、Gui__LoadMsg、MainMenu__OnUpdate、
SoundPlayer__InitSoundBuffers、Stage__LoadStageData、Supervisor__AddedCallback），
外加 player 轮的 Player__AddedCallback。
"""

EXPECTED_MD5 = "b0cc28ec904e9efce2439ad857c938b5"

# (NC 地址, CC0 名, 支撑字面量条数, 第二重证据)
# 第二重证据是**人工复核**出来的，不是匹配器给的 —— METHOD 要求"光有匹配不够"。
# 备注会写进 Ghidra 并随 th06nc-re-data 发布，**必须 ASCII**。
STR_NAMES = [
    (0x140009700, "AsciiManager__AddedCallback", 3,
     "LEA'd by 0x140009400 and never called: the chain-callback signature. "
     "0x140009400 (which carries the same three anm names, inlined) is called by "
     "Supervisor__AddedCallback and is AsciiManager::RegisterChain."),
    (0x140011F80, "BulletManager__AddedCallback", 2,
     "Called by GameManager__AddedCallback (0x14003a9c0) and address-taken there too, "
     "matching CC0 where GameManager::AddedCallback brings the bullet chain up."),
    (0x14002B390, "EffectManager__AddedCallback", 5,
     "LEA'd by 0x14002b490 and never called: the chain-callback signature. "
     "0x14002b490 is called by GameManager__AddedCallback and is "
     "EffectManager::RegisterChain with AddedCallback inlined."),
    (0x14002B7A0, "Ending__LoadEnding", 1,
     "Called by 0x14002bac0, matching CC0 Ending.cpp:223 where Ending::OnUpdate "
     "loads the next ending file. The six literal call sites in "
     "Ending::AddedCallback were inlined into 0x14002c8d0."),
    (0x14002C8D0, "Ending__AddedCallback", 9,
     "LEA'd by 0x14002b840 (Ending::RegisterChain) and never called. "
     "Ending::LoadEnding is inlined into it, which is why it also carries "
     "that function's error message."),
    (0x14003CD20, "Gui__OnDraw", 5,
     "LEA'd by 0x140041ca0 and never called: chain-callback signature. It calls "
     "0x140041820 (Gui__DrawStageElements below), exactly as CC0 Gui.cpp:95 does. "
     "The rival 0x14003fca0 is CALLED (by 0x14003cc50, 43 bytes) and is a helper."),
    (0x14003D500, "Gui__ActualAddedCallback", 28,
     "28 supporting literals - every face/msg/front/loading asset name. Called by "
     "0x140041ca0, the same function that registers Gui__OnDraw, matching CC0 "
     "where Gui::AddedCallback forwards to ActualAddedCallback."),
    (0x14003ECE0, "GuiImpl__MsgRead", 3,
     "Called by 0x140036260 = EnemyManager__RunEclTimeline (named in the ECL round), "
     "matching CC0 EnemyManager.cpp:297/301 g_Gui.MsgRead(...). The one-line "
     "forwarder Gui::MsgRead was inlined."),
    (0x140041820, "Gui__DrawStageElements", 3,
     "Called by Gui__OnDraw (0x14003cd20), matching CC0 Gui.cpp:95. Both ends of "
     "this edge were named independently by their own literals."),
    (0x140058CC0, "MusicRoom__AddedCallback", 4,
     "LEA'd by 0x1400581b0 (MusicRoom::RegisterChain) and never called."),
    (0x140073D50, "ResultScreen__AddedCallback", 4,
     "LEA'd by 0x14006dd20 and never called. 0x14006dd20 also takes the address of "
     "0x14006e3a0, already named ResultScreen__OnUpdate in the menu round, so it is "
     "ResultScreen::RegisterChain."),
    (0x14006D310, "ResultScreen__HandleReplaySaveKeyboard", 1,
     "Called by 0x14006e3a0 = ResultScreen__OnUpdate (named in the menu round), "
     "matching CC0 ResultScreen.cpp:1699. Its rival for the 'replay' literal was "
     "that same OnUpdate, which already has a name."),
    (0x14007B360, "Supervisor__LoadPbg3", 1,
     "Called from five places - GameManager__AddedCallback, MainMenu__OnUpdate, "
     "Supervisor__AddedCallback, Ending__AddedCallback and WinMain - which is the "
     "call shape of the archive loader, one call per .dat."),
]

# 匹配器给出、人工判定为**假阳性**或**证据不足**的，登记在此防止以后又被"发现"一次。
# METHOD:「批量手法的价值一半在产出，一半在『哪些看着像但是错的』。」
REJECTED = {
    0x140054870: (
        "MainMenu__DrawReplayMenu -- 判据接受了它（DrawReplayMenu 的两条格式串双向唯一），"
        "但它被 0x140055a30 取地址且从不被调用，是 chain 回调的签名；而 CC0 里 "
        "DrawReplayMenu 从来不当函数指针用，MainMenu::OnDraw 才是（OnDraw 在 "
        "MainMenu.cpp:80 调 DrawReplayMenu）。更可能是 OnDraw 内联了它。不入库。"),
    0x1400678A0: (
        "MainMenu__ReplayHandling -- 引用了 CC0 里只属于 ReplayHandling 的 "
        "g_GameErrorContext.Log(\"japanese\")（MainMenu.cpp:1280），但它的唯一调用者是 "
        "0x1400454f0 = WinMain，而 CC0 里 ReplayHandling 只被 MainMenu::OnUpdate 调用。"
        "调用关系与 CC0 矛盾 —— 矛盾即拒绝。"),
    0x1400454F0: (
        "SoundPlayer__InitializeDSound -- 上一轮的假阳性，实为 WinMain。"
        "本轮判据 (b) 把它**自动**拒绝了（它同时带着 main.cpp 一条自由函数的消息）。"
        "见 th06nc_logname_spec.REJECTED。"),
    0x14003FCA0: (
        "Gui__OnDraw -- 真身是 0x14003cd20（被 LEA、不被调用）。本条是被 0x14003cc50 "
        "调用的辅助函数，只是把 'BONUS %8d' 也带了一份。"),
    0x140009400: (
        "AsciiManager__AddedCallback -- 实为 AsciiManager::RegisterChain（内联了 "
        "AddedCallback，所以带着同样三条 anm 文件名）。真身是 0x140009700。"),
    0x14002B490: (
        "EffectManager__AddedCallback -- 实为 EffectManager::RegisterChain。"
        "真身是 0x14002b390。"),
    0x14006E3A0: (
        "ResultScreen__HandleReplaySaveKeyboard -- 它已经是 ResultScreen__OnUpdate"
        "（menu 轮），HandleReplaySaveKeyboard 被内联进来了一份。真身是 0x14006d310。"),
    0x1400785C0: (
        "Stage__LoadStageData -- 匹配器给出这个名字（那条 'ステージデータ' 错误消息确实"
        "属于 CC0 的 Stage::LoadStageData），日志消息那轮也这么判过，**两轮都错**。"
        "STD 那轮证明它是 Stage::AddedCallback，LoadStageData 被内联了进去。"
        "这是 §4.5 那个盲点的第三例：**内联者自己不贡献字面量，判据就看不见它**。"
        "见 th06nc_std_spec.RENAMES。"),
}

# 自动判据认输、靠 chain 回调结构签名（判据 d）才拆开的平票。
# 记下来是为了说清「哪几条不是纯自动产物」。
TIEBREAKS = {
    0x140009700: "vs 0x140009400",
    0x14002B390: "vs 0x14002b490",
    0x14006D310: "vs 0x14006e3a0",
}
# 0x14003cd20 (Gui__OnDraw) 不在此列：匹配器自己就以 5:1 判赢了 0x14003fca0，
# chain 回调签名只是**追加**的第二重证据，不是拆平票用的。

# 上一轮（日志消息）已经入库、本轮**独立复现**的地址。不重复入库，只作自证。
REPRODUCED = {
    0x1400020B0: "AnmManager__LoadAnm",
    0x140007620: "AnmManager__LoadSurface",
    0x14003A0D0: "GameErrorContext__Flush",
    0x14003A9C0: "GameManager__AddedCallback",
    0x14003EB80: "Gui__LoadMsg",
    0x1400479E0: "MainMenu__OnUpdate",
    0x140068160: "Player__AddedCallback",
    0x1400767D0: "SoundPlayer__InitSoundBuffers",
    0x14007A6D0: "Supervisor__AddedCallback",
}


def validate():
    errors = []
    addrs = [a for a, _n, _c, _m in STR_NAMES]
    names = [n for _a, n, _c, _m in STR_NAMES]
    if len(addrs) != len(set(addrs)):
        errors.append("地址重复")
    if len(names) != len(set(names)):
        errors.append("名字重复")
    for a, n, cnt, memo in STR_NAMES:
        if not (0x140000000 <= a < 0x141000000):
            errors.append("%s 的地址不在映像范围内" % n)
        if cnt < 1:
            errors.append("%s 没有支撑字面量" % n)
        if not memo:
            errors.append("%s 没写第二重证据 —— METHOD 要求光有匹配不够" % n)
        if memo and not memo.isascii():
            errors.append("%s 的备注会写进 Ghidra 并发布，必须 ASCII" % n)
        if "__" not in n:
            errors.append("%s 不是 Class__method 形式" % n)
        if a in REJECTED:
            errors.append("%s 的地址同时在 REJECTED 里" % n)
        if a in REPRODUCED:
            errors.append("%s 是上一轮已入库的地址，本轮只该当自证" % n)
    for a in TIEBREAKS:
        if a not in addrs:
            errors.append("TIEBREAKS 里的 0x%x 不在 STR_NAMES 中" % a)
    if 0x1400454F0 not in REJECTED:
        errors.append("WinMain 那条假阳性必须留在 REJECTED 里")
    return errors
