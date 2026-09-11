"""TH06NC AsciiManager：字符串表布局（engine/anm/th06nc/03-ascii-manager-strings.md）。

数据即结论；mods/th06nc/autoplay/native/check.py 用它对账 th06nc.h。
"""
G_ASCII_MANAGER = 0x1403DE620      # 0x140009400 三个 ChainElem 的 arg
FN_REGISTER_CHAIN = 0x140009400
FN_ON_UPDATE = 0x1400090D0         # calc 链优先级 1
FN_ON_DRAW = 0x140009260           # draw 链优先级 0x13；第一件事调 DRAW_STRINGS
FN_DRAW_STRINGS = 0x140009890
FN_INLINED_ADDSTRING_FPS = 0x14007A3A0   # "%.02lffps" 那处，AddString 内联

MGR_SCALE_X = 0x00
MGR_SCALE_Y = 0x04
MGR_IS_GUI = 0x52C8
MGR_IS_SELECTED = 0x52C9
MGR_COLOR = 0x52CC
MGR_NUM_STRINGS = 0x7BB0
MGR_STRINGS = 0x7BB4
MAX_STRINGS = 0x100
STRING_SIZE = 0x60

STR_SCALE_X = 0x00
STR_SCALE_Y = 0x04
STR_COLOR = 0x08
STR_IS_GUI = 0x0C
STR_POS = 0x10
STR_TEXT = 0x1C
STR_TEXT_CAP = 0x40
STR_IS_SELECTED = 0x5C

CHAR_ADVANCE = 12.0                # 0x14030cea8（TH06 是 14.0）
LINE_HEIGHT = 16.0                 # 0x14030ceb8
FPS_POS = (758.0, 464.0)           # 0x14030d048 / 0x14030d018：虚拟屏高 480、宽随宽高比


def validate():
    errors = []
    if STR_TEXT + STR_TEXT_CAP != STR_IS_SELECTED:
        errors.append("text[64] 之后紧接 isSelected")
    if STR_IS_SELECTED + 4 != STRING_SIZE:
        errors.append("记录大小应为 0x60")
    if MGR_STRINGS != MGR_NUM_STRINGS + 4:
        errors.append("strings 紧接 numStrings")
    if G_ASCII_MANAGER + MGR_NUM_STRINGS != 0x1403E61D0:
        errors.append("numStrings 绝对地址应为 0x1403e61d0（0x14007a3a0 直读）")
    if G_ASCII_MANAGER + MGR_STRINGS + STR_TEXT != 0x1403E61F0:
        errors.append("strings[0].text 绝对地址应为 0x1403e61f0（0x14007a3a0 的 strcpy 目标）")
    return errors
