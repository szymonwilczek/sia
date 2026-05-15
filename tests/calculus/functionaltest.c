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
  ASSERT_STR_EQ(s, "x^2/2");
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
  ASSERT_STR_EQ(s, "x^3/3");
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
  ASSERT_STR_EQ(s, "x*ln(x) - x");
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
  ASSERT_TRUE(strcmp(s, "x + x^2/2") == 0 || strcmp(s, "x^2/2 + x") == 0);
  free(s);
  ast_free(expr);
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
  test_integrate_x();
  test_integrate_4x();
  test_integrate_constant();
  test_integrate_x_squared();
  test_integrate_exp();
  test_integrate_ln();
  test_integrate_hoisted_trig_constant();
  test_integrate_linearity_sum();
  test_integrate_1_over_x();
  test_integrate_1_div_x();
  test_integrate_x_div_x();
  test_integrate_rational();
  test_int_definite();
}
