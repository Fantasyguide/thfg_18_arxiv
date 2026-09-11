# AI 代打：观察/动作协议（stg-agent-proto）与 TH18 桥 —— 设计

> **版本**：跨项目、跨版本。本文出现的游戏地址一律带前缀（`th18:0x…`），全部指 TH18 v1.00a
> （`th18.exe`，md5 `9969cac756098c1da05a81de45437a70`，imagebase `0x400000`）。
>
> **状态**：设计定稿 2026-09-10。决策见 §2，落地计划另出（`writing-plans`）。

## 0. 一句话结论

做一条**与游戏实现无关的「观察 → 动作」协议**，TH18 和隔壁 `stg-engine` 各接一个后端，
同一个 Python 客户端、同一个模型两边通用：在 `stg-engine` 上 headless 高速训练或调算法，
到真 `th18.exe` 上验证。TH18 侧只需要**一个 thcrap 断点**（`th18:0x462966`）：
它所在的位置既是上一帧世界的终态（读观察），又是本帧输入进入引擎前的最后一站（写动作），
且写进去的动作会**原样录进 replay**。

## 1. 存量对齐：为什么这件事比看起来小

| 发现 | 出处 | 对设计的意义 |
| --- | --- | --- |
| TH18 弹幕区 384×448，x 以中轴为 0、y 以顶边为 0 | `mods/th18.v1.00a/mouse-control/TARGET.md` §几何（钳位 `±0x5c00` / `[0x1000,0xd800]` 反推） | 与 `stg-engine` 的 `FIELD_HALF_W=192` / `FIELD_HEIGHT=448`（`crates/stg-core/src/world.rs:100`）**同一坐标系**。协议不做坐标换算 |
| 一帧内的输入顺序：`INPUT_HELD = 原始输入` → `call Input__compute_edges`（`th18:0x462966`）→ 写 replay 流 → Player 读 | `mouse-control/AUDIT.md` §B（一手，已实跑） | 在那条 `call` 之前整体改写 `INPUT_HELD`，边沿正确、录进 replay、玩家对象零写入 |
| 该断点位于 `ReplayManager__on_tick__record_replay`（`th18:0x462940`）；回放模式走另一条 `th18:0x462a50` | 同上 | 看录像时桥自动失效，不会双重驱动 |
| 抽取所需的池 / 链表 / 字段偏移绝大多数已有一手记录 | §5.2 的表逐条列出处 | 引擎层只剩三处缺口（§7） |
| `stg-engine` 已规划 `env.step(actions) -> obs`，动作空间 = `ActionInput`；`stg-harness serve` 已有线格式 v1 与 `BTN_*` 词表 | `design_doc.md` §8、`crates/stg-harness/src/viewer.rs`、`crates/stg-core/src/input.rs` | 协议是这套东西的推广，不另起炉灶 |
| thcrap 插件骨架（版本守卫 / `thcrap_bp.h` / Makefile / `check_constants.py` / 交叉编译链路）已两次实跑 | `mods/th18.v1.00a/mouse-control`、`runtime-probe` | TH18 后端直接复用，不新造基础设施 |

## 2. 决策记录

| # | 决策 | 理由 | 被否的选项 |
| --- | --- | --- | --- |
| D1 | 首个消费者 = 算法 / RL **锁步** | 训练与评估都要「每帧等动作」的确定性；LLM 异步只是超时参数不同（§4.6） | LLM 优先（需要额外的文字摘要层，推后） |
| D2 | 协议规范 + Python 参考客户端放**独立小仓** `stg-agent-proto` | 两个项目都是它的消费者；像 `live-anm` 一样按格式版本独立演进 | 放 `stg-engine`（renkolab 反向依赖游戏引擎仓）；放 renkolab（stg-engine 反向依赖逆向仓） |
| D3 | 线上数值 = **Q16.16 定点（i32）+ BAM 角（u16）**，与 `stg-engine` I1/I2 同型 | `stg-engine` 零转换直出；TH18 只在 DLL 里转一次，±512 px 范围内 float32 → Q16.16 不丢精度；整数流可哈希、可对拍 | f32 + 弧度（TH18 直出更省事，但 AI 侧轨迹不能位级复现） |
| D4 | 传输 = TCP、长度前缀二进制；**AI 侧监听，游戏侧主动连** | AI 大概率在 Linux 机（GPU），游戏在 Windows 机；连不上即直通，游戏不受影响 | 共享内存（只能同机）；UDP（要自己做可靠性） |
| D5 | 同步模型只有一种：**锁步 + 超时** | 超时 = ∞ 即纯锁步，= 0 即异步「有新动作就用」；一套机制覆盖 D1 两种消费者 | 两套模式（多一倍状态机） |
| D6 | TH18 后端 = thcrap 插件 + 单断点，网络 I/O 跑在游戏线程 | 锁步本来就要游戏等；读写同线程无竞态（`mouse-control/AUDIT.md` §B 已确认断点只在游戏线程跑） | 外部进程 `ReadProcessMemory` + `SendInput`（撕裂读、与帧不对齐、replay 不一致）；插件内独立线程 + 共享内存（同机限制） |
| D7 | 观察布局由握手时的 **schema 描述**，客户端不写死 | 两个后端字段集合不同（TH18 有金钱 / 卡牌，stg-engine 有时停）；加字段不破坏旧客户端 | 固定布局 + 协议版本号硬切 |
| D8 | 动作词表以 `stg-engine` 的 `BTN_*` 为准并延伸 | 0–6 已冻结（回放格式的一部分）；TH18 特有的 C / D 键占新位 | 以 TH18 的 `INPUT_HELD` 位义为准（引擎每作可能变） |

