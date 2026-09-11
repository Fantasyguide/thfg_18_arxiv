# stg-agent-proto（协议仓：规范 + Python 客户端 + C 桥核心）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **状态：搁置（2026-09-11）。** 用户改为原型先行——算法内嵌的单 DLL（`mods/th06nc/autoplay/`），
> 协议、Python 客户端、C 网络核心推后。本计划保留，接协议时从 Task 1 开始。

**Goal:** 建起独立仓 `stg-agent-proto`：线格式 v1 规范、Python 参考客户端（AI 侧监听、解 OBS 成 numpy、发 ACT）、以及各游戏桥共用的 C 核心（编码 / 网络 / 锁步），并用 Python ⇄ C 的回环测试证明两边说的是同一种话。

**Architecture:** 仓库分三块：`SPEC.md` 是唯一权威；`src/stgagent/` 是 Python 客户端（AI 侧永远是监听方）；`c/` 是被 th06nc / th18 桥 `#include` 的 C 核心，用 `SA_POSIX` 宏在 Linux 上以 BSD socket 编译并接受 pytest 的回环测试，在 Windows 上以 winsock 编译。没有任何游戏知识进这个仓。

**Tech Stack:** Python 3.10+、numpy、pytest；C99（gcc 宿主测试 / clang `x86_64-pc-windows-msvc` 与 `i686-pc-windows-msvc` 交叉编译）。

**Spec:** `/data/sunyunbo/www/renkolab/docs/superpowers/specs/2026-09-10-ai-agent-bridge-design.md`（§4 协议 v1、§5.7 桥核心、§8 测试）。

## Global Constraints

- 仓库路径 `/data/sunyunbo/www/stg-agent-proto`；包名 `stgagent`；协议版本常量 `PROTO_VERSION = 1`。
- 消息帧：`u32 length`（小端，不含自身）+ `u8 type` + payload。type：`1 HELLO / 2 OBS / 3 ACT / 4 CTRL / 5 BYE`。
- 数值：`fx` = i32 Q16.16 小端；`angle` = u16 BAM 小端（一圈 65536）。字段类型集合只有 `fx angle i32 u32 u16 u8`。
- 动作位（spec §4.4.1）：`UP=0 DOWN=1 LEFT=2 RIGHT=3 SHOT=4 BOMB=5 SLOW=6 TIMESTOP=7 CARD_USE=8 CARD_SWITCH=9`。
- 场界：`half_w=192 height=448`，原点 `center-top`。
- `spell` 表**可选**：客户端不得假设它存在。
- ACT 固定 12 字节：`u32 frame, u32 buttons, u32 flags`；`flags bit0 = PASSTHROUGH`。
- 同步：后端发 `OBS(n)` 后阻塞收 `ACT(n)`；`frame < n` 丢弃继续等；`frame > n` 断开；超时沿用上一帧 buttons。
- C 核心不依赖任何游戏、任何加载器；`SA_POSIX` 未定义时用 winsock。静态缓冲，断点 / 钩子内不分配堆。
- 本仓库不含任何 ZUN 数据。
- 提交信息末尾带 `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`。

---

## 文件结构

```
stg-agent-proto/
├── README.md                 一段话 + 两个后端怎么连 + 怎么跑测试
├── SPEC.md                   线格式 v1 规范（Task 1）
├── pyproject.toml            包 stgagent，deps numpy，dev pytest
├── .gitignore
├── src/stgagent/
│   ├── __init__.py           导出 PROTO_VERSION、BTN_*、Listener、Session、Observation
│   ├── consts.py             PROTO_VERSION、MsgType、BTN_*、ACT_PASSTHROUGH、PHASE_*
│   ├── fixed.py              fx/angle 与 float 互转（numpy 向量化）
│   ├── framing.py            read_msg / write_msg（阻塞 socket 上的长度前缀帧）
│   ├── schema.py             Hello 数据类 + 由 tables 生成 numpy dtype
│   ├── obs.py                decode_obs(bytes, hello) -> Observation
│   ├── act.py                encode_act(frame, buttons, flags) -> bytes
│   ├── session.py            Listener（accept 一个后端）/ Session（hello / obs 迭代 / send_act / ctrl）
│   └── fake_backend.py       纯 Python 假后端：发 HELLO / OBS，收 ACT —— 测试与演示用
├── tests/
│   ├── test_fixed.py test_framing.py test_schema.py test_obs.py test_act.py
│   ├── test_session.py       Python 假后端 ⇄ Session 回环
│   └── test_c_core.py        编译 c/tests/fake_game.c，与 Session 回环
└── c/
    ├── stgagent.h            公开 API（下面 Task 6 给全文）
    ├── sa_proto.c            HELLO JSON 生成、OBS 编码、ACT 解码、fx/bam 换算
    ├── sa_net.c              连接 / 长度前缀收发 / 超时；SA_POSIX 与 winsock 两套
    ├── sa_lockstep.c         发 OBS → 等 ACT → 超时沿用
    ├── tests/fake_game.c     假游戏：连上 Python 监听端，发 N 帧 OBS，回显 ACT 到 stdout
    └── Makefile              `make test-host`（gcc）/ `make cross-x64` / `make cross-x86`（只验编译）
```

---

### Task 1: 仓库骨架 + SPEC.md

**Files:**
- Create: `/data/sunyunbo/www/stg-agent-proto/{README.md,SPEC.md,pyproject.toml,.gitignore}`
- Create: `src/stgagent/__init__.py`、`src/stgagent/consts.py`
- Test: `tests/test_consts.py`

**Interfaces:**
- Produces: `stgagent.consts` 里的 `PROTO_VERSION: int = 1`、`class MsgType(IntEnum): HELLO=1 OBS=2 ACT=3 CTRL=4 BYE=5`、`BTN_UP..BTN_CARD_SWITCH: int`（位掩码，`1 << bit`）、`ACTION_NAMES: dict[int, str]`（bit → 名）、`ACT_PASSTHROUGH = 1`、`PHASE_IN_GAME=1<<0 … PHASE_REPLAY_PLAYBACK=1<<7`。

- [ ] **Step 1: 建仓与包骨架**

```bash
mkdir -p /data/sunyunbo/www/stg-agent-proto/{src/stgagent,tests,c/tests}
cd /data/sunyunbo/www/stg-agent-proto && git init -q
```

`pyproject.toml`：

```toml
[build-system]
requires = ["setuptools>=68"]
build-backend = "setuptools.build_meta"

[project]
name = "stgagent"
version = "0.1.0"
description = "stg-agent-proto v1 reference client: observation/action protocol for STG AI agents"
requires-python = ">=3.10"
dependencies = ["numpy>=1.24"]

[project.optional-dependencies]
dev = ["pytest>=7"]

[tool.setuptools.packages.find]
where = ["src"]

[tool.pytest.ini_options]
testpaths = ["tests"]
```

`.gitignore`：

```
__pycache__/
*.pyc
*.egg-info/
build/
dist/
c/build/
.pytest_cache/
```

- [ ] **Step 2: 写 consts.py 的失败测试**

`tests/test_consts.py`：

```python
from stgagent import consts


def test_proto_version_is_1():
    assert consts.PROTO_VERSION == 1


def test_msg_types_match_spec():
    assert (consts.MsgType.HELLO, consts.MsgType.OBS, consts.MsgType.ACT,
            consts.MsgType.CTRL, consts.MsgType.BYE) == (1, 2, 3, 4, 5)


def test_action_bits_match_spec():
    assert consts.BTN_UP == 1 << 0
    assert consts.BTN_SLOW == 1 << 6
    assert consts.BTN_TIMESTOP == 1 << 7
    assert consts.BTN_CARD_USE == 1 << 8
    assert consts.BTN_CARD_SWITCH == 1 << 9
    assert consts.ACTION_NAMES[0] == "UP" and consts.ACTION_NAMES[9] == "CARD_SWITCH"


def test_phase_bits_match_spec():
    assert consts.PHASE_IN_GAME == 1 and consts.PHASE_REPLAY_PLAYBACK == 1 << 7
    assert consts.ACT_PASSTHROUGH == 1
```

- [ ] **Step 3: 跑测试确认失败**

Run: `cd /data/sunyunbo/www/stg-agent-proto && pip install -e '.[dev]' -q && pytest tests/test_consts.py -v`
Expected: FAIL，`ModuleNotFoundError: No module named 'stgagent.consts'`

- [ ] **Step 4: 写 consts.py 与 __init__.py**

`src/stgagent/consts.py`：

