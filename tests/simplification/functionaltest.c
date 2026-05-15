#include "../test_suites.h"
#include "../test_support.h"

static void test_simplify_zero_mul(void) {
  TEST("simplify: 0 * x -> 0");
  ParseResult r = parse("0 * x");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_TRUE(is_num_node(s, 0));
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_identity_add(void) {
  TEST("simplify: x + 0 -> x");
  ParseResult r = parse("x + 0");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_EQ(s->type, AST_VARIABLE);
  ASSERT_STR_EQ(s->as.variable, "x");
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_const_fold(void) {
  TEST("simplify: 2 * 3 -> 6");
  ParseResult r = parse("2 * 3");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_EQ(s->type, AST_NUMBER);
  ASSERT_CNEAR(s->as.number, c_real(6.0), 1e-9);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_x_minus_x(void) {
  TEST("simplify: x - x -> 0");
  ParseResult r = parse("x - x");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_TRUE(is_num_node(s, 0));
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_x_plus_x(void) {
  TEST("simplify: x + x -> 2*x");
  ParseResult r = parse("x + x");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "2*x");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_x_times_x(void) {
  TEST("simplify: x * x -> x^2");
  ParseResult r = parse("x * x");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "x^2");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_x_div_x(void) {
  TEST("simplify: x / x -> 1");
  ParseResult r = parse("x / x");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_TRUE(is_num_node(s, 1));
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_power_of_power(void) {
  TEST("simplify: (x^2)^3 -> x^6");
  ParseResult r = parse("(x^2)^3");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "x^6");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_one_pow(void) {
  TEST("simplify: 1^x -> 1");
  ParseResult r = parse("1^x");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_TRUE(is_num_node(s, 1));
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_double_neg(void) {
  TEST("simplify: -(-x) -> x");
  AstNode *x = ast_variable("x", 1);
  AstNode *neg = ast_unary_neg(ast_unary_neg(x));
  AstNode *s = sym_simplify(neg);
  ASSERT_EQ(s->type, AST_VARIABLE);
  ASSERT_STR_EQ(s->as.variable, "x");
  ast_free(s);
  PASS();
}

static void test_simplify_mul_neg_one(void) {
  TEST("simplify: (-1)*x -> (-x)");
  ParseResult r = parse("(-1)*x");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_EQ(s->type, AST_UNARY_NEG);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_coeff_merge(void) {
  TEST("simplify: 2*(3*x) -> 6*x");
  ParseResult r = parse("2*(3*x)");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "6*x");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_constant_hoisting(void) {
  TEST("simplify: x*2*y*3 -> 6*x*y");
  ParseResult r = parse("x*2*y*3");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "6*x*y");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_mul_reciprocal(void) {
  TEST("simplify: x * (1/x) -> 1");
  ParseResult r = parse("x * (1/x)");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_TRUE(is_num_node(s, 1));
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_cancel_div(void) {
  TEST("simplify: x * (y/x) -> y");
  ParseResult r = parse("x * (y/x)");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_EQ(s->type, AST_VARIABLE);
  ASSERT_STR_EQ(s->as.variable, "y");
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_exp_ln(void) {
  TEST("simplify: exp(ln(x)) -> x");
  ParseResult r = parse("exp(ln(x))");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_EQ(s->type, AST_VARIABLE);
  ASSERT_STR_EQ(s->as.variable, "x");
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_elementary_functions(void) {
  TEST("simplify: abs(-16), sqrt(49), log(1000, 10), log(8, 2)");

  ParseResult r1 = parse("abs(-16)");
  AstNode *s1 = sym_simplify(ast_clone(r1.root));
  ASSERT_TRUE(is_num_node(s1, 16));
  ast_free(s1);
  parse_result_free(&r1);

  ParseResult r2 = parse("sqrt(49)");
  AstNode *s2 = sym_simplify(ast_clone(r2.root));
  ASSERT_TRUE(is_num_node(s2, 7));
  ast_free(s2);
  parse_result_free(&r2);

  ParseResult r3 = parse("log(1000, 10)");
  AstNode *s3 = sym_simplify(ast_clone(r3.root));
  ASSERT_TRUE(is_num_node(s3, 3));
  ast_free(s3);
  parse_result_free(&r3);

  ParseResult r4 = parse("log(8, 2)");
  AstNode *s4 = sym_simplify(ast_clone(r4.root));
  ASSERT_TRUE(is_num_node(s4, 3));
  ast_free(s4);
  parse_result_free(&r4);

  PASS();
}

static void test_simplify_distributive(void) {
  TEST("simplify: (x^3 + x^2) * x^(-2) -> x + 1");
  ParseResult r = parse("(x^3 + x^2) * x^(-2)");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "x + 1");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_expand(void) {
  TEST("expand: (x+1)^2 -> 1 + 2*x + x^2 (or similar collected form)");
  ParseResult r = parse("(x+1)^2");
  AstNode *expr = ast_clone(r.root);
  expr = sym_expand(expr);
  expr = ast_canonicalize(expr);
  expr = sym_collect_terms(expr);
  char *str = ast_to_string(expr);
  ASSERT_STR_EQ(str, "1 + 2*x + x^2");
  free(str);
  ast_free(expr);
  parse_result_free(&r);
  PASS();
}

void tests_simplification_functional(void) {
  test_simplify_zero_mul();
  test_simplify_identity_add();
  test_simplify_const_fold();
  test_simplify_x_minus_x();
  test_simplify_x_plus_x();
  test_simplify_x_times_x();
  test_simplify_x_div_x();
  test_simplify_power_of_power();
  test_simplify_one_pow();
  test_simplify_double_neg();
  test_simplify_mul_neg_one();
  test_simplify_coeff_merge();
  test_simplify_constant_hoisting();
  test_simplify_mul_reciprocal();
  test_simplify_cancel_div();
  test_simplify_exp_ln();
  test_simplify_elementary_functions();
  test_simplify_distributive();
  test_expand();
}