## 3. 架构

```
                 stg-agent-proto（独立仓）
                 ├── SPEC.md          线格式 v1（本文 §4 的规范化版本）
                 └── python/stgagent/ 参考客户端：Listener / Session / numpy 解码 / Gym 风格 Env
                                 ▲ TCP（AI 侧监听）
          ┌──────────────────────┼──────────────────────────┐
          │                      │                          │
  TH18 后端                        │                    stg-engine 后端
  mods/th18.v1.00a/agent-bridge/    │                    stg-harness agent-serve
  thcrap 插件 DLL + 1 个断点         │                    view() → OBS，ACT → InputFrame
  读 zPlayer/zBullet/zEnemy/…       │                    额外能力：reset / seed / 快照
  写 INPUT_HELD                     │
          └──────── 同一份 HELLO schema 的 Tier 0 字段名与类型 ──────────┘
```

数据流每帧一次往返：后端发 `OBS(n)`，客户端回 `ACT(n)`，后端把它写进本帧输入。
帧号是唯一同步锚。

## 4. 协议 v1

### 4.1 帧格式

每条消息：`u32 length`（小端，不含自身）+ `u8 type` + payload。

| type | 名 | 方向 | payload |
| --- | --- | --- | --- |
| 1 | `HELLO` | 后端 → 客户端 | UTF-8 JSON（§4.2） |
| 2 | `OBS` | 后端 → 客户端 | 二进制（§4.3） |
| 3 | `ACT` | 客户端 → 后端 | 二进制 12 字节（§4.4） |
| 4 | `CTRL` | 双向 | UTF-8 JSON（§4.5） |
| 5 | `BYE` | 双向 | 空 |

连接建立后后端先发 `HELLO`；客户端可回 `CTRL` 调参；之后进入逐帧循环。

### 4.2 `HELLO`

```json
{
  "proto": 1,
  "backend": "th18.v1.00a",            // 或 "stg-engine@<ENGINE_VER>"
  "tick_hz": 60,
  "field": {"half_w": 192, "height": 448, "origin": "center-top"},
  "number": {"fx": "q16.16-i32-le", "angle": "bam-u16-le"},
  "actions": [{"bit": 0, "name": "UP"}, …, {"bit": 8, "name": "CARD_USE"}, …],
  "caps": ["lockstep", "passthrough"],  // stg-engine 另有 "reset", "seed", "snapshot", "unthrottled"
  "tables": [
    {"id": 1, "name": "player",  "cap": 1,    "stride": 28, "fields": [{"name": "x", "type": "fx", "off": 0}, …]},
    {"id": 2, "name": "bullets", "cap": 2000, "stride": 30, "fields": [ … ]},
    …
  ]
}
```

- `actions` 只列该后端**支持**的位。客户端对不支持的位写 1 是协议错误，后端忽略并计数。
- `tables` 是 `OBS` 的解码字典。客户端按名字取字段，不按偏移写死。
- 字段类型集合：`fx`（i32 Q16.16）、`angle`（u16 BAM）、`i32`、`u32`、`u16`、`u8`。

### 4.3 `OBS`

```
u32 frame            后端帧号（TH18 = TIME_IN_STAGE；stg-engine = World.frame()）
u32 phase            局面相位掩码（§4.3.1）
u8  table_count
表 × table_count：
    u8  table_id
    u16 count
    count × stride 字节（stride 与字段布局由 HELLO 给出）
```

count 上限由 HELLO 的表描述给出（`"cap"`），客户端据此预分配。

