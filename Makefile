CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -Wpedantic -std=c11
LDFLAGS ?=

SRCS    := src/shell.c src/main.c
TEST    := tests/test_shell.c
TEST_C  := tests/test_counters.c
BIN     := pipe-shell
TESTBIN := build/test_shell

.PHONY: all test clean

all: $(BIN)

$(BIN): $(SRCS) include/shell.h
	$(CC) $(CFLAGS) -Iinclude $(SRCS) -o $@ $(LDFLAGS)

$(TESTBIN): $(TEST) $(TEST_C) src/shell.c include/shell.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Iinclude -Itests $(TEST) $(TEST_C) src/shell.c -o $@ $(LDFLAGS)

test: $(BIN) $(TESTBIN)
	./$(TESTBIN)

clean:
	rm -f $(BIN)
	rm -rf build