```python
"""stg-agent-proto v1 常量。唯一权威是仓库根的 SPEC.md；本文件只是它的机器可读形式。"""
from enum import IntEnum

PROTO_VERSION = 1


class MsgType(IntEnum):
    HELLO = 1
    OBS = 2
    ACT = 3
    CTRL = 4
    BYE = 5


# 动作位（bit 号）。0–6 与 stg-engine 的 BTN_* 冻结一致；7 以上是延伸。
_ACTION_BITS = {
    "UP": 0, "DOWN": 1, "LEFT": 2, "RIGHT": 3,
    "SHOT": 4, "BOMB": 5, "SLOW": 6, "TIMESTOP": 7,
    "CARD_USE": 8, "CARD_SWITCH": 9,
}
ACTION_NAMES = {bit: name for name, bit in _ACTION_BITS.items()}

BTN_UP = 1 << 0
BTN_DOWN = 1 << 1
BTN_LEFT = 1 << 2
BTN_RIGHT = 1 << 3
BTN_SHOT = 1 << 4
BTN_BOMB = 1 << 5
BTN_SLOW = 1 << 6
BTN_TIMESTOP = 1 << 7
BTN_CARD_USE = 1 << 8
BTN_CARD_SWITCH = 1 << 9

# ACT.flags
ACT_PASSTHROUGH = 1

# OBS.phase 位
PHASE_IN_GAME = 1 << 0
PHASE_PAUSED = 1 << 1
PHASE_DIALOGUE = 1 << 2
PHASE_SHOP = 1 << 3
PHASE_BOMB_ACTIVE = 1 << 4
PHASE_SPELL_ACTIVE = 1 << 5
PHASE_PLAYER_CONTROLLABLE = 1 << 6
PHASE_REPLAY_PLAYBACK = 1 << 7

# 字段类型 → (struct 格式, numpy dtype 字符串, 字节数)
FIELD_TYPES = {
    "fx": ("<i", "<i4", 4),
    "angle": ("<H", "<u2", 2),
    "i32": ("<i", "<i4", 4),
    "u32": ("<I", "<u4", 4),
    "u16": ("<H", "<u2", 2),
    "u8": ("<B", "u1", 1),
}

ACT_SIZE = 12
```

`src/stgagent/__init__.py`：

```python
from .consts import (PROTO_VERSION, MsgType, ACTION_NAMES, ACT_PASSTHROUGH,
                     BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_SHOT, BTN_BOMB, BTN_SLOW,
                     BTN_TIMESTOP, BTN_CARD_USE, BTN_CARD_SWITCH)

__all__ = ["PROTO_VERSION", "MsgType", "ACTION_NAMES", "ACT_PASSTHROUGH",
           "BTN_UP", "BTN_DOWN", "BTN_LEFT", "BTN_RIGHT", "BTN_SHOT", "BTN_BOMB",
           "BTN_SLOW", "BTN_TIMESTOP", "BTN_CARD_USE", "BTN_CARD_SWITCH"]
```

- [ ] **Step 5: 跑测试确认通过**

Run: `pytest tests/test_consts.py -v`
Expected: 4 passed

- [ ] **Step 6: 写 SPEC.md**

把 renkolab spec 的 §4 逐节搬成规范化文本（不是复制粘贴设计讨论，只留规则）。必须包含这些小节，每节内容与 renkolab spec §4 对应小节一致：

```markdown
# stg-agent-proto v1

## 0. 术语与约定
后端 = 被观察的游戏（TH18 / th06nc / stg-engine）；客户端 = AI 侧。客户端监听，后端主动连。小端。

## 1. 消息帧
u32 length（不含自身）+ u8 type + payload。type 表（1–5）。

## 2. HELLO（后端 → 客户端，JSON）
字段：proto、backend、tick_hz、field{half_w,height,origin}、number{fx,angle}、
actions[{bit,name}]、caps[]、tables[{id,name,cap,stride,fields[{name,type,off}]}]。
字段类型集合：fx angle i32 u32 u16 u8。
规则：actions 只列支持的位；tables 是 OBS 解码字典；客户端按名字取字段。

## 3. OBS（后端 → 客户端，二进制）
u32 frame、u32 phase、u8 table_count、[u8 table_id、u16 count、count×stride]。
phase 位表（0–7）。
Tier 0 表：player / bullets / enemies 必发；spell 可选。各表字段名与类型（照 spec §4.3.2 的表）。
坐标系：x∈[-half_w, half_w]，y∈[0,height]，y 向下。

## 4. ACT（客户端 → 后端，12 字节）
u32 frame、u32 buttons、u32 flags（bit0 PASSTHROUGH）。动作位表（0–9）。

## 5. CTRL（双向，JSON）
timeout_ms / passthrough / tables / reset / unthrottled / event。

## 6. 同步语义
锁步 + 超时五条规则（spec §4.6）。

## 7. 版本
proto 字段；加字段不 bump；改语义 bump。
```

- [ ] **Step 7: README.md 与首次提交**

README 三段：这是什么（两句）、怎么用（`pip install -e .`，`python -m stgagent.session --listen 0.0.0.0:7788` 等 Task 5 实现后补）、怎么测（`pytest` + `make -C c test-host`）。

```bash
git add -A && git commit -q -m "chore: 仓库骨架 + SPEC v1 + consts

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 2: fixed.py —— Q16.16 / BAM 换算

**Files:**
- Create: `src/stgagent/fixed.py`
- Test: `tests/test_fixed.py`

**Interfaces:**
- Produces: `fx_to_float(x) -> float | np.ndarray`（`x / 65536`）、`float_to_fx(f) -> int | np.ndarray`（round half to even，钳到 i32）、`bam_to_rad(b) -> float | np.ndarray`（`b / 65536 * 2π`，结果落在 `[0, 2π)`）、`rad_to_bam(r) -> int | np.ndarray`（`round(r / 2π * 65536) & 0xFFFF`）。都接受标量或 numpy 数组。

- [ ] **Step 1: 失败测试**

```python
import math
import numpy as np
from stgagent import fixed


def test_fx_roundtrip_scalar():
    assert fixed.fx_to_float(65536) == 1.0
    assert fixed.float_to_fx(-184.0) == -184 * 65536
    assert fixed.fx_to_float(fixed.float_to_fx(0.0078125)) == 0.0078125  # 1/128 精确


def test_fx_vectorized():
    a = np.array([0, 32768, -65536], dtype="<i4")
    np.testing.assert_allclose(fixed.fx_to_float(a), [0.0, 0.5, -1.0])


def test_float_to_fx_clamps_to_i32():
    assert fixed.float_to_fx(1e9) == 2**31 - 1
    assert fixed.float_to_fx(-1e9) == -2**31


def test_bam_roundtrip():
    assert fixed.rad_to_bam(math.pi) == 32768
    assert fixed.rad_to_bam(-math.pi / 2) == 49152      # 负角回绕
    assert abs(fixed.bam_to_rad(16384) - math.pi / 2) < 1e-9
    b = np.array([0, 16384, 32768], dtype="<u2")
    np.testing.assert_allclose(fixed.bam_to_rad(b), [0, math.pi / 2, math.pi])
```

- [ ] **Step 2: 跑测试确认失败**

Run: `pytest tests/test_fixed.py -v` → FAIL（`ImportError`）

- [ ] **Step 3: 实现**

```python
"""Q16.16 定点与 BAM 角的换算。线上是整数；float 只在客户端这一侧出现。"""
import math
import numpy as np

_ONE = 65536.0
_I32_MIN, _I32_MAX = -(2**31), 2**31 - 1


def fx_to_float(x):
    return np.asarray(x, dtype=np.int64) / _ONE if isinstance(x, np.ndarray) else x / _ONE


def float_to_fx(f):
    if isinstance(f, np.ndarray):
        return np.clip(np.rint(f * _ONE), _I32_MIN, _I32_MAX).astype("<i4")
    v = int(round(f * _ONE))
    return max(_I32_MIN, min(_I32_MAX, v))


def bam_to_rad(b):
    if isinstance(b, np.ndarray):
        return np.asarray(b, dtype=np.float64) * (2 * math.pi / _ONE)
    return b * (2 * math.pi / _ONE)


def rad_to_bam(r):
    if isinstance(r, np.ndarray):
        return (np.rint(r * (_ONE / (2 * math.pi))).astype(np.int64) & 0xFFFF).astype("<u2")
    return int(round(r * (_ONE / (2 * math.pi)))) & 0xFFFF
```

- [ ] **Step 4: 跑测试确认通过**

Run: `pytest tests/test_fixed.py -v` → 4 passed

- [ ] **Step 5: 提交**

```bash
git add src/stgagent/fixed.py tests/test_fixed.py
git commit -q -m "feat: fixed —— Q16.16 / BAM 换算

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 3: framing.py + act.py

**Files:**
- Create: `src/stgagent/framing.py`、`src/stgagent/act.py`
- Test: `tests/test_framing.py`、`tests/test_act.py`

**Interfaces:**
- Produces: `framing.write_msg(sock, mtype: int, payload: bytes) -> None`；`framing.read_msg(sock) -> tuple[int, bytes]`（阻塞读到完整一帧；对端关闭抛 `ConnectionError`）；`framing.pack_msg(mtype, payload) -> bytes`、`framing.unpack_from(buf: bytes) -> tuple[int, bytes, int]`（返回 type、payload、消耗字节数；不完整返回 `(0, b"", 0)`）。
- Produces: `act.encode_act(frame: int, buttons: int, flags: int = 0) -> bytes`（12 字节）、`act.decode_act(b: bytes) -> tuple[int, int, int]`。

- [ ] **Step 1: 失败测试**

`tests/test_framing.py`：

```python
import socket
import threading
from stgagent import framing


def test_pack_unpack_roundtrip():
    b = framing.pack_msg(2, b"\x01\x02\x03")
    assert b == b"\x04\x00\x00\x00" + b"\x02" + b"\x01\x02\x03"
    t, p, n = framing.unpack_from(b + b"tail")
    assert (t, p, n) == (2, b"\x01\x02\x03", 8)


def test_unpack_incomplete_returns_zero():
    assert framing.unpack_from(b"\x04\x00\x00\x00\x02\x01") == (0, b"", 0)


def test_socket_roundtrip():
    a, b = socket.socketpair()
    t = threading.Thread(target=framing.write_msg, args=(a, 3, b"x" * 1000))
    t.start()
    assert framing.read_msg(b) == (3, b"x" * 1000)
    t.join()
    a.close()
    try:
        framing.read_msg(b)
        assert False, "对端关闭应抛 ConnectionError"
    except ConnectionError:
        pass
```

