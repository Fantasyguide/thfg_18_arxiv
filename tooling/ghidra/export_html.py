#!/usr/bin/env python3
"""把 Ghidra 库的当前状态导成一份可离线翻的 HTML —— 「不抢锁的那个视图」。

**为什么要有它**：Ghidra 的工程锁是**工程级独占**的，GUI 开着就没人能跑 driver，
MCP 开着库 driver 也进不去。所以「人在 GUI 里看」和「模型在跑分析」是互斥的。
这个导出器把两者解耦：**导出时占一次锁（几分钟），之后你随便翻都不占**。

**导什么**：对齐 `ghidra-re` MCP 那些只读工具能看到的东西，让「你在 HTML 里看到的」
≈「模型调 MCP 能看到的」：

    decompile_function          -> 反编译 C
    disassemble_function        -> 反汇编
    get_xrefs_to / _from        -> 双向交叉引用
    get_call_graph              -> 调用者 / 被调用者
    list_decompiler_variables   -> 局部变量与参数
    read_bytes                  -> 入口原始字节（定 hook 点用）
    get_structure/list_local_types -> 结构体与类型
    get_strings                 -> 字符串表
    list_functions / list_names -> 函数表 / 符号表

还多给一样 MCP 没有的：**每个名字是谁给的**（ExpHP / 我们 / Ghidra 分析器 / 默认），
这决定了你该信它几分。

    python export_html.py th18 [--limit N] [--no-decomp]

产物在 `local/<版本>/state/`（gitignored）。用 `serve.sh` 起个只绑 127.0.0.1 的
http.server 看，或直接 scp 下来。附带一份 `data/functions.jsonl` 给模型冷启动读。
"""
import argparse
import html
import json
import os
import re
import sys
import time

# 单个函数最多列多少条反汇编。最大的 ECL VM 约 6600 条，留足余量。
DISASM_LIMIT = 20000
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _driver import REPO, open_program, resolve                      # noqa: E402
from symbols import exphp_baseline                                   # noqa: E402

CSS = """
:root{--bg:#fbfaf8;--fg:#1c1a17;--dim:#6b6560;--line:#e2ddd6;--card:#fff;
      --acc:#8c5a2b;--ok:#2f6f4f;--warn:#a8620f;--code:#f5f2ed}
@media(prefers-color-scheme:dark){:root{--bg:#16150f;--fg:#e8e3da;--dim:#948d84;
      --line:#2e2b25;--card:#1d1c16;--acc:#d9a066;--ok:#7fbf9a;--warn:#e0a35c;--code:#232019}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);
     font:14px/1.6 ui-sans-serif,system-ui,"Noto Sans CJK SC",sans-serif}
a{color:var(--acc);text-decoration:none}a:hover{text-decoration:underline}
header{position:sticky;top:0;background:var(--bg);border-bottom:1px solid var(--line);
       padding:10px 20px;display:flex;gap:18px;align-items:baseline;flex-wrap:wrap;z-index:9}
header b{font-size:15px}header nav a{margin-right:14px;font-size:13px}
main{max-width:1180px;margin:0 auto;padding:22px 20px 60px}
h1{font-size:20px;margin:0 0 4px}h2{font-size:15px;margin:30px 0 10px;
   padding-bottom:5px;border-bottom:1px solid var(--line)}
.sub{color:var(--dim);font-size:13px;margin:0 0 18px}
code,pre,.mono{font-family:ui-monospace,"JetBrains Mono",Menlo,Consolas,monospace}
pre{background:var(--code);border:1px solid var(--line);border-radius:6px;
    padding:12px 14px;overflow-x:auto;font-size:12.5px;line-height:1.55}
table{border-collapse:collapse;width:100%;font-size:13px}
th,td{text-align:left;padding:6px 10px;border-bottom:1px solid var(--line);vertical-align:top}
th{color:var(--dim);font-weight:600;font-size:12px;text-transform:uppercase;letter-spacing:.04em}
tr:hover td{background:var(--card)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px;margin:16px 0}
.stat{background:var(--card);border:1px solid var(--line);border-radius:8px;padding:12px 14px}
.stat .n{font-size:22px;font-weight:600}.stat .l{color:var(--dim);font-size:12px}
.tag{display:inline-block;padding:1px 7px;border-radius:10px;font-size:11px;
     border:1px solid var(--line);color:var(--dim);white-space:nowrap}
.tag.exphp{color:var(--acc);border-color:var(--acc)}
.tag.ours{color:var(--ok);border-color:var(--ok)}
.tag.auto{color:var(--warn);border-color:var(--warn)}
input[type=search]{width:100%;padding:9px 12px;font-size:14px;border-radius:6px;
     border:1px solid var(--line);background:var(--card);color:var(--fg)}
.bar{height:8px;background:var(--code);border-radius:4px;overflow:hidden;margin:6px 0}
.bar>i{display:block;height:100%;background:var(--acc)}
.cols{display:grid;grid-template-columns:1fr 1fr;gap:22px}
@media(max-width:800px){.cols{grid-template-columns:1fr}}
.muted{color:var(--dim)}.right{text-align:right}
"""

