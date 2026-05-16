VERSION   := 0.9.2-5
CODENAME  := "Jordan's Jewel"

CC      ?= gcc
CFLAGS  := -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE \
           "-DSIA_VERSION=\"$(VERSION)\"" "-DSIA_CODENAME=\"$(CODENAME)\"" \
           -Wall -Wextra -Wpedantic -Werror \
           -Wshadow -Wstrict-prototypes -Wmissing-prototypes \
           -Wno-unused-parameter -O2 -g
LDFLAGS :=
LDLIBS  := -lm

SRC     := lexer.c ast.c parser.c eval.c symbolic.c limits.c canonical.c \
		   symtab.c matrix.c latex.c fractions.c logarithm.c factorial.c \
		   number_theory.c trigonometry/trigonometry.c \
		   solve.c main.c
OBJ     := $(SRC:.c=.o)
DEP     := $(SRC:.c=.d)
BIN     := sia

CORE    := lexer.c ast.c parser.c eval.c symbolic.c limits.c canonical.c \
		   symtab.c matrix.c latex.c fractions.c logarithm.c factorial.c \
		   number_theory.c trigonometry/trigonometry.c \
		   solve.c
TEST_MODULES := tests/test_support.c $(wildcard tests/*/*.c)
TEST_SRC := test_sia.c $(TEST_MODULES) $(CORE)
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
	rm -f $(OBJ) $(DEP) $(BIN) $(TEST_BIN) *.o

-include $(DEP)
