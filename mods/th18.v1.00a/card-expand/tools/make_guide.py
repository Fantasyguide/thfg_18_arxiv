#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""make_guide.py —— 把玩家手册（markdown）+ 卡面数据（cards.js）生成一本 XeLaTeX 图册。

用法（在 card-guide 目录里跑；路径一律相对当前目录，不写死本机路径）：
    python3 make_guide.py <cards.js> <PLAYER-GUIDE.md> <out.tex> [<cards 图片目录>=cards]

产出 guide.tex：
  · **统一模板，一卡一页**（owner 2026-10-20）：本 mod 所有卡（58–92、106–118、201）按 id 升序，
    每张卡另起一页：卡面 PNG + **卡面文字**（cards.js 的 name/desc ✓）+ 玩家手册正文 ✓
    （没有正文的卡只留卡面，不补空话 ✗）；旧版的「第一部分系统卡 / 第二部分双栏其它卡」已取消。
编译（任何装了 XeLaTeX + 中文字体的机器）：
    xelatex -interaction=nonstopmode -halt-on-error guide.tex   （跑两遍出目录）

中文字体：默认 `Microsoft YaHei`（Windows 自带）。其它机器可用环境变量
`GUIDE_CJK_FONT` 换字体名，或用 fontconfig 把该名字映射到本机字体
（Linux 例：`FONTCONFIG_FILE=<fonts.conf> xelatex ...`，fonts.conf 里指向本机字体目录）。

★ 卡名宏（owner 2026-10-15 口径）：
  · 系统卡 106–117 用**纯字母**别名宏：\\hmx/\\HMX = 106 红魔乡、\\hzc/\\HZC = 114 辉针城 … \\gxs/\\GXS = 117 鬼形兽
    （小写 = 普通，大写 = 加粗；LaTeX 控制序列**不能带数字** ✗，所以不能用 \\c106 这种写法）
  · 其它卡在正文里统一输出**加粗的「编号　卡名」**（⇒ 「伟大之作」自动变「92　伟大之作」✓）
  · xeCJK 把汉字设成 catcode 11（字母）⇒ 宏名会被紧跟的汉字吞掉（`\\HMX的` 会被当成一个控制序列 ✗）
    ⇒ 每个宏后面补一对 `{}` 断开 ✓
