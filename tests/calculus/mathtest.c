#include "../test_suites.h"
#include "../test_support.h"

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
  ParseResult r =
      parse("asin(x) + acos(x)/2 + atan(x^2) + sinh(x) - cosh(x)/3 + tanh(x)");
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

static int simplifies_to(const char *expr, const char *expected) {
  ParseResult r = parse(expr);
  if (!r.root)
    return 0;

  AstNode *result = sym_full_simplify(ast_clone(r.root));
  if (!result) {
    parse_result_free(&r);
    return 0;
  }

  char *str = ast_to_string(result);
  int ok = strcmp(str, expected) == 0;

  free(str);
  ast_free(result);
  parse_result_free(&r);
  return ok;
}

static void test_directional_limit_reciprocal(void) {
  TEST("calculus: lim(1/x, x, 0+) = inf and 0- = -inf");
  ASSERT_TRUE(simplifies_to("lim(1/x, x, 0+)", "inf"));
  ASSERT_TRUE(simplifies_to("lim(1/x, x, 0-)", "-inf"));
  PASS();
}

static void test_directional_limit_abs_ratio(void) {
  TEST("calculus: lim(abs(x)/x, x, 0-) = -1");
  ASSERT_TRUE(simplifies_to("lim(abs(x)/x, x, 0-)", "-1"));
  ASSERT_TRUE(simplifies_to("lim(abs(x)/x, x, 0+)", "1"));
  PASS();
}

static void test_two_sided_vertical_asymptotes(void) {
  TEST("calculus: two-sided vertical asymptote signs");
  ASSERT_TRUE(simplifies_to("lim(1/x, x, 0)", "undefined"));
  ASSERT_TRUE(simplifies_to("lim(1/x^2, x, 0)", "inf"));
  PASS();
}

static void test_signed_infinity_arithmetic(void) {
  TEST("calculus: signed infinity arithmetic");
  ASSERT_TRUE(simplifies_to("(-1)*inf", "-inf"));
  ASSERT_TRUE(simplifies_to("1/inf", "0"));
  ASSERT_TRUE(simplifies_to("inf + inf", "inf"));
  ASSERT_TRUE(simplifies_to("inf - inf", "undefined"));
  PASS();
}

static void test_infinite_power_simplification(void) {
  TEST("calculus: infinite power simplification");
  ASSERT_TRUE(simplifies_to("2^inf", "inf"));
  ASSERT_TRUE(simplifies_to("inf^inf", "inf"));
  ASSERT_TRUE(simplifies_to("2^-inf", "0"));
  ASSERT_TRUE(simplifies_to("inf^0", "undefined"));
  PASS();
}

static void test_rational_polynomial_limit_at_negative_infinity(void) {
  TEST("calculus: rational polynomial limit at -inf");
  ASSERT_TRUE(simplifies_to("lim((x^3 - x) / (x^2 + 1), x, -inf)", "-inf"));
  PASS();
}

static void test_directional_abs_denominator_limit(void) {
  TEST("calculus: directional limit through abs denominator");
  ASSERT_TRUE(simplifies_to("lim((x^2 - 9) / abs(x - 3), x, 3-)", "-6"));
  ASSERT_TRUE(simplifies_to("lim((x^2 - 9) / abs(x - 3), x, 3+)", "6"));
  PASS();
}

void tests_calculus_mathtest(void) {
  test_symbolic_derivative_matches_finite_difference();
  test_integral_derivative_roundtrip_polynomial();
  test_directional_limit_reciprocal();
  test_directional_limit_abs_ratio();
  test_two_sided_vertical_asymptotes();
  test_signed_infinity_arithmetic();
  test_infinite_power_simplification();
  test_directional_abs_denominator_limit();
  test_rational_polynomial_limit_at_negative_infinity();
}