`tests/test_act.py`：

```python
from stgagent import act
from stgagent.consts import BTN_LEFT, BTN_SHOT, ACT_PASSTHROUGH


def test_encode_act_is_12_bytes_le():
    b = act.encode_act(7, BTN_LEFT | BTN_SHOT, ACT_PASSTHROUGH)
    assert len(b) == 12
    assert b == (7).to_bytes(4, "little") + (0x14).to_bytes(4, "little") + (1).to_bytes(4, "little")
    assert act.decode_act(b) == (7, 0x14, 1)
```

- [ ] **Step 2: 跑测试确认失败**

Run: `pytest tests/test_framing.py tests/test_act.py -v` → ImportError

- [ ] **Step 3: 实现**

`src/stgagent/framing.py`：

```python
"""长度前缀帧：u32 length（不含自身）+ u8 type + payload。"""
import struct

_HDR = struct.Struct("<IB")


def pack_msg(mtype: int, payload: bytes) -> bytes:
    return _HDR.pack(len(payload) + 1, mtype) + payload


def unpack_from(buf: bytes):
    if len(buf) < 4:
        return 0, b"", 0
    length = struct.unpack_from("<I", buf, 0)[0]
    if len(buf) < 4 + length or length < 1:
        return 0, b"", 0
    return buf[4], bytes(buf[5:4 + length]), 4 + length


def _recv_exact(sock, n: int) -> bytes:
    out = bytearray()
    while len(out) < n:
        chunk = sock.recv(n - len(out))
        if not chunk:
            raise ConnectionError("peer closed")
        out += chunk
    return bytes(out)


def read_msg(sock):
    length = struct.unpack("<I", _recv_exact(sock, 4))[0]
    if length < 1:
        raise ConnectionError("bad frame length 0")
    body = _recv_exact(sock, length)
    return body[0], body[1:]


def write_msg(sock, mtype: int, payload: bytes) -> None:
    sock.sendall(pack_msg(mtype, payload))
```

`src/stgagent/act.py`：

```python
import struct
from .consts import ACT_SIZE

_ACT = struct.Struct("<III")


def encode_act(frame: int, buttons: int, flags: int = 0) -> bytes:
    return _ACT.pack(frame & 0xFFFFFFFF, buttons & 0xFFFFFFFF, flags & 0xFFFFFFFF)


def decode_act(b: bytes):
    if len(b) != ACT_SIZE:
        raise ValueError(f"ACT must be {ACT_SIZE} bytes, got {len(b)}")
    return _ACT.unpack(b)
```

- [ ] **Step 4: 跑测试确认通过** → 4 passed

- [ ] **Step 5: 提交**

```bash
git add src/stgagent/framing.py src/stgagent/act.py tests/test_framing.py tests/test_act.py
git commit -q -m "feat: framing + act 编解码

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 4: schema.py + obs.py —— HELLO 驱动的 OBS 解码

**Files:**
- Create: `src/stgagent/schema.py`、`src/stgagent/obs.py`
- Test: `tests/test_schema.py`、`tests/test_obs.py`

**Interfaces:**
- Produces: `schema.Field(name, type, off)`、`schema.Table(id, name, cap, stride, fields: list[Field], dtype: np.dtype)`、`schema.Hello(proto, backend, tick_hz, half_w, height, origin, actions: dict[int,str], caps: set[str], tables: dict[int, Table])`；`schema.parse_hello(payload: bytes) -> Hello`（校验 proto==1、字段类型合法、`off+size <= stride`、字段不重叠；违规抛 `SchemaError`）；`Hello.table_by_name(name) -> Table | None`。
- Produces: `obs.Observation(frame: int, phase: int, tables: dict[str, np.ndarray])`，`obs.decode_obs(payload: bytes, hello: Hello) -> Observation`（每表一个 structured ndarray，长度 = count；未知 table_id 抛 `ValueError`；count > cap 抛 `ValueError`）。

- [ ] **Step 1: 失败测试**

`tests/test_schema.py`：

```python
import json
import numpy as np
import pytest
from stgagent import schema

HELLO = {
    "proto": 1, "backend": "fake", "tick_hz": 60,
    "field": {"half_w": 192, "height": 448, "origin": "center-top"},
    "number": {"fx": "q16.16-i32-le", "angle": "bam-u16-le"},
    "actions": [{"bit": 0, "name": "UP"}, {"bit": 4, "name": "SHOT"}],
    "caps": ["lockstep"],
    "tables": [
        {"id": 1, "name": "player", "cap": 1, "stride": 12,
         "fields": [{"name": "x", "type": "fx", "off": 0}, {"name": "y", "type": "fx", "off": 4},
                    {"name": "state", "type": "u8", "off": 8}, {"name": "lives", "type": "u8", "off": 9},
                    {"name": "power", "type": "u16", "off": 10}]},
        {"id": 2, "name": "bullets", "cap": 4, "stride": 10,
         "fields": [{"name": "x", "type": "fx", "off": 0}, {"name": "y", "type": "fx", "off": 4},
                    {"name": "angle", "type": "angle", "off": 8}]},
    ],
}


def test_parse_hello_builds_dtypes():
    h = schema.parse_hello(json.dumps(HELLO).encode())
    assert h.backend == "fake" and h.half_w == 192 and h.caps == {"lockstep"}
    assert h.actions == {0: "UP", 4: "SHOT"}
    t = h.table_by_name("bullets")
    assert t.id == 2 and t.dtype.itemsize == 10
    assert t.dtype.names == ("x", "y", "angle")
    assert t.dtype["angle"] == np.dtype("<u2")


def test_rejects_wrong_proto():
    bad = dict(HELLO, proto=2)
    with pytest.raises(schema.SchemaError):
        schema.parse_hello(json.dumps(bad).encode())


def test_rejects_field_past_stride():
    bad = json.loads(json.dumps(HELLO))
    bad["tables"][1]["fields"][2]["off"] = 9   # angle 2 字节从 9 起 → 11 > stride 10
    with pytest.raises(schema.SchemaError):
        schema.parse_hello(json.dumps(bad).encode())


def test_rejects_overlapping_fields():
    bad = json.loads(json.dumps(HELLO))
    bad["tables"][0]["fields"][1]["off"] = 2
    with pytest.raises(schema.SchemaError):
        schema.parse_hello(json.dumps(bad).encode())
```

`tests/test_obs.py`：

```python
import json
import struct
import pytest
from stgagent import schema, obs
from tests.test_schema import HELLO


def _obs_bytes():
    body = struct.pack("<IIB", 42, 0b100001, 2)
    body += struct.pack("<BH", 1, 1) + struct.pack("<iiBBH", -184 << 16, 400 << 16, 0, 3, 128)
    body += struct.pack("<BH", 2, 2)
    body += struct.pack("<iiH", 10 << 16, 20 << 16, 16384)
    body += struct.pack("<iiH", -10 << 16, 30 << 16, 49152)
    return body


def test_decode_obs_tables():
    h = schema.parse_hello(json.dumps(HELLO).encode())
    o = obs.decode_obs(_obs_bytes(), h)
    assert o.frame == 42 and o.phase == 0b100001
    p = o.tables["player"]
    assert len(p) == 1 and p["lives"][0] == 3 and p["x"][0] == -184 << 16
    b = o.tables["bullets"]
    assert len(b) == 2 and list(b["angle"]) == [16384, 49152] and b["y"][1] == 30 << 16


def test_missing_optional_table_is_absent():
    h = schema.parse_hello(json.dumps(HELLO).encode())
    o = obs.decode_obs(_obs_bytes(), h)
    assert "spell" not in o.tables


def test_count_over_cap_rejected():
    h = schema.parse_hello(json.dumps(HELLO).encode())
    body = struct.pack("<IIB", 1, 0, 1) + struct.pack("<BH", 2, 5) + b"\0" * 50
    with pytest.raises(ValueError):
        obs.decode_obs(body, h)
```

- [ ] **Step 2: 跑测试确认失败** → ImportError

- [ ] **Step 3: 实现**

`src/stgagent/schema.py`：

```python
"""HELLO 的解析与校验；把 tables 变成 numpy structured dtype。"""
import json
from dataclasses import dataclass, field
import numpy as np
from .consts import PROTO_VERSION, FIELD_TYPES


class SchemaError(ValueError):
    pass


@dataclass(frozen=True)
class Field:
    name: str
    type: str
    off: int


@dataclass(frozen=True)
class Table:
    id: int
    name: str
    cap: int
    stride: int
    fields: tuple
    dtype: np.dtype


@dataclass
class Hello:
    proto: int
    backend: str
    tick_hz: int
    half_w: int
    height: int
    origin: str
    actions: dict
    caps: set
    tables: dict = field(default_factory=dict)      # id -> Table

    def table_by_name(self, name):
        for t in self.tables.values():
            if t.name == name:
                return t
        return None


