# TH16 ANM — 想要的 Ghidra 重命名（待 T10 统一落库）

> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>
> **这份文件是追加式的。** 每个 R 任务把自己想要的命名追加到末尾，由 T10 用 MCP 一次性落库，
> 再跑 `tooling/ghidra/symbols.py export th16`。**别在这里删别人的段落。**

---

## 批次 R1 · ANM v8 文件格式（来自 [`01-format.md`](01-format.md)）

### 函数改名

| 地址 | 现名 | 建议名 | 依据 |
| --- | --- | --- | --- |
| `0x46d0c0` | `AnmLoaded::load_one_script` | `AnmLoaded__preread_external_png_for_entry` | 它不碰 script，只在 `has_data==0` 且名字非 `@` 时预读同名 png |
| `0x46d1c0` | `FUN_0046d1c0` | `AnmLoaded__parse_all_entries` | 遍历 entry 链表调 `0x46d3b0`，收尾预跑每个 script 模板 |
| `0x46d3b0` | `FUN_0046d3b0` | `AnmLoaded__parse_one_entry` | 解析单个 entry：建贴图 + sprite 表 + script 表 |
| `0x46c920` | `FUN_0046c920` | `AnmTextureSlot__create_from_png_memory` | D3DX 从内存解码 png，按 `x/y/w/h` 裁剪 |
| `0x46cb60` | `FUN_0046cb60` | `AnmTextureSlot__create_from_thtx` | 从 THTX 块建贴图 |
| `0x46cd30` | `FUN_0046cd30` | `AnmTextureSlot__create_blank` | `@` 名字走这里，建空贴图，返回字节数 |
| `0x46cd80` | `FUN_0046cd80` | `AnmTextureSlot__create_blank_screen_sized` | `@R` 名字走这里 |
| `0x46d8a0` | `FUN_0046d8a0` | `AnmLoaded__store_runtime_sprite` | 写 `+0x11c` 的 0x44 字节运行时 sprite 并算 UV |
| `0x46d690` | `FUN_0046d690` | `AnmManager__reload_or_free_pending_anms` | 遍历 31 个槽，按标志重建或释放 |

### 数据符号

| 地址 | 建议名 | 依据 |
| --- | --- | --- |
| `0x491b90` | `ANM_FORMAT_BYTES_PER_PIXEL`（`int[9]`） | `0x46cccb` / `0x46cd10` 按 format 索引取 bpp |
| `0x491bb4` | `ANM_FORMAT_TO_D3DFORMAT`（`int[9]`） | `0x46cc45` 按 format 索引取 `D3DFORMAT` |
| `0x494280` | `STR_ERR_ANM_BAD_VERSION` | version ≠ 8 的错误串（Shift-JIS） |
| `0x494244` | `STR_ERR_ANM_LOAD_FAILED` | 读文件失败 |
| `0x4942a0` | `STR_ERR_ANM_TEXTURE_LOAD_FAILED` | 外部贴图读不进来 |
| `0x4942e8` | `STR_ERR_ANM_TEXTURE_CREATE_FAILED_NAMED` | 建贴图失败（带 `%s`） |
| `0x49432c` | `STR_ERR_ANM_TEXTURE_CREATE_FAILED` | 建贴图失败（无名） |
| `0x4d9d2c` | `RESOLUTION_X` | `0x46d441` 写进 `entry+0x0a` |
| `0x4d9d30` | `RESOLUTION_Y` | `0x46d44c` 写进 `entry+0x0c` |
| `0x4d9d34` | `SCREEN_COORD_SCALE` | `0x46cbc6` 与 `2.0f` 比较决定是否降采样 |

### 结构体（建议建；本批只给字段图，不强求 T10 一次建完）

`AnmEntryHeader`（0x40 字节，见 [`01-format.md`](01-format.md) §3）、
`AnmSpriteRecord`（0x14）、`AnmScriptTableEntry`（8）、`AnmInstrHeader`（8）、
`ThtxHeader`（0x10）、`AnmTextureSlot`（0x18）。
`AnmLoaded` 已知字段：`+0x108`/`+0x10c`/`+0x110`/`+0x114`/`+0x118`/`+0x11c`/`+0x120`/`+0x124`/`+0x130`，
总大小 `0x13c`（`0x46cf80` 的 `operator_new`）。

### 注释（`set_comment`）

| 地址 | 注释 |
| --- | --- |
| `0x46d404` | ANM v8 版本闸门：只接受 version==8，无旧版本分支 |
| `0x46d4b4` | sprite 偏移表硬编码在 entry+0x40 → header 固定 0x40 字节 |
| `0x46d5c8` | script 表每项 8 字节 {i32 id; u32 offset}；id 被跳过，按位置索引 |
| `0x46cccb` | 源行距 = THTX.width * bpp[THTX.format]，与 entry.format 无关 |
