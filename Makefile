VERSION   := 0.9.5-6
CODENAME  := Jordan's Jewel

CC      ?= gcc
CFLAGS  := -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE \
           "-DSIA_VERSION=\"$(VERSION)\"" "-DSIA_CODENAME=\"$(CODENAME)\"" \
           -Wall -Wextra -Wpedantic -Werror \
           -Wshadow -Wstrict-prototypes -Wmissing-prototypes \
           -Wno-unused-parameter -O2 -g \
           -Iinclude
LDFLAGS :=
LDLIBS  := -lm

BUILD_DIR := build

MAIN_SRC  := src/main.c
CORE_SRC  := $(wildcard src/core/*.c) \
             $(wildcard src/io/*.c) \
             $(wildcard src/math/*.c) \
             $(wildcard src/parser/*.c) \
             $(wildcard src/symbolic/*.c)

SRC       := $(MAIN_SRC) $(CORE_SRC)
OBJ       := $(SRC:%.c=$(BUILD_DIR)/%.o)
DEP       := $(OBJ:.o=.d)
BIN       := sia

TEST_MAIN := test_sia.c
TEST_MODS := tests/test_support.c $(wildcard tests/*/*.c)
TEST_SRC  := $(TEST_MAIN) $(TEST_MODS)
TEST_OBJ  := $(TEST_SRC:%.c=$(BUILD_DIR)/%.o)
TEST_DEP  := $(TEST_OBJ:.o=.d)
TEST_BIN  := test_sia

CORE_OBJ  := $(CORE_SRC:%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean test valgrind

all: compile_flags.txt $(BIN)

compile_flags.txt: Makefile
	@echo "Generating compile_flags.txt..."
	@echo "-std=c11" > $@
	@echo "-D_POSIX_C_SOURCE=200809L" >> $@
	@echo "-D_DEFAULT_SOURCE" >> $@
	@echo "-DSIA_VERSION=\"$(VERSION)\"" >> $@
	@echo "-DSIA_CODENAME=\"$(CODENAME)\"" >> $@
	@echo "-Iinclude" >> $@
	@echo "-Wall" >> $@
	@echo "-Wextra" >> $@
	@echo "-Wpedantic" >> $@
	@echo "-Werror" >> $@
	@echo "-Wshadow" >> $@
	@echo "-Wstrict-prototypes" >> $@
	@echo "-Wmissing-prototypes" >> $@

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test: compile_flags.txt $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJ) $(CORE_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

valgrind: $(TEST_BIN)
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_BIN)

clean:
	rm -rf $(BUILD_DIR) $(BIN) $(TEST_BIN) compile_flags.txt

-include $(DEP) $(TEST_DEP)