#### 4.3.1 `phase` 位

| 位 | 名 | 含义 | TH18 来源 |
| --- | --- | --- | --- |
| 0 | `IN_GAME` | 游戏线程存在 | `th18:0x4cf2e4 != 0` |
| 1 | `PAUSED` | 暂停菜单开着 | `[th18:0x4cf40c] + 0x1ec != 0`（`engine/card/th18/05` §3） |
| 2 | `DIALOGUE` | 对话中 | `GUI(th18:0x4cf2e0) + 0x1b0 != 0` |
| 3 | `SHOP` | 商店开着（敌 / 弹 / GUI 冻结） | `th18:0x4cf2a4 != 0` |
| 4 | `BOMB_ACTIVE` | 放炸弹中 | `BOMB_MGR(th18:0x4cf2b8) + 0x30 != 0` |
| 5 | `SPELL_ACTIVE` | 符卡进行中 | `SPELLCARD(th18:0x4cf2c0) + 0x78 & 1` |
| 6 | `PLAYER_CONTROLLABLE` | 自机处于可操作态 | `player + 0x476ac` 状态机取值（§7 G4 待定哪些值算可操作） |
| 7 | `REPLAY_PLAYBACK` | 回放模式（后端不注入） | 走到 `th18:0x462a50` 那条 tick 时为真；v1 的 TH18 桥在此模式下根本不发 OBS |

stg-engine 后端按其 `PlayerState.life_state` / `freeze_left` / `boss_ui` 填同名位；没有的位恒 0。

#### 4.3.2 Tier 0 表（两个后端都必须提供）

字段名与类型两边**逐字相同**，这是「同一模型两边通用」的实际契约；顺序与 stride 可不同。

**`player`**（count = 1）

| 字段 | 类型 | TH18 来源 | stg-engine 来源 |
| --- | --- | --- | --- |
| `x`, `y` | fx | `player+0x62c/+0x630`（1/128 px 定点）`<< 9` —— **精确转换，不经 float** | `PlayerState.x/y` |
| `hit_radius` | fx | `player+0x4799c`（聚焦 `+0x479a0` 同值） | `hit_radius` |
| `focus` | u8 | `player+0x476cc` | `input & BTN_SLOW` |
| `state` | u8 | `player+0x476ac`（原值透传） | `life_state` |
| `lives`, `bombs` | u8 | `th18:0x4ccd48` / `th18:0x4ccd58` | `lives` / `bombs` |
| `life_frags`, `bomb_frags` | u8 | `th18:0x4ccd4c` / `th18:0x4ccd5c` | `life_pieces` / `bomb_pieces` |
| `power` | u16 | `th18:0x4ccd38` | `power` |
| `score` | u32 | `th18:0x4cccfc` | `score`（截 u32） |
| `graze` | u32 | `th18:0x4ccd0c` | `graze` |

**`bullets`**（TH18 cap 2000，stg-engine cap 8192）

| 字段 | 类型 | TH18 来源 |
| --- | --- | --- |
| `x`, `y` | fx | `bullet+0x638/+0x63c` |
| `vx`, `vy` | fx | `bullet+0x644/+0x648` |
| `speed` | fx | `bullet+0x650` |
| `angle` | angle | `bullet+0x654`（弧度 → BAM） |
| `radius` | fx | `bullet+0x658`，若 `flags & 0x40` 再乘 `bullet+0xf38` |
| `flags` | u8 | bit0 = 参与碰撞（`flags & 0x2`）、bit1 = 圆形判定（`& 0x10`）、bit2 = 已擦（`& 0x4`） |
| `state` | u8 | `bullet+0xf68`（0 空槽不发；3 = 消弹中照发，让 AI 自己决定忽略） |
| `type` | u16 | `bullet+0xf98` |

池：`BULLET_MGR(th18:0x4cf2bc) + 0x108c` 起 `0x7d0` 槽，stride `0xfa0`（ExpHP `zBulletManager.bullets`；`engine.h` 的 `CE_BM_BULLETS=0xec` 是前一槽的哨兵，扫描从 `0x108c` 起）。

**`enemies`**（TH18 cap 256）

