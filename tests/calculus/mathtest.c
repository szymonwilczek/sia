#include "../test_support.h"
#include "../test_suites.h"

static Complex eval_expr_at(const AstNode *expr, double x) {
  SymTab st;
  symtab_init(&st);
  symtab_set(&st, "x", c_real(x));
  EvalResult result = eval(expr, &st);
  Complex value = result.ok ? result.value : c_make(NAN, NAN);
  eval_result_free(&result);
  symtab_free(&st);
  return value;
}

static void test_symbolic_derivative_matches_finite_difference(void) {
  TEST("calculus: symbolic derivative matches finite difference");
  ParseResult r = parse("asin(x) + acos(x)/2 + atan(x^2) + sinh(x) - cosh(x)/3 + tanh(x)");
  ASSERT_TRUE(r.root != NULL);

  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);

  double x = 0.25;
  double h = 1e-6;
  Complex symbolic = eval_expr_at(d, x);
  Complex high = eval_expr_at(r.root, x + h);
  Complex low = eval_expr_at(r.root, x - h);
  Complex finite = c_div(c_sub(high, low), c_real(2.0 * h));

  ASSERT_TRUE(!isnan(symbolic.re) && !isnan(symbolic.im));
  ASSERT_TRUE(!isnan(finite.re) && !isnan(finite.im));
  ASSERT_CNEAR(symbolic, finite, 1e-5);

  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_integral_derivative_roundtrip_polynomial(void) {
  TEST("calculus: d/dx integral polynomial roundtrip");
  ParseResult r = parse("3*x^2 + 2*x + 1");
  ASSERT_TRUE(r.root != NULL);

  AstNode *integral = sym_integrate(r.root, "x");
  ASSERT_TRUE(integral != NULL);
  AstNode *derivative = sym_diff(integral, "x");
  ASSERT_TRUE(derivative != NULL);
  derivative = sym_simplify(derivative);

  Complex expected = eval_expr_at(r.root, 1.75);
  Complex actual = eval_expr_at(derivative, 1.75);
  ASSERT_TRUE(!isnan(expected.re) && !isnan(expected.im));
  ASSERT_TRUE(!isnan(actual.re) && !isnan(actual.im));
  ASSERT_CNEAR(actual, expected, 1e-9);

  ast_free(derivative);
  ast_free(integral);
  parse_result_free(&r);
  PASS();
}

void tests_calculus_mathtest(void) {
  test_symbolic_derivative_matches_finite_difference();
  test_integral_derivative_roundtrip_polynomial();
}
