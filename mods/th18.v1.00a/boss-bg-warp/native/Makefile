# th18_bgwarp ——「boss 背景扭曲」顶点效果验证 mod（32 位 Windows DLL）
#
# 工具链：llvm-mingw（msvcrt 变体，Linux 宿主），与 ../runtime-probe 同一套。
# 必须 32 位：TH18 是 32 位 PE，thcrap 会拒绝位数不符的插件（thcrap/src/plugin.cpp WRONG_ARCH）。
#
# 本 DLL **不需要 patch / 不需要 JSON / 不需要 thcrap 栈**：
# 它自己用 VirtualProtect 装两个内联 hook。装法 = 把 DLL 放进 <thcrap>/bin。

TOOLCHAIN ?= $(HOME)/Tools/llvm-mingw-20260826-msvcrt-ubuntu-22.04-x86_64
CC        := $(TOOLCHAIN)/bin/i686-w64-mingw32-gcc
OBJDUMP   := $(TOOLCHAIN)/bin/i686-w64-mingw32-objdump

TARGET := th18_bgwarp.dll
SRC    := th18_bgwarp.c route_b.c bgwarp_core.c
DEF    := th18_bgwarp.def

CFLAGS  := -O2 -Wall -Wextra
LDFLAGS := -shared -static-libgcc -s -lkernel32

all: $(TARGET)

$(TARGET): $(SRC) $(DEF)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(DEF) $(LDFLAGS)

# 把源码里写死的 RVA / 签名字节 / 结构偏移拿去和真 exe 逐字节对账（换 build 后先跑这个）
check:
	python3 check_constants.py

verify: $(TARGET)
	@echo "--- 位数 / 类型 ---"
	@file $(TARGET)
	@echo "--- 导出表 ---"
	@$(OBJDUMP) -p $(TARGET) | grep -A 6 '^Export Table:'
	@echo "--- 依赖的 DLL ---"
	@$(OBJDUMP) -p $(TARGET) | grep 'DLL Name'

clean:
	rm -f $(TARGET) *.o

.PHONY: all check verify clean
