#!/usr/bin/env python3
"""我们自己那层符号的往返：DB ⇄ `games/<版本>/symbols.json`。

**为什么要有这东西**：Ghidra 工程在 `local/`，而 `local/*` 是 gitignored 的（仓库不留版权
字节）。所以「把 zAbilityManager* 绑到 AbilityManager::on_tick 的 this」这种成果只活在本地
DB 里——换台机器就没了，`bootstrap.py --reanalyze` 一跑也没了。ExpHP 那层能从 vendor 重放，
我们这层不能，于是就有了这个文件。

    symbols.py status <ver>    # DB ⇄ 仓库文件对 diff，两个方向都报
    symbols.py export <ver>    # DB → 仓库
    symbols.py apply  <ver>    # 仓库 → DB（覆盖语义：我们这层压过 ExpHP）

**只导出「我们的」**：`th-re-data` 上游无 LICENSE，不擅自转发。导出时现场与 ExpHP 数据
diff，凡逐字相同的一律剔掉——判定规则见 `_is_ours_*`。函数原型 / 调用约定 / 全局类型绑定
一律留下，因为 ExpHP 根本不提供这些（它给名字和结构体布局，不给绑定），而正是绑定决定了
反编译好不好读。

⚠️ **自定义存储必须连存储一起往返**。`bind_types.py` 绑 `__thiscall` 时把 this 钉在 ECX、
其余参数排栈（`CUSTOM_STORAGE`）。若只导「名字 + 类型」，`apply` 会按动态存储重建，
而 `__thiscall` 下 Ghidra 会自己插一个 auto `this`，把 `self` 挤到第一个**栈**参数位上——
268 个签名一次性静默毁掉。所以 `custom_storage` / `storage` / `ret_storage` 三个字段是必需的。
**这条是实测踩出来的**，别为了让 json 好看把它们删了。

**已知盲区**：结构体字段的归属靠「字段名或长度与 ExpHP 不同」判定。如果你改了某字段的类型
但**既没改名、也没改长度**（比如 char[4] → int），导出会漏掉它。养成改类型时顺手改名的习惯
（本来也该改——你既然搞懂了它是什么，field_40 这名字就该退休了）。
"""
import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _driver import REPO, open_program, resolve                      # noqa: E402
from import_th_re_data import _keep, _san                            # noqa: E402
from import_th_re_data_structs import struct_members                 # noqa: E402
from import_th_re_data_labels import label_name                      # noqa: E402

TH_RE_TAG = "[th-re-data]"
TH06_RE_TAG = "[th06-re]"      # import_th06_re_xml.py 打的标
VENDOR_TAGS = (TH_RE_TAG, TH06_RE_TAG)
GHIDRA_DEFAULT_PREFIXES = ("FUN_", "DAT_", "LAB_", "SUB_", "UNK_", "EXT_",
                           "thunk_FUN_", "switchD", "switchdataD", "caseD")
COMMENT_KINDS = ("plate", "pre", "eol", "post", "repeatable")

# Ghidra 各分析器自己写的样板注释，不是人的笔记，别往仓库里倒。
# （实测 th18：这几条占掉 1856 条里的 1850 条。）
ANALYZER_COMMENT = re.compile(
    r"^(Library Function -"
    r"|IMAGE_THUNK_DATA32$"
    r"|IMAGE_IMPORT_"
    r"|TypeDescriptor\.name$"
    r"|\d+\s+Ordinal_\d+"
    r"|.*RTTI "
    r"|guard_check_icall$"
    r"|Rsrc_[A-Za-z0-9_]+ Size of resource:"
    r"|SEHandlerTable "
    r"|meta pointer for "
    r"|(const )?[\w:]+::`?vftable"
    r"|terminator for class)")


# ──────────────────────────────────────────────────────── ExpHP 基线（判定归属用）

