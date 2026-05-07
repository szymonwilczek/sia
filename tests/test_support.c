#include "test_support.h"

int tests_run = 0;
int tests_passed = 0;

void test_suite_run(const char *name, TestSuiteFn fn) {
  int run_before = tests_run;
  int pass_before = tests_passed;

  printf("\nRUN %s\n", name);
  fn();

  int ran = tests_run - run_before;
  int passed = tests_passed - pass_before;
  printf("%s %s (%d/%d)\n", passed == ran ? "OK" : "FAIL", name, passed, ran);
}

int is_num_node(const AstNode *n, double v) {
  return n && n->type == AST_NUMBER &&
         c_abs(c_sub(n->as.number, c_real(v))) < 1e-9;
}

int fraction_is(Fraction f, long long num, long long den) {
  return f.num == num && f.den == den;
}