| 字段 | 类型 | TH18 来源 |
| --- | --- | --- |
| `x`, `y` | fx | `data.abs_pos.pos` = `enemy + 0x122c + 0x88` |
| `hurt_w`, `hurt_h` | fx | `data.hurtbox_size` `+0x110`（自机弹打得中的框） |
| `hit_w`, `hit_h` | fx | `data.hitbox_size` `+0x118`（撞自机的框） |
| `hp`, `hp_max` | i32 | `data.life.current/maximum` = `enemy+0x6220/+0x6224` |
| `flags` | u16 | bit0 = boss（`enemy_id` 命中 `ENEMY_MGR+0x48` 四槽）、bit1 = 无判定（`data.no_hitbox_timer.current > 0`，`+0x5104+4`）、bit2 = 无敌（`invuln_timer`，`+0x50f0+4`）、bit3 = 符卡段（`data.life.is_spell`） |
| `id` | u32 | `enemy+0x6830` |

遍历 `ENEMY_MGR(th18:0x4cf2d0) + 0x18c` 链表；**跳过** `parent_enemy_id == 0` 且子程序名以 `main`/`Main` 开头的关卡脚本敌人（`card-expand/native/cards/lolk.c` 的识别法）。

**`spell`**（count = 1；**可选**：后端定位不到就整表不发，或全 0。客户端不得假设它存在）

| 字段 | 类型 | TH18 来源 |
| --- | --- | --- |
| `active` | u8 | `SPELLCARD+0x78 & 1` |
| `id` | i32 | `SPELLCARD+0x74` |
| `time_left_frames` | u32 | `GUI+0x1b8` 秒 × 60 + `GUI+0x1bc` 厘秒 × 60 / 100 |
| `bonus` | i32 | `SPELLCARD+0x7c` |

#### 4.3.3 Tier 1 表（TH18 补齐 §7 缺口后加；stg-engine 已有）

- **`lasers`**：`type u8`（`laser+0x14`）、`state u8`（`+0x10`）、`x, y`（`+0x54`）、`angle`（`+0x6c`）、`length`（`+0x70`）、`width`（`+0x74`）、`speed`（`+0x78`）、`flags u8`。遍历 `LASER_MGR(th18:0x4cf3f4) + 0x794` 链表。
- **`laser_nodes`**：曲线激光的折线，`laser_index u16, x, y`（`zLaserCurveNode` stride `0x20`，pos 在 `+0`）。
- **`items`**：`x, y, type u8, state u8`。偏移待 §7 G2。
- **`shots`**（自机弹）：`x, y, damage i32`。`player + 0x620 + 0xf50` 起 512 槽（`engine.h` §自机弹池）。

### 4.4 `ACT`

```
u32 frame       必须等于最近一条 OBS 的 frame，否则丢弃
u32 buttons     动作位（§4.4.1）
u32 flags       bit0 PASSTHROUGH：本帧不覆盖，放行玩家原始输入
```

#### 4.4.1 动作位与 TH18 映射

| 位 | 名 | 语义 | TH18 `INPUT_HELD` | stg-engine |
| --- | --- | --- | --- | --- |
| 0 | `UP` | 电平 | `0x010` | `BTN_UP` |
| 1 | `DOWN` | 电平 | `0x020` | `BTN_DOWN` |
| 2 | `LEFT` | 电平 | `0x040` | `BTN_LEFT` |
| 3 | `RIGHT` | 电平 | `0x080` | `BTN_RIGHT` |
| 4 | `SHOT` | 电平 | `0x001` | `BTN_SHOT` |
| 5 | `BOMB` | 沿（后端只送电平，边沿由引擎自己算） | `0x002` | `BTN_BOMB` |
| 6 | `SLOW` | 电平 | `0x008` | `BTN_SLOW` |
| 7 | `TIMESTOP` | 沿 | 不支持 | `BTN_TIMESTOP` |
| 8 | `CARD_USE` | 沿 | `0x400`（C） | 不支持 |
| 9 | `CARD_SWITCH` | 沿 | `0x800`（D） | 不支持 |

位义出处：`engine/input/th18/01-key-mapping.md` §2、`mouse-control/native/th18.h`。
`PAUSE`（`0x100`）**故意不开放**：AI 不该能暂停。

### 4.5 `CTRL`

客户端 → 后端（所有后端）：`{"timeout_ms": 0..∞}`、`{"passthrough": true|false}`、
`{"tables": [1,2,3]}`（订阅子集，省带宽）。
stg-engine 额外：`{"reset": {"seed": 123}}`、`{"unthrottled": true}`（不按 60 Hz 节流，收到 ACT 就步进）。
后端 → 客户端：`{"event": "stage_start"|"stage_end"|"game_over"}`（v1 仅 stg-engine 实现；TH18 靠 `phase` 位自行判断）。

### 4.6 同步语义（锁步 + 超时）

