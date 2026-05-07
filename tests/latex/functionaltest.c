#include "../test_support.h"
#include "../test_suites.h"

static void test_latex_number(void) {
  TEST("latex: integer renders as plain number");
  AstNode *n = ast_number(42);
  char *s = ast_to_latex(n);
  ASSERT_STR_EQ(s, "42");
  free(s);
  ast_free(n);
  PASS();
}


static void test_latex_fraction(void) {
  TEST("latex: 1/2 renders as fraction");
  AstNode *n = ast_number(0.5);
  char *s = ast_to_latex(n);
  ASSERT_STR_EQ(s, "\\frac{1}{2}");
  free(s);
  ast_free(n);
  PASS();
}


static void test_latex_elementary_functions(void) {
  TEST("latex: abs, sqrt, log(x, 10), log(x, 2)");

  ParseResult r1 = parse("abs(x)");
  char *s1 = ast_to_latex(r1.root);
  ASSERT_STR_EQ(s1, "\\left|x\\right|");
  free(s1);
  parse_result_free(&r1);

  ParseResult r2 = parse("sqrt(x)");
  char *s2 = ast_to_latex(r2.root);
  ASSERT_STR_EQ(s2, "\\sqrt{x}");
  free(s2);
  parse_result_free(&r2);

  ParseResult r3 = parse("log(x, 10)");
  char *s3 = ast_to_latex(r3.root);
  ASSERT_STR_EQ(s3, "\\log_{10}\\left(x\\right)");
  free(s3);
  parse_result_free(&r3);

  ParseResult r4 = parse("log(x, 2)");
  char *s4 = ast_to_latex(r4.root);
  ASSERT_STR_EQ(s4, "\\log_{2}\\left(x\\right)");
  free(s4);
  parse_result_free(&r4);

  PASS();
}


static void test_latex_variable(void) {
  TEST("latex: single char variable");
  AstNode *n = ast_variable("x", 1);
  char *s = ast_to_latex(n);
  ASSERT_STR_EQ(s, "x");
  free(s);
  ast_free(n);
  PASS();
}


static void test_latex_pi(void) {
  TEST("latex: pi -> \\pi");
  AstNode *n = ast_variable("pi", 2);
  char *s = ast_to_latex(n);
  ASSERT_STR_EQ(s, "\\pi");
  free(s);
  ast_free(n);
  PASS();
}


static void test_latex_multichar_var(void) {
  TEST("latex: multi-char var -> \\mathrm{var}");
  AstNode *n = ast_variable("abc", 3);
  char *s = ast_to_latex(n);
  ASSERT_STR_EQ(s, "\\mathrm{abc}");
  free(s);
  ast_free(n);
  PASS();
}


static void test_latex_add(void) {
  TEST("latex: x + 1");
  ParseResult r = parse("x + 1");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "x + 1");
  free(s);
  parse_result_free(&r);
  PASS();
}


static void test_latex_sub(void) {
  TEST("latex: x - 1");
  ParseResult r = parse("x - 1");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "x - 1");
  free(s);
  parse_result_free(&r);
  PASS();
}


static void test_latex_mul_implicit(void) {
  TEST("latex: 4*x -> 4 x (implicit mul)");
  ParseResult r = parse("4*x");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "4 x");
  free(s);
  parse_result_free(&r);
  PASS();
}


static void test_latex_mul_cdot(void) {
  TEST("latex: 3*4 -> 3 \\cdot 4");
  ParseResult r = parse("3*4");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "3 \\cdot 4");
  free(s);
  parse_result_free(&r);
  PASS();
}


static void test_latex_div_frac(void) {
  TEST("latex: a/b -> \\frac{a}{b}");
  ParseResult r = parse("a/b");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "\\frac{a}{b}");
  free(s);
  parse_result_free(&r);
  PASS();
}