JS_INDEX = """
const q=document.getElementById('q'),tb=document.getElementById('rows'),
      cnt=document.getElementById('cnt');let DATA=[];
fetch('index.json').then(r=>r.json()).then(d=>{DATA=d;render('')});
function render(f){
  f=f.trim().toLowerCase();
  let rows=DATA;
  if(f){let re=null;try{re=new RegExp(f,'i')}catch(e){}
    rows=DATA.filter(x=>re?re.test(x.n)||re.test(x.a):(x.n.toLowerCase().includes(f)||x.a.includes(f)));}
  cnt.textContent=rows.length+' / '+DATA.length;
  tb.innerHTML=rows.slice(0,600).map(x=>
    `<tr><td class=mono><a href="f/${x.a}.html">${x.a}</a></td>`+
    `<td class=mono>${x.n}</td><td class=right>${x.s}</td>`+
    `<td class=right>${x.x}</td><td><span class="tag ${x.l}">${x.l}</span></td></tr>`).join('')
    +(rows.length>600?`<tr><td colspan=5 class=muted>…… 只显示前 600 条，再筛细一点</td></tr>`:'');
}
q.addEventListener('input',e=>render(e.target.value));
"""


def esc(s):
    return html.escape(str(s) if s is not None else "")


def page(title, body, depth=0, nav=True):
    up = "../" * depth
    n = ""
    if nav:
        n = (f'<nav><a href="{up}index.html">函数</a><a href="{up}structs.html">结构体</a>'
             f'<a href="{up}opcodes.html">opcode 表</a><a href="{up}globals.html">全局</a>'
             f'<a href="{up}strings.html">字符串</a></nav>')
    return (f"<!doctype html><meta charset=utf-8>"
            f"<meta name=viewport content='width=device-width,initial-scale=1'>"
            f"<title>{esc(title)}</title><style>{CSS}</style>"
            f"<header><b>{esc(title)}</b>{n}</header><main>{body}</main>")


# ─────────────────────────────────────────────────────────── 采集

def layer_of(addr, name, src, base, ours):
    """这个名字是谁给的 —— 决定你该信它几分。"""
    if name.startswith(("FUN_", "DAT_", "LAB_", "thunk_FUN_")):
        return "none"
    if addr in ours:
        return "ours"
    if base["funcs"].get(addr) == name or base["statics"].get(addr) == name:
        return "exphp"
    return "ours" if src == "USER_DEFINED" else "auto"