1. 后端在帧边界发 `OBS(n)`，然后阻塞收包直到：收到 `frame == n` 的 `ACT`（用它），或超时（沿用上一帧 `buttons`）。
2. `frame < n` 的 `ACT` 是迟到包，丢弃并继续等；`frame > n` 是协议错误，断开。
3. 超时默认 ∞（纯锁步）。异步消费者设 0：不等，取「上一次收到的最新 ACT」。
4. 未连接 / 断开 / `PASSTHROUGH`：后端不覆盖输入，游戏照常由玩家操作。
5. TH18 `phase` 里 `IN_GAME` 为 0 的帧根本不会走到断点（断点在游戏线程的 tick 里），客户端在菜单期间收不到任何 OBS，这是**预期行为**。

### 4.7 带宽与延迟

满池 2000 弹 × 30 B ≈ 60 KB/帧，60 Hz ≈ 3.6 MB/s，局域网无压力。`TCP_NODELAY` 必开。
锁步往返在同一局域网约 0.3–1 ms，远小于 16.7 ms 帧预算。

## 5. TH18 后端：`mods/th18.v1.00a/agent-bridge/`

### 5.1 结构（复用 mouse-control 骨架）

```
agent-bridge/
├── README.md / TARGET.md / AUDIT.md      同 mouse-control 三件套
├── native/
│   ├── th18.h              全部死绑量（地址 / 偏移 / 位义）集中一处
│   ├── thcrap_bp.h         断点 ABI（照抄）
│   ├── dll_main.c          入口 / 版本守卫 / 日志 / 读 agent.cfg（host:port、timeout）
│   ├── bp_agent.c          BP_agent_frame：调 th18 抽取器 → 桥核心一帧 → 写 INPUT_HELD
│   ├── extract_th18.c      th18 专属：读池 / 链表，填进核心的中立记录
│   ├── core/               ★ 与加载方式、与作品无关的桥核心（见 §5.7）
│   │   ├── proto.c/.h      HELLO 生成、OBS 编码（float → Q16.16、弧度 → BAM）、ACT 解码
│   │   ├── net.c/.h        winsock 连接 / 长度前缀收发 / 断线重连
│   │   └── lockstep.c/.h   发 OBS → 等 ACT → 超时沿用 的状态机
│   ├── th18_agent.def / Makefile / check_constants.py
└── patch/
    ├── patch.js / files.js
    └── th18.v1.00a.js      breakpoints: agent_frame → BP_agent_frame @ 0x462966
```

### 5.2 断点

| 项 | 值 | 依据 |
| --- | --- | --- |
| 地址 | `th18:0x462966` | `mouse-control/TARGET.md` hook 2：`ReplayManager__on_tick__record_replay` 里的 `call Input__compute_edges` |
| `expected` / `cavesize` | `e8 55 82 fc ff` / 5 | 同上；相对 call 挪进 cave 由 thcrap 修偏移 |
| 返回 | 1（放行原指令） | 不改控制流 |

断点体内顺序：**先读后写**。

1. 若未连接：尝试重连（每 60 帧一次），失败即直接返回 1。
2. 编码 `OBS(n)`：此时所有子系统的上一帧 tick 已完成（§7 G1 待验），读到的是一致终态。
3. 发送；按超时策略收 `ACT(n)`。
4. 若 `ACT.flags & PASSTHROUGH` 为 0：`INPUT_HELD = map(buttons)`（**整体覆盖**，不是 OR —— 与 mouse-control 不同，AI 模式下键盘方向与按键全部让位；F9 一键退出）。
5. 返回 1，引擎接着算边沿、写 replay、跑 Player tick。

### 5.3 与 mouse-control 的关系

两者都写 `INPUT_HELD`，**不能同时启用**。agent-bridge 的 patch 在 `patch.js` 里声明与
`th18_mouse` 冲突（thcrap 无冲突机制，靠 README 说明 + DLL 启动时检测 `th18_mouse.dll`
已加载则拒绝初始化）。

### 5.4 版本守卫与死绑登记

沿用两处 `.text` 签名（`th18:0x45b170`、`th18:0x45caa0`）+ 本断点处的 `expected`。
`TARGET.md` 登记换版本必须重取的量：断点三元组、两处签名、§4.3 表里的全部全局地址与偏移、
`INPUT_HELD` 位义、钳位与几何。

### 5.5 replay 安全

写入发生在 `th18:0x463060` 写流之前 → AI 的每一帧输入都进 replay。验收项：用脚本策略
（走方框 + 持续射击）跑一关并存 replay，回放与实跑逐帧位置一致。**这也是「注入点选对了」的
唯一硬证据。**

### 5.6 审计

