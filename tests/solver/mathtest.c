#include "../test_suites.h"
#include "../test_support.h"

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

static int complex_in(const Complex *roots, size_t count, Complex target,
                      double eps) {
  for (size_t i = 0; i < count; i++)
    if (c_abs(c_sub(roots[i], target)) < eps)
      return 1;
  return 0;
}

static void test_solver_linear_explicit_equation(void) {
  TEST("solve: linear AST_EQ 2*x + 1 = 7 -> x = 3");
  ParseResult pr = parse("2*x + 1 = 7");
  ASSERT_TRUE(pr.root != NULL);
  ASSERT_TRUE(pr.root->type == AST_EQ);

  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 1);
  ASSERT_CNEAR(r.roots[0], c_real(3.0), 1e-9);

  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}

static void test_solver_quadratic_explicit_equation(void) {
  TEST("solve: quadratic AST_EQ x^2 = 4 -> {-2, 2}");
  ParseResult pr = parse("x^2 = 4");
  ASSERT_TRUE(pr.root != NULL);
  ASSERT_TRUE(pr.root->type == AST_EQ);

  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 2);
  ASSERT_TRUE(complex_in(r.roots, r.count, c_real(2.0), 1e-9));
  ASSERT_TRUE(complex_in(r.roots, r.count, c_real(-2.0), 1e-9));

  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}

static void test_solver_rational_reduces_to_polynomial(void) {
  TEST("solve: rational (x^2-1)/(x-1) = 0 -> {-1}");
  ParseResult pr = parse("(x^2 - 1)/(x - 1) = 0");
  ASSERT_TRUE(pr.root != NULL);

  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 1);
  ASSERT_CNEAR(r.roots[0], c_real(-1.0), 1e-9);

  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}

static void test_solver_rational_extraneous_root_filtered(void) {
  TEST("solve: rational (x^2-1)/(x-1) = 2 has no valid roots");
  ParseResult pr = parse("(x^2 - 1)/(x - 1) = 2");
  ASSERT_TRUE(pr.root != NULL);

  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 0);

  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}

static void test_solver_rational_valid_root_preserved(void) {
  TEST("solve: rational 1/(x-3) = 1 -> x = 4 (forbidden x=3 unaffected)");
  ParseResult pr = parse("1/(x - 3) = 1");
  ASSERT_TRUE(pr.root != NULL);

  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 1);
  ASSERT_CNEAR(r.roots[0], c_real(4.0), 1e-9);

  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}

static void test_solver_equation_backward_compat(void) {
  TEST("solve: implicit form x^2 - 4 still resolves to {-2, 2}");
  ParseResult pr = parse("x^2 - 4");
  ASSERT_TRUE(pr.root != NULL);
  ASSERT_TRUE(pr.root->type != AST_EQ);

  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 2);
  ASSERT_TRUE(complex_in(r.roots, r.count, c_real(2.0), 1e-9));
  ASSERT_TRUE(complex_in(r.roots, r.count, c_real(-2.0), 1e-9));

  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}

static void test_solver_symbolic_coeffs_fail_clean(void) {
  TEST("solve: free symbols return clean error (no Newton crash)");
  ParseResult pr = parse("a*x - b = c*x + d");
  ASSERT_TRUE(pr.root != NULL);

  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(!r.ok);
  ASSERT_TRUE(r.error != NULL);

  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}

static void test_solver_symbolic_coeffs_resolve_with_symtab(void) {
  TEST("solve: linear symbolic coeffs eval through symtab");
  ParseResult pr = parse("a*x - b = c*x + d");
  ASSERT_TRUE(pr.root != NULL);

  SymTab st;
  symtab_init(&st);
  symtab_set(&st, "a", c_real(3));
  symtab_set(&st, "b", c_real(1));
  symtab_set(&st, "c", c_real(1));
  symtab_set(&st, "d", c_real(5));
  /* expected: (3 - 1)*x = 1 + 5 -> x = 3 */
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 1);
  ASSERT_CNEAR(r.roots[0], c_real(3.0), 1e-9);

  solve_result_free(&r);
  symtab_free(&st);
  parse_result_free(&pr);
  PASS();
}

static void test_solver_nested_fraction_equation(void) {
  TEST("solve: nested rational 1/(1 + 1/x) = 2 -> x = -2");
  ParseResult pr = parse("1/(1 + 1/x) = 2");
  ASSERT_TRUE(pr.root != NULL);

  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 1);
  ASSERT_CNEAR(r.roots[0], c_real(-2.0), 1e-9);

  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}

static void test_solver_log_compound_argument(void) {
  TEST("solve: log_2(x^2 - 5) = 2 -> {-3, 3}");
  ParseResult pr = parse("log(x^2 - 5, 2) = 2");
  ASSERT_TRUE(pr.root != NULL);

  SymTab st;
  memset(&st, 0, sizeof(st));
  SolveResult r = sym_solve(pr.root, "x", c_real(0), &st);
  ASSERT_TRUE(r.ok);
  ASSERT_EQ((int)r.count, 2);
  ASSERT_TRUE(complex_in(r.roots, r.count, c_real(3.0), 1e-9));
  ASSERT_TRUE(complex_in(r.roots, r.count, c_real(-3.0), 1e-9));

  solve_result_free(&r);
  parse_result_free(&pr);
  PASS();
}

void tests_solver_mathtest(void) {
  test_solver_roots_satisfy_polynomial();
  test_solver_newton_root_satisfies_transcendental();
  test_solver_linear_explicit_equation();
  test_solver_quadratic_explicit_equation();
  test_solver_rational_reduces_to_polynomial();
  test_solver_rational_extraneous_root_filtered();
  test_solver_rational_valid_root_preserved();
  test_solver_equation_backward_compat();
  test_solver_symbolic_coeffs_fail_clean();
  test_solver_symbolic_coeffs_resolve_with_symtab();
  test_solver_nested_fraction_equation();
  test_solver_log_compound_argument();
}
