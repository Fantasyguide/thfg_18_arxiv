# card-expand —— patch 生成器、行为 DLL、资源与验证入口。
ROWS ?= 255
PATCH := ../patch/th18.v1.00a.js

.PHONY: all quick check package gen verify files clean test-host anm anm-verify atlas-check sht sht-verify ecl ecl-ufo snd-check voice dll dllx87 dllverify

all: quick dll

## quick —— 日常开发反馈：主机 C 单测 + 可导入的 Python 单测。
quick: test-host
	python3 -m pytest -q

## step1 —— 已退役（2026-09-07）：58 行、行为零变化的第 1 步交付态不再进 dist / modkit。
##          真要复现那一态做回归对分：make ROWS=58 gen verify dll files（仓库里 patch/ 提交的是 255 行 = step3）。

## step3 —— 战线 B 验证态：255 行 + 分配器搬迁 + patch-test（空槽→id 58 + 追踪断点）
step3: anm sht
	$(MAKE) ROWS=255 gen verify test-host voice dll dllx87 files

## check —— 提交前完整只读门：代码、资源、站点、文档和 DLL ABI。
##          ★ `tools/engine_abi_check.py`（2026-09-14 加）：对账「我们声明的调用约定」与
##            「真 exe 里那个函数的 `ret N`」。加它的缘由是一次真事故：`0x48a730` 是 `ret 4`
##            而我们按 `fastcall(void*)` 调 ⇒ 每调一次少吃 4 字节栈 ⇒ 进灵界几秒后 `ret` 跳飞
##            （AUDIT.md §CR-5）。无样本/无 capstone 时它自己跳过并 exit 0。
##          ★ `tools/sitecheck.py --all`（2026-09-18 加）：**站点体检**——每个窗口查「≥5 字节 / 整指令边界 /
##            / 窗内无 cave 修不了的近相对分支 / 与所有 binhack·站点零重叠」，并把 `sdk.c` 里所有
##            `ce_retaddr_set()` 落点逐个验「栈中性」。加它的缘由见 AUDIT.md §BE-DW：一个窗口 = 一条
##            `ja rel32` 的站点，进 cave 会跳到「本格 + (原目标−站点)」= 池页没写过的那一段 ⇒ 头号崩溃。
##            **只判红「已声明」的；park 的只提示**（现在没装）。无样本/无 capstone 时自己跳过并 exit 0。
check: quick cards-check anm-verify sht-verify snd-check dllx87 dllverify
	python3 sites.py check
	python3 tools/contract_check.py  # JSON/行为契约（category ⟺ 主动行为；有行为必须在 cards.js 里）
	python3 tools/gen_drift.py      # 生成物漂移：patch/*.js 与 sites_gen.h 必须是 sites.py 现算的那一份
	python3 tools/sitecheck.py --all
	python3 tools/capcheck.py
	python3 tools/engine_abi_check.py
	python3 tools/anm_hud_alpha_check.py
	python3 ../../../../tooling/check-docs.py

## package —— 生成可安装的完整发布包。
package: dist

## snd-check —— 音效表 51 处站点的扫描 + 完整性审计（只读，不产出）
snd-check:
	python3 tests/test_sound_sites.py
	python3 tests/test_sound_emit.py
	python3 tests/test_build_voice.py
	python3 ../assets/build_voice.py --check

## voice  —— 语音资源校验（含 voice.js ↔ ORDER.txt 对账）+ 拷进 patch/th18/voice/ + 生成 voice_ids.h
##           ⚠ 先跑 derive_astralup.py：把 TH13 的 se_astralup.wav 从 local/ 现取到 assets/voice/
voice:
	python3 ../assets/voice/derive_astralup.py
	python3 ../assets/build_voice.py

