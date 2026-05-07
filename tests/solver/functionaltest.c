#include "../test_support.h"
#include "../test_suites.h"

static void test_solve_linear(void) {
  TEST("solve: linear 2*x + 6 = 0 -> x = -3");
  ParseResult pr = parse("2*x + 6");
  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 1);
  ASSERT_CNEAR(r.roots[0], c_real(-3.0), 1e-10);
  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}


static void test_solve_quadratic(void) {
  TEST("solve: quadratic x^2 - 4 = 0 -> x = -2, 2");
  ParseResult pr = parse("x^2 - 4");
  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 2);
  ASSERT_CNEAR(r.roots[0], c_real(-2.0), 1e-10);
  ASSERT_CNEAR(r.roots[1], c_real(2.0), 1e-10);
  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}


static void test_solve_quadratic_double_root(void) {
  TEST("solve: double root x^2 - 2x + 1 = 0 -> x = 1");
  ParseResult pr = parse("x^2 - 2*x + 1");
  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(r.ok);
  /* quadratic solver might find two identical roots */
  ASSERT_TRUE(r.count >= 1);
  ASSERT_CNEAR(r.roots[0], c_real(1.0), 1e-10);
  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}


static void test_solve_quadratic_complex(void) {
  TEST("solve: complex roots x^2 + 1 = 0 -> x = -i, i");
  ParseResult pr = parse("x^2 + 1");
  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 2);
  ASSERT_CNEAR(r.roots[0], c_make(0, -1), 1e-10);
  ASSERT_CNEAR(r.roots[1], c_make(0, 1), 1e-10);
  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}


static void test_solve_newton_sin(void) {
  TEST("solve: Newton sin(x) = 0 near 3 -> pi");
  ParseResult pr = parse("sin(x)");
  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(3.0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 1);
  ASSERT_CNEAR(r.roots[0], c_real(3.14159265358979), 1e-8);
  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}


static void test_solve_newton_exp(void) {
  TEST("solve: Newton exp(x) - 2 = 0 near 1 -> ln(2)");
  ParseResult pr = parse("exp(x) - 2");
  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(1.0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 1);
  ASSERT_CNEAR(r.roots[0], c_real(0.693147180559945), 1e-8);
  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}


static void test_solve_log_base(void) {
  TEST("solve: log(x, 2) - 4 = 0 -> x = 16");
  ParseResult pr = parse("log(x, 2) - 4");
  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0.0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 1);
  ASSERT_CNEAR(r.roots[0], c_real(16.0), 1e-10);
  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}


static void test_solve_log_variable_base(void) {
  TEST("solve: log(2, x) - 4 = 0 -> x = 2^(1/4)");
  ParseResult pr = parse("log(2, x) - 4");
  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0.0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 1);
  ASSERT_CNEAR(r.roots[0], c_real(pow(2.0, 0.25)), 1e-10);
  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}


void tests_solver_functional(void) {
  test_solve_linear();
  test_solve_quadratic();
  test_solve_quadratic_double_root();
  test_solve_quadratic_complex();
  test_solve_newton_sin();
  test_solve_newton_exp();
  test_solve_log_base();
  test_solve_log_variable_base();
}