def _build_table(d) -> Table:
    stride = int(d["stride"])
    fields, spans, names, formats, offsets = [], [], [], [], []
    for f in d["fields"]:
        if f["type"] not in FIELD_TYPES:
            raise SchemaError(f"table {d['name']}: unknown field type {f['type']!r}")
        size = FIELD_TYPES[f["type"]][2]
        off = int(f["off"])
        if off + size > stride:
            raise SchemaError(f"table {d['name']}: field {f['name']} exceeds stride")
        for a, b in spans:
            if off < b and a < off + size:
                raise SchemaError(f"table {d['name']}: field {f['name']} overlaps another field")
        spans.append((off, off + size))
        fields.append(Field(f["name"], f["type"], off))
        names.append(f["name"]); formats.append(FIELD_TYPES[f["type"]][1]); offsets.append(off)
    dtype = np.dtype({"names": names, "formats": formats, "offsets": offsets, "itemsize": stride})
    return Table(int(d["id"]), d["name"], int(d["cap"]), stride, tuple(fields), dtype)


def parse_hello(payload: bytes) -> Hello:
    d = json.loads(payload.decode("utf-8"))
    if d.get("proto") != PROTO_VERSION:
        raise SchemaError(f"proto {d.get('proto')} != {PROTO_VERSION}")
    tables = {}
    for td in d["tables"]:
        t = _build_table(td)
        if t.id in tables:
            raise SchemaError(f"duplicate table id {t.id}")
        tables[t.id] = t
    return Hello(
        proto=d["proto"], backend=d["backend"], tick_hz=int(d["tick_hz"]),
        half_w=int(d["field"]["half_w"]), height=int(d["field"]["height"]),
        origin=d["field"]["origin"],
        actions={int(a["bit"]): a["name"] for a in d["actions"]},
        caps=set(d.get("caps", [])), tables=tables,
    )
```

`src/stgagent/obs.py`：

```python
"""OBS 解码：u32 frame, u32 phase, u8 table_count, [u8 id, u16 count, count×stride]。"""
import struct
from dataclasses import dataclass
import numpy as np
from .schema import Hello

_HDR = struct.Struct("<IIB")
_TBL = struct.Struct("<BH")


@dataclass
class Observation:
    frame: int
    phase: int
    tables: dict          # name -> structured ndarray (len == count)


def decode_obs(payload: bytes, hello: Hello) -> Observation:
    frame, phase, n = _HDR.unpack_from(payload, 0)
    pos = _HDR.size
    tables = {}
    for _ in range(n):
        tid, count = _TBL.unpack_from(payload, pos)
        pos += _TBL.size
        t = hello.tables.get(tid)
        if t is None:
            raise ValueError(f"unknown table id {tid}")
        if count > t.cap:
            raise ValueError(f"table {t.name}: count {count} > cap {t.cap}")
        nbytes = count * t.stride
        if pos + nbytes > len(payload):
            raise ValueError(f"table {t.name}: truncated")
        tables[t.name] = np.frombuffer(payload, dtype=t.dtype, count=count, offset=pos).copy()
        pos += nbytes
    return Observation(frame, phase, tables)
```

- [ ] **Step 4: 跑测试确认通过** → 7 passed

- [ ] **Step 5: 提交**

```bash
git add src/stgagent/schema.py src/stgagent/obs.py tests/test_schema.py tests/test_obs.py
git commit -q -m "feat: schema + obs —— HELLO 驱动的 numpy 解码

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 5: session.py + fake_backend.py —— 监听、握手、逐帧循环

**Files:**
- Create: `src/stgagent/session.py`、`src/stgagent/fake_backend.py`
- Modify: `src/stgagent/__init__.py`（导出 `Listener, Session, Observation`）
- Test: `tests/test_session.py`

**Interfaces:**
- Produces: `session.Listener(host="127.0.0.1", port=7788)`，`.accept(timeout=None) -> Session`（阻塞等一个后端连上并收到 HELLO）；`.port` 属性（port=0 时取实际端口）；`.close()`。
- Produces: `session.Session`：`.hello: Hello`；`.next_obs() -> Observation | None`（收到 BYE 或断开返回 None；收到 CTRL 存进 `.events: list[dict]` 并继续）；`.send_act(frame, buttons, flags=0)`；`.send_ctrl(dict)`；`.close()`；`__iter__` 迭代 obs。
- Produces: `fake_backend.FakeBackend(host, port, hello: dict, frames: int)`，`.run()`（连上、发 HELLO、每帧发一个 OBS：player 在 (0,400)，两颗弹；等 ACT，记进 `.acts: list[tuple]`；帧数满后发 BYE）。`fake_backend.DEFAULT_HELLO` 是 Task 4 测试里那份 HELLO。

- [ ] **Step 1: 失败测试**

`tests/test_session.py`：

```python
import threading
from stgagent import Listener, BTN_SHOT, BTN_LEFT
from stgagent.fake_backend import FakeBackend, DEFAULT_HELLO


def test_loopback_three_frames():
    lis = Listener("127.0.0.1", 0)
    fb = FakeBackend("127.0.0.1", lis.port, DEFAULT_HELLO, frames=3)
    t = threading.Thread(target=fb.run, daemon=True)
    t.start()
    s = lis.accept(timeout=5)
    assert s.hello.backend == "fake"
    seen = []
    for o in s:
        seen.append(o.frame)
        s.send_act(o.frame, BTN_SHOT | (BTN_LEFT if o.frame % 2 else 0))
    t.join(5)
    assert seen == [0, 1, 2]
    assert fb.acts == [(0, BTN_SHOT, 0), (1, BTN_SHOT | BTN_LEFT, 0), (2, BTN_SHOT, 0)]
    assert len(o.tables["bullets"]) == 2
    lis.close()


def test_ctrl_events_are_collected():
    lis = Listener("127.0.0.1", 0)
    fb = FakeBackend("127.0.0.1", lis.port, DEFAULT_HELLO, frames=1, ctrl_before_bye={"event": "stage_end"})
    t = threading.Thread(target=fb.run, daemon=True)
    t.start()
    s = lis.accept(timeout=5)
    for o in s:
        s.send_act(o.frame, 0)
    t.join(5)
    assert s.events == [{"event": "stage_end"}]
    lis.close()
```

- [ ] **Step 2: 跑测试确认失败** → ImportError

- [ ] **Step 3: 实现**

`src/stgagent/session.py`：

```python
"""AI 侧：监听一个后端，握手，逐帧 OBS → ACT。"""
import json
import socket
from .consts import MsgType
from .framing import read_msg, write_msg
from .schema import parse_hello
from .obs import decode_obs, Observation
from .act import encode_act


class Session:
    def __init__(self, sock: socket.socket):
        self.sock = sock
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.events = []
        mtype, payload = read_msg(sock)
        if mtype != MsgType.HELLO:
            raise ConnectionError(f"expected HELLO, got type {mtype}")
        self.hello = parse_hello(payload)

    def next_obs(self):
        while True:
            try:
                mtype, payload = read_msg(self.sock)
            except ConnectionError:
                return None
            if mtype == MsgType.OBS:
                return decode_obs(payload, self.hello)
            if mtype == MsgType.CTRL:
                self.events.append(json.loads(payload.decode("utf-8")))
                continue
            if mtype == MsgType.BYE:
                return None
            raise ConnectionError(f"unexpected message type {mtype}")

    def __iter__(self):
        while True:
            o = self.next_obs()
            if o is None:
                return
            yield o

    def send_act(self, frame: int, buttons: int, flags: int = 0) -> None:
        write_msg(self.sock, MsgType.ACT, encode_act(frame, buttons, flags))

    def send_ctrl(self, d: dict) -> None:
        write_msg(self.sock, MsgType.CTRL, json.dumps(d).encode("utf-8"))

    def close(self):
        try:
            write_msg(self.sock, MsgType.BYE, b"")
        except OSError:
            pass
        self.sock.close()


class Listener:
    def __init__(self, host="127.0.0.1", port=7788):
        self._srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._srv.bind((host, port))
        self._srv.listen(1)
        self.port = self._srv.getsockname()[1]

    def accept(self, timeout=None) -> Session:
        self._srv.settimeout(timeout)
        conn, _ = self._srv.accept()
        conn.settimeout(None)
        return Session(conn)

    def close(self):
        self._srv.close()
```

`src/stgagent/fake_backend.py`：