def collect(prog, base, ours_addrs, limit=None, decomp=True):
    from ghidra.app.decompiler import DecompInterface, DecompileOptions
    from ghidra.util.task import ConsoleTaskMonitor
    from ghidra.program.model.symbol import SourceType

    mon = ConsoleTaskMonitor()
    lst, fm, st = prog.getListing(), prog.getFunctionManager(), prog.getSymbolTable()
    mem, refs = prog.getMemory(), prog.getReferenceManager()

    di = None
    if decomp:
        di = DecompInterface()
        di.setOptions(DecompileOptions())
        di.openProgram(prog)

    funcs, t0 = [], time.time()
    all_f = list(fm.getFunctions(True))
    if limit:
        all_f = all_f[:limit]
    total = len(all_f)

    for i, f in enumerate(all_f):
        if i % 200 == 0:
            print(f"      反编译 {i}/{total} ({time.time()-t0:.0f}s)", flush=True)
        entry = f.getEntryPoint()
        a = "%08x" % entry.getOffset()
        sym = f.getSymbol()
        src = str(sym.getSource()) if sym else "DEFAULT"

        rec = {
            "addr": a, "name": f.getName(),
            "sig": f.getSignature().getPrototypeString(True),
            "cc": f.getCallingConventionName(),
            "size": int(f.getBody().getNumAddresses()),
            "thunk": bool(f.isThunk()),
            "layer": layer_of(entry.getOffset(), f.getName(), src, base, ours_addrs),
            "src": src,
        }

        # 调用图（= MCP 的 get_call_graph）
        rec["callers"] = sorted({("%08x" % x.getEntryPoint().getOffset(), x.getName())
                                 for x in f.getCallingFunctions(mon)})
        rec["callees"] = sorted({("%08x" % x.getEntryPoint().getOffset(), x.getName())
                                 for x in f.getCalledFunctions(mon)})
        # 入口的引用数（= MCP 的 get_xrefs_to）
        rec["xrefs"] = sum(1 for _ in refs.getReferencesTo(entry))

        # 注释
        from ghidra.program.model.listing import CodeUnit
        cm = {}
        for k, cid in (("plate", CodeUnit.PLATE_COMMENT), ("pre", CodeUnit.PRE_COMMENT),
                       ("eol", CodeUnit.EOL_COMMENT)):
            t = lst.getComment(cid, entry)
            if t:
                cm[k] = t
        rec["comments"] = cm

        # 入口前 16 字节（= MCP 的 read_bytes）—— 定 hook 点要看能不能塞 5 字节 jmp
        try:
            import jpype
            buf = jpype.JArray(jpype.JByte)(16)
            mem.getBytes(entry, buf)
            rec["entry_bytes"] = " ".join("%02x" % (b & 0xFF) for b in buf)
        except Exception:
            rec["entry_bytes"] = ""

        # 反汇编（= MCP 的 disassemble_function）
        # ⚠️ 上限曾是 4000 且**静默截断**：TH06NC 的 ECL VM（27928 字节 ≈ 6600 条）
        # 被砍掉最后三分之一，页面上却照样写「反汇编（4000 条）」——照着它做统计会
        # 得出错误结论（实际踩过）。现在上限抬高，且截断必须在页面上说出来。
        dis = []
        truncated = False
        it = lst.getInstructions(f.getBody(), True)
        while it.hasNext():
            ins = it.next()
            dis.append(("%08x" % ins.getAddress().getOffset(), str(ins)))
            if len(dis) >= DISASM_LIMIT:
                truncated = it.hasNext()
                break
        rec["disasm"] = dis
        rec["disasm_truncated"] = truncated

        # 反编译 + 局部变量（= decompile_function / list_decompiler_variables）
        rec["c"] = ""
        rec["vars"] = []
        if di is not None:
            r = di.decompileFunction(f, 60, mon)
            if r.decompileCompleted():
                rec["c"] = r.getDecompiledFunction().getC()
                hf = r.getHighFunction()
                if hf is not None:
                    sm = hf.getLocalSymbolMap()
                    for s in sm.getSymbols():
                        rec["vars"].append({"name": s.getName(),
                                            "type": s.getDataType().getName(),
                                            "param": bool(s.isParameter())})
            else:
                rec["c"] = "// 反编译失败：" + str(r.getErrorMessage())
        funcs.append(rec)

    if di is not None:
        di.dispose()

    # 结构体（= list_structures / get_structure）
    dtm = prog.getDataTypeManager()
    structs = []
    for dt in dtm.getAllStructures():
        structs.append({
            "name": dt.getName(), "size": dt.getLength(),
            "desc": dt.getDescription() or "",
            "fields": [{"off": c.getOffset(), "name": c.getFieldName(),
                        "type": c.getDataType().getName(), "size": c.getLength(),
                        "note": c.getComment() or ""}
                       for c in dt.getComponents() if c.getFieldName()],
        })
    structs.sort(key=lambda s: s["name"])

    # 全局（= list_names + get_type_info）
    globs = []
    di2 = lst.getDefinedData(True)
    while di2.hasNext():
        d = di2.next()
        s = d.getPrimarySymbol()
        if s is None or s.getSource() == SourceType.DEFAULT:
            continue
        globs.append({"addr": "%08x" % d.getAddress().getOffset(), "name": s.getName(),
                      "type": d.getDataType().getName(), "size": d.getLength(),
                      "layer": layer_of(d.getAddress().getOffset(), s.getName(),
                                        str(s.getSource()), base, ours_addrs),
                      "xrefs": sum(1 for _ in refs.getReferencesTo(d.getAddress()))})

    # opcode 表（labels.json 套进来的那批）
    ops = {}
    for sym in st.getAllSymbols(False):
        m = re.match(r"^([a-z]+)__case_(.+)$", sym.getName())
        if not m:
            continue
        addr = sym.getAddress()
        host = fm.getFunctionContaining(addr)
        ops.setdefault(m.group(1), []).append(
            {"addr": "%08x" % addr.getOffset(), "case": m.group(2),
             "host": host.getName() if host else "?",
             "host_addr": "%08x" % host.getEntryPoint().getOffset() if host else ""})
    for g in ops:
        ops[g].sort(key=lambda r: r["addr"])

    # 字符串（= get_strings）
    strs = []
    di3 = lst.getDefinedData(True)
    while di3.hasNext():
        d = di3.next()
        if d.hasStringValue():
            v = str(d.getValue())
            if len(v) >= 4:
                strs.append({"addr": "%08x" % d.getAddress().getOffset(), "text": v[:300],
                             "xrefs": sum(1 for _ in refs.getReferencesTo(d.getAddress()))})

    meta = {"exe": os.path.basename(str(prog.getExecutablePath() or "")),
            "md5": str(prog.getExecutableMD5() or ""),
            "imagebase": "%08x" % prog.getImageBase().getOffset(),
            "generated": time.strftime("%Y-%m-%d %H:%M")}
    return funcs, structs, globs, ops, strs, meta


