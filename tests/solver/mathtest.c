#include "../test_support.h"
#include "../test_suites.h"

static Complex eval_expr_at(const AstNode *expr, Complex x) {
  SymTab st;
  symtab_init(&st);
  symtab_set(&st, "x", x);
  EvalResult result = eval(expr, &st);
  Complex value = result.ok ? result.value : c_make(NAN, NAN);
  eval_result_free(&result);
  symtab_free(&st);
  return value;
}

static void test_solver_roots_satisfy_polynomial(void) {
  TEST("solve: returned polynomial roots satisfy equation");
  ParseResult pr = parse("x^2 - 5*x + 6");
  ASSERT_TRUE(pr.root != NULL);

  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 2);

  for (size_t i = 0; i < r.count; i++) {
    Complex residual = eval_expr_at(pr.root, r.roots[i]);
    ASSERT_TRUE(!isnan(residual.re) && !isnan(residual.im));
    ASSERT_CNEAR(residual, c_real(0.0), 1e-9);
  }

  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}

static void test_solver_newton_root_satisfies_transcendental(void) {
  TEST("solve: Newton root satisfies transcendental equation");
  ParseResult pr = parse("cos(x) - x");
  ASSERT_TRUE(pr.root != NULL);

  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0.7), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 1);

  Complex residual = eval_expr_at(pr.root, r.roots[0]);
  ASSERT_TRUE(!isnan(residual.re) && !isnan(residual.im));
  ASSERT_CNEAR(residual, c_real(0.0), 1e-8);

  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}

void tests_solver_mathtest(void) {
  test_solver_roots_satisfy_polynomial();
  test_solver_newton_root_satisfies_transcendental();
}