def exphp_baseline(data_dir):
    """把 ExpHP 那层「本该长什么样」算出来，作为 diff 的基线。"""
    b = dict(funcs={}, statics={}, labels={}, structs={}, bitfield_types=set())
    if not data_dir:
        return b

    def j(name):
        p = os.path.join(data_dir, name)
        return json.load(open(p, encoding="utf-8")) if os.path.exists(p) else None

    for r in j("funcs.json") or []:
        if _keep(r.get("name"), False):
            b["funcs"][int(r["addr"], 16)] = _san(r["name"])
    for r in j("statics.json") or []:
        if _keep(r.get("name"), False):
            b["statics"][int(r["addr"], 16)] = _san(r["name"])
    # 位域类型（zAnmBitfieldsHi/Lo）也是 ExpHP 的东西，由导入器按 insertBitFieldAt 生成。
    # 它们的组件布局是 Ghidra 的位域打包行为，没法在这儿廉价复现，所以整体排除在结构体
    # diff 之外——代价是我们若手改这两个类型，导出会漏掉（就 2 个类型，认了）。
    b["bitfield_types"] = {_san(k) for k in (j("type-bitfields.json") or {})}
    for group, rows in (j("labels.json") or {}).items():
        for a_hex, suffix in rows:
            b["labels"].setdefault(int(a_hex, 16), set()).add(label_name(group, suffix))

    structs = {}
    structs.update(j("type-structs-ext.json") or {})
    structs.update(j("type-structs-own.json") or {})
    for name, rows in structs.items():
        # ⚠️ 必须模拟导入器的**顺序追加**（StructureDataType.add），不能按声明偏移建表。
        # ExpHP 有些结构体的行是乱序甚至偏移重复的（th18 zMainMenu 在 0x20 上出现两次：
        # menu_state 和 select），add() 不认声明偏移，于是实际落点从那里起整体错位。
        # 基线若按声明偏移算，这类结构体会整片报成「我们改的」——全是假阳性。
        place, running = {}, 0
        for _off, fname, _ty, size in struct_members(rows):
            place[running] = (fname, size)
            running += size
        b["structs"][_san(name)] = place
    return b


# ──────────────────────────────────────────────────────── DB → 我们那层的视图

