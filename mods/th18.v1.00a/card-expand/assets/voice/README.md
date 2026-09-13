# voice/ —— 角色语音（音效表扩容的 32 个新 id）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

面向**写卡的人**：给一张卡加一句语音，只需要放一个 wav、在 `ORDER.txt` 加一行、
在 `voice.js` 写一条，然后代码里 `ce_play_voice(NAME, x)`。DLL 不碰 dat，也不走 thcrap 的文件替换。

## 放什么

| 文件 | 说明 |
| --- | --- |
| `ORDER.txt` | 一行一个 `NAME`，**只追加**。行号 k → 音效 id `0x54+k`、wav 下标 `72+k`。`#` 开头是注释 |
| `<NAME>.wav` | PCM（`fmt` tag 1）。**建议 16-bit 单声道** —— 声像对单声道才有意义 |
| `_src/` | 第三方 / 原始素材（原件 + 出处 README）|
| `make_melodies.py` | 合成本目录的钢琴曲（纯 stdlib，可复现）。`MELODIES` 里一首一条 |
| `convert_voice.py` | 把 `_src/NAME.ogg`（用户给的压缩语音）转成上面要的 wav：`ffmpeg` 44.1 kHz 16-bit 单声道，+6 dB 再软限幅到峰值 −0.5 dBFS（把 RMS 从素材常见的 −15 拉到基准 −10 附近）。`python3 convert_voice.py NAME` 或 `--all` |

`NAME` 只能是字母数字下划线 —— 它要变成 C 宏 `CE_VOICE_<NAME>`。

**上限 32 条**（`native/sound_sites.py` 的 `NEW_N`）。原生 71 个 se 共 5.8 MB；
32 条 × 2 秒 × 44.1 kHz × 16 bit 单声道 ≈ 5.6 MB，同量级，全量常驻内存。

## 索引怎么来

```
ORDER 第 k 行（0 起）  →  音效 id = 0x54 + k,  wav 下标 = 72 + k
```

`make voice` 会打印这张对照表，并生成 `native/voice_ids.h`（`CE_VOICE_<NAME>`）。
**写卡的人不碰数字**：

```c
ce_play_voice(SPADE_10_ACTIVATE, player_x());
```

## `voice.js` 怎么写

`patch/th18/voice.js`（thcrap 会把栈里每个 patch 的这个文件深合并）：

```json
{
  "SPADE_10_ACTIVATE": { "wav": "SPADE_10_ACTIVATE", "id": 84, "volume_db": 0, "priority": 100 }
}
```

| 字段 | 说明 |
| --- | --- |
| key | 给人看的名字，只出现在日志里 |
| `wav` | **`ORDER.txt` 里的 NAME**，决定用哪个文件 |
| `id` | **必填**，= `0x54 + ORDER.txt 行号`（十进制写）。`make voice` 会与 ORDER.txt 对账，不一致直接报错 |
| `volume_db` | cfg 行 `+8` 的低 word：DirectSound 的**百分之一 dB 衰减**，`-5000..0`，缺省 `0`（不衰减）。**只能衰减不能增益** |
| `priority` | cfg 行 `+8` 的高 word，`0..100`，缺省 100 |

⚠️ **没有 `pan`**：声像不在表里，由 `ce_play_voice(NAME, x)` 的 x 参数在运行时算
（消费者 `0x4775d9` 的 `SetPan`）。旧字段 `volume` / `pan` 现在会直接报错。

## 响度

原生 71 个 wav **一律 peak 归一化**（peak 中位 −0.06 dBFS），响度差异全压在 `volume_db` 上，
而它只能衰减 —— **所以想让一个音更响，只能改 wav 本身**。基准（`engine/_shared/th18-sound-table.md` §9）：

| | dBFS |
| --- | --- |
| 原生 wav 的 RMS 中位 | −13.1 |
| 最响的 `se_release`（Tenshi 发动音，与卡牌语音同帧一起响）| −5.1 |
| **语音目标 `VOICE_RMS_TARGET`** | **−10.0**（peak ≤ −0.5）|

`make voice` 会打印每条语音的 peak / rms，偏离基准 ±4 dB 就提醒。
**只做 peak 归一化是不够的** —— 钢琴那种波峰因数 14 dB 的素材，峰值顶满了 RMS 还是只有 −17，
听着就是轻。`make_melodies.py` 用 tanh 软限幅把波峰因数压到 9.5 dB 才够（驱动量二分求解到目标 RMS）。

