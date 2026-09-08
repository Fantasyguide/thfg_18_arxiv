# card-expand —— patch 生成器、行为 DLL、资源与验证入口。
ROWS ?= 255
PATCH := ../patch/th18.v1.00a.js

.PHONY: all quick check package gen verify files clean test-host anm anm-verify sht sht-verify ecl snd-check voice dll dllx87 dllverify

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
check: quick anm-verify sht-verify snd-check dllx87 dllverify
	python3 sites.py check
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
voice:
	python3 ../assets/build_voice.py

## voice_ids.h 是生成物：裸 make dll 也要能把它做出来（否则新 clone 缺文件直接停）
voice_ids.h: ../assets/voice/ORDER.txt $(wildcard ../assets/voice/*.wav) ../patch/th18/voice.js
	python3 ../assets/build_voice.py

## anm    —— abcard.anm（../assets/cards/ 新卡图）+ ability.anm（../assets/ability/ 贴图副本 + 特效脚本）→ build/；生成 anm_ids.h
anm:
	python3 ../assets/build_abcard.py
	python3 ../assets/build_ability.py

## ecl    —— 开发辅助：st01–st06 空壳（道中删除、boss 血量 ÷100、死时多掉钱）→ build/ecl/，单独一个可勾选的 patch（_devstage）
ecl:
	python3 ../assets/ecl/make_dev_ecl.py

## sht    —— 往零售 pl00–03.sht 追加新卡子机的 shooterset → build/sht/；生成 sht_ids.h（APPEND 为空时不产出 .sht）
sht:
	python3 ../assets/sht/append_shooterset.py

## sht_ids.h 是生成物（依赖 anm_ids.h：shooter 的 anm 字段指 ability.anm 的脚本）
sht_ids.h: anm_ids.h ../assets/sht/append_shooterset.py
	python3 ../assets/sht/append_shooterset.py

## sht-verify —— 只校验 build/sht/*.sht：解析不变式 + 前 23 组逐字节不变
sht-verify:
	python3 ../assets/sht/append_shooterset.py --verify-only

## anm-verify —— 只校验（build/abcard.anm 已存在）：entry 数 / 原贴图逐张一致 / 新 entry 字段 / JSON 索引
anm-verify:
	python3 ../assets/build_abcard.py --verify-only
	python3 ../assets/build_ability.py --verify-only

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
LDFLAGS   := -shared -static-libgcc -s -lkernel32 -Wl,--no-insert-timestamp

## dll    —— 编 DLL（依赖 gen 出的 sites_gen.h）
dll: $(DLL)
SRCS      := dll_main.c selfcheck_core.c selfcheck.c unlocked.c text.c cards_def.c cards.c menu.c bp_trace.c sdk_core.c sdk_actions.c sdk.c shop_core.c shop.c sound.c $(wildcard cards/*.c)
OBJS      := $(SRCS:.c=.o)
%.o: %.c card_expand.h sites_gen.h thcrap_bp.h cards_def.h sdk.h sdk_core.h engine.h shop_core.h sound.h voice_ids.h sht_ids.h $(wildcard cards/*.h)
	$(CC) $(CFLAGS) -c -o $@ $<
$(DLL): $(OBJS) th18_card_expand.def
	$(CC) $(CFLAGS) -o $@ $(OBJS) th18_card_expand.def $(LDFLAGS)

## test-host —— 纯逻辑（cards_def.c）的主机单测，Linux gcc 直接跑
HOSTCC ?= cc
test-host:
	@mkdir -p tests/build
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_cards_def tests/test_cards_def.c cards_def.c
	./tests/build/test_cards_def
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_sdk_core tests/test_sdk_core.c sdk_core.c
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
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_firelord_core tests/test_firelord_core.c cards/firelord_core.c -lm
	./tests/build/test_firelord_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_truesilver_core tests/test_truesilver_core.c cards/truesilver_core.c
	./tests/build/test_truesilver_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_guding_core tests/test_guding_core.c cards/guding_core.c
	./tests/build/test_guding_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_double_core tests/test_double_core.c cards/double_core.c
	./tests/build/test_double_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_collect2x_core tests/test_collect2x_core.c cards/collect2x_core.c
	./tests/build/test_collect2x_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_peashooter_core tests/test_peashooter_core.c cards/peashooter_core.c
	./tests/build/test_peashooter_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_ouroboros_core tests/test_ouroboros_core.c cards/ouroboros_core.c
	./tests/build/test_ouroboros_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_whirlwind_core tests/test_whirlwind_core.c cards/whirlwind_core.c
	./tests/build/test_whirlwind_core
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_stock_core tests/test_stock_core.c cards/stock_core.c
	./tests/build/test_stock_core

## x87 自检 —— 只看我们自己的目标文件
dllx87: $(OBJS)
	@n=$$($(OBJDUMP) -d $(OBJS) | grep -cE '^\s+[0-9a-f]+:\s+[0-9a-f ]+\s+f(ld|st|add|sub|mul|div|ild|ist|xch|com)' || true); \
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
	@test "$$(grep -Ec '^[[:space:]]+[0-9]+[[:space:]]+0x[0-9a-f]+[[:space:]]+[A-Za-z_]' tests/build/dll-headers.txt)" = 22
	@test "$$(grep -c 'DLL Name:' tests/build/dll-headers.txt)" = 2
	@grep -q 'DLL Name: KERNEL32.dll' tests/build/dll-headers.txt
	@grep -q 'DLL Name: msvcrt.dll' tests/build/dll-headers.txt
	@echo "DLL ABI: 22 exports, PE32/i386, dependencies KERNEL32.dll + msvcrt.dll"

## dist   —— 三个 patch + DLL + zip；_255（patch-step3）里带重建的 th18/abcard.anm（≈22 MB）与 th18/ability.anm（≈1.7 MB），都含零售贴图
## dist —— 在 ../dist/ 摆出可直接安装的整套：一个 DLL + patch-step3 / patch-test / patch-devstage
##         （dist/ 已 gitignore：DLL 是构建产物，不入库；随时 make dist 重建）
DIST := ../dist
dist: anm sht ecl
	rm -rf $(DIST) && mkdir -p $(DIST)/bin $(DIST)/patch-step3 $(DIST)/patch-test $(DIST)/patch-devstage/th18
	$(MAKE) -s ROWS=255 gen dll files >/dev/null
	cp $(DLL) $(DIST)/bin/ && cp -r ../patch-test/. $(DIST)/patch-test/
	cp -r ../patch/. $(DIST)/patch-step3/
	cp build/abcard.anm build/ability.anm $(DIST)/patch-step3/th18/
	for f in build/sht/pl0[0-3].sht; do [ -f "$$f" ] && cp "$$f" $(DIST)/patch-step3/th18/; done; true
	python3 mkfiles.py $(DIST)/patch-step3 >/dev/null
	cp ../patch-devstage/patch.js $(DIST)/patch-devstage/ && cp build/ecl/st0[1-6].ecl build/ecl/st0[1-6]bs.ecl $(DIST)/patch-devstage/th18/ && python3 mkfiles.py $(DIST)/patch-devstage >/dev/null
	cp ../DIST-README.txt $(DIST)/README.txt
	cd $(DIST) && rm -f th18_card_expand_bundle.zip && zip -qr th18_card_expand_bundle.zip bin patch-step3 patch-test patch-devstage README.txt
	@echo "dist/ 就绪："; ls $(DIST)

## release —— 构建 dist，同步进发布仓库 local/vendor/th18_modkit 并提交；PUSH=1 再推
release: dist
	python3 release.py