# ─────────────────────────────────────────────────────────── 渲染

def render_func(rec, out_dir):
    a, nm = rec["addr"], rec["name"]
    b = [f"<h1 class=mono>{esc(nm)}</h1>",
         f"<p class=sub><code>0x{a}</code> · {rec['size']} 字节 · {rec['xrefs']} 处引用 · "
         f"<span class='tag {rec['layer']}'>{rec['layer']}</span> · {esc(rec['cc'] or '')}"
         f"{' · thunk' if rec['thunk'] else ''}</p>",
         f"<pre>{esc(rec['sig'])}</pre>"]

    for k, label in (("plate", "plate 注释"), ("pre", "pre 注释"), ("eol", "行尾注释")):
        if rec["comments"].get(k):
            b.append(f"<h2>{label}</h2><pre>{esc(rec['comments'][k])}</pre>")

    b.append("<h2>hook 信息</h2>")
    b.append(f"<p class=sub>入口 <code>0x{a}</code>，前 16 字节 "
             f"<code>{esc(rec['entry_bytes'])}</code>。5 字节 <code>jmp rel32</code> "
             f"会覆盖下面头几条指令——看清边界再下手。</p>")
    if rec["disasm"]:
        head = "\n".join(f"{d[0]}  {d[1]}" for d in rec["disasm"][:6])
        b.append(f"<pre>{esc(head)}</pre>")

    if rec["c"]:
        b.append(f"<h2>反编译（模型读到的就是这个）</h2><pre>{esc(rec['c'])}</pre>")

    if rec["vars"]:
        rows = "".join(f"<tr><td class=mono>{esc(v['name'])}</td><td class=mono>{esc(v['type'])}</td>"
                       f"<td>{'参数' if v['param'] else ''}</td></tr>" for v in rec["vars"])
        b.append(f"<h2>局部变量与参数</h2><table><tr><th>名字</th><th>类型</th><th></th></tr>{rows}</table>")

    b.append("<div class=cols>")
    for key, label in (("callers", "调用者"), ("callees", "被调用")):
        rows = "".join(f'<tr><td class=mono><a href="{x[0]}.html">0x{x[0]}</a></td>'
                       f'<td class=mono>{esc(x[1])}</td></tr>' for x in rec[key]) \
            or '<tr><td colspan=2 class=muted>无</td></tr>'
        b.append(f"<div><h2>{label}（{len(rec[key])}）</h2><table>{rows}</table></div>")
    b.append("</div>")

    if rec["disasm"]:
        full = "\n".join(f"{d[0]}  {d[1]}" for d in rec["disasm"])
        warn = ""
        if rec.get("disasm_truncated"):
            warn = (f"<p class=sub><b>⚠️ 已截断</b>：只列出前 {DISASM_LIMIT} 条，"
                    f"函数还没完。别拿这一页做全函数统计。</p>")
        b.append(f"<h2>反汇编（{len(rec['disasm'])} 条）</h2>{warn}<pre>{esc(full)}</pre>")

    with open(os.path.join(out_dir, "f", a + ".html"), "w", encoding="utf-8") as fh:
        fh.write(page(nm, "".join(b), depth=1))


