#include "../test_suites.h"
#include "../test_support.h"

static void test_integrate_sin(void) {
  TEST("int: integral(sin(x), x) = -cos(x)");
  ParseResult r = parse("sin(x)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "(-cos(x))");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_cos(void) {
  TEST("int: integral(cos(x), x) = sin(x)");
  ParseResult r = parse("cos(x)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "sin(x)");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_sinh(void) {
  TEST("int: integral(sinh(x), x) = cosh(x)");
  ParseResult r = parse("sinh(x)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "cosh(x)");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_cosh(void) {
  TEST("int: integral(cosh(x), x) = sinh(x)");
  ParseResult r = parse("cosh(x)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "sinh(x)");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_latex_sin(void) {
  TEST("latex: sin(x) -> \\sin\\left(x\\right)");
  ParseResult r = parse("sin(x)");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "\\sin\\left(x\\right)");
  free(s);
  parse_result_free(&r);
  PASS();
}

static void test_latex_inverse_trig(void) {
  TEST("latex: asin, acos, atan");

  ParseResult r1 = parse("asin(x)");
  char *s1 = ast_to_latex(r1.root);
  ASSERT_STR_EQ(s1, "\\asin\\left(x\\right)");
  free(s1);
  parse_result_free(&r1);

  ParseResult r2 = parse("acos(x)");
  char *s2 = ast_to_latex(r2.root);
  ASSERT_STR_EQ(s2, "\\acos\\left(x\\right)");
  free(s2);
  parse_result_free(&r2);

  ParseResult r3 = parse("atan(x)");
  char *s3 = ast_to_latex(r3.root);
  ASSERT_STR_EQ(s3, "\\atan\\left(x\\right)");
  free(s3);
  parse_result_free(&r3);

  PASS();
}

static void test_latex_hyperbolic(void) {
  TEST("latex: sinh, cosh, tanh");

  ParseResult r1 = parse("sinh(x)");
  char *s1 = ast_to_latex(r1.root);
  ASSERT_STR_EQ(s1, "\\sinh\\left(x\\right)");
  free(s1);
  parse_result_free(&r1);

  ParseResult r2 = parse("cosh(x)");
  char *s2 = ast_to_latex(r2.root);
  ASSERT_STR_EQ(s2, "\\cosh\\left(x\\right)");
  free(s2);
  parse_result_free(&r2);

  ParseResult r3 = parse("tanh(x)");
  char *s3 = ast_to_latex(r3.root);
  ASSERT_STR_EQ(s3, "\\tanh\\left(x\\right)");
  free(s3);
  parse_result_free(&r3);

  PASS();
}

void tests_trigonometry_functional(void) {
  test_integrate_sin();
  test_integrate_cos();
  test_integrate_sinh();
  test_integrate_cosh();
  test_latex_sin();
  test_latex_inverse_trig();
  test_latex_hyperbolic();
}
