# 宿主（Linux gcc）单测：策略与抽取器都不依赖 Windows。
CC ?= gcc
CFLAGS ?= -std=c99 -O1 -g -Wall -Wextra -Werror -I..
build:
	mkdir -p build
build/test_policy: build test_policy.c ../policy.c ../policy.h ../world.h
	$(CC) $(CFLAGS) -o $@ test_policy.c ../policy.c -lm
build/test_extract: build test_extract.c ../extract.c ../extract.h ../th06nc.h ../world.h
	$(CC) $(CFLAGS) -o $@ test_extract.c ../extract.c -lm
test: build/test_policy build/test_extract
	./build/test_policy
	./build/test_extract
test-policy: build/test_policy
	./build/test_policy
.PHONY: test test-policy