def collect(prog, base):
    """从 DB 里挑出「我们的」东西。"""
    from ghidra.program.model.listing import CodeUnit
    from ghidra.program.model.symbol import SourceType, SymbolType

    DEF = SourceType.DEFAULT
    # 只认人工来源。Ghidra 自动分析产的 switchdataD_* / caseD_* / "default" / s_* 字符串标签
    # 都是 ANALYSIS，靠这条一次性挡掉——比维护前缀黑名单可靠得多。
    # 只认 USER_DEFINED。实测 th18：IMPORTED 那 602 个是 RTTI/PE 分析器产的
    # vftable / _tls_index / _guard_check_icall 之流，不是人写的。
    HUMAN = (SourceType.USER_DEFINED,)
    lst, fm, st, dtm = (prog.getListing(), prog.getFunctionManager(),
                        prog.getSymbolTable(), prog.getDataTypeManager())
    out = dict(funcs=[], statics=[], labels=[], comments=[], structs={})
    blocks = {b.getName() for b in prog.getMemory().getBlocks()}

    def default_name(n):
        return n.startswith(GHIDRA_DEFAULT_PREFIXES)

    # ── 函数：名字、调用约定、原型 ────────────────────────────────
    for f in fm.getFunctions(True):
        a = f.getEntryPoint().getOffset()
        name, rec = f.getName(), {}
        sym = f.getSymbol()
        # 同样只认 USER_DEFINED：FunctionID / demangler 分析器会给一大堆 CRT 函数
        # 起名（___scrt_acquire_startup_lock、`scalar_deleting_destructor'…），那不是我们的活
        named = sym is not None and sym.getSource() in HUMAN and not default_name(name)
        if named and base["funcs"].get(a) != name:
            rec["name"] = name                       # 我们起的名 / 我们改过的名
        # 原型只在人工设过时才导（否则会把上千条 Ghidra 自动猜测一起倒出来）
        if str(f.getSignatureSource()) == "USER_DEFINED":
            rec["cc"] = f.getCallingConventionName()
            rec["ret"] = f.getReturnType().getName()
            # ⚠️ 自定义存储必须连存储一起导。bind_types.py 把 this 钉在 ECX、其余排栈；
            # 只导「名字+类型」的话，apply 会按动态存储重建，__thiscall 下 Ghidra 会自己
            # 插一个 auto `this`，把我们的 self 挤到第一个栈参数位上——268 个签名一次性毁掉。
            # （实测过，这条注释就是那次的代价。）
            custom = bool(f.hasCustomVariableStorage())
            rec["params"] = [dict({"name": p.getName(), "type": p.getDataType().getName()},
                                  **({"storage": str(p.getVariableStorage())} if custom else {}))
                             for p in f.getParameters()]
            if custom:
                rec["custom_storage"] = True
                rec["ret_storage"] = str(f.getReturn().getVariableStorage())
        if rec:
            rec["addr"] = "0x%08x" % a
            out["funcs"].append(rec)

    # ── 符号：全局（有已定义数据）与裸标签 ────────────────────────
    for sym in st.getAllSymbols(False):
        if sym.getSource() not in HUMAN or sym.getSymbolType() == SymbolType.FUNCTION:
            continue
        a, name = sym.getAddress().getOffset(), sym.getName()
        if default_name(name):
            continue
        d = lst.getDataAt(sym.getAddress())
        if d is not None and d.isDefined():
            if base["statics"].get(a) == name:
                continue                             # 与 ExpHP 逐字相同 → 是他的，不导
            out["statics"].append({"addr": "0x%08x" % a, "name": name,
                                   "type": d.getDataType().getName()})
        else:
            if name in base["labels"].get(a, ()) or base["statics"].get(a) == name:
                continue        # ExpHP 的：labels.json 生成的，或 statics 里那几个没定义数据的
            out["labels"].append({"addr": "0x%08x" % a, "name": name})

    # ── 注释：丢掉 [th-re-data] 前缀的 ───────────────────────────
    kinds = dict(plate=CodeUnit.PLATE_COMMENT, pre=CodeUnit.PRE_COMMENT,
                 eol=CodeUnit.EOL_COMMENT, post=CodeUnit.POST_COMMENT,
                 repeatable=CodeUnit.REPEATABLE_COMMENT)
    it = lst.getCommentAddressIterator(prog.getMemory(), True)
    while it.hasNext():
        a = it.next()
        for kind, cid in kinds.items():
            txt = lst.getComment(cid, a)
            if not txt or txt.startswith(VENDOR_TAGS):
                continue
            if kind == "eol" and txt in blocks:
                continue                             # PE 装载器给段头写的 ".text"/".rdata"
            if ANALYZER_COMMENT.match(txt):
                continue
            out["comments"].append({"addr": "0x%08x" % a.getOffset(),
                                    "kind": kind, "text": txt})

    # ── 结构体：只记与 ExpHP 布局不同的槽 ────────────────────────
    for dt in dtm.getAllStructures():
        sname = dt.getName()
        if not sname.startswith("z") or sname in base["bitfield_types"]:
            continue
        want = base["structs"].get(sname)
        diff = {}
        for c in dt.getComponents():
            off, fn = c.getOffset(), c.getFieldName()
            if fn is None:
                continue
            exp = want.get(off) if want else None
            if exp and exp[0] == fn and exp[1] == c.getLength():
                continue                             # 与 ExpHP 一致 → 是他的
            diff["0x%x" % off] = {"name": fn, "type": c.getDataType().getName(),
                                  "size": c.getLength()}
        if diff:
            if want is None:
                diff["__size"] = dt.getLength()      # ExpHP 完全没有的结构体
            out["structs"][sname] = diff
    return out


# ──────────────────────────────────────────────────────── 仓库 → DB