```python
"""纯 Python 假后端。测试 Session 用；也是「一个后端最少要做什么」的可执行说明。"""
import json
import socket
import struct
from .consts import MsgType, PHASE_IN_GAME, PHASE_PLAYER_CONTROLLABLE
from .framing import read_msg, write_msg
from .act import decode_act

DEFAULT_HELLO = {
    "proto": 1, "backend": "fake", "tick_hz": 60,
    "field": {"half_w": 192, "height": 448, "origin": "center-top"},
    "number": {"fx": "q16.16-i32-le", "angle": "bam-u16-le"},
    "actions": [{"bit": 0, "name": "UP"}, {"bit": 1, "name": "DOWN"}, {"bit": 2, "name": "LEFT"},
                {"bit": 3, "name": "RIGHT"}, {"bit": 4, "name": "SHOT"}, {"bit": 5, "name": "BOMB"},
                {"bit": 6, "name": "SLOW"}],
    "caps": ["lockstep", "passthrough"],
    "tables": [
        {"id": 1, "name": "player", "cap": 1, "stride": 12,
         "fields": [{"name": "x", "type": "fx", "off": 0}, {"name": "y", "type": "fx", "off": 4},
                    {"name": "state", "type": "u8", "off": 8}, {"name": "lives", "type": "u8", "off": 9},
                    {"name": "power", "type": "u16", "off": 10}]},
        {"id": 2, "name": "bullets", "cap": 4, "stride": 10,
         "fields": [{"name": "x", "type": "fx", "off": 0}, {"name": "y", "type": "fx", "off": 4},
                    {"name": "angle", "type": "angle", "off": 8}]},
    ],
}


class FakeBackend:
    def __init__(self, host, port, hello, frames, ctrl_before_bye=None):
        self.host, self.port, self.hello, self.frames = host, port, hello, frames
        self.ctrl_before_bye = ctrl_before_bye
        self.acts = []

    def _obs(self, frame):
        body = struct.pack("<IIB", frame, PHASE_IN_GAME | PHASE_PLAYER_CONTROLLABLE, 2)
        body += struct.pack("<BH", 1, 1) + struct.pack("<iiBBH", 0, 400 << 16, 0, 3, 100)
        body += struct.pack("<BH", 2, 2)
        body += struct.pack("<iiH", 10 << 16, (20 + frame) << 16, 16384)
        body += struct.pack("<iiH", -10 << 16, (30 + frame) << 16, 49152)
        return body

    def run(self):
        s = socket.create_connection((self.host, self.port), timeout=5)
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        write_msg(s, MsgType.HELLO, json.dumps(self.hello).encode("utf-8"))
        for f in range(self.frames):
            write_msg(s, MsgType.OBS, self._obs(f))
            while True:
                mtype, payload = read_msg(s)
                if mtype == MsgType.ACT:
                    frame, buttons, flags = decode_act(payload)
                    if frame == f:
                        self.acts.append((frame, buttons, flags))
                        break
                # 迟到的 ACT 或 CTRL：丢弃 / 忽略，继续等
        if self.ctrl_before_bye:
            write_msg(s, MsgType.CTRL, json.dumps(self.ctrl_before_bye).encode("utf-8"))
        write_msg(s, MsgType.BYE, b"")
        s.close()
```

`__init__.py` 加：

```python
from .session import Listener, Session
from .obs import Observation
__all__ += ["Listener", "Session", "Observation"]
```

- [ ] **Step 4: 跑全部测试确认通过**

Run: `pytest -v` → 全部 passed

- [ ] **Step 5: 提交**

```bash
git add -A && git commit -q -m "feat: Listener/Session + Python 假后端回环

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 6: C 核心 —— stgagent.h + sa_proto.c（编码，宿主可测）

**Files:**
- Create: `c/stgagent.h`、`c/sa_proto.c`、`c/Makefile`、`c/tests/test_proto.c`

**Interfaces:**
- Produces（`c/stgagent.h` 全文见 Step 3）：
  - 描述结构 `sa_field_t{name,type,off}`、`sa_table_t{id,name,cap,stride,fields,nfields}`、`sa_desc_t{backend,tick_hz,half_w,height,action_bits,caps,ncaps,tables,ntables}`。
  - `int sa_hello_json(const sa_desc_t*, char *out, int cap)` → 写入 JSON 字节数（不含 NUL；溢出返回 -1）。
  - OBS 构造（静态缓冲，`SA_OBS_CAP` 字节）：`void sa_obs_begin(uint32_t frame, uint32_t phase)`；`int sa_obs_table_begin(const sa_table_t*)`；`void *sa_obs_row(void)`（返回下一条记录的写指针，超 cap 返回 NULL）；`void sa_obs_table_end(void)`；`const uint8_t *sa_obs_bytes(int *len)`。
  - 换算：`int32_t sa_fx(float)`、`uint16_t sa_bam(float rad)`、`int32_t sa_fx_from_sub128(int32_t)`（`<< 9`）。
  - 记录写入辅助：`sa_put_fx(void *row, int off, float)`、`sa_put_bam(row, off, float)`、`sa_put_i32 / sa_put_u32 / sa_put_u16 / sa_put_u8`。
  - ACT：`typedef struct { uint32_t frame, buttons, flags; } sa_act_t;`、`int sa_act_decode(const uint8_t *p, int n, sa_act_t *out)`。

- [ ] **Step 1: 失败测试**

`c/tests/test_proto.c`：

```c
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../stgagent.h"

static const sa_field_t PF[] = { {"x","fx",0}, {"y","fx",4}, {"state","u8",8} };
static const sa_table_t T_PLAYER = { 1, "player", 1, 9, PF, 3 };
static const sa_field_t BF[] = { {"x","fx",0}, {"angle","angle",4} };
static const sa_table_t T_BULLETS = { 2, "bullets", 3, 6, BF, 2 };
static const sa_table_t *TABLES[] = { &T_PLAYER, &T_BULLETS };
static const char *CAPS[] = { "lockstep" };

int main(void)
{
    /* 换算 */
    assert(sa_fx(1.0f) == 65536);
    assert(sa_fx(-184.0f) == -184 * 65536);
    assert(sa_fx_from_sub128(-23552) == -184 * 65536);       /* TH18 自机定点 << 9 */
    assert(sa_bam((float)M_PI) == 32768);
    assert(sa_bam((float)(-M_PI / 2)) == 49152);

    /* HELLO */
    sa_desc_t d = { "fake", 60, 192, 448, 0x7f, CAPS, 1, TABLES, 2 };
    char js[4096];
    int n = sa_hello_json(&d, js, sizeof js);
    assert(n > 0);
    assert(strstr(js, "\"proto\":1"));
    assert(strstr(js, "\"backend\":\"fake\""));
    assert(strstr(js, "{\"bit\":6,\"name\":\"SLOW\"}"));
    assert(!strstr(js, "TIMESTOP"));                          /* 0x7f 不含 bit 7 */
    assert(strstr(js, "{\"id\":2,\"name\":\"bullets\",\"cap\":3,\"stride\":6,\"fields\":[{\"name\":\"x\",\"type\":\"fx\",\"off\":0},{\"name\":\"angle\",\"type\":\"angle\",\"off\":4}]}"));

    /* OBS */
    sa_obs_begin(42, 0x21);
    assert(sa_obs_table_begin(&T_PLAYER));
    void *r = sa_obs_row(); assert(r);
    sa_put_fx(r, 0, -184.0f); sa_put_fx(r, 4, 400.0f); sa_put_u8(r, 8, 3);
    sa_obs_table_end();
    assert(sa_obs_table_begin(&T_BULLETS));
    for (int i = 0; i < 3; i++) { r = sa_obs_row(); assert(r); sa_put_fx(r, 0, (float)i); sa_put_bam(r, 4, 0.0f); }
    assert(sa_obs_row() == NULL);                             /* cap 3 */
    sa_obs_table_end();
    int len; const uint8_t *b = sa_obs_bytes(&len);
    assert(len == 9 + (3 + 9) + (3 + 3 * 6));
    assert(b[0] == 42 && b[4] == 0x21 && b[8] == 2);
    assert(b[9] == 1 && b[10] == 1 && b[11] == 0);            /* table 1, count 1 */
    assert(memcmp(b + 12, "\x00\x00\x48\xff", 4) == 0);      /* -184 << 16 */
    assert(b[21] == 2 && b[22] == 3 && b[23] == 0);           /* table 2, count 3 */

    /* ACT */
    sa_act_t a; uint8_t act[12] = { 7,0,0,0, 0x14,0,0,0, 1,0,0,0 };
    assert(sa_act_decode(act, 12, &a) == 1 && a.frame == 7 && a.buttons == 0x14 && a.flags == 1);
    assert(sa_act_decode(act, 11, &a) == 0);
    puts("test_proto ok");
    return 0;
}
```

`c/Makefile`：

```make
CC      ?= gcc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -DSA_POSIX
XWIN    ?= $(HOME)/.cache/cargo-xwin/xwin
XCLANG  ?= /data/sunyunbo/miniconda3/envs/xwin/bin/clang
SRC     := sa_proto.c sa_net.c sa_lockstep.c

build:
	mkdir -p build

build/test_proto: build tests/test_proto.c sa_proto.c stgagent.h
	$(CC) $(CFLAGS) -o $@ tests/test_proto.c sa_proto.c -lm

build/fake_game: build tests/fake_game.c $(SRC) stgagent.h
	$(CC) $(CFLAGS) -o $@ tests/fake_game.c $(SRC) -lm

test-host: build/test_proto build/fake_game
	./build/test_proto

# 只验「能在 Windows 目标上编译」，产物不运行。x64 给 th06nc，x86 给 th18。
cross-%: build
	$(XCLANG) --target=$*-pc-windows-msvc -std=c99 -O2 -Wall -c \
	  -isystem $(XWIN)/crt/include -isystem $(XWIN)/sdk/include/ucrt \
	  -isystem $(XWIN)/sdk/include/um -isystem $(XWIN)/sdk/include/shared \
	  $(SRC) && mv *.o build/ && echo "cross $* ok"

cross-x64: ; $(MAKE) cross-x86_64
cross-x86: ; $(MAKE) cross-i686

.PHONY: test-host cross-x64 cross-x86
```

- [ ] **Step 2: 跑测试确认失败**

Run: `cd c && make build/test_proto` → 编译失败（`stgagent.h` 不存在）

- [ ] **Step 3: 写 stgagent.h**

```c
/* stgagent.h —— stg-agent-proto v1 的 C 桥核心公开 API。
 * 与加载方式无关、与作品无关、与位宽无关。只依赖 C 标准库 + socket。
 * 静态缓冲，钩子 / 断点内零堆分配。 */
#ifndef STGAGENT_H
#define STGAGENT_H
#include <stdint.h>

#define SA_PROTO_VERSION 1
#ifndef SA_OBS_CAP
#define SA_OBS_CAP (256 * 1024)
#endif