"""
import json
import os
import re
import sys
import pathlib

# ── 符号：优先给 LaTeX 命令（比字体的字形稳 ✓）
SYM = [('→', r'$\rightarrow$'), ('←', r'$\leftarrow$'), ('↔', r'$\leftrightarrow$'),
       ('⇒', r'$\Rightarrow$'), ('×', r'$\times$'), ('−', r'$-$'), ('±', r'$\pm$'),
       ('∓', r'$\mp$'), ('≥', r'$\ge$'), ('≤', r'$\le$'), ('≠', r'$\ne$'),
       ('≈', r'$\approx$'), ('·', r'\textperiodcentered{}'), ('√', r'$\surd$')]
ESC = [('\\', r'\textbackslash{}'), ('&', r'\&'), ('%', r'\%'), ('$', r'\$'), ('#', r'\#'),
       ('_', r'\_'), ('{', r'\{'), ('}', r'\}'), ('~', r'\textasciitilde{}'),
       ('^', r'\textasciicircum{}')]

# ── ★ 卡名宏体系（见文件头）──────────────────────────────────────────
CARD_ALIAS = {106: 'hmx', 107: 'yym', 108: 'yyc', 109: 'hyz', 110: 'fsl', 111: 'dld',
              112: 'xlc', 113: 'slm', 114: 'hzc', 115: 'gzz', 116: 'tkz', 117: 'gxs'}


def esc(s):
    for a, b in ESC:
        s = s.replace(a, b)
    for a, b in SYM:
        s = s.replace(a, b)
    return s


INLINE = re.compile(r'`([^`]+)`|\*\*([^*]+)\*\*')


def _tt(s):
    """`code` → \\texttt，并在 / : . - 后补可断点（文件:行 这种长串在双栏里会撑出边界 ✗）。"""
    s = esc(s)
    for ch in ('/', ':', '.', '-', r'\_'):
        s = s.replace(ch, ch + r'\allowbreak{}')
    return r'\texttt{' + s + '}'


def inline(s):
    """行内 markdown：`code` / **bold** → LaTeX（其余部分转义 ✓）"""
    out, i = [], 0
    for m in INLINE.finditer(s):
        out.append(esc(s[i:m.start()]))
        if m.group(1) is not None:
            out.append(_tt(m.group(1)))
        else:
            out.append(r'\textbf{' + esc(m.group(2)) + '}')
        i = m.end()
    out.append(esc(s[i:]))
    return ''.join(out)


LABEL = re.compile(r'^\*\*([^*]+)\*\*[：:]\s*(.*)$')


def table(rows):
    cells = [[c.strip() for c in r.strip().strip('|').split('|')] for r in rows]
    cells = [c for c in cells if not all(re.fullmatch(r':?-{2,}:?', x or '') for x in c)]
    if not cells:
        return ''
    n = max(len(c) for c in cells)
    spec = '|' + '|'.join(['X'] * n) + '|'
    body = []
    for r in cells:
        r = r + [''] * (n - len(r))
        body.append(' & '.join(inline(x) for x in r) + r' \\ \hline')
    return ('\\begin{center}\n{\\small\n\\begin{tabularx}{\\textwidth}{' + spec +
            '}\n\\hline\n' + '\n'.join(body) + '\n\\end{tabularx}}\n\\end{center}')


def md2tex(text):
    lines, out, i = text.split('\n'), [], 0
    while i < len(lines):
        ln = lines[i]
        if ln.startswith('|'):                      # 表格
            rows = []
            while i < len(lines) and lines[i].startswith('|'):
                rows.append(lines[i]); i += 1
            out.append(table(rows)); continue
        if ln.startswith('- '):                     # 列表
            items = []
            while i < len(lines) and (lines[i].startswith('- ') or
                                      (items and lines[i].startswith('  ') and lines[i].strip())):
                if lines[i].startswith('- '):
                    items.append(lines[i][2:].strip())
                elif lines[i].strip():
                    items[-1] += ' ' + lines[i].strip()
                i += 1
            out.append('\\begin{itemize}[leftmargin=1.3em,itemsep=1pt,topsep=2pt,parsep=0pt]\n' +
                       '\n'.join('  \\item ' + inline(x) for x in items) + '\n\\end{itemize}')
            continue
        if ln.startswith('> '):                      # 引用块（markdown `>`）—— 以前会把 `>` 原样印出来 ✗
            para = [ln[2:].strip()]; i += 1
            while (i < len(lines) and lines[i].strip()
                   and not lines[i].startswith(('- ', '|', '---', '>'))
                   and not LABEL.match(lines[i])):
                para.append(lines[i].strip()); i += 1
            out.append('\\begin{quote}\\small ' + inline(' '.join(para)) + '\\end{quote}')
            continue
        m = LABEL.match(ln)
        if m:                                       # **作用**：xxx  →  \paragraph{作用}xxx
            out.append('\\paragraph{%s}%s' % (esc(m.group(1)), inline(m.group(2))))
            i += 1; continue
        if ln.startswith('---'):
            i += 1; continue
        if not ln.strip():
            i += 1; continue
        para = [ln.strip()]; i += 1                # 普通段落：合并连续行 ✓
        while (i < len(lines) and lines[i].strip() and not lines[i].startswith(('- ', '|', '---'))
               and not LABEL.match(lines[i])):
            para.append(lines[i].strip()); i += 1
        out.append(inline(' '.join(para)))
    return '\n\n'.join(x for x in out if x.strip())


def parse_cards(js_path):
    t = pathlib.Path(js_path).read_text(encoding='utf-8')
    cards = {}
    for blk in re.split(r'\n  "(?=\d+": \{)', t):
        m = re.match(r'(\d+)": \{(.*)', blk, re.S)
        if not m:
            continue
        cid, body = int(m.group(1)), m.group(2)
        nm = re.search(r'"name":\s*"([^"]*)"', body)
        inm = re.search(r'"internal_name":\s*"([^"]*)"', body)
        d = re.search(r'"desc":\s*\[(.*?)\]', body, re.S)
        desc = re.findall(r'"((?:[^"\\]|\\.)*)"', d.group(1)) if d else []
        cards[cid] = {'name': nm.group(1) if nm else str(cid),
                      'internal': inm.group(1) if inm else '',
                      'desc': [x.replace('\\"', '"') for x in desc]}
    return cards


def parse_guide(md_path):
    """按 `### <id> ...` 切出各卡正文 ✓（跳过 `## 速查` 那种总览 ✓）"""
    t = pathlib.Path(md_path).read_text(encoding='utf-8')
    secs = {}
    parts = re.split(r'^### (\d+)[^\n]*$', t, flags=re.M)
    for i in range(1, len(parts) - 1, 2):
        secs[int(parts[i])] = parts[i + 1].strip()
    return secs


def card_macros(cards):
    """系统卡的别名宏定义（**纯字母** ✓）"""
    L = ['% ── 卡名宏（自动生成 ✓；小写 = 普通，大写 = 加粗；其它卡在正文里用「编号　卡名」）──']
    for cid, al in sorted(CARD_ALIAS.items()):
        if cid not in cards:
            continue
        nm = cards[cid]['name']
        L.append('\\newcommand{\\%s}{%d %s}' % (al, cid, nm))
        L.append('\\newcommand{\\%s}{\\textbf{%d %s}}' % (al.upper(), cid, nm))
    return '\n'.join(L)


def _protect(tex):
    """把 \\texttt{...}（内部名/字段）换成占位符，避免卡名替换误伤 ✓"""
    store = []

    def rep(m):
        store.append(m.group(0)); return '\x00%d\x00' % (len(store) - 1)
    return re.sub(r'\\texttt\{[^{}]*\}', rep, tex), store


def _restore(tex, store):
    for i, s in enumerate(store):
        tex = tex.replace('\x00%d\x00' % i, s)
    return tex


def _protect_bold(tex):
    """把 **已经加粗** 的整段（\\textbf{...}）换成占位符。
    ★ owner 2026-10-20：PLAYER-GUIDE 正文里卡名已经手写成 **卡名** ⇒ 这里不能再套一层，
      也不能在加粗段里补「编号 」（否则「红魔乡的」会变成「106 红魔乡的」，等于改了正文 ✗）。
    """
    out, store, i = [], [], 0
    while True:
        j = tex.find(r'\textbf{', i)
        if j < 0:
            out.append(tex[i:])
            break
        out.append(tex[i:j])
        k, depth = j + len(r'\textbf{'), 1
        while k < len(tex) and depth:
            if tex[k] == '{' and tex[k - 1] != '\\':
                depth += 1
            elif tex[k] == '}' and tex[k - 1] != '\\':
                depth -= 1
            k += 1
        store.append(tex[j:k])
        out.append('\x02%d\x02' % (len(store) - 1))
        i = k
    return ''.join(out), store


def _restore_bold(tex, store):
    for i, s in enumerate(store):
        tex = tex.replace('\x02%d\x02' % i, s)
    return tex


def linkify(tex, cards):
    """卡名 → 宏 / 加粗文本。先「编号+卡名」再裸名；边界只排除数字紧贴 ✓。
       ★ 宏后补 `{}`：xeCJK 下汉字是 catcode 11 字母，会把宏名吞进控制序列 ✗。"""
    tex, store = _protect(tex)
    tex, bstore = _protect_bold(tex)   # ★ 已有的 **加粗** 整段不再二次套 bold / 补编号
    marks = []          # ★ 已替换过的片段先收进占位符：否则"编号+卡名"替完后，
                        #   "裸卡名"规则会再钻进刚生成的那段里 ⇒ 「92 92 伟大之作」✗（owner 2026-10-15 指出）
    def hold(s):
        marks.append(s)
        return '\x01%d\x01' % (len(marks) - 1)
    for nm, cid in sorted(((cards[c]['name'], c) for c in cards), key=lambda x: -len(x[0])):
        if len(nm) < 2:
            continue
        if cid in CARD_ALIAS:
            macro = '\\' + CARD_ALIAS[cid] + '{}'
            bold = '\\' + CARD_ALIAS[cid].upper() + '{}'
        else:
            macro = bold = '\\textbf{%d %s}' % (cid, nm)
        # ★ 一条规则吃掉「编号 卡名」「卡名 编号」「裸卡名」三种形态，且**连相邻的同一个编号一起吃掉**
        #   （否则旧写法「风神录 110」会被替成「110 风神录 110」✗ —— owner 2026-10-15 指出）
        def _rep(m, nm=nm, cid=cid, bold=bold, macro=macro):
            return hold(bold if (m.group(1) or m.group(2)) else macro)
        tex = re.sub(r'(?<![0-9])(?:(%d)\s*)?%s(?:\s*(%d))?(?![0-9A-Za-z\u4e00-\u9fff])'
                     % (cid, re.escape(nm), cid), _rep, tex)
    for i, mk in enumerate(marks):
        tex = tex.replace('\x01%d\x01' % i, mk)
    tex = _restore_bold(tex, bstore)
    return _restore(tex, store)


# ★ 中文字体：不写死路径；默认 Windows 自带的 Microsoft YaHei，其它机器用
#   GUIDE_CJK_FONT 换字体名，或靠 fontconfig 把这个名字映射到本机字体（见文件头）。
CJK_FONT = os.environ.get('GUIDE_CJK_FONT', 'Microsoft YaHei')

PRE = r'''\documentclass[11pt,a4paper,fontset=none]{ctexart}
\usepackage{xeCJK}
\setCJKmainfont{@CJK@}
\setCJKsansfont{@CJK@}
\setCJKmonofont{@CJK@}
\usepackage[a4paper,margin=2.1cm]{geometry}
\usepackage{graphicx}
\usepackage{xcolor}
\usepackage{tabularx}
\usepackage{multicol}
\usepackage{enumitem}
\usepackage[hidelinks]{hyperref}
\usepackage{needspace}
\pagestyle{plain}
\setcounter{secnumdepth}{0}
% ★ 带圈数字 ①②③…（U+2460..U+24FF）在 Latin 字体里没有字形 ⇒ 交给中文区 ✓
\xeCJKDeclareCharClass{CJK}{"2460 -> "24FF}
\xeCJKDeclareCharClass{CJK}{"2160 -> "217F}   % 罗马数字 Ⅰ Ⅱ Ⅲ
\xeCJKDeclareCharClass{CJK}{"2605 -> "2606}   % ★ ☆
\setlength{\parindent}{0pt}
\setlength{\parskip}{4pt}
\linespread{1.06}
\newcommand{\cardtitle}[1]{\section{#1}}
\newcommand{\printedlabel}{{\small\itshape 卡面文字}\par\smallskip}
\newcommand{\divider}{\par\medskip{\color{gray}\hrule}\medskip}
% ★ 部分标题：**不强制分页**（article 的 \part 会另起一页 ⇒ 那页只剩一行字 ✗，owner 2026-10-15 指出）
\newcommand{\parthdr}[2]{\par\needspace{3\baselineskip}{\Large\bfseries #1\quad #2}\par\medskip\addcontentsline{toc}{section}{#1 #2}}
\begin{document}
'''
# ★ 字体名在 PRE 里用占位符，读完环境变量再填 —— 避免 % 格式化误伤导言区的 % 注释 ✓
PRE = PRE.replace('@CJK@', CJK_FONT)


def main():
    js, md, out = sys.argv[1], sys.argv[2], sys.argv[3]
    imgdir = sys.argv[4] if len(sys.argv) > 4 else 'cards'
    cards = parse_cards(js)
    guide = parse_guide(md)
<<<<<<< HEAD
    sysids = [i for i in range(106, 118) if i in cards]      # ★ 系统卡 = 106–117
    extra = [i for i in (118,) if i in cards]               # ★ 附：118 挑战卡
    skip = {200}                                            # ★ 不进图册：200（测试卡）；201 是调试卡，owner 2026-10-19 要它进册
    others = sorted(i for i in cards if i not in sysids and i not in extra and i not in skip)
=======
    skip = {200}                    # ★ 不进图册：200（测试卡）；201 是调试卡，owner 2026-10-19 要它进册
    # ★ owner 2026-10-20：所有卡统一模板、按 id 升序 —— 58…92、106…118、201 一条线排下去 ✓
    ids = sorted(i for i in cards if i not in skip)
>>>>>>> 16f924cf (card-guide: 同步 modkit 一卡一页版 make_guide.py；106/109 协同句收成一句)

    L = [PRE, card_macros(cards)]
    L.append(r'\begin{center}')
    L.append(r'{\Huge\bfseries 卡牌手册}\\[6pt]')
    L.append(r'{\large 本 mod 全部卡片图鉴（按卡号排列）}\\[10pt]')
    L.append(r'{\small 系统卡 106--117 = \textbf{原作编号 + 100}：106 = 红魔乡，\ldots，117 = 鬼形兽。}')
    L.append(r'\end{center}')
    L.append(r'\vspace{6pt}')
<<<<<<< HEAD
    L.append(r'本册分两部分：\textbf{第一部分}逐张讲 106--117 怎么用、代价是什么（正文来自玩家手册）；'
             r'\textbf{第二部分}是本 mod 其它卡片的图鉴 —— 卡面、卡面上印的字，以及每张一段展开说明（定位 / 数值 / 用法 / 注意事项）。'
             r'118 是挑战卡；201 是临时调试卡，正式版会删。测试卡 200 不进本册。')
=======
    L.append(r'本册\textbf{一卡一页}：每张卡先给卡图与卡面文字，下面接玩家手册里的展开说明'
             r'（定位 / 数值 / 用法 / 注意事项）；正文长时自然续页，没有正文的卡只留卡面。'
             r'58--92 是其它卡，106--117 是系统卡，118 是挑战卡；201 是调试卡，正常游玩用不到。')
>>>>>>> 16f924cf (card-guide: 同步 modkit 一卡一页版 make_guide.py；106/109 协同句收成一句)
    L.append(r'\tableofcontents')
    L.append(r'\clearpage')

    for n, cid in enumerate(ids):
        c = cards[cid]
        if n:                       # ★ 第 1 张紧跟目录后的那一页；其余每张卡另起一页 ✓
            L.append(r'\clearpage')
        # ★ 页眉/编号/名字与系统卡一致：\cardtitle = \section，标题就是「编号　卡名」✓
        # ★ owner 2026-10-20：109 还在开发中 ⇒ 标题后缀（指南里同步标注；卡面 desc 也加了一句）
        note = '（开发中）' if cid == 109 else ''
        L.append(r'\cardtitle{%d %s%s}' % (cid, esc(c['name']), esc(note)))
        # ★ internal_name 只用来找卡图（<internal_name>_max.png），与系统卡同一套 ✓
        img = pathlib.Path('%s/%s_max.png' % (imgdir, c['internal'])) if c['internal'] else None
        if img is not None and img.exists():
            L.append(r'\noindent\begin{minipage}[t]{0.41\textwidth}')
            L.append(r'\includegraphics[width=\linewidth]{%s}' % str(img))
            L.append(r'\end{minipage}\hfill')
            L.append(r'\begin{minipage}[t]{0.55\textwidth}')
        else:                       # 没卡图就整行给卡面文字（不硬凑占位图 ✗）
            L.append(r'\noindent\begin{minipage}[t]{0.98\textwidth}')
        L.append(r'\printedlabel')
        L.append(r'{\small %s}\par' % '\\\\'.join(inline(x) for x in c['desc']))
        L.append(r'\end{minipage}')
        L.append(r'\divider')
        if cid in guide:            # ★ 没有正文的卡就留空（不硬凑 ✗）
            L.append(linkify(md2tex(guide[cid]), cards))
        L.append('')
<<<<<<< HEAD
    # ── 附：118 挑战卡 ──
    if extra:
        L.append(r'\clearpage')
        L.append(r'\section{附：118 挑战・十二合一（挑战卡）}')
        for cid in extra:
            c = cards[cid]
            L.append(r'\noindent\begin{minipage}[t]{0.41\textwidth}')
            L.append(r'\includegraphics[width=\linewidth]{%s/%s_max.png}' % (imgdir, c['internal']))
            L.append(r'\end{minipage}\hfill')
            L.append(r'\begin{minipage}[t]{0.55\textwidth}')
            L.append(r'\printedlabel')
            L.append(r'{\small %s}\par' % '\\\\'.join(inline(x) for x in c['desc']))
            L.append(r'\end{minipage}')
            if cid in guide:                    # ★ 118 也带展开说明（owner 2026-10-19）
                L.append(r'\divider')
                L.append(r'{\footnotesize ' + linkify(md2tex(guide[cid]), cards) + '}')
            L.append('')
    # ── 第二部分：其它卡 ──
    L.append(r'\clearpage')
    L.append(r'\parthdr{第二部分}{本 mod 其它卡}')
    L.append(r'{\small 下列卡片按卡号排列；每张给卡面图、卡面上印的字，以及一段展开说明。}')
    L.append(r'\begin{multicols}{2}')
    for cid in others:
        c = cards[cid]
        img = pathlib.Path('%s/%s_max.png' % (imgdir, c['internal']))
        L.append(r'\needspace{0.20\textheight}')
        if c['internal'] and img.exists():
            L.append(r'\noindent\begin{minipage}[c]{0.33\linewidth}')
            L.append(r'\includegraphics[width=\linewidth]{%s}' % str(img))
            L.append(r'\end{minipage}\hfill')
            L.append(r'\begin{minipage}[c]{0.64\linewidth}')
        else:
            L.append(r'\noindent\begin{minipage}[c]{0.98\linewidth}')
        L.append(r'{\bfseries %d %s}\par\smallskip' % (cid, esc(c['name'])))
        L.append(r'{\footnotesize %s}\par' % '\\\\'.join(inline(x) for x in c['desc']))
        L.append(r'\end{minipage}')
        if cid in guide:                    # ★ 非系统卡的展开说明（owner 2026-10-19）
            L.append(r'{\footnotesize ' + linkify(md2tex(guide[cid]), cards) + '}')
        L.append(r'\par\smallskip{\color{gray}\hrule}\smallskip')
    L.append(r'\end{multicols}')
=======
>>>>>>> 16f924cf (card-guide: 同步 modkit 一卡一页版 make_guide.py；106/109 协同句收成一句)
    L.append(r'\end{document}')
    pathlib.Path(out).write_text('\n'.join(x for x in L if x is not None), encoding='utf-8')
    print('  已写 %s' % out)
    print('  共 %d 张卡（一卡一页；有正文的 %d 张）' % (len(ids), sum(1 for i in ids if i in guide)))
    miss = [i for i in ids
            if cards[i]['internal'] and not pathlib.Path('%s/%s_max.png' % (imgdir, cards[i]['internal'])).exists()]
    print('  缺卡面图：%s' % (miss or '无 ✓'))


if __name__ == '__main__':
    main()