def apply_layer(prog, data, dry=False):
    from ghidra.program.model.data import DataUtilities, PointerDataType
    from ghidra.program.model.data.DataUtilities import ClearDataMode
    from ghidra.program.model.listing import (CodeUnit, Function, ParameterImpl,
                                              ReturnParameterImpl, VariableStorage)
    from ghidra.program.model.symbol import SourceType
    from ghidra.util.data.DataTypeParser import AllowedDataTypes
    from ghidra.util.data import DataTypeParser

    US = SourceType.USER_DEFINED
    af = prog.getAddressFactory().getDefaultAddressSpace().getAddress
    lst, fm, st, dtm = (prog.getListing(), prog.getFunctionManager(),
                        prog.getSymbolTable(), prog.getDataTypeManager())
    parser = DataTypeParser(dtm, dtm, None, AllowedDataTypes.ALL)
    n = dict(funcs=0, protos=0, statics=0, labels=0, comments=0, fields=0, failed=0)

    def dt_of(spec):
        try:
            return parser.parse(spec)
        except Exception:                                            # noqa: BLE001
            return None

    STACK = re.compile(r"^Stack\[(-?0[xX][0-9a-fA-F]+|-?\d+)\]:(\d+)$")
    REG = re.compile(r"^([A-Za-z][A-Za-z0-9_]*):(\d+)$")

    def storage_of(spec):
        """把 Ghidra 的 `ECX:4` / `Stack[0x4]:1` 字符串还原成 VariableStorage。"""
        if not spec or spec in ("<VOID>", "<UNASSIGNED>"):
            return None
        m = STACK.match(spec)
        if m:
            try:
                return VariableStorage(prog, int(m.group(1), 0), int(m.group(2)))
            except Exception:                                        # noqa: BLE001
                return None
        m = REG.match(spec)
        if m:
            reg = prog.getRegister(m.group(1))
            if reg is not None:
                try:
                    return VariableStorage(prog, reg)
                except Exception:                                    # noqa: BLE001
                    return None
        return None

    for r in data.get("funcs", []):
        f = fm.getFunctionAt(af(int(r["addr"], 16)))
        if f is None:
            n["failed"] += 1
            continue
        if r.get("name") and f.getName() != r["name"]:
            if not dry:
                f.setName(r["name"], US)
            n["funcs"] += 1
        if "params" in r:
            ret = dt_of(r.get("ret") or "void")
            custom = bool(r.get("custom_storage"))
            ps, bad = [], False
            for p in r["params"]:
                dt = dt_of(p["type"])
                if dt is None:
                    bad = True
                    break
                if custom:
                    vstor = storage_of(p.get("storage"))   # 别叫 st —— 外层 st 是 SymbolTable
                    if vstor is None:
                        bad = True
                        break
                    ps.append(ParameterImpl(p["name"], dt, vstor, prog))
                else:
                    ps.append(ParameterImpl(p["name"], dt, prog))
            if bad or ret is None:
                n["failed"] += 1
            else:
                if not dry:
                    if custom:
                        rst = storage_of(r.get("ret_storage"))
                        rv = (ReturnParameterImpl(ret, rst, prog) if rst is not None
                              else ReturnParameterImpl(ret, prog))
                        # varargs 重载：传 python list 的那个 List<Variable> 重载匹配不上
                        f.updateFunction(r.get("cc"), rv,
                                         Function.FunctionUpdateType.CUSTOM_STORAGE,
                                         True, US, *ps)
                    else:
                        f.updateFunction(r.get("cc"), None,
                                         Function.FunctionUpdateType.DYNAMIC_STORAGE_FORMAL_PARAMS,
                                         True, US, ps)
                    f.setReturnType(ret, US)
                n["protos"] += 1

    for r in data.get("statics", []):
        a = af(int(r["addr"], 16))
        p = st.getPrimarySymbol(a)
        if not dry:
            if p is not None and not p.isDynamic():
                p.setName(r["name"], US)
            else:
                st.createLabel(a, r["name"], US)
        dt = dt_of(r["type"]) if r.get("type") else None
        if dt is not None and not dry:
            try:
                DataUtilities.createData(prog, a, dt, -1, ClearDataMode.CLEAR_ALL_CONFLICT_DATA)
            except Exception:                                        # noqa: BLE001
                n["failed"] += 1
        n["statics"] += 1

    for r in data.get("labels", []):
        a = af(int(r["addr"], 16))
        if not any(s.getName() == r["name"] for s in st.getSymbols(a)):
            if not dry:
                st.createLabel(a, r["name"], US)
            n["labels"] += 1

    kinds = dict(plate=CodeUnit.PLATE_COMMENT, pre=CodeUnit.PRE_COMMENT,
                 eol=CodeUnit.EOL_COMMENT, post=CodeUnit.POST_COMMENT,
                 repeatable=CodeUnit.REPEATABLE_COMMENT)
    for r in data.get("comments", []):
        cid = kinds.get(r.get("kind"))
        if cid is None:
            n["failed"] += 1
            continue
        if not dry:
            lst.setComment(af(int(r["addr"], 16)), cid, r["text"])
        n["comments"] += 1

    from ghidra.program.model.data import CategoryPath, StructureDataType
    for sname, slots in data.get("structs", {}).items():
        dt = dtm.getDataType(CategoryPath("/"), sname)
        size = slots.get("__size")
        if dt is None and size:
            dt = StructureDataType(CategoryPath("/"), sname, int(size), dtm)
            if not dry:
                dt = dtm.addDataType(dt, None)
        if dt is None:
            n["failed"] += 1
            continue
        for off_hex, slot in slots.items():
            if off_hex == "__size":
                continue
            fdt = dt_of(slot["type"])
            if fdt is None:
                n["failed"] += 1
                continue
            if not dry:
                try:
                    dt.replaceAtOffset(int(off_hex, 16), fdt, int(slot["size"]), slot["name"], None)
                except Exception:                                    # noqa: BLE001
                    n["failed"] += 1
                    continue
            n["fields"] += 1
    return n


