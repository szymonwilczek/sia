#include "../test_suites.h"
#include "../test_support.h"

static void test_diff_constant(void) {
  TEST("diff: d/dx(5) = 0");
  ParseResult r = parse("5");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  ASSERT_TRUE(is_num_node(d, 0));
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_variable(void) {
  TEST("diff: d/dx(x) = 1");
  ParseResult r = parse("x");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  ASSERT_TRUE(is_num_node(d, 1));
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_x_squared(void) {
  TEST("diff: d/dx(x^2) = 2*x");
  ParseResult r = parse("x^2");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "2*x");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_polynomial(void) {
  TEST("diff: d/dx(3*x^2 + 2*x + 1)");
  ParseResult r = parse("3*x^2 + 2*x + 1");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "6*x + 2");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_exp(void) {
  TEST("diff: d/dx(exp(x)) = exp(x)");
  ParseResult r = parse("exp(x)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "exp(x)");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_ln(void) {
  TEST("diff: d/dx(ln(x)) = 1/x");
  ParseResult r = parse("ln(x)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "1/x");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_log_base10(void) {
  TEST("diff: d/dx(log(x, 10)) = 1/(x*ln(10))");
  ParseResult r = parse("log(x, 10)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "1/(x*ln(10))");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_log_base2(void) {
  TEST("diff: d/dx(log(x, 2)) = 1/(x*ln(2))");
  ParseResult r = parse("log(x, 2)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "1/(x*ln(2))");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_abs(void) {
  TEST("diff: d/dx(abs(x)) = x/abs(x)");
  ParseResult r = parse("abs(x)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "x/abs(x)");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_chain_sin_x2(void) {
  TEST("diff: d/dx(sin(x^2)) uses chain rule");
  ParseResult r = parse("sin(x^2)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_TRUE(strcmp(s, "cos(x^2)*2*x") == 0 || strcmp(s, "2*cos(x^2)*x") == 0);
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_x_to_x(void) {
  TEST("diff: d/dx(x^x) = x^x*ln(x) + x^x");
  ParseResult r = parse("x^x");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "x^x*ln(x) + x^x");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_indefinite_integral_same_variable(void) {
  TEST("diff: d/dx(int(sqrt(x), x)) = sqrt(x)");
  ParseResult r = parse("int(sqrt(x), x)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "sqrt(x)");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_chain_sin_x2_third_order(void) {
  TEST("diff: d/dx^3(sin(x^2)) stays collected");
  ParseResult r = parse("sin(x^2)");
  AstNode *d = sym_diff_n(r.root, "x", 3);
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_TRUE(strcmp(s, "-12*x*sin(x^2) + -8*x^3*cos(x^2)") == 0 ||
              strcmp(s, "-8*x^3*cos(x^2) + -12*x*sin(x^2)") == 0);
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_abs_second_order(void) {
  TEST("diff: d/dx^2(abs(x)) -> 0 after abs identities");
  ParseResult r = parse("abs(x)");
  AstNode *d = sym_diff_n(r.root, "x", 2);
  ASSERT_TRUE(d != NULL);
  ASSERT_TRUE(is_num_node(d, 0));
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_x(void) {
  TEST("int: integral(x, x) = x^2/2");
  ParseResult r = parse("x");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_TRUE(strcmp(s, "x^2/2") == 0 || strcmp(s, "1/2*x^2") == 0);
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_4x(void) {
  TEST("int: integral(4*x, x) = 4*x^2/2");
  ParseResult r = parse("4*x");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_TRUE(strcmp(s, "4*x^2/2") == 0 || strcmp(s, "2*x^2") == 0);
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_constant(void) {
  TEST("int: integral(5, x) = 5*x");
  ParseResult r = parse("5");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "5*x");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_x_squared(void) {
  TEST("int: integral(x^2, x) = x^3/3");
  ParseResult r = parse("x^2");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_TRUE(strcmp(s, "x^3/3") == 0 || strcmp(s, "1/3*x^3") == 0);
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_sqrt_x(void) {
  TEST("int: integral(sqrt(x), x) = 2/3*x^(3/2)");
  ParseResult r = parse("sqrt(x)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "2/3*x^3/2");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_exp(void) {
  TEST("int: integral(exp(x), x) = exp(x)");
  ParseResult r = parse("exp(x)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "exp(x)");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_ln(void) {
  TEST("int: integral(ln(x), x) = x*ln(x) - x");
  ParseResult r = parse("ln(x)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_TRUE(strcmp(s, "x*ln(x) - x") == 0 ||
              strcmp(s, "(-x) + x*ln(x)") == 0 ||
              strcmp(s, "x*ln(x) + (-x)") == 0);
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_hoisted_trig_constant(void) {
  TEST("int: integral(2*(3*cos(x)), x) = 6*sin(x)");
  ParseResult r = parse("2*(3*cos(x))");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "6*sin(x)");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_linearity_sum(void) {
  TEST("int: integral(sin(x) + cos(x), x)");
  ParseResult r = parse("sin(x) + cos(x)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_TRUE(strcmp(s, "(-cos(x)) + sin(x)") == 0 ||
              strcmp(s, "sin(x) + (-cos(x))") == 0);
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_linear_substitution_cos(void) {
  TEST("int: integral(cos(3*x), x) = 1/3*sin(3*x)");
  ParseResult r = parse("cos(3*x)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_TRUE(strcmp(s, "1/3*sin(3*x)") == 0 || strcmp(s, "sin(3*x)/3") == 0);
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_linear_substitution_exp(void) {
  TEST("int: integral(exp(2*x), x) = 1/2*exp(2*x)");
  ParseResult r = parse("exp(2*x)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_TRUE(strcmp(s, "1/2*exp(2*x)") == 0 || strcmp(s, "exp(2*x)/2") == 0);
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_linear_substitution_shifted_sin(void) {
  TEST("int: integral(sin(2*x + 1), x) = -1/2*cos(2*x + 1)");
  ParseResult r = parse("sin(2*x + 1)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_TRUE(strcmp(s, "(-1/2*cos(2*x + 1))") == 0 ||
              strcmp(s, "-1/2*cos(2*x + 1)") == 0 ||
              strcmp(s, "(-cos(2*x + 1))/2") == 0 ||
              strcmp(s, "-1/2*cos(1 + 2*x)") == 0 ||
              strcmp(s, "(-1/2*cos(1 + 2*x))") == 0);
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_1_over_x(void) {
  TEST("int: integral(x^(-1), x) = ln(x)");
  ParseResult r = parse("x^(-1)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "ln(x)");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_1_div_x(void) {
  TEST("int: integral(1/x, x) via ast_canonicalize = ln(x)");
  ParseResult r = parse("1/x");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  expr = sym_simplify(expr);
  AstNode *result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "ln(x)");
  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_x_div_x(void) {
  TEST("int: integral(x/x, x) via ast_canonicalize = x");
  ParseResult r = parse("x/x");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  expr = sym_simplify(expr);
  AstNode *result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);
  result = sym_simplify(result);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "x");
  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_rational(void) {
  TEST("int: integral((x^3 + x^2) / x^2, x) via ast_canonicalize");
  ParseResult r = parse("(x^3 + x^2) / x^2");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  expr = sym_simplify(expr);
  AstNode *result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);
  result = sym_simplify(result);
  char *s = ast_to_string(result);
  ASSERT_TRUE(strcmp(s, "x + x^2/2") == 0 || strcmp(s, "x^2/2 + x") == 0 ||
              strcmp(s, "x + 1/2*x^2") == 0 || strcmp(s, "1/2*x^2 + x") == 0);
  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_partial_fractions_linear_roots(void) {
  TEST("int: partial fractions integral(1/(x^2 - 1), x)");
  ParseResult r = parse("1/(x^2 - 1)");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  AstNode *result = NULL;
  char *s = NULL;

  expr = sym_simplify(expr);
  result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);

  s = ast_to_string(result);
  ASSERT_TRUE(strstr(s, "ln(abs(") != NULL);
  ASSERT_TRUE(strstr(s, "abs(-1 + x)") != NULL ||
              strstr(s, "abs(x + -1)") != NULL ||
              strstr(s, "abs(x - 1)") != NULL);
  ASSERT_TRUE(strstr(s, "abs(1 + x)") != NULL ||
              strstr(s, "abs(x + 1)") != NULL);

  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_partial_fractions_long_division(void) {
  TEST("int: partial fractions long division integral((x^2 + 1)/(x - 1), x)");
  ParseResult r = parse("(x^2 + 1)/(x - 1)");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  AstNode *result = NULL;
  char *s = NULL;

  expr = sym_simplify(expr);
  result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);

  s = ast_to_string(result);
  ASSERT_TRUE(strstr(s, "ln(abs(") != NULL);
  ASSERT_TRUE(strstr(s, "abs(-1 + x)") != NULL ||
              strstr(s, "abs(x + -1)") != NULL ||
              strstr(s, "abs(x - 1)") != NULL);
  ASSERT_TRUE(strstr(s, "x") != NULL);

  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_partial_fractions_quadratic(void) {
  TEST("int: partial fractions irreducible quadratic integral((2*x + 3)/(x^2 + "
       "2*x + 5), x)");
  ParseResult r = parse("(2*x + 3)/(x^2 + 2*x + 5)");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  AstNode *result = NULL;
  char *s = NULL;

  expr = sym_simplify(expr);
  result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);

  s = ast_to_string(result);
  ASSERT_TRUE(strstr(s, "atan(") != NULL);
  ASSERT_TRUE(strstr(s, "ln(") != NULL);
  ASSERT_TRUE(strstr(s, "5 + 2*x + x^2") != NULL ||
              strstr(s, "x^2 + 2*x + 5") != NULL);

  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_partial_fractions_repeated_root(void) {
  TEST("int: partial fractions repeated root integral(1/(x*(x - 1)^2), x)");
  ParseResult r = parse("1/(x*(x - 1)^2)");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  AstNode *result = NULL;
  char *s = NULL;

  expr = sym_simplify(expr);
  result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);

  s = ast_to_string(result);
  ASSERT_TRUE(strstr(s, "ln(abs(") != NULL);
  ASSERT_TRUE(
      strstr(s, "(-1 + x)^-1") != NULL || strstr(s, "1/(-1 + x)") != NULL ||
      strstr(s, "(x - 1)^-1") != NULL || strstr(s, "1/(x - 1)") != NULL);

  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_partial_fractions_mixed_linear_quadratic(void) {
  TEST("int: partial fractions mixed factors integral(1/(x^3 + x), x)");
  ParseResult r = parse("1/(x^3 + x)");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  AstNode *result = NULL;
  char *s = NULL;

  expr = sym_simplify(expr);
  result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);

  s = ast_to_string(result);
  ASSERT_TRUE(strstr(s, "ln(abs(") != NULL);
  ASSERT_TRUE(strstr(s, "x^2 + 1") != NULL || strstr(s, "1 + x^2") != NULL);

  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_partial_fractions_cubic_factorization(void) {
  TEST("int: partial fractions cubic factorization integral(x^4/(x^3 - 1), x)");
  ParseResult r = parse("x^4/(x^3 - 1)");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  AstNode *result = NULL;
  char *s = NULL;

  expr = sym_simplify(expr);
  result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);

  s = ast_to_string(result);
  ASSERT_TRUE(strstr(s, "x^2") != NULL);
  ASSERT_TRUE(strstr(s, "ln(") != NULL || strstr(s, "atan(") != NULL);

  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_partial_fractions_symbolic_sqrt(void) {
  TEST("int: partial fractions symbolic sqrt integral(x^4/(x^3 - 1), x)");
  ParseResult r = parse("x^4/(x^3 - 1)");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  AstNode *result = NULL;
  char *s = NULL;

  expr = sym_simplify(expr);
  result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);

  s = ast_to_string(result);
  ASSERT_TRUE(strstr(s, "sqrt(3)") != NULL);
  ASSERT_TRUE(strstr(s, "atan(") != NULL);
  ASSERT_TRUE(strstr(s, "ln(") != NULL);
  ASSERT_TRUE(strstr(s, "10864") == NULL);
  ASSERT_TRUE(strstr(s, "18817") == NULL);

  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_partial_fractions_four_linear(void) {
  TEST("int: partial fractions 4 linear factors "
       "integral(1/(x*(x-1)*(x-2)*(x-3)), x)");
  ParseResult r = parse("1/(x*(x-1)*(x-2)*(x-3))");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  AstNode *result = NULL;
  char *s = NULL;

  expr = sym_simplify(expr);
  result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);

  s = ast_to_string(result);
  ASSERT_TRUE(strstr(s, "ln(abs(") != NULL);
  ASSERT_TRUE(strstr(s, "abs(x)") != NULL);
  ASSERT_TRUE(strstr(s, "abs(-1 + x)") != NULL ||
              strstr(s, "abs(x - 1)") != NULL);
  ASSERT_TRUE(strstr(s, "abs(-2 + x)") != NULL ||
              strstr(s, "abs(x - 2)") != NULL);
  ASSERT_TRUE(strstr(s, "abs(-3 + x)") != NULL ||
              strstr(s, "abs(x - 3)") != NULL);

  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_partial_fractions_repeated_quadratic(void) {
  TEST("int: partial fractions repeated quadratic integral(1/(x^2 + 1)^2, x)");
  ParseResult r = parse("1/(x^2 + 1)^2");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  AstNode *result = NULL;
  char *s = NULL;

  expr = sym_simplify(expr);
  result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);

  s = ast_to_string(result);
  ASSERT_TRUE(strstr(s, "atan(x)") != NULL);
  ASSERT_TRUE(strstr(s, "x") != NULL);

  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_partial_fractions_irrational_roots(void) {
  TEST("int: partial fractions irrational roots integral(1/(x^2 - 2), x)");
  ParseResult r = parse("1/(x^2 - 2)");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  AstNode *result = NULL;
  char *s = NULL;

  expr = sym_simplify(expr);
  result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);

  s = ast_to_string(result);
  ASSERT_TRUE(strstr(s, "sqrt(8)") != NULL || strstr(s, "sqrt(") != NULL);
  ASSERT_TRUE(strstr(s, "ln(abs(") != NULL);
  ASSERT_TRUE(strstr(s, "10864") == NULL);
  ASSERT_TRUE(strstr(s, "18817") == NULL);

  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_by_parts_x_cos(void) {
  TEST("int: by parts integral(x*cos(x), x)");
  ParseResult r = parse("x*cos(x)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_TRUE(strcmp(s, "x*sin(x) + cos(x)") == 0 ||
              strcmp(s, "cos(x) + x*sin(x)") == 0);
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_by_parts_x2_exp(void) {
  TEST("int: by parts integral(x^2*exp(x), x)");
  ParseResult r = parse("x^2*exp(x)");
  AstNode *result = sym_integrate(r.root, "x");
  AstNode *diff = NULL;
  AstNode *expected = NULL;
  char *diff_s = NULL;
  char *expected_s = NULL;

  ASSERT_TRUE(result != NULL);
  result = sym_full_simplify(result);

  diff = sym_diff(result, "x");
  diff = ast_canonicalize(diff);
  diff = sym_full_simplify(diff);
  expected = ast_canonicalize(ast_clone(r.root));
  expected = sym_full_simplify(expected);
  ASSERT_TRUE(diff != NULL);
  ASSERT_TRUE(expected != NULL);

  diff_s = ast_to_string(diff);
  expected_s = ast_to_string(expected);
  ASSERT_STR_EQ(diff_s, expected_s);

  free(diff_s);
  free(expected_s);
  ast_free(diff);
  ast_free(expected);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_by_parts_recurring_exp_sin(void) {
  TEST("int: by parts recurring integral(exp(x)*sin(x), x)");
  ParseResult r = parse("exp(x)*sin(x)");
  AstNode *result = sym_integrate(r.root, "x");
  AstNode *diff = NULL;
  AstNode *expected = NULL;
  char *diff_s = NULL;
  char *expected_s = NULL;

  ASSERT_TRUE(result != NULL);

  diff = sym_diff(result, "x");
  diff = ast_canonicalize(diff);
  diff = sym_full_simplify(diff);
  expected = ast_canonicalize(ast_clone(r.root));
  expected = sym_full_simplify(expected);
  ASSERT_TRUE(diff != NULL);
  ASSERT_TRUE(expected != NULL);

  diff_s = ast_to_string(diff);
  expected_s = ast_to_string(expected);
  ASSERT_STR_EQ(diff_s, expected_s);

  free(diff_s);
  free(expected_s);
  ast_free(diff);
  ast_free(expected);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

/* Canonical Form */

static void test_int_definite(void) {
  TEST("int: definite integral(x, x, 0, 2) = 2");
  AstNode *expr = ast_variable("x", 1);
  AstNode *result = sym_integrate(expr, "x");
  ast_free(expr);
  ast_free(result);
  PASS();
}

void tests_calculus_functional(void) {
  test_diff_constant();
  test_diff_variable();
  test_diff_x_squared();
  test_diff_polynomial();
  test_diff_exp();
  test_diff_ln();
  test_diff_log_base10();
  test_diff_log_base2();
  test_diff_abs();
  test_diff_chain_sin_x2();
  test_diff_chain_sin_x2_third_order();
  test_diff_abs_second_order();
  test_diff_x_to_x();
  test_diff_indefinite_integral_same_variable();
  test_integrate_x();
  test_integrate_4x();
  test_integrate_constant();
  test_integrate_x_squared();
  test_integrate_sqrt_x();
  test_integrate_exp();
  test_integrate_ln();
  test_integrate_hoisted_trig_constant();
  test_integrate_linearity_sum();
  test_integrate_linear_substitution_cos();
  test_integrate_linear_substitution_exp();
  test_integrate_linear_substitution_shifted_sin();
  test_integrate_1_over_x();
  test_integrate_1_div_x();
  test_integrate_x_div_x();
  test_integrate_rational();
  test_integrate_partial_fractions_linear_roots();
  test_integrate_partial_fractions_long_division();
  test_integrate_partial_fractions_quadratic();
  test_integrate_partial_fractions_repeated_root();
  test_integrate_partial_fractions_mixed_linear_quadratic();
  test_integrate_partial_fractions_cubic_factorization();
  test_integrate_partial_fractions_symbolic_sqrt();
  test_integrate_partial_fractions_four_linear();
  test_integrate_partial_fractions_repeated_quadratic();
  test_integrate_partial_fractions_irrational_roots();
  test_integrate_by_parts_x_cos();
  test_integrate_by_parts_x2_exp();
  test_integrate_by_parts_recurring_exp_sin();
  test_int_definite();
}