涉及断点与游戏线程上的网络 I/O，必过 `mods/_template/AUDIT-checklist.md`。额外关注：
无 x87（`-mfpmath=sse -msse2` + 反汇编自检，同 mouse-control）；断点内不分配堆内存
（编码缓冲静态预分配 128 KB）；winsock 阻塞 recv 的超时用 `SO_RCVTIMEO`，不开线程。

### 5.7 桥核心与加载方式解耦（为后续作品留的缝）

`core/` 只依赖 winsock 与 C 标准库，**不知道** thcrap、不知道 th18、不知道自己是 32 位还是
64 位：抽取器把各作的对象读成核心定义的中立记录（字段即 §4.3.2 的表），加载器负责
「什么时候调一帧」。TH18 的加载器是 thcrap 断点；将来 th06nc 的加载器是已经做过一次的
代理 DLL + `Player__OnUpdate` 入口跳板（`mods/th06nc/mouse-control`，x64）。
所以核心从第一天起按 32/64 位两种目标编译并跑同一套夹具测试，
避免日后移植时才发现指针宽度或对齐假设。

## 6. stg-engine 后端：`stg-harness agent-serve`

- 命令：`stg-harness agent-serve --ecl <image> --connect host:port [--seed N] [--unthrottled]`。
- 映射：`world.view()` 五池 → 表（`Fx.raw()` / `Angle` 原样入流，`alive_words` 位扫即 count）；
  `ACT.buttons` → `InputFrame.actions[0].buttons`（位号一致，无需转换）；`phase` 从
  `life_state` / `freeze_left` / `boss_ui` 合成。
- `HELLO.caps` 多 `reset` / `seed` / `snapshot` / `unthrottled`；`CTRL.reset` 走 `World::new(seed)` + 重载 image。
- 它替代不了 M5 的 `stg-py` 进程内零拷贝 env，但先于 M5 提供**跨进程、跨语言、跨后端**同一接口；
  M5 落地时 `stg-py` 直接复用同一份 schema 与 numpy dtype。

## 6b. th06nc 后端：`mods/th06nc/agent-bridge/`（Tier 0 盘点，2026-09-11）

> 本节地址一律 `th06nc:` 前缀，指 Steam New Classic（`th06nc.exe`，md5 `b0cc28ec904e9efce2439ad857c938b5`，imagebase `0x140000000`，x86-64）。

**结论：现有逆向结果够 Tier 0**。四张表的字段全部有一手偏移，只有三处时序 🟡 要在实机上验（§6b.4）。
加载方式沿用 `mods/th06nc/mouse-control` 已经做过的代理 DLL（`VCRUNTIME140_1.dll`）+
`Player__OnUpdate` 入口 13 字节跳板；核心按 §5.7 共用，只加 `extract_th06nc.c` 与 x64 加载器。

### 6b.1 坐标系：一个减法

th06nc 自机 / 弹 / 敌坐标是**关卡区局部、原点在左上角**（出生点 `(192, 384)`，可动区
`(8,16)+(368,416)`，`engine/player/th06nc/01` §2.11），关卡区仍是 384×448。协议的中轴原点只需
`x' = x − 192`，`y` 不动。转换在抽取器里做，HELLO 里 `field` 与 TH18 完全相同。

### 6b.2 Tier 0 字段来源