def render_index(funcs, meta, out_dir):
    by = Counter(f["layer"] for f in funcs)
    named = sum(1 for f in funcs if f["layer"] != "none")
    pct = 100 * named // max(1, len(funcs))
    stats = "".join(f'<div class=stat><div class=n>{v}</div><div class=l>{k}</div></div>'
                    for k, v in (("函数总数", len(funcs)), ("已命名", named),
                                 ("ExpHP 给名", by["exphp"]), ("我们命名", by["ours"]),
                                 ("分析器给名", by["auto"]), ("仍是 FUN_", by["none"])))
    b = [f"<h1>{esc(meta['exe'])} — 库状态</h1>",
         f"<p class=sub>md5 <code>{esc(meta['md5'])}</code> · imagebase "
         f"<code>0x{meta['imagebase']}</code> · 导出于 {meta['generated']}</p>",
         f"<div class=bar><i style='width:{pct}%'></i></div>",
         f"<p class=sub>{named}/{len(funcs)} 已命名（{pct}%）。"
         f"<b>层</b>：<span class='tag exphp'>exphp</span> ExpHP 给的名字 · "
         f"<span class='tag ours'>ours</span> 我们起的（进 symbols.json） · "
         f"<span class='tag auto'>auto</span> Ghidra 分析器猜的（FunctionID / demangler，"
         f"别当结论） · <span class=tag>none</span> 还是 FUN_。</p>",
         f"<div class=grid>{stats}</div>",
         "<h2>全库检索</h2>",
         "<p class=sub>按名字或地址过滤，支持正则。共 <span id=cnt></span></p>",
         "<input type=search id=q placeholder='比如  Card  |  ^AnmVm  |  0045be'>",
         "<table><tr><th>地址</th><th>名字</th><th class=right>字节</th>"
         "<th class=right>引用</th><th>层</th></tr><tbody id=rows></tbody></table>",
         f"<script>{JS_INDEX}</script>"]
    open(os.path.join(out_dir, "index.html"), "w", encoding="utf-8").write(
        page(meta["exe"] + " 库状态", "".join(b)))
    json.dump([{"a": f["addr"], "n": f["name"], "s": f["size"], "x": f["xrefs"], "l": f["layer"]}
               for f in funcs], open(os.path.join(out_dir, "index.json"), "w"),
              ensure_ascii=False, separators=(",", ":"))


def render_structs(structs, out_dir):
    b = [f"<h1>结构体与类型（{len(structs)}）</h1>",
         "<p class=sub>ExpHP 的 type-structs / 位域导进来的布局。字段的 note 列是 ExpHP "
         "原始类型声明；名字形如 <code>field_xx</code> 的是他也没标出来的区域。</p>"]
    for s in structs:
        rows = "".join(
            f"<tr><td class=mono>+0x{f['off']:x}</td><td class=mono>{esc(f['name'])}</td>"
            f"<td class=mono>{esc(f['type'])}</td><td class=right>{f['size']}</td>"
            f"<td class=muted>{esc(f['note'])}</td></tr>" for f in s["fields"])
        desc = f"<p class=sub>{esc(s['desc'])}</p>" if s["desc"] else ""
        b.append(f"<h2 id={esc(s['name'])} class=mono>{esc(s['name'])} "
                 f"<span class=muted>0x{s['size']:x} 字节 · {len(s['fields'])} 字段</span></h2>{desc}"
                 f"<table><tr><th>偏移</th><th>字段</th><th>类型</th><th class=right>大小</th>"
                 f"<th>ExpHP 声明</th></tr>{rows}</table>")
    open(os.path.join(out_dir, "structs.html"), "w", encoding="utf-8").write(
        page("结构体", "".join(b)))


def render_opcodes(ops, out_dir):
    total = sum(len(v) for v in ops.values())
    b = [f"<h1>opcode 表（{total}）</h1>",
         "<p class=sub>来自 ExpHP <code>labels.json</code>：VM 指令分发函数体内的 switch case "
         "标签，每条 = 一个 opcode → 处理分支地址。</p>"]
    for g in sorted(ops):
        rows = "".join(
            f"<tr><td class=mono><a href='f/{r['host_addr']}.html#'>0x{r['addr']}</a></td>"
            f"<td class=mono>{esc(r['case'])}</td><td class=mono>{esc(r['host'])}</td></tr>"
            for r in ops[g])
        b.append(f"<h2>{esc(g)}（{len(ops[g])}）</h2>"
                 f"<table><tr><th>地址</th><th>opcode</th><th>宿主函数</th></tr>{rows}</table>")
    open(os.path.join(out_dir, "opcodes.html"), "w", encoding="utf-8").write(
        page("opcode 表", "".join(b)))