enum { SA_MSG_HELLO = 1, SA_MSG_OBS = 2, SA_MSG_ACT = 3, SA_MSG_CTRL = 4, SA_MSG_BYE = 5 };

/* 动作位（bit 号），与 SPEC §4 一致 */
enum { SA_BTN_UP = 0, SA_BTN_DOWN = 1, SA_BTN_LEFT = 2, SA_BTN_RIGHT = 3, SA_BTN_SHOT = 4,
       SA_BTN_BOMB = 5, SA_BTN_SLOW = 6, SA_BTN_TIMESTOP = 7, SA_BTN_CARD_USE = 8, SA_BTN_CARD_SWITCH = 9 };
#define SA_ACT_PASSTHROUGH 1u

/* OBS.phase 位 */
#define SA_PHASE_IN_GAME             (1u << 0)
#define SA_PHASE_PAUSED              (1u << 1)
#define SA_PHASE_DIALOGUE            (1u << 2)
#define SA_PHASE_SHOP                (1u << 3)
#define SA_PHASE_BOMB_ACTIVE         (1u << 4)
#define SA_PHASE_SPELL_ACTIVE        (1u << 5)
#define SA_PHASE_PLAYER_CONTROLLABLE (1u << 6)
#define SA_PHASE_REPLAY_PLAYBACK     (1u << 7)

typedef struct { const char *name; const char *type; uint16_t off; } sa_field_t;
typedef struct { uint8_t id; const char *name; uint16_t cap; uint16_t stride;
                 const sa_field_t *fields; int nfields; } sa_table_t;
typedef struct { const char *backend; uint16_t tick_hz; int16_t half_w; int16_t height;
                 uint32_t action_bits;            /* 支持的动作位掩码 */
                 const char **caps; int ncaps;
                 const sa_table_t **tables; int ntables; } sa_desc_t;
typedef struct { uint32_t frame, buttons, flags; } sa_act_t;

/* ---- 换算 ---- */
int32_t  sa_fx(float v);                 /* Q16.16，round-half-even 后钳 i32 */
int32_t  sa_fx_from_sub128(int32_t sub); /* 1/128 px 定点 → Q16.16（精确，<< 9） */
uint16_t sa_bam(float rad);              /* 弧度 → BAM u16（回绕） */

/* ---- 记录写入 ---- */
void sa_put_fx (void *row, int off, float v);
void sa_put_bam(void *row, int off, float rad);
void sa_put_i32(void *row, int off, int32_t v);
void sa_put_u32(void *row, int off, uint32_t v);
void sa_put_u16(void *row, int off, uint16_t v);
void sa_put_u8 (void *row, int off, uint8_t v);

/* ---- HELLO / OBS / ACT 编解码（无 I/O）---- */
int  sa_hello_json(const sa_desc_t *d, char *out, int cap);
void sa_obs_begin(uint32_t frame, uint32_t phase);
int  sa_obs_table_begin(const sa_table_t *t);   /* 0 = 缓冲放不下（表头 + cap×stride） */
void *sa_obs_row(void);                         /* NULL = 已到 cap */
void sa_obs_table_end(void);
const uint8_t *sa_obs_bytes(int *len);
int  sa_act_decode(const uint8_t *p, int n, sa_act_t *out);  /* 1 ok / 0 bad */

/* ---- 网络（sa_net.c）---- */
int  sa_net_connect(const char *host, uint16_t port, int timeout_ms);  /* 1 connected */
int  sa_net_connected(void);
void sa_net_close(void);
int  sa_net_send(uint8_t type, const uint8_t *payload, int len);       /* 1 ok / 0 断线 */
/* 收一帧：返回 type（>0），0 = 超时，-1 = 断线。payload 拷进 out（最多 cap） */
int  sa_net_recv(uint8_t *out, int cap, int *len, int timeout_ms);

/* ---- 锁步（sa_lockstep.c）---- */
typedef struct {
    int      timeout_ms;      /* <0 = 无限等；0 = 不等，取最近一条 */
    sa_act_t last;            /* 沿用的上一帧动作 */
    uint32_t dropped_late;    /* frame < n 的迟到包计数 */
    uint32_t timeouts;
} sa_lockstep_t;
void sa_lockstep_init(sa_lockstep_t *s, int timeout_ms);
/* 发送已构造好的 OBS，等 ACT(frame)。返回 1 = 拿到新 ACT，0 = 超时沿用，-1 = 断线。
 * 无论哪种，*out 都填好可用的动作。 */
int  sa_lockstep_frame(sa_lockstep_t *s, uint32_t frame, sa_act_t *out);

#endif
```

- [ ] **Step 4: 写 sa_proto.c**

```c
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "stgagent.h"

static const char *ACTION_NAMES[10] = { "UP","DOWN","LEFT","RIGHT","SHOT","BOMB","SLOW",
                                        "TIMESTOP","CARD_USE","CARD_SWITCH" };

int32_t sa_fx(float v)
{
    double d = rint((double)v * 65536.0);
    if (d > 2147483647.0) return INT32_MAX;
    if (d < -2147483648.0) return INT32_MIN;
    return (int32_t)d;
}
int32_t sa_fx_from_sub128(int32_t sub) { return (int32_t)((uint32_t)sub << 9); }
uint16_t sa_bam(float rad)
{
    double d = rint((double)rad * (65536.0 / (2.0 * M_PI)));
    long long i = (long long)d;
    return (uint16_t)(i & 0xFFFF);
}

static void le32(uint8_t *p, uint32_t v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }
static void le16(uint8_t *p, uint16_t v) { p[0]=v; p[1]=v>>8; }
void sa_put_fx (void *r, int o, float v)    { le32((uint8_t*)r+o, (uint32_t)sa_fx(v)); }
void sa_put_bam(void *r, int o, float v)    { le16((uint8_t*)r+o, sa_bam(v)); }
void sa_put_i32(void *r, int o, int32_t v)  { le32((uint8_t*)r+o, (uint32_t)v); }
void sa_put_u32(void *r, int o, uint32_t v) { le32((uint8_t*)r+o, v); }
void sa_put_u16(void *r, int o, uint16_t v) { le16((uint8_t*)r+o, v); }
void sa_put_u8 (void *r, int o, uint8_t v)  { ((uint8_t*)r)[o] = v; }

/* ---- HELLO ---- */
static int cat(char *out, int cap, int n, const char *s)
{
    int l = (int)strlen(s);
    if (n < 0 || n + l >= cap) return -1;
    memcpy(out + n, s, l + 1);
    return n + l;
}
static int catf(char *out, int cap, int n, const char *fmt, long v)
{
    char tmp[32];
    snprintf(tmp, sizeof tmp, fmt, v);
    return cat(out, cap, n, tmp);
}
int sa_hello_json(const sa_desc_t *d, char *out, int cap)
{
    int n = 0, i, j, first;
    n = cat(out, cap, n, "{\"proto\":1,\"backend\":\"");
    n = cat(out, cap, n, d->backend);
    n = catf(out, cap, n, "\",\"tick_hz\":%ld,", d->tick_hz);
    n = catf(out, cap, n, "\"field\":{\"half_w\":%ld,", d->half_w);
    n = catf(out, cap, n, "\"height\":%ld,\"origin\":\"center-top\"},", d->height);
    n = cat(out, cap, n, "\"number\":{\"fx\":\"q16.16-i32-le\",\"angle\":\"bam-u16-le\"},\"actions\":[");
    for (i = 0, first = 1; i < 10; i++) {
        if (!(d->action_bits & (1u << i))) continue;
        n = cat(out, cap, n, first ? "" : ","); first = 0;
        n = catf(out, cap, n, "{\"bit\":%ld,\"name\":\"", i);
        n = cat(out, cap, n, ACTION_NAMES[i]);
        n = cat(out, cap, n, "\"}");
    }
    n = cat(out, cap, n, "],\"caps\":[");
    for (i = 0; i < d->ncaps; i++) {
        n = cat(out, cap, n, i ? ",\"" : "\""); n = cat(out, cap, n, d->caps[i]); n = cat(out, cap, n, "\"");
    }
    n = cat(out, cap, n, "],\"tables\":[");
    for (i = 0; i < d->ntables; i++) {
        const sa_table_t *t = d->tables[i];
        n = cat(out, cap, n, i ? "," : "");
        n = catf(out, cap, n, "{\"id\":%ld,\"name\":\"", t->id);
        n = cat(out, cap, n, t->name);
        n = catf(out, cap, n, "\",\"cap\":%ld,", t->cap);
        n = catf(out, cap, n, "\"stride\":%ld,\"fields\":[", t->stride);
        for (j = 0; j < t->nfields; j++) {
            n = cat(out, cap, n, j ? ",{\"name\":\"" : "{\"name\":\"");
            n = cat(out, cap, n, t->fields[j].name);
            n = cat(out, cap, n, "\",\"type\":\"");
            n = cat(out, cap, n, t->fields[j].type);
            n = catf(out, cap, n, "\",\"off\":%ld}", t->fields[j].off);
        }
        n = cat(out, cap, n, "]}");
    }
    n = cat(out, cap, n, "]}");
    return n;
}

/* ---- OBS ---- */
static uint8_t g_obs[SA_OBS_CAP];
static int g_len, g_tbl_hdr, g_count, g_cap, g_stride, g_overflow;

