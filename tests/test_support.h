#ifndef SIA_TEST_SUPPORT_H
#define SIA_TEST_SUPPORT_H

#include "sia/ast.h"
#include "sia/canonical.h"
#include "sia/eval.h"
#include "sia/latex.h"
#include "../src/parser/lexer.h"
#include "sia/limits.h"
#include "sia/matrix.h"
#include "sia/parser.h"
#include "sia/solve.h"
#include "sia/symbolic.h"
#include "sia/symtab.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int tests_run;
extern int tests_passed;

typedef void (*TestSuiteFn)(void);

void test_suite_run(const char *name, TestSuiteFn fn);
int is_num_node(const AstNode *n, double v);
int fraction_is(Fraction f, long long num, long long den);

#define TEST(name)                                                             \
  do {                                                                         \
    tests_run++;                                                               \
    printf("  %-55s", name);                                                   \
  } while (0)
#define PASS()                                                                 \
  do {                                                                         \
    tests_passed++;                                                            \
    printf("OK\n");                                                            \
  } while (0)
#define FAIL(msg)                                                              \
  do {                                                                         \
    printf("FAIL: %s\n", msg);                                                 \
  } while (0)

#define ASSERT_EQ(a, b)                                                        \
  do {                                                                         \
    if ((a) != (b)) {                                                          \
      FAIL(#a " != " #b);                                                      \
      return;                                                                  \
    }                                                                          \
  } while (0)
#define ASSERT_NEAR(a, b, eps)                                                 \
  do {                                                                         \
    if (fabs((double)(a) - (double)(b)) > (eps)) {                             \
      char _buf[128];                                                          \
      snprintf(_buf, sizeof _buf, "%g != %g", (double)(a), (double)(b));       \
      FAIL(_buf);                                                              \
      return;                                                                  \
    }                                                                          \
  } while (0)
#define ASSERT_CNEAR(a, b, eps)                                                \
  do {                                                                         \
    if (c_abs(c_sub((a), (b))) > (eps)) {                                      \
      char _buf[128];                                                          \
      snprintf(_buf, sizeof _buf, "complex mismatch");                         \
      FAIL(_buf);                                                              \
      return;                                                                  \
    }                                                                          \
  } while (0)
#define ASSERT_STR_EQ(a, b)                                                    \
  do {                                                                         \
    if (strcmp((a), (b)) != 0) {                                               \
      char _buf[256];                                                          \
      snprintf(_buf, sizeof _buf, "'%s' != '%s'", (a), (b));                   \
      FAIL(_buf);                                                              \
      return;                                                                  \
    }                                                                          \
  } while (0)
#define ASSERT_TRUE(x)                                                         \
  do {                                                                         \
    if (!(x)) {                                                                \
      FAIL(#x " is false");                                                    \
      return;                                                                  \
    }                                                                          \
  } while (0)

#endif