def render_globals(globs, out_dir):
    rows = "".join(
        f"<tr><td class=mono>0x{g['addr']}</td><td class=mono>{esc(g['name'])}</td>"
        f"<td class=mono>{esc(g['type'])}</td><td class=right>{g['size']}</td>"
        f"<td class=right>{g['xrefs']}</td><td><span class='tag {g['layer']}'>{g['layer']}</span></td></tr>"
        for g in sorted(globs, key=lambda x: x["addr"]))
    b = [f"<h1>全局（{len(globs)}）</h1>",
         "<p class=sub>带类型的全局才会在反编译里显示成 <code>PTR-&gt;field</code>；"
         "类型是 <code>undefined*</code> 或 <code>void *</code> 的说明还没绑好。</p>",
         f"<table><tr><th>地址</th><th>名字</th><th>类型</th><th class=right>大小</th>"
         f"<th class=right>引用</th><th>层</th></tr>{rows}</table>"]
    open(os.path.join(out_dir, "globals.html"), "w", encoding="utf-8").write(
        page("全局", "".join(b)))


def render_strings(strs, out_dir):
    rows = "".join(f"<tr><td class=mono>0x{s['addr']}</td><td class=right>{s['xrefs']}</td>"
                   f"<td class=mono>{esc(s['text'])}</td></tr>"
                   for s in sorted(strs, key=lambda x: -x["xrefs"])[:4000])
    b = [f"<h1>字符串（{len(strs)}）</h1>",
         "<p class=sub>按引用数排序。找资源名 / 格式串 / 调试残留的入口。最多显示 4000 条。</p>",
         f"<table><tr><th>地址</th><th class=right>引用</th><th>内容</th></tr>{rows}</table>"]
    open(os.path.join(out_dir, "strings.html"), "w", encoding="utf-8").write(
        page("字符串", "".join(b)))


def write_jsonl(funcs, out_dir):
    """给模型冷启动读的：一行一个函数，含反编译文本。反汇编不进来（太占体积）。"""
    p = os.path.join(out_dir, "data", "functions.jsonl")
    with open(p, "w", encoding="utf-8") as fh:
        for f in funcs:
            fh.write(json.dumps({
                "addr": f["addr"], "name": f["name"], "sig": f["sig"], "size": f["size"],
                "layer": f["layer"], "xrefs": f["xrefs"],
                "callers": [c[1] for c in f["callers"]],
                "callees": [c[1] for c in f["callees"]],
                "comments": f["comments"], "decompiled": f["c"],
            }, ensure_ascii=False) + "\n")
    return p


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__)
    ap.add_argument("version", help="版本号，如 th16 / th18")
    ap.add_argument("--limit", type=int, help="只导前 N 个函数（调试用）")
    ap.add_argument("--no-decomp", action="store_true", help="跳过反编译（快，但就没了大头）")
    a = ap.parse_args()

    P = resolve(a.version)
    base = exphp_baseline(P["data_dir"])
    ours = set()
    if P["symbols"].exists():
        d = json.load(open(P["symbols"], encoding="utf-8"))
        for k in ("funcs", "statics", "labels"):
            ours |= {int(r["addr"], 16) for r in d.get(k, [])}

    out = P["vdir"] / "state"
    for sub in ("", "f", "data"):
        os.makedirs(out / sub, exist_ok=True)

    print(f"[export] {a.version} -> {out.relative_to(REPO)}")
    with open_program(P["proj_dir"].resolve(), P["proj_name"], P["program"]) as prog:
        funcs, structs, globs, ops, strs, meta = collect(
            prog, base, ours, a.limit, not a.no_decomp)

    print(f"      渲染 {len(funcs)} 个函数页 …", flush=True)
    for rec in funcs:
        render_func(rec, out)
    render_index(funcs, meta, out)
    render_structs(structs, out)
    render_opcodes(ops, out)
    render_globals(globs, out)
    render_strings(strs, out)
    jl = write_jsonl(funcs, out)

    size = sum(os.path.getsize(os.path.join(dp, f))
               for dp, _, fs in os.walk(out) for f in fs)
    print(f"\n[export] 完成：函数 {len(funcs)} · 结构体 {len(structs)} · 全局 {len(globs)} · "
          f"opcode {sum(len(v) for v in ops.values())} · 字符串 {len(strs)}")
    print(f"         {out.relative_to(REPO)}  共 {size/1048576:.1f} MB")
    print(f"         模型用：{os.path.relpath(jl, REPO)}")
    print(f"\n看它：tooling/ghidra/serve.sh {a.version}")


if __name__ == "__main__":
    main()