static void test_latex_pow(void) {
  TEST("latex: x^2 -> x^{2}");
  ParseResult r = parse("x^2");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "x^{2}");
  free(s);
  parse_result_free(&r);
  PASS();
}


static void test_latex_nested_pow(void) {
  TEST("latex: x^(n+1) -> x^{n + 1}");
  ParseResult r = parse("x^(n+1)");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "x^{n + 1}");
  free(s);
  parse_result_free(&r);
  PASS();
}


static void test_latex_sqrt(void) {
  TEST("latex: sqrt(x) -> \\sqrt{x}");
  ParseResult r = parse("sqrt(x)");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "\\sqrt{x}");
  free(s);
  parse_result_free(&r);
  PASS();
}


static void test_latex_abs(void) {
  TEST("latex: abs(x) -> \\left|x\\right|");
  ParseResult r = parse("abs(x)");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "\\left|x\\right|");
  free(s);
  parse_result_free(&r);
  PASS();
}


static void test_latex_unary_neg(void) {
  TEST("latex: -(x+1) -> -\\left(x + 1\\right)");
  AstNode *inner = ast_binop(OP_ADD, ast_variable("x", 1), ast_number(1));
  AstNode *neg = ast_unary_neg(inner);
  char *s = ast_to_latex(neg);
  ASSERT_STR_EQ(s, "-\\left(x + 1\\right)");
  free(s);
  ast_free(neg);
  PASS();
}


static void test_latex_matrix(void) {
  TEST("latex: 2x2 matrix -> pmatrix env");
  ParseResult r = parse("[1, 2; 3, 4]");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "\\begin{pmatrix}\n1 & 2 \\\\\n3 & 4\n\\end{pmatrix}");
  free(s);
  parse_result_free(&r);
  PASS();
}


static void test_latex_frac_no_parens(void) {
  TEST("latex: (x+1)/(x-1) -> no parens inside frac");
  ParseResult r = parse("(x+1)/(x-1)");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "\\frac{x + 1}{x - 1}");
  free(s);
  parse_result_free(&r);
  PASS();
}


static void test_latex_inverse_pow(void) {
  TEST("latex: x^(-1) -> \\frac{1}{x}");
  ParseResult r = parse("x^(-1)");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "\\frac{1}{x}");
  free(s);
  parse_result_free(&r);
  PASS();
}


static void test_latex_complex_expr(void) {
  TEST("latex: 4*x^2 + 3*x + 1");
  ParseResult r = parse("4*x^2 + 3*x + 1");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "4 x^{2} + 3 x + 1");
  free(s);
  parse_result_free(&r);
  PASS();
}


static void test_latex_complex_num(void) {
  TEST("latex: complex number 2 + 3i");
  AstNode *n = ast_complex(2.0, 3.0);
  char *s = ast_to_latex(n);
  ASSERT_STR_EQ(s, "2 + 3i");
  free(s);
  ast_free(n);
  PASS();
}


static void test_latex_imaginary(void) {
  TEST("latex: pure imaginary 5i");
  AstNode *n = ast_complex(0.0, 5.0);
  char *s = ast_to_latex(n);
  ASSERT_STR_EQ(s, "5i");
  free(s);
  ast_free(n);
  PASS();
}

/* Solver */


void tests_latex_functional(void) {
  test_latex_number();
  test_latex_fraction();
  test_latex_elementary_functions();
  test_latex_variable();
  test_latex_pi();
  test_latex_multichar_var();
  test_latex_add();
  test_latex_sub();
  test_latex_mul_implicit();
  test_latex_mul_cdot();
  test_latex_div_frac();
  test_latex_pow();
  test_latex_nested_pow();
  test_latex_sqrt();
  test_latex_abs();
  test_latex_unary_neg();
  test_latex_matrix();
  test_latex_frac_no_parens();
  test_latex_inverse_pow();
  test_latex_complex_expr();
  test_latex_complex_num();
  test_latex_imaginary();
}