void sa_obs_begin(uint32_t frame, uint32_t phase)
{
    le32(g_obs, frame); le32(g_obs + 4, phase); g_obs[8] = 0;
    g_len = 9; g_tbl_hdr = -1; g_overflow = 0;
}
int sa_obs_table_begin(const sa_table_t *t)
{
    if (g_len + 3 + (int)t->cap * (int)t->stride > SA_OBS_CAP) { g_overflow = 1; return 0; }
    g_tbl_hdr = g_len; g_obs[g_len] = t->id; le16(g_obs + g_len + 1, 0);
    g_len += 3; g_count = 0; g_cap = t->cap; g_stride = t->stride;
    return 1;
}
void *sa_obs_row(void)
{
    if (g_tbl_hdr < 0 || g_count >= g_cap) return NULL;
    void *p = g_obs + g_len;
    memset(p, 0, (size_t)g_stride);
    g_len += g_stride; g_count++;
    return p;
}
void sa_obs_table_end(void)
{
    if (g_tbl_hdr < 0) return;
    le16(g_obs + g_tbl_hdr + 1, (uint16_t)g_count);
    g_obs[8]++; g_tbl_hdr = -1;
}
const uint8_t *sa_obs_bytes(int *len) { *len = g_len; return g_obs; }

int sa_act_decode(const uint8_t *p, int n, sa_act_t *out)
{
    if (n != 12) return 0;
    out->frame   = p[0] | p[1]<<8 | p[2]<<16 | (uint32_t)p[3]<<24;
    out->buttons = p[4] | p[5]<<8 | p[6]<<16 | (uint32_t)p[7]<<24;
    out->flags   = p[8] | p[9]<<8 | p[10]<<16 | (uint32_t)p[11]<<24;
    return 1;
}
```

- [ ] **Step 5: 跑测试确认通过**

Run: `cd c && make build/test_proto && ./build/test_proto`
Expected: `test_proto ok`

- [ ] **Step 6: 提交**

```bash
git add c/stgagent.h c/sa_proto.c c/Makefile c/tests/test_proto.c
git commit -q -m "feat(c): 桥核心 —— HELLO/OBS/ACT 编解码 + fx/bam 换算

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 7: C 核心 —— sa_net.c + sa_lockstep.c + 假游戏，与 Python 回环

**Files:**
- Create: `c/sa_net.c`、`c/sa_lockstep.c`、`c/tests/fake_game.c`
- Test: `tests/test_c_core.py`（pytest 编译并驱动假游戏）

**Interfaces:**
- Consumes: Task 6 的全部 API。
- Produces: `c/tests/fake_game` 可执行文件：`fake_game <host> <port> <frames> <timeout_ms>`，连上后发 HELLO（两张表同 Task 5 的 DEFAULT_HELLO 布局）、每帧 OBS、把每帧拿到的动作按 `frame buttons flags got` 一行打到 stdout（`got` = `sa_lockstep_frame` 的返回值），最后发 BYE、退出码 0。

- [ ] **Step 1: 失败测试**

`tests/test_c_core.py`：

```python
import os
import subprocess
import threading
from pathlib import Path
import pytest
from stgagent import Listener, BTN_SHOT, BTN_RIGHT

C_DIR = Path(__file__).resolve().parents[1] / "c"
BIN = C_DIR / "build" / "fake_game"


@pytest.fixture(scope="module")
def fake_game():
    subprocess.run(["make", "-C", str(C_DIR), "build/fake_game"], check=True)
    return BIN


def test_c_fake_game_loopback(fake_game):
    lis = Listener("127.0.0.1", 0)
    proc = subprocess.Popen([str(fake_game), "127.0.0.1", str(lis.port), "5", "-1"],
                            stdout=subprocess.PIPE, text=True)
    s = lis.accept(timeout=5)
    assert s.hello.backend == "c-fake"
    assert s.hello.table_by_name("bullets").dtype.itemsize == 10
    frames = []
    for o in s:
        frames.append(o.frame)
        assert len(o.tables["player"]) == 1 and len(o.tables["bullets"]) == 2
        s.send_act(o.frame, BTN_SHOT | (BTN_RIGHT if o.frame == 3 else 0))
    out, _ = proc.communicate(timeout=5)
    assert proc.returncode == 0
    assert frames == [0, 1, 2, 3, 4]
    lines = [tuple(map(int, l.split())) for l in out.strip().splitlines()]
    assert lines == [(0, 16, 0, 1), (1, 16, 0, 1), (2, 16, 0, 1), (3, 24, 0, 1), (4, 16, 0, 1)]
    lis.close()


def test_c_timeout_holds_last_action(fake_game):
    lis = Listener("127.0.0.1", 0)
    proc = subprocess.Popen([str(fake_game), "127.0.0.1", str(lis.port), "3", "200"],
                            stdout=subprocess.PIPE, text=True)
    s = lis.accept(timeout=5)
    o = s.next_obs(); s.send_act(o.frame, BTN_SHOT)     # 帧 0 正常回
    o = s.next_obs()                                     # 帧 1 故意不回 → 超时沿用
    o = s.next_obs(); s.send_act(o.frame, BTN_RIGHT)     # 帧 2
    assert s.next_obs() is None
    out, _ = proc.communicate(timeout=5)
    lines = [tuple(map(int, l.split())) for l in out.strip().splitlines()]
    assert lines == [(0, 16, 0, 1), (1, 16, 0, 0), (2, 8, 0, 1)]
    lis.close()
```

- [ ] **Step 2: 跑测试确认失败**

Run: `pytest tests/test_c_core.py -v` → make 失败（`sa_net.c` 不存在）

- [ ] **Step 3: 写 sa_net.c**

```c
/* sa_net.c —— 一条 TCP 连接、长度前缀收发、可超时 recv。
 * SA_POSIX：BSD socket（宿主测试）；否则 winsock2（游戏内）。 */
#include <string.h>
#include "stgagent.h"

#ifdef SA_POSIX
#  include <arpa/inet.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <netdb.h>
#  include <netinet/tcp.h>
#  include <sys/socket.h>
#  include <sys/time.h>
#  include <unistd.h>
   typedef int sock_t;
#  define SA_BAD (-1)
#  define sa_closesock close
#  define sa_wouldblock() (errno == EAGAIN || errno == EWOULDBLOCK)
#else
#  include <winsock2.h>
#  include <ws2tcpip.h>
   typedef SOCKET sock_t;
#  define SA_BAD INVALID_SOCKET
#  define sa_closesock closesocket
#  define sa_wouldblock() (WSAGetLastError() == WSAETIMEDOUT || WSAGetLastError() == WSAEWOULDBLOCK)
#endif

static sock_t g_s = SA_BAD;
static int g_inited;

static void set_timeout(int ms)
{
#ifdef SA_POSIX
    struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
    setsockopt(g_s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
#else
    DWORD d = (DWORD)ms;
    setsockopt(g_s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&d, sizeof d);
#endif
}

int sa_net_connect(const char *host, uint16_t port, int timeout_ms)
{
    struct addrinfo hints, *res = NULL, *ai;
    char portstr[8];
    int one = 1;
#ifndef SA_POSIX
    if (!g_inited) { WSADATA w; if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return 0; g_inited = 1; }
#else
    g_inited = 1;
#endif
    if (g_s != SA_BAD) sa_net_close();
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    snprintf(portstr, sizeof portstr, "%u", (unsigned)port);
    if (getaddrinfo(host, portstr, &hints, &res) != 0) return 0;
    for (ai = res; ai; ai = ai->ai_next) {
        g_s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (g_s == SA_BAD) continue;
        set_timeout(timeout_ms > 0 ? timeout_ms : 1000);
        if (connect(g_s, ai->ai_addr, (int)ai->ai_addrlen) == 0) break;
        sa_closesock(g_s); g_s = SA_BAD;
    }
    freeaddrinfo(res);
    if (g_s == SA_BAD) return 0;
    setsockopt(g_s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
    return 1;
}
int  sa_net_connected(void) { return g_s != SA_BAD; }
void sa_net_close(void) { if (g_s != SA_BAD) { sa_closesock(g_s); g_s = SA_BAD; } }

static int send_all(const uint8_t *p, int n)
{
    while (n > 0) {
        int k = (int)send(g_s, (const char *)p, n, 0);
        if (k <= 0) { sa_net_close(); return 0; }
        p += k; n -= k;
    }
    return 1;
}
int sa_net_send(uint8_t type, const uint8_t *payload, int len)
{
    uint8_t hdr[5];
    uint32_t l = (uint32_t)len + 1;
    if (g_s == SA_BAD) return 0;
    hdr[0]=l; hdr[1]=l>>8; hdr[2]=l>>16; hdr[3]=l>>24; hdr[4]=type;
    return send_all(hdr, 5) && (len == 0 || send_all(payload, len));
}

/* 返回 1 读满，0 超时（未读到任何字节），-1 断线。已读部分留在 buf/got 里供下次继续。 */
static int recv_into(uint8_t *buf, int want, int *got, int timeout_ms)
{
    set_timeout(timeout_ms < 0 ? 0 : (timeout_ms == 0 ? 1 : timeout_ms));
    while (*got < want) {
        int k = (int)recv(g_s, (char *)buf + *got, want - *got, 0);
        if (k > 0) { *got += k; continue; }
        if (k == 0) { sa_net_close(); return -1; }
        if (sa_wouldblock()) return 0;
        sa_net_close(); return -1;
    }
    return 1;
}

static uint8_t g_hdr[5]; static int g_hdr_got;
static int g_body_got;

int sa_net_recv(uint8_t *out, int cap, int *len, int timeout_ms)
{
    int r; uint32_t l;
    if (g_s == SA_BAD) return -1;
    r = recv_into(g_hdr, 5, &g_hdr_got, timeout_ms);
    if (r <= 0) return r;
    l = g_hdr[0] | g_hdr[1]<<8 | g_hdr[2]<<16 | (uint32_t)g_hdr[3]<<24;
    if (l < 1 || (int)(l - 1) > cap) { sa_net_close(); return -1; }
    r = recv_into(out, (int)(l - 1), &g_body_got, timeout_ms);
    if (r <= 0) return r;
    *len = (int)(l - 1);
    g_hdr_got = 0; g_body_got = 0;
    return g_hdr[4];
}
```

