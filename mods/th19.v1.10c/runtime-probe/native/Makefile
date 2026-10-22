# TH19 v1.10c 只读运行时探针 —— 交叉编译成 32 位 Windows DLL
# 工具链：llvm-mingw（msvcrt），同 th18 的 runtime-probe。
TOOLCHAIN ?= $(HOME)/Tools/llvm-mingw-20260826-msvcrt-ubuntu-22.04-x86_64
CC        := $(TOOLCHAIN)/bin/i686-w64-mingw32-gcc

TARGET := th19_probe2.dll
SRC    := th19_probe2.c
DEF    := th19_probe2.def

CFLAGS  := -O2 -Wall -Wextra
LDFLAGS := -shared -static-libgcc -s -lkernel32

all: $(TARGET)

$(TARGET): $(SRC) $(DEF)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(DEF) $(LDFLAGS)

clean:
	rm -f $(TARGET)
