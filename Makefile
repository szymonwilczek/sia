CC      ?= gcc
CFLAGS  := -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE \
           -Wall -Wextra -Wpedantic -Werror \
           -Wshadow -Wstrict-prototypes -Wmissing-prototypes \
           -Wno-unused-parameter -O2 -g
LDFLAGS :=
LDLIBS  := -lm

SRC     := lexer.c ast.c parser.c eval.c symbolic.c main.c
OBJ     := $(SRC:.c=.o)
DEP     := $(SRC:.c=.d)
BIN     := sia

TEST_SRC := test_sia.c lexer.c ast.c parser.c eval.c symbolic.c
TEST_OBJ := $(TEST_SRC:.c=.o)
TEST_BIN := test_sia

.PHONY: all clean test valgrind

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

valgrind: $(TEST_BIN)
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_BIN)

clean:
	rm -f $(OBJ) $(DEP) $(BIN) $(TEST_BIN) $(TEST_OBJ)

-include $(DEP)