| 表 / 字段 | th06nc 来源 | 可信度 |
| --- | --- | --- |
| `player.x/y` | `g_Player th06nc:0x1404ff3a0 + 0x7730/+0x7734`（f32） | ✅ |
| `player.hit_radius` | `+0x774c`（1.25，被弹与擦弹共用） | ✅ |
| `player.focus` / `state` | `+0x785c` / `+0x7898`（0 ALIVE / 1 SPAWNING / 2 DEAD / 3 INVULNERABLE） | ✅ |
| `player.lives` / `bombs` | `th06nc:0x1404ff0f0` / `0x1404ff0f1`（u8） | ✅ |
| `player.power` | `th06nc:0x1404f1e88`（u16） | ✅ |
| `player.score` | `th06nc:0x1404f2798`（u64，截 u32） | ✅ |
| `player.graze` | `th06nc:0x1404ff0cc`（关卡内） | ✅ |
| `player.life_frags` / `bomb_frags` | 本作无此概念，恒 0 | — |
| `bullets` 池 | `g_Bullets th06nc:0x1403ec2a8`，640 × `0x620`；活跃数 `th06nc:0x1404e12a8` | ✅ |
| `bullets.x/y` / `vx/vy` / `speed` / `angle` | `+0x30` / `+0x08` / `+0x00` / `+0x40` | ✅ |
| `bullets.state` | `+0x44`（i16：0 空、1 飞行、2–4 出现动画、5 消失） | ✅ |
| `bullets.radius` | `min(grazeSize.x, grazeSize.y) × 0.5`，`grazeSize` 在 `+0x5f4/+0x5f8`（`CalcKillBoxCollision` 的第三实参） | ✅ 字段；🟡 半径公式按碰撞函数取 min，待对一遍 `0x14006a980` |
| `bullets.flags` | bit0 参与碰撞 = `state == 1`；bit1 圆形 = 恒 1（NC 被弹判定是圆）；bit2 已擦 = `+0x618` | ✅ |
| `bullets.type` | 本作无 `type` 字段可直接读，先填 `spriteDesc` 指针低 16 位（`+0x150`）作类别哈希 | 🟡 |
| `enemies` 池 | `g_Enemies th06nc:0x140aa1e98`，256 × `0x10b0`；占用 = `+0xbc` bit7 | ✅ |
| `enemies.x/y` | `+0xb0/+0xb4` | ✅ |
| `enemies.hp` / `hp_max` | `+0x234` / `+0x238` | ✅（`SpawnEnemy` 直写） |
| `enemies.hit_w/hit_h` / `hurt_w/hurt_h` | `+0x2c0/+0x2c4`（`hitboxDimensions`，只有一组，两对字段填同值） | 🟡 weak（只有 opcode 103 一个锚） |
| `enemies.flags` bit0 boss | `+0x230`（`bossId`）≥ 0 | 🟡 weak |
| `enemies.flags` bit3 符卡段 | `spellcardInfo.isActive th06nc:0x140badf60` 全局，逐敌同值 | ✅ |
| `enemies.id` | 槽下标 | — |
| `spell.active` | `th06nc:0x140badf60` | ✅ |
| `spell.id` / `time_left_frames` / `bonus` | 未定位；Tier 0 阶段**不发 `spell` 表**（§4.3.2 可选规则），`SPELL_ACTIVE` 位照发 | ❓ |
| `frame` | `g_BulletManager th06nc:0x1403ec2a0 + 0x04`（`ZunTimer.current`，每帧 +1） | ✅ |
| `phase.IN_GAME` | `g_Player` 已由 `Player__AddedCallback` 初始化且 chain 在跑（钩子被调即为真） | — |
| `phase.BOMB_ACTIVE` | `th06nc:0x140509268` | ✅ |
| `phase.SPELL_ACTIVE` | 同 `spell.active` | ✅ |
| `phase.PAUSED` / `DIALOGUE` | 未定位；钩子在暂停时是否仍被调 = §6b.4 的验证项之一 | ❓ |

Tier 1 里 `lasers` 也已经齐了（`th06nc:0x1404e12b8`，64 × `0x298`：`pos +0x278`、`angle +0x260`、
`width +0x04`、`startOffset/endOffset +0x10/+0x14`、`inUse +0x274`、`state +0x290`），
`items` 池未反（只有 `ItemManager__SpawnItem`）。

### 6b.3 注入

`INPUT_HELD th06nc:0x140a6ec60`，位义（`engine/menu/th06nc/02` §8）：`0x01` 射击、`0x02` 炸弹、
`0x04` 低速、`0x10/0x20/0x40/0x80` 上下左右。与 TH18 的差别只有低速位（`0x04` vs `0x008`），
其余同形。动作词表 `UP..SLOW` 全支持，`CARD_*` / `TIMESTOP` 不支持。

### 6b.4 必须实机验的三条时序 🟡

1. **钩子处的世界是不是终态。** 钩子在 `Player__OnUpdate` 入口，它是 calc 链的一个回调；
   已知 Stage 优先级 6、EnemyManager 9，Player 与 BulletManager 的优先级未读出。若 Player 排在
   弹 / 敌之前，读到的是「上一帧终态」（正好）；若排在之后，读到的是「本帧已更新一半」。
   两种都能用，但**观察语义不同**，必须知道是哪一种。验法：读 `ChainElem` 注册处的优先级，
   或实机记录同一帧内 `bulletCount` 与钩子先后。
2. **炸弹的按下沿在哪算。** `Player__OnUpdate` 自己测 bit1 的按下沿（`engine/player/th06nc/01`
   §2），所以在入口整体覆盖 `INPUT_HELD` 后沿判定应当正确；但「上一帧的位图」存在哪、
   会不会被我们的覆盖污染，要看一次。