def merge_th06_re_xml(base, P):
    """TH06 还有第三层上游：th06-re 的 Ghidra XML（1520 个函数名）。

    它由 `import_th06_re_xml.py` 重放，和 ExpHP 那层一样**只进不出**——
    上游没有 LICENSE，不擅自转发。不并进基线的话，导出会把 ~900 个社区名字
    当成「我们的活」倒进 games/<版本>/symbols.json。
    """
    if P["vdir"].name != "th06.v1.02h":
        return base
    try:
        from import_th06_re_xml import XML_PATH, flatten, parse
    except ImportError:
        return base
    if not XML_PATH.is_file():
        return base
    funcs, data_symbols, _comments = parse(
        XML_PATH.read_text(encoding="utf-8", errors="replace"))
    for addr, name, _sig in funcs:
        base["funcs"].setdefault(addr, flatten(name))
    for addr, name in data_symbols:
        base["labels"].setdefault(addr, set()).add(flatten(name))
        base["statics"].setdefault(addr, flatten(name))
    return base


# ──────────────────────────────────────────────────────── diff / 落盘

def _key(kind, r):
    if kind == "comments":
        return (r["addr"], r["kind"])
    return r["addr"]


def diff(db, repo):
    """返回 {类别: (只在 DB 有, 只在仓库有)}。"""
    out = {}
    for kind in ("funcs", "statics", "labels", "comments"):
        d = {_key(kind, r): r for r in db.get(kind, [])}
        f = {_key(kind, r): r for r in repo.get(kind, [])}
        out[kind] = ([d[k] for k in d if d[k] != f.get(k)],
                     [f[k] for k in f if k not in d])
    ds, fs = db.get("structs", {}), repo.get("structs", {})
    only_db, only_file = [], []
    for s in set(ds) | set(fs):
        for off in set(ds.get(s, {})) | set(fs.get(s, {})):
            a, b = ds.get(s, {}).get(off), fs.get(s, {}).get(off)
            if a != b:
                (only_db if a is not None else only_file).append({"addr": f"{s}@{off}"})
    out["structs"] = (only_db, only_file)
    return out