★ **id 显式写死、不靠顺序**：thcrap 会把栈里每个 patch 的 `voice.js` **深合并**成一个对象，
合并后的迭代顺序不由我们决定，所以 DLL 是按 `id` 定位 cfg 行的。`make voice` 负责保证
`id` 与 `ORDER.txt` 的行号一致 —— 写错了构建就停，不会带着错的索引出包。

## 跑什么

```bash
cd ../../native
make voice          # 校验 + 拷进 patch/th18/voice/ + 生成 voice_ids.h
python3 ../assets/build_voice.py --check    # 只校验
```

## 边界

- **语音就是 SE**：可叠加、跟随游戏的 SE 音量（`0x5704ac`）、不做独占通道、不打断、
  过关不停。设计决定见 `docs/superpowers/specs/2026-09-05-voice-expand-design.md` §4。
- 引擎的 RIFF 解析不挑格式（原生 71 个就是 44.1k/22.05k × 8/16 bit × 单/双声道混用），
  但 `build_voice.py` 只放行 PCM —— 别的 tag 我们没验过。
- wav 文件随 `_255` patch 分发（`patch/th18/voice/`），`files.js` 收它们的 crc。
现有曲目（`make_melodies.py` 的 `MELODIES`，一首一条，可单独出 `make_melodies.py NAME`）：

| NAME | 状态 | 形状 |
| --- | --- | --- |
| `ROYAL_RAGTIME` | **已登记**（ORDER.txt / voice.js）| 4.60 s。拉格泰姆：左手八分 oom-pah、右手 3+3+2 切分、C7→F 与 G7→C |
| `ROYAL_FANFARE` | 备选，未登记 | 4.41 s。I–IV–V–I 号角 + 结尾洗牌琶音 |

用户提供的语音（`_src/*.ogg` → `convert_voice.py`）：

| NAME | id | 用在 | 长度 / 响度 |
| --- | --- | --- | --- |
| `FIRELORD_SUMMON` | `0x55` | 炎魔之王（72）召唤成功那帧，叠在发动音 `0x4d` 上 | 4.55 s，RMS −10.7 |
| `FIRELORD_ATTACK` | `0x56` | 炎魔之王投火球那帧（火球落地仍是原生 `0x2c`）| 4.29 s，RMS −11.2；周期 8 s > 长度，不自叠 |

`ROYAL_RAGTIME` 与 `ability.anm` script70 的对齐（帧号 = 60 fps；四分音符 30 帧 = 120 BPM 的 2/4，一小节 60 帧）：

| 帧 | 演出 | 音乐 |
| --- | --- | --- |
| 0–59 | 五张黑桃逐张弹出（0/10/20/30/40）| 小节 1：C |
| **60** | 金色横幅 + trophy 音效 | **小节 2 的强拍**，重音拉满；C7 → F |
| 120 | +888 GOLD 已出 | 小节 3：F → G7 |
| 180 | 淡出进行中 | 终奏 C，落进 170–194 的淡出里 |

★ **拉格泰姆用的是风格不是曲子** —— 左手 oom-pah、3+3+2 切分、副属/属七都是通用手法，
不涉及任何人的版权。（《骗中骗》拿 Scott Joplin 配老千牌局之后，这个风格就等于「牌桌」了；
Joplin 1917 年去世，作品本身也早已进入公有领域。）

要换成拉格泰姆：`ORDER.txt` 与 `voice.js` 各把 `ROYAL_FANFARE` 改成 `ROYAL_RAGTIME`，
`royal.c` 里的 `ce_play_voice(ROYAL_FANFARE, …)` 同改，`make voice && make dll`。

## `files.js` 与分发

`patch/th18/voice/*.wav` 是构建产物（`make voice` 从 `assets/voice/` 拷过去），在**本仓**
gitignored，所以入库的 `patch/files.js` **不会**列出语音 wav。素材本身（`assets/voice/*.wav`）入库
—— 除非是原生 wav，那种要单独 gitignore（renkolab 不留版权字节，同 `abcard.anm` 的政策）。
`make dist` / `make release` 会对 `dist/patch-step3/` 重新生成 files.js，那份**会**列出它们
—— 语音随 modkit 发布。thcrap 本地解析文件不看 files.js（那是更新/下载机制用的），
所以本地开发时 wav 不在 files.js 里也照样能播。