3. **replay 录不录我们写的位。** 本作有 replay（`ResultScreen__HandleReplaySaveKeyboard`），
   录制点未定位。TH18 那条「写进去的动作原样录进 replay」在这里**不能承诺**，先按不保证处理。

三条都是读几处代码的事，不是新子系统；放进落地计划的 th06nc 阶段第一步。

## 7. 引擎层缺口（先进 `engine/`，再动 mod）

| # | 缺口 | 落点 | 验法 |
| --- | --- | --- | --- |
| G1 | th18 里 `ReplayManager` 这条 tick 的 UpdateFunc 优先级是否先于所有子系统（th16 的输入采集在 `Window::do_frame` 里、tick 之前；th18 待验 🟡） | `engine/_shared/frame-loop.md` 加 th18 节 | 枚举 `register__on_tick` 调用方，列完整优先级表 |
| G2 | `zItem` 的位置 / 类型 / 状态偏移（ExpHP 只给 5 个成员；`engine/player/th18/02` 已反出物理循环 `th18:0x445bf4` 读 pos 的位置） | `engine/item/th18/01-pool-layout.md`（新子系统目录） | 从 `th18:0x445bf4` 的 `pos += vel` 反推字段 |
| G3 | 激光：直线 / 无限 / 曲线三类的几何字段确认、曲线节点表 `+0x1800` 的长度与有效范围 | `engine/bullet/th18/01-lasers.md` | 对照 `engine/bullet/th16/03-lasers.md` 逐字段在 th18 重取 |
| G4 | `player+0x476ac` 状态机 0–4 各值含义、自机无敌计时器偏移 | `engine/player/th18/01` 补节 | `Player__on_tick__body` `th18:0x45be90` 的 switch 逐 case |

G1 是 Tier 0 的前提；G2–G4 只挡 Tier 1 与 `PLAYER_CONTROLLABLE` 位。

## 8. 测试与验收

| 层 | 测什么 | 怎么测 |
| --- | --- | --- |
| 协议仓 | 编解码往返、schema 驱动的 numpy dtype 生成 | pytest，用两个后端各一份录制的 HELLO+OBS 样本当夹具 |
| 协议仓 | **跨后端契约**：两份 HELLO 的 Tier 0 表字段名与类型逐字相同 | pytest 直接比对 |
| stg-engine 后端 | 确定性：同一 ACT 流跑两遍，OBS 流哈希一致 | harness 集成测试 |
| TH18 后端 | `make check` / `make verify`（死绑量对账、位数、导出名、依赖、无 x87） | 同 mouse-control |
| TH18 后端 | 活进程：`player.x/y` 落在 `[-184,184]×[32,432]`；弹 count 与画面数量级一致 | Python 端断言（继承 runtime-probe 的 IN-RANGE 判据） |
| TH18 后端 | **replay 一致性**（§5.5） | 脚本策略跑一关，存 replay，回放对拍 |
| 端到端 | 同一个脚本策略在两个后端上都能跑完一段而不崩 | 手工 + 日志 |

## 9. 不做的事（v1 边界）

- TH18 侧的 reset / 调速 / 快照：引擎没有这些口，用练习模式与人工重开代替。
- 给 LLM 的文字化局面摘要：客户端适配器，协议不管。
- 画面帧（截图）：协议只送结构化状态。
- 多人 / 双自机：`player` 表 count 恒 1（stg-engine 有 `MAX_PLAYERS`，v1 只送 0 号）。
- 弹道预测、危险场估计：AI 侧的事。
- 在 replay 回放模式下注入。
- **th06nc 后端的 Tier 1**（道具池、激光折线）：Tier 0 之后再说，见 §6b。

## 10. 落地顺序（交给 `writing-plans`）

1. 协议仓：`SPEC.md` + `stgagent` 客户端 + 夹具测试（不依赖任何游戏）。
2. stg-engine：`agent-serve`，先让脚本策略在 headless 上跑通，作为协议的第一个活后端。
3. **th06nc 后端 Tier 0（先于 TH18，2026-09-11 拍板）**：先验 §6b.4 三条时序，再 `extract_th06nc.c` +
   x64 代理加载器（复用 `mods/th06nc/mouse-control` 的跳板与 `check.py`）。`spell` 表按 §4.3.2 的可选规则，
   定位不到就不发。
4. renkolab 引擎层：G1（TH18 必做），G4（顺手）。
5. TH18 后端 Tier 0：DLL + patch + 三件套文档 + 审计 + 活进程验收 + replay 对拍。
6. 引擎层 G2、G3 → TH18 Tier 1。
7. Gym 风格 `StgEnv` 包装（reset 只在有 `reset` 能力的后端可用）。