- [ ] **Step 4: 写 sa_lockstep.c**

```c
/* sa_lockstep.c —— 发 OBS(n) → 等 ACT(n) → 超时沿用上一帧。 */
#include "stgagent.h"

void sa_lockstep_init(sa_lockstep_t *s, int timeout_ms)
{
    s->timeout_ms = timeout_ms;
    s->last.frame = 0; s->last.buttons = 0; s->last.flags = 0;
    s->dropped_late = 0; s->timeouts = 0;
}

int sa_lockstep_frame(sa_lockstep_t *s, uint32_t frame, sa_act_t *out)
{
    static uint8_t buf[4096];
    int len, r;
    const uint8_t *obs = sa_obs_bytes(&len);
    if (!sa_net_send(SA_MSG_OBS, obs, len)) { *out = s->last; return -1; }
    for (;;) {
        r = sa_net_recv(buf, sizeof buf, &len, s->timeout_ms);
        if (r == -1) { *out = s->last; return -1; }
        if (r == 0)  { s->timeouts++; *out = s->last; return 0; }
        if (r == SA_MSG_ACT) {
            sa_act_t a;
            if (!sa_act_decode(buf, len, &a)) continue;
            if (a.frame < frame) { s->dropped_late++; continue; }
            if (a.frame > frame) { sa_net_close(); *out = s->last; return -1; }
            s->last = a; *out = a; return 1;
        }
        /* CTRL / BYE：v1 的桥核心不解释 CTRL（超时等由加载器配置），BYE 视为断线 */
        if (r == SA_MSG_BYE) { sa_net_close(); *out = s->last; return -1; }
    }
}
```

- [ ] **Step 5: 写 fake_game.c**

```c
#include <stdio.h>
#include <stdlib.h>
#include "../stgagent.h"

static const sa_field_t PF[] = { {"x","fx",0},{"y","fx",4},{"state","u8",8},{"lives","u8",9},{"power","u16",10} };
static const sa_table_t T_PLAYER = { 1, "player", 1, 12, PF, 5 };
static const sa_field_t BF[] = { {"x","fx",0},{"y","fx",4},{"angle","angle",8} };
static const sa_table_t T_BULLETS = { 2, "bullets", 4, 10, BF, 3 };
static const sa_table_t *TABLES[] = { &T_PLAYER, &T_BULLETS };
static const char *CAPS[] = { "lockstep", "passthrough" };

int main(int argc, char **argv)
{
    const char *host = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? atoi(argv[2]) : 7788;
    int frames = argc > 3 ? atoi(argv[3]) : 10;
    int timeout = argc > 4 ? atoi(argv[4]) : -1;
    char js[4096]; int n; sa_lockstep_t ls; sa_act_t a;
    sa_desc_t d = { "c-fake", 60, 192, 448, 0x7f, CAPS, 2, TABLES, 2 };

    if (!sa_net_connect(host, (uint16_t)port, 2000)) { fprintf(stderr, "connect failed\n"); return 2; }
    n = sa_hello_json(&d, js, sizeof js);
    if (n < 0 || !sa_net_send(SA_MSG_HELLO, (const uint8_t *)js, n)) return 3;
    sa_lockstep_init(&ls, timeout);
    for (uint32_t f = 0; f < (uint32_t)frames; f++) {
        void *r;
        sa_obs_begin(f, SA_PHASE_IN_GAME | SA_PHASE_PLAYER_CONTROLLABLE);
        sa_obs_table_begin(&T_PLAYER);
        r = sa_obs_row(); sa_put_fx(r, 0, 0.0f); sa_put_fx(r, 4, 400.0f); sa_put_u8(r, 8, 0); sa_put_u8(r, 9, 3); sa_put_u16(r, 10, 100);
        sa_obs_table_end();
        sa_obs_table_begin(&T_BULLETS);
        r = sa_obs_row(); sa_put_fx(r, 0, 10.0f);  sa_put_fx(r, 4, 20.0f + f); sa_put_bam(r, 8, 1.5707963f);
        r = sa_obs_row(); sa_put_fx(r, 0, -10.0f); sa_put_fx(r, 4, 30.0f + f); sa_put_bam(r, 8, -1.5707963f);
        sa_obs_table_end();
        int got = sa_lockstep_frame(&ls, f, &a);
        printf("%u %u %u %d\n", f, a.buttons, a.flags, got);
        fflush(stdout);
        if (got < 0) return 4;
    }
    sa_net_send(SA_MSG_BYE, NULL, 0);
    sa_net_close();
    return 0;
}
```

- [ ] **Step 6: 跑测试确认通过**

Run: `pytest tests/test_c_core.py -v` → 2 passed；再 `make -C c test-host` → `test_proto ok`

- [ ] **Step 7: 交叉编译验证**

Run: `make -C c cross-x64 && make -C c cross-x86`
Expected: 两次 `cross … ok`（winsock 分支能过编译；产物不运行）

- [ ] **Step 8: 提交**

```bash
git add c/ tests/test_c_core.py
git commit -q -m "feat(c): 网络 + 锁步 + 假游戏，与 Python 客户端回环通过

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 8: 命令行监听器 + README 收口

**Files:**
- Create: `src/stgagent/__main__.py`
- Modify: `README.md`

**Interfaces:**
- Produces: `python -m stgagent --listen HOST:PORT [--policy square|idle]`：接一个后端，`idle` 每帧回 0，`square` 走方框 + 持续射击（每 60 帧换方向：右、下、左、上），每 60 帧打印一行 `frame=… player=(x,y) bullets=N phase=0x..`。这就是 th06nc / th18 桥实机验收时对面跑的东西。

- [ ] **Step 1: 实现**

```python
"""python -m stgagent --listen 0.0.0.0:7788 --policy square"""
import argparse
from . import Listener, BTN_SHOT, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT
from .fixed import fx_to_float

_SQUARE = [BTN_RIGHT, BTN_DOWN, BTN_LEFT, BTN_UP]


def policy_square(o):
    return BTN_SHOT | _SQUARE[(o.frame // 60) % 4]


def policy_idle(o):
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--listen", default="0.0.0.0:7788")
    ap.add_argument("--policy", choices=["square", "idle"], default="square")
    a = ap.parse_args()
    host, port = a.listen.rsplit(":", 1)
    pol = policy_square if a.policy == "square" else policy_idle
    lis = Listener(host, int(port))
    print(f"listening on {host}:{lis.port}")
    while True:
        s = lis.accept()
        print(f"backend {s.hello.backend} connected; tables={[t.name for t in s.hello.tables.values()]}")
        for o in s:
            if o.frame % 60 == 0:
                p = o.tables["player"][0]
                print(f"frame={o.frame} player=({fx_to_float(int(p['x'])):.1f},{fx_to_float(int(p['y'])):.1f}) "
                      f"bullets={len(o.tables.get('bullets', ()))} phase={o.phase:#x}")
            s.send_act(o.frame, pol(o))
        print("backend left")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: 手工验证**

Run（两个终端）：`python -m stgagent --listen 127.0.0.1:7788` 与 `c/build/fake_game 127.0.0.1 7788 180 -1`
Expected: 监听端打印 `frame=0 … frame=60 … frame=120 …` 三行后 `backend left`；假游戏退出码 0。

- [ ] **Step 3: README 补齐用法段并提交**

```bash
git add -A && git commit -q -m "feat: 命令行监听器（square/idle 策略）+ README

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

## 自检记录

- **Spec 覆盖**：§4.1 帧格式（Task 3/6/7）、§4.2 HELLO（Task 4/6）、§4.3 OBS 与 phase（Task 4/6）、§4.4 ACT 与动作位（Task 1/3/6）、§4.5 CTRL（Session 收集事件；后端侧 v1 不解释，spec 允许）、§4.6 同步五条（Task 7 的 lockstep：迟到丢弃、超前断开、超时沿用；`PASSTHROUGH` 由加载器读 `flags`）、§5.7 核心与加载方式解耦（`c/` 不含任何游戏或加载器代码，`SA_POSIX` 双目标）、§8 协议仓两行测试（往返 + dtype；跨后端契约测试要等第二个真后端的 HELLO 夹具，留到 th06nc 桥计划）。
- **偏离 spec 一处**：spec §5.1 把 `core/` 画在 `mods/th18.v1.00a/agent-bridge/native/` 下；本计划把它放进协议仓 `c/`，因为它要被 th06nc（先做）与 th18 共用，跨 repo 共享的东西放独立仓才不用复制。spec 已同步改。
- **类型一致性**：`sa_table_t.fields` 是 `const sa_field_t*`，测试与假游戏都用静态数组初始化；`sa_lockstep_frame` 返回值语义（1/0/-1）在 Task 7 测试的第四列直接断言。