def load_repo(path):
    if not os.path.exists(path):
        return {}
    return json.load(open(path, encoding="utf-8"))


def sort_all(d):
    for k in ("funcs", "statics", "labels", "comments"):
        d[k] = sorted(d.get(k, []), key=lambda r: (r["addr"], r.get("kind", "")))
    d["structs"] = {k: dict(sorted(v.items())) for k, v in sorted(d.get("structs", {}).items())}
    return d


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__)
    ap.add_argument("action", choices=("status", "export", "apply"))
    ap.add_argument("version", help="版本号，如 th16 / th18")
    ap.add_argument("--dry-run", action="store_true", help="只报数，不写文件/不写库")
    a = ap.parse_args()

    P = resolve(a.version)
    base = exphp_baseline(P["data_dir"])
    merge_th06_re_xml(base, P)
    repo = load_repo(P["symbols"])
    rel = P["symbols"].relative_to(REPO)

    if a.action == "apply":
        if not repo:
            print(f"[symbols] {rel} 不存在，没什么可回放的")
            return
        with open_program(P["proj_dir"].resolve(), P["proj_name"], P["program"],
                          tx="apply our symbol layer", commit=not a.dry_run,
                          expect_md5=repo.get("exe_md5")) as prog:
            n = apply_layer(prog, repo, a.dry_run)
        print(("[dry-run] " if a.dry_run else "") + "[symbols apply] " +
              " ".join(f"{k}={v}" for k, v in n.items()))
        return

    with open_program(P["proj_dir"].resolve(), P["proj_name"], P["program"]) as prog:
        db = sort_all(collect(prog, base))
        db["version"] = P["vdir"].name
        db["exe_md5"] = str(prog.getExecutableMD5() or "").lower()

    if a.action == "status":
        d = diff(db, repo)
        tot_db = sum(len(x[0]) for x in d.values())
        tot_file = sum(len(x[1]) for x in d.values())
        if not tot_db and not tot_file:
            print(f"[symbols status] {rel}：0 漂移，DB 与仓库一致")
            return
        print(f"[symbols status] {rel}")
        for kind, (only_db, only_file) in d.items():
            if only_db:
                print(f"  DB 独有（未导出）{kind} {len(only_db)} 项：")
                for r in only_db[:8]:
                    print("    " + json.dumps(r, ensure_ascii=False)[:110])
                if len(only_db) > 8:
                    print(f"    …… 另有 {len(only_db) - 8} 项")
            if only_file:
                print(f"  仓库独有（未回放）{kind} {len(only_file)} 项")
        if tot_db:
            print(f"\n  → 共 {tot_db} 项只在 DB 里。跑 `symbols.py export {a.version}` 存回仓库。")
        return

    # export
    old = sum(len(repo.get(k, [])) for k in ("funcs", "statics", "labels", "comments"))
    new = sum(len(db.get(k, [])) for k in ("funcs", "statics", "labels", "comments"))
    if a.dry_run:
        print(f"[dry-run] [symbols export] 将写 {new} 条（原 {old} 条） -> {rel}")
        return
    P["symbols"].parent.mkdir(parents=True, exist_ok=True)
    ordered = {"version": db["version"], "exe_md5": db["exe_md5"],
               "funcs": db["funcs"], "statics": db["statics"], "labels": db["labels"],
               "comments": db["comments"], "structs": db["structs"]}
    with open(P["symbols"], "w", encoding="utf-8") as fh:
        json.dump(ordered, fh, ensure_ascii=False, indent=1, sort_keys=False)
        fh.write("\n")
    print(f"[symbols export] {new} 条（原 {old} 条） -> {rel}")


if __name__ == "__main__":
    main()
