"""用 ZUN 原版日志消息当坐标系，从 CC0 迁到 TH06NC 的函数名。

**为什么用这条路**：另外两个批量方案都实测失败（见 engine/_shared/th06nc-name-transfer.md）——
常量签名在 ground truth 上命中 0，调用图自举只多出 1 个（且是已知的）。
两者都卡在「需要锚点才能产生锚点」。日志消息**不需要任何前置锚点**。

手法：NC 以 Shift-JIS 原样保留了 24 条 TH06 的日志消息；CC0 把它们抽在 i18n.tpl 的
TH_* 宏里，且每个宏的使用点带着函数名。于是「消息文本」就是两个二进制的共同坐标系。

出处：名字全部来自 CC0 th06-decomp（CC0 1.0，可转发）。
`th-re-data` / `th06-re` 没有 LICENSE，它们的名字不许混进来 —— 有测试守着。
"""

EXPECTED_MD5 = "b0cc28ec904e9efce2439ad857c938b5"

# (NC 地址, CC0 名, 支撑的消息条数, 置信度, 备注)
#   exact = 消息在 CC0 侧唯一指向这个函数，且 NC 侧只有这一个函数引用它
#   inlined = 同上，但 NC 把别的函数内联了进来，注释里写明是哪个
LOG_NAMES = [
    (0x1400020B0, "AnmManager__LoadAnm", 2, "exact",
     "self-validation: this address was already named AnmManager__LoadAnm in the ANM round; "
     "the matcher reproduced it without knowing."),
    (0x140007620, "AnmManager__LoadSurface", 1, "exact", ""),
    (0x14003A0D0, "GameErrorContext__Flush", 1, "exact", ""),
    (0x14003A9C0, "GameManager__AddedCallback", 8, "inlined",
     "7 of the 8 messages are GameManager::AddedCallback's; the 8th belongs to "
     "EclManager::Load, which CC0 calls from here (GameManager.cpp:427) and NC inlined."),
    (0x14003EB80, "Gui__LoadMsg", 1, "exact", ""),
    (0x1400479E0, "MainMenu__OnUpdate", 1, "exact", ""),
    (0x1400767D0, "SoundPlayer__InitSoundBuffers", 1, "exact", ""),
    (0x1400785C0, "Stage__AddedCallback", 1, "inlined",
     "CORRECTED 2026-09-11: this round named it Stage__LoadStageData after the error message "
     "it emits, but the STD round proved it is Stage::AddedCallback with LoadStageData "
     "inlined - it is registered as the calc chain's addedCallback and it is the only "
     "referencer of g_StageFiles, which CC0's LoadStageData never touches. "
     "See th06nc_std_spec.RENAMES."),
    (0x14007A6D0, "Supervisor__AddedCallback", 1, "exact", ""),
    (0x14007B5D0, "Supervisor__LoadConfig", 1, "exact", ""),
]
# ⚠️ 一条被内联骗过去的，靠人工复核拦下来了。
# 0x1400454f0 带着 SoundPlayer::InitializeDSound 的成功消息，匹配器给出了那个名字。
# 但它的调用者是 CRT 启动代码，自身调 SteamAPI_Shutdown / SetDllDirectoryA /
# _beginthreadex，有 80 个 callee —— 这是**程序入口**，InitializeDSound 被内联进去了。
# 同一函数还带着 main.cpp 的 TH_ERR_OPTION_CHANGED_RESTART，那条在 CC0 侧属于
# 自由函数，所以没被算作冲突 —— **判据的漏洞**，见 validate() 里的说明。
WINMAIN = (0x1400454F0, "WinMain",
           "[NC first-hand] Application entry, called from the CRT startup. Several functions "
           "are inlined into it, including SoundPlayer::InitializeDSound (whose success message "
           "lives here). Also calls SteamAPI_* and SetDllDirectoryA.")

# 匹配器认为唯一、但人工判定为**假阳性**的，记在这里防止以后又被"发现"一次。
REJECTED = {
    0x1400454F0: "SoundPlayer__InitializeDSound —— 实为 WinMain，见上",
}

# 本轮判对了地址、**判错了名字**的。与 REJECTED 不同：地址是对的，只是把内联者
# 认成了被内联者。记下来是为了让人明白「有锚 ≠ 一定对」。
CORRECTED = {
    0x1400785C0: ("Stage__LoadStageData", "Stage__AddedCallback",
                  "STD 那轮从 chain 注册与 g_StageFiles 的唯一引用者两条独立证据改判；"
                  "TH06 原版两者是分开的两个函数（th06:0x4044c0 / th06:0x404690）。"),
}


def validate():
    errors = []
    addrs = [a for a, _n, _c, _k, _m in LOG_NAMES]
    names = [n for _a, n, _c, _k, _m in LOG_NAMES]
    if len(addrs) != len(set(addrs)):
        errors.append("地址重复")
    if len(names) != len(set(names)):
        errors.append("名字重复")
    for a, n, cnt, kind, memo in LOG_NAMES:
        if not (0x140000000 <= a < 0x141000000):
            errors.append("%s 的地址不在映像范围内" % n)
        if kind not in ("exact", "inlined"):
            errors.append("%s: bad confidence tag" % n)
        if cnt < 1:
            errors.append("%s 没有支撑消息" % n)
        if kind == "inlined" and not memo:
            errors.append("%s 标了 inlined 却没写清是谁被内联了" % n)
        if memo and not memo.isascii():
            errors.append("%s 的备注会写进 Ghidra 并发布，必须 ASCII" % n)
        if "__" not in n:
            errors.append("%s 不是 Class__method 形式" % n)
    if WINMAIN[0] not in REJECTED:
        errors.append("WinMain 那条假阳性必须留在 REJECTED 里")
    if not WINMAIN[2].isascii():
        errors.append("WinMain 的注释必须 ASCII")
    # 判据漏洞的记录：CC0 侧落在自由函数里的消息也应算作冲突证据。
    # 这条目前靠人工复核补，没有自动化 —— 别把它当成已解决。
    return errors