## voice_ids.h 是生成物：裸 make dll 也要能把它做出来（否则新 clone 缺文件直接停）
voice_ids.h: ../assets/voice/ORDER.txt $(wildcard ../assets/voice/*.wav) ../patch/th18/voice.js
	python3 ../assets/build_voice.py

## anm    —— abcard.anm（../assets/cards/ 新卡图）+ ability.anm（../assets/ability/ 贴图副本 + 特效脚本）
##           + pl0X.anm（../assets/build_player_anm.py：四个文件都是「原生部分逐块不变 + 纯追加」
##             —— pl02/pl03 追加兽灵脚本（AL59）、pl00/pl01/pl03 追加灵界弹（113，AUDIT §CJ），
##             见 pl_anm_ids.h）→ build/
## ability-text —— 原版卡（id < 57）的文案：改 `ability.txt`（逐字节外科手术，源在 local/）。
##   为什么不是 cards.js：`native/text.c` 的文案重定向只覆盖 id >= 57（见 assets/derive_ability_txt.py 的头注）。
ability-text:
	python3 ../assets/derive_ability_txt.py

anm:
	python3 ../assets/build_abcard.py
	python3 ../assets/player_art_src/make_player_art.py   # AL62：玩家兽灵弹源图（补默认 + 切到 18/art-src 供用户改）
	python3 ../assets/build_ability.py
	python3 ../assets/build_player_anm.py

## pl_anm_ids.h 是生成物：pl0X 追加脚本号（append_shooterset.py 会交叉核对灵界那三个）
pl_anm_ids.h: ../assets/build_player_anm.py
	python3 ../assets/build_player_anm.py

## ecl    —— 开发辅助：st01–st06 空壳（道中删除、boss 血量 ÷100、死时多掉钱）→ build/ecl/，单独一个可勾选的 patch（_devstage）
ecl:
	python3 ../assets/ecl/make_dev_ecl.py

## sht    —— 往原生 pl00–03.sht 追加新卡子机的 shooterset → build/sht/；生成 sht_ids.h（APPEND 为空时不产出 .sht）
sht: pl_anm_ids.h
	python3 ../assets/sht/append_shooterset.py

## ecl-ufo —— 112 星莲船的 UFO 本体 ECL（**从 TH12 原样移植** 4 色 + 4 个自计时等价改写 + EtBreak）：
##           把 9 个子程序合进原生 default.ecl → build/ecl/default.ecl（合并后 39 子、二进制往返回验）
##           ⚠ **本目标只产出文件，不发布**：要真生效还需 ① release.py 加一行（映射进 patch-step3）
##             ② TH12 的 `enemy.anm` script135–138 + `enemy/ufo_anim.png` 移植（否则本体能飘/能飞走/能消弹但没贴图）
##           依据与三步原文见 assets/ecl/ufo.ecl.txt 的文件头/文件尾
ecl-ufo:
	python3 ../assets/ecl/make_ufo_ecl.py

## sht_ids.h 是生成物（依赖 anm_ids.h：shooter 的 anm 字段指 ability.anm 的脚本；
## 依赖 pl_anm_ids.h：pl02/pl03 的 anm 字段指 pl0X.anm 追加脚本，两边交叉核对）
sht_ids.h: anm_ids.h pl_anm_ids.h ../assets/sht/append_shooterset.py
	python3 ../assets/sht/append_shooterset.py

## sht-verify —— 只校验 build/sht/*.sht：解析不变式 + 前 23 组逐字节不变
##               + 独立回读 pl0X.anm/sht：sprite 越界、sht 的 anm/hit_fx 必须存在、主路径无 delete
sht-verify:
	python3 ../assets/sht/append_shooterset.py --verify-only
	python3 ../assets/check_player_anm_bounds.py

## anm-verify —— 只校验：entry 数 / 原贴图逐张一致 / 新 entry 字段 / JSON 索引 / pl0X.anm 的原生部分逐字节一致 + sprite/script 硬校验
anm-verify:
	python3 ../assets/build_abcard.py --verify-only
	python3 ../assets/build_ability.py --verify-only
	python3 ../assets/check_ability_anm_bounds.py
	python3 ../assets/build_player_anm.py --verify-only

## atlas-check —— 图集的**独立**对账（只读，人工/CI 可选，**故意不进 `check`**）：
##   ① ORDER.txt 的图集段 vs `ATLAS-MERGE-AUDIT.md` §4.3 的成员明细**逐格**比；
##   ② 自己画一遍画布（与 build_ability.py 是两条独立代码路径）再按矩形裁出来 `tobytes()` 比；
##   ③ 预览图落到 build/atlas-preview/。
##   为什么不进 `check`：它读一份**文档**（审计报告）⇒ 文档一改就红；而"图集逐像素自检"这条
##   更硬的检查已经在 `anm-verify` 里（`build_ability.py --verify-only`，从编译好的 anm 回读再比）。
atlas-check:
	python3 ../assets/ability/make_atlas.py

## cards-check —— cards.js 静态校验（desc<=6 行 / 每行<64 字节 / 无 ASCII %）。
## ★ 为什么要有：DLL 的自检在**运行时**逐卡查，任何一张不合规就「整批新卡禁用」（玩家看到「卡全没了」）。
## 2026-09-09 真踩过（119 的 desc 写了 7 行）→ 挪到构建期，红着退出。
cards-check:
	python3 mkfiles.py --check

## list   —— 25 个查表实例的全表
list:
	python3 sites.py list

## gen    —— 生成 patch（ROWS=58 是「行为零变化」的第 1 步）
gen:
	python3 sites.py gen --rows $(ROWS) -o $(PATCH)

## verify —— 把已生成的 patch 拿回真 exe 逐条对账
verify:
	python3 sites.py verify

## files  —— 刷新 patch/ 与 patch-test/ 的 files.js（crc32 清单，递归子目录）
files:
	python3 mkfiles.py

clean:
	rm -f $(PATCH)

## conflicts —— 与同一 patch 栈里其它 patch 求交集（base_tsa 要拿装机上的文件）
##   make conflicts OTHERS="<thcrap>/repos/nmlgc/base_tsa/th18.v1.00a.js"
conflicts:
	python3 sites.py conflicts $(OTHERS)

# ---- DLL：全有或全无的门（BP_ce_gate 里填表 + 回读验证 100 处）----
# TOOLCHAIN 显式指定时优先；否则先查 PATH，最后兼容当前开发机的默认安装目录。
TOOLCHAIN ?=
DEFAULT_TOOLCHAIN := $(HOME)/Tools/llvm-mingw-20260826-msvcrt-ubuntu-22.04-x86_64
ifeq ($(strip $(TOOLCHAIN)),)
CC      := $(or $(shell command -v i686-w64-mingw32-gcc 2>/dev/null),$(DEFAULT_TOOLCHAIN)/bin/i686-w64-mingw32-gcc)
OBJDUMP := $(or $(shell command -v i686-w64-mingw32-objdump 2>/dev/null),$(DEFAULT_TOOLCHAIN)/bin/i686-w64-mingw32-objdump)
else
CC      := $(TOOLCHAIN)/bin/i686-w64-mingw32-gcc
OBJDUMP := $(TOOLCHAIN)/bin/i686-w64-mingw32-objdump
endif
DLL       := th18_card_expand.dll
# -fno-math-errno：让 sqrtf 编成一条 sqrtss。不加的话它会带一条「负数就调 libm 设 errno」的退路，
# 而 libm 的 float 返回值走 st0，我们这边接它就得用 fstp —— 直接顶掉下面 dllx87 的「零 x87」不变式。
CFLAGS    := -O2 -Wall -Wextra -mfpmath=sse -msse2 -fno-math-errno -I.
LDFLAGS   := -shared -static-libgcc -s -lkernel32 -luser32 -Wl,--no-insert-timestamp

## dll    —— 编 DLL（依赖 gen 出的 sites_gen.h）
dll: $(DLL)
SRCS      := dll_main.c selfcheck_core.c selfcheck.c unlocked.c text.c cards_def.c cards.c menu.c start_deck.c bp_trace.c sdk_core.c sdk_actions.c sdk.c shop_core.c shop.c sound.c $(wildcard cards/*.c)
OBJS      := $(SRCS:.c=.o)
## ⚠ 依赖表必须列全**被 include 的头**：漏一个就会出现「改了头、别的 .o 不重编」的静默错
##   （2026-09-12 实际踩到：`sdk/public.h` 里的 `ce_hooks_t` 尾部加了新槽，而它不在表里
##    ⇒ 别的卡的 .o 不会重编，新的 `sdk.c` 会去读它们那截短的旧结构 = 越界读）。
##   `anm_ids.h` 同理（`sdk.h` include 它，`make anm` 会重写它）。`sdk/*.h` 用通配兜住。
%.o: %.c card_expand.h sites_gen.h thcrap_bp.h cards_def.h sdk.h sdk_core.h engine.h shop_core.h sound.h voice_ids.h sht_ids.h anm_ids.h $(wildcard sdk/*.h) $(wildcard cards/*.h)
	$(CC) $(CFLAGS) -c -o $@ $<
$(DLL): $(OBJS) th18_card_expand.def
	$(CC) $(CFLAGS) -o $@ $(OBJS) th18_card_expand.def $(LDFLAGS)

## test-host —— 纯逻辑（cards_def.c）的主机单测，Linux gcc 直接跑
HOSTCC ?= cc
test-host:
	@mkdir -p tests/build
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_cards_def tests/test_cards_def.c cards_def.c
	./tests/build/test_cards_def
## ★ 2026-09-22：`sdk_core.c` 现在调 `hud_card_shelf_x0()`（货架给原生「当前卡」那块 UI 让位，
##   一手几何在 `cards/hud_dim.{h,c}`）⇒ 所有链 sdk_core.c 的主机单测都要一起链 `cards/hud_dim.c`。
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_sdk_core tests/test_sdk_core.c sdk_core.c cards/hud_dim.c
	./tests/build/test_sdk_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_sdk_actions tests/test_sdk_actions.c sdk_actions.c
	./tests/build/test_sdk_actions
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_selfcheck_core tests/test_selfcheck_core.c selfcheck_core.c
	./tests/build/test_selfcheck_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_public_sdk_compile tests/test_public_sdk_compile.c
	./tests/build/test_public_sdk_compile
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_shop_core tests/test_shop_core.c shop_core.c
	./tests/build/test_shop_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_blue_eyes_core tests/test_blue_eyes_core.c cards/blue_eyes_core.c -lm
	./tests/build/test_blue_eyes_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_broken_core_core tests/test_broken_core_core.c cards/broken_core_core.c -lm
	./tests/build/test_broken_core_core
## ★ 2026-10-02（87 天顶剑）：泰拉瑞亚运动（绕锚点转圈 / 相位寿命 / 透明度）+ 节奏 + 挑最近的敌人。
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_zenith_core tests/test_zenith_core.c cards/zenith_core.c -lm
	./tests/build/test_zenith_core
## ★ 2026-10-02（88 杰拉）：SDK 的「复制代数闸 + 可见数台账」（跨卡共享件；语义 = sdk_core.h 末节）。
##   链 sdk_core.c + cards/hud_dim.c（与 test_sdk_core 同款依赖）。
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_dup_layer tests/test_dup_layer.c sdk_core.c cards/hud_dim.c
	./tests/build/test_dup_layer
## ★ 2026-10-02（89 星星炮）：开火节奏（复刻原版白板 useTime = 12）/ 瞄准 / 星星弹幕
##   （定速直线 + 自转 + 闪烁 + 出界）与命中免疫计时（复刻默认 localNPCHitCooldown = 10）。
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_star_cannon_core tests/test_star_cannon_core.c cards/star_cannon_core.c
	./tests/build/test_star_cannon_core
## ★ 2026-10-02（90 冰淇淋）：miss 之后「火力写回生前值 + 符卡 = 生前 + 3（钳上限）」，
##   以及「快照只在正常态采、同一次 miss 只写一遍」。
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_ice_cream_core tests/test_ice_cream_core.c cards/ice_cream_core.c
	./tests/build/test_ice_cream_core
## ★ 2026-10-03（92 伟大之作）：血量 → 金钱枚数（至少 1 / 至多上限）与「转换量 → 冷却帧数」
##   （底价 + 弹幕数·单价 + 血量·单价，钳上下限）。
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_great_work_core tests/test_great_work_core.c cards/great_work_core.c
	./tests/build/test_great_work_core
## ★ 2026-10-02（91 鞘翅重锤）：时机条状态机（激活帧丢 C / 没敌人光标不动 / 卡准 HIT / 卡歪 MISS /
##   3s 没敌人或总预算到 TIMEOUT / 砸完 RETURN / 缓冲 DONE / 无敌帧声明单调不增）。
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_elytra_mace_core tests/test_elytra_mace_core.c cards/elytra_mace_core.c
	./tests/build/test_elytra_mace_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_firelord_core tests/test_firelord_core.c cards/firelord_core.c -lm
	./tests/build/test_firelord_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_truesilver_core tests/test_truesilver_core.c cards/truesilver_core.c
	./tests/build/test_truesilver_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_guding_core tests/test_guding_core.c cards/guding_core.c
	./tests/build/test_guding_core
## ★ 2026-10-02（86 东尼的悲恸）：速收判据（收取 vs 超时 / 用时 / 中途作废）+ 段边界锁定与释放。
##   链 guding_core.c：槽原语（活动槽 / 死亡段）复用古锭刀那份，不再抄一遍。
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_tony_grief_core tests/test_tony_grief_core.c cards/tony_grief_core.c cards/guding_core.c
	./tests/build/test_tony_grief_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_collect2x_core tests/test_collect2x_core.c cards/collect2x_core.c
	./tests/build/test_collect2x_core
## test_vitem_batch —— ★★ 2026-09-15（NEXT.md §1x.2）：自建实体（110/111 绿点）→ 114 收点批 的
##   **接线级**单测。它**同时**链 sdk_core.c 与 cards/collect2x_core.c（两个模块各自那份单测只链自己），
##   按 cards/faith.c 与 cards/collect2x.c 里同一段代码的形状一帧一帧对跑（成员 +1 / 不进批分 /
##   在飞 presence / 微档不入批 / 114 不在场零副作用 / 帧内顺序无关）。见测试文件头。
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_vitem_batch tests/test_vitem_batch.c sdk_core.c cards/collect2x_core.c cards/hud_dim.c
	./tests/build/test_vitem_batch
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_peashooter_core tests/test_peashooter_core.c cards/peashooter_core.c
	./tests/build/test_peashooter_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_season_core tests/test_season_core.c cards/season_core.c
	./tests/build/test_season_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_lolk_core tests/test_lolk_core.c cards/lolk_core.c
	./tests/build/test_lolk_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_ouroboros_core tests/test_ouroboros_core.c cards/ouroboros_core.c
	./tests/build/test_ouroboros_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_whirlwind_core tests/test_whirlwind_core.c cards/whirlwind_core.c
	./tests/build/test_whirlwind_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_stock_core tests/test_stock_core.c cards/stock_core.c
	./tests/build/test_stock_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_animal_core tests/test_animal_core.c cards/animal_core.c
	./tests/build/test_animal_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_faith_core tests/test_faith_core.c cards/faith_core.c
	./tests/build/test_faith_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_mof_core tests/test_mof_core.c cards/mof_core.c cards/faith_core.c
	./tests/build/test_mof_core
## test_mof_green_core —— ★★ 2026-09-18（110 变绿）：名单表的**不变式**（脚本差 ÷ 5 == 变体差、
##   sprite 差 == 变体差 × 12）+ 判据（只认「此刻真的改上了」的行、只认 enemy.anm）+ 关掩码与逐帧计划 +
##   内存原语（swap 备份 / apply 幂等 / restore 精确还原）+ 运行期同形体检。见 `cards/mof_green_core.h`。
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_mof_green_core tests/test_mof_green_core.c cards/mof_green_core.c
	./tests/build/test_mof_green_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_th06rank_core tests/test_th06rank_core.c cards/th06rank_core.c
	./tests/build/test_th06rank_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_youkai_core tests/test_youkai_core.c cards/youkai_core.c
	./tests/build/test_youkai_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_signal_core tests/test_signal_core.c cards/signal_core.c cards/faith_core.c cards/mof_core.c
	./tests/build/test_signal_core
## test_faith_hud_core —— ★★ 组合 UI（110/111 共用那一块）的纯逻辑 + **与 111 自己那份实现的逐值对账**
##   （行 1 的档/条宽/强度在两边是同一个一手式子；这里链上 signal_core.c 断言恒等）。
##   ★ 2026-09-15h：本块的压暗并进共享件 ⇒ 这条也链 `cards/hud_dim.c`（§H 钉「换算 320」+ 门限）。
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_faith_hud_core tests/test_faith_hud_core.c cards/faith_hud_core.c cards/signal_core.c cards/faith_core.c cards/mof_core.c cards/hud_dim.c
	./tests/build/test_faith_hud_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_trance_core tests/test_trance_core.c cards/trance_core.c
	./tests/build/test_trance_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_ufo_core tests/test_ufo_core.c cards/ufo_core.c
	./tests/build/test_ufo_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_senro_core tests/test_senro_core.c cards/senro_core.c
	./tests/build/test_senro_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_trance_bgm_core tests/test_trance_bgm_core.c cards/trance_bgm_core.c
	./tests/build/test_trance_bgm_core
## test_hud_dim_core —— ★★ 跨卡统一件 `cards/hud_dim.c`（HUD 压暗的唯一实现；动态统一、区域按各卡块矩形）：
##   表面→自机坐标换算（320）/ 进 = 矩形 + 28 的四条边 / 16 px 滞回带（出 = 矩形 + 44）/ 步长与钳位
##   （255↔32、±51）/ pp == NULL / 逐帧连续 / **六块真块的可触发区不变式**。
##   取值 = ❓ 用户口径（UI 统一，**不是复刻**），见 `cards/hud_dim.h` 文件头与 `TUNING.md` §7。
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_hud_dim_core tests/test_hud_dim_core.c cards/hud_dim.c
	./tests/build/test_hud_dim_core
## test_input_synth_core —— ★★ 调试件「逐帧合成方向输入」的纯逻辑（`cards/input_synth.h`；**不需要 sdk.h**，
##   所以主机上直接编，不链任何引擎件）：
##   ① 默认模式 = 左/松/右/松（含松开帧）；② **半周期镜像不变量**（⇒ 整周期净位移精确为 0，任意 N/M）；
##   ③ 位域卫生（只碰四个方向位，绝不碰 Z/X/Shift/C/D）；④ 两种动力学各 600 帧的离线位移复算
##   （A = 一手 TH18 真模型「无惯性」；B = 反事实惯性模型，证明有惯性时每整周期也归零）。
##   用法与一手依据见 `DEV-INPUT-SYNTH.md`。
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_input_synth_core tests/test_input_synth_core.c
	./tests/build/test_input_synth_core


## x87 自检 —— 只看我们自己的目标文件
dllx87: $(OBJS)
	@n=$$($(OBJDUMP) -d $(OBJS) | grep -cE '^\s+[0-9a-f]+:\s+[0-9a-f ]+\s+\bf(ld|st|add|sub|mul|div|ild|ist|xch|com)' || true); \
	 echo "x87 in our objects: $$n"; test "$$n" = 0

## dllverify —— 位数 / 导出表 / 依赖；任何 ABI 漂移都失败。
dllverify: $(DLL)
	@mkdir -p tests/build
	@file $(DLL) | tee tests/build/dll-file.txt
	@grep -Eq 'PE32.*Intel 80386' tests/build/dll-file.txt
	@$(OBJDUMP) -p $(DLL) > tests/build/dll-headers.txt
	@awk '/^[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]+@[0-9]+/{gsub("@", "", $$2); print $$1, $$2}' th18_card_expand.def | tr -d '\r' | while read sym ordinal; do \
		grep -Eq "^[[:space:]]+$$ordinal[[:space:]]+0x[0-9a-f]+[[:space:]]+$$sym$$" tests/build/dll-headers.txt || exit 1; \
	done
## ★ 2026-09-18：导出条数**从 def 现算**（原来是写死的 23 —— 站点表一变就假红/假绿）。
	@test "$$(grep -Ec '^[[:space:]]+[0-9]+[[:space:]]+0x[0-9a-f]+[[:space:]]+[A-Za-z_]' tests/build/dll-headers.txt)" = \
	      "$$(grep -Ec '^[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]+@[0-9]+' th18_card_expand.def)"
	@test "$$(grep -c 'DLL Name:' tests/build/dll-headers.txt)" = 3
	@grep -q 'DLL Name: KERNEL32.dll' tests/build/dll-headers.txt
	@grep -q 'DLL Name: msvcrt.dll' tests/build/dll-headers.txt
	@grep -q 'DLL Name: USER32.dll' tests/build/dll-headers.txt
	@echo "DLL ABI: 23 exports (22 BP_* + thcrap_plugin_init), PE32/i386, dependencies KERNEL32.dll + msvcrt.dll + USER32.dll"

## dist   —— 三个 patch + DLL + zip；_255（patch-step3）里带重建的 th18/abcard.anm（≈22 MB）、th18/ability.anm（≈1.7 MB）
##         与四个 th18/pl0X.anm（= 原生文件 + 追加的兽灵/灵界弹 entry 与脚本）
## dist —— 在 ../dist/ 摆出可直接安装的整套：一个 DLL + patch-step3 / patch-test / patch-devstage
##         （dist/ 已 gitignore：DLL 是构建产物，不入库；随时 make dist 重建）
DIST := ../dist
dist: anm sht ecl ecl-ufo ability-text
	rm -rf $(DIST) && mkdir -p $(DIST)/bin $(DIST)/patch-step3 $(DIST)/patch-test $(DIST)/patch-devstage/th18
	$(MAKE) -s ROWS=255 gen dll files >/dev/null
	cp $(DLL) $(DIST)/bin/ && cp -r ../patch-test/. $(DIST)/patch-test/
	cp -r ../patch/. $(DIST)/patch-step3/
	for f in build/abcard.anm build/ability.anm build/pl00.anm build/pl01.anm build/pl02.anm build/pl03.anm; do \
	    case ",$$YK_FILES_EXCLUDE," in *,th18/$${f##*/},*) echo "  [YK_FILES_EXCLUDE] 不替换 $$f";; *) cp $$f $(DIST)/patch-step3/th18/;; esac; done
	for f in build/sht/pl0[0-3].sht; do [ -f "$$f" ] || continue; \
	    case ",$$YK_FILES_EXCLUDE," in *,th18/$${f##*/},*) echo "  [YK_FILES_EXCLUDE] 不替换 $$f";; *) cp "$$f" $(DIST)/patch-step3/th18/;; esac; done; true
	cp build/ecl/default.ecl $(DIST)/patch-step3/th18/    # 112 的 UFO ECL（TH12 移植；原版 30 子原样保留）
	python3 mkfiles.py $(DIST)/patch-step3 >/dev/null
	cp ../patch-devstage/patch.js $(DIST)/patch-devstage/ && cp build/ecl/st0[1-6].ecl build/ecl/st0[1-6]bs.ecl $(DIST)/patch-devstage/th18/ && python3 mkfiles.py $(DIST)/patch-devstage >/dev/null
	cp ../DIST-README.txt $(DIST)/README.txt
	cd $(DIST) && rm -f th18_card_expand_bundle.zip && zip -qr th18_card_expand_bundle.zip bin patch-step3 patch-test patch-devstage README.txt
	@echo "dist/ 就绪："; ls $(DIST)

## release-check —— ★ 发布前闸（`tools/release_guard.py check`）：**冻结中直接中止**；
##   顺带跑 sites.py check（patch 声明 ↔ .def 双向对账 + 站点预算）与 check_deploy_pair（DLL ↔ pl0X.sht）。
##   为什么要有它：2026-09-18 那次事故里，冻结状态只写在人读的文档（`local/DLL-SYNC.md` 的两节 ⛔，
##   其中一半还不入库），一个会话**只读了文件开头**就发了 3 次版 ⇒ 纪律必须变成闸，见 AUDIT §BE-EG。
release-check:
	python3 tools/release_guard.py check --modkit "$${MODKIT_DIR:-}"

## release —— 构建 dist，同步进发布仓库 local/vendor/th18_modkit 并提交；PUSH=1 再推
##   ⚠ 顺序：**先过 release-check（冻结即中止）**，再 dist，然后发布，最后 record 记账
##   （账 = patch js / DLL / 四份 pl0X.sht / 两份 anm / cards.js / files.js 的 md5 快照，写进
##    入库的 `RELEASE-STATE.json` + 本机 `local/DLL-SYNC.md`）。
## ★ 2026-10-01：`release` 走的是 `--full-batch` 版的闸 —— 这一批**会连四份 pl0X.sht 一起**写进
##   modkit，所以「游戏里的 sht 与当前构建不同」是**预期**而不是错误（否则改 sht 的批次会被循环锁住，
##   见 AUDIT §BE 详录 #165）。**手工跑的 `release-check` 与 `sync_modkit_dll.sh` 都仍是严格版。**
release-check-full:
	python3 tools/release_guard.py check --full-batch --modkit "$${MODKIT_DIR:-}"

release: release-check-full dist
	python3 release.py
	python3 tools/release_guard.py record --modkit "$${MODKIT_DIR:-未指定}"
