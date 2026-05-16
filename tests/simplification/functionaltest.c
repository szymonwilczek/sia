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

static void test_simplify_power_grouping(void) {
  TEST("simplify: x*y*x^2 -> x^3*y");
  ParseResult r = parse("x*y*x^2");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "x^3*y");
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
  TEST("simplify: exp/ln exact symbolic constants");
  ParseResult r = parse("exp(ln(x))");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_EQ(s->type, AST_VARIABLE);
  ASSERT_STR_EQ(s->as.variable, "x");
  ast_free(s);
  parse_result_free(&r);

  ParseResult r2 = parse("exp(1)");
  AstNode *s2 = sym_simplify(ast_clone(r2.root));
  ASSERT_EQ(s2->type, AST_VARIABLE);
  ASSERT_STR_EQ(s2->as.variable, "e");
  ast_free(s2);
  parse_result_free(&r2);

  ParseResult r3 = parse("exp(2)");
  AstNode *s3 = sym_simplify(ast_clone(r3.root));
  char *str3 = ast_to_string(s3);
  ASSERT_STR_EQ(str3, "e^2");
  free(str3);
  ast_free(s3);
  parse_result_free(&r3);

  ParseResult r4 = parse("ln(e)");
  AstNode *s4 = sym_simplify(ast_clone(r4.root));
  ASSERT_TRUE(is_num_node(s4, 1));
  ast_free(s4);
  parse_result_free(&r4);

  ParseResult r5 = parse("ln(exp(x))");
  AstNode *s5 = sym_simplify(ast_clone(r5.root));
  ASSERT_EQ(s5->type, AST_VARIABLE);
  ASSERT_STR_EQ(s5->as.variable, "x");
  ast_free(s5);
  parse_result_free(&r5);

  PASS();
}

static void test_simplify_sqrt_power_domain_guard(void) {
  TEST("simplify: sqrt powers preserve unknown domain");

  ParseResult r1 = parse("sqrt(x)^2");
  AstNode *s1 = sym_full_simplify(ast_clone(r1.root));
  char *str1 = ast_to_string(s1);
  ASSERT_STR_EQ(str1, "sqrt(x)^2");
  free(str1);
  ast_free(s1);
  parse_result_free(&r1);

  ParseResult r2 = parse("sqrt(abs(x))^2");
  AstNode *s2 = sym_full_simplify(ast_clone(r2.root));
  char *str2 = ast_to_string(s2);
  ASSERT_STR_EQ(str2, "abs(x)");
  free(str2);
  ast_free(s2);
  parse_result_free(&r2);

  ParseResult r3 = parse("sqrt(x)^4");
  AstNode *s3 = sym_full_simplify(ast_clone(r3.root));
  char *str3 = ast_to_string(s3);
  ASSERT_STR_EQ(str3, "sqrt(x)^4");
  free(str3);
  ast_free(s3);
  parse_result_free(&r3);

  ParseResult r4 = parse("sqrt(x^2)");
  AstNode *s4 = sym_full_simplify(ast_clone(r4.root));
  char *str4 = ast_to_string(s4);
  ASSERT_STR_EQ(str4, "abs(x)");
  free(str4);
  ast_free(s4);
  parse_result_free(&r4);

  PASS();
}

static void test_simplify_log_rules_with_domains(void) {
  TEST("simplify: logarithm rules require positive domains");

  ParseResult r1 = parse("ln(x*y)");
  AstNode *e1 = sym_expand(r1.root);
  char *s1 = ast_to_string(e1);
  ASSERT_STR_EQ(s1, "ln(x*y)");
  free(s1);
  ast_free(e1);
  parse_result_free(&r1);

  ParseResult r2 = parse("ln(exp(x)*2)");
  AstNode *e2 = sym_expand(r2.root);
  char *s2 = ast_to_string(e2);
  ASSERT_TRUE(strcmp(s2, "x + ln(2)") == 0 || strcmp(s2, "ln(2) + x") == 0);
  free(s2);
  ast_free(e2);
  parse_result_free(&r2);

  ParseResult r3 = parse("log(2^x, 2)");
  AstNode *s3 = sym_simplify(ast_clone(r3.root));
  ASSERT_EQ(s3->type, AST_VARIABLE);
  ASSERT_STR_EQ(s3->as.variable, "x");
  ast_free(s3);
  parse_result_free(&r3);

  ParseResult r4 = parse("ln(2)+ln(3)");
  AstNode *s4 = sym_simplify(ast_clone(r4.root));
  char *str4 = ast_to_string(s4);
  ASSERT_STR_EQ(str4, "ln(6)");
  free(str4);
  ast_free(s4);
  parse_result_free(&r4);

  ParseResult r5 = parse("2*ln(3)");
  AstNode *s5 = sym_simplify(ast_clone(r5.root));
  char *str5 = ast_to_string(s5);
  ASSERT_STR_EQ(str5, "ln(9)");
  free(str5);
  ast_free(s5);
  parse_result_free(&r5);

  ParseResult r6 = parse("exp(x+y)");
  AstNode *s6 = sym_simplify(ast_clone(r6.root));
  char *str6 = ast_to_string(s6);
  ASSERT_TRUE(strcmp(str6, "exp(x)*exp(y)") == 0 ||
              strcmp(str6, "exp(y)*exp(x)") == 0);
  free(str6);
  ast_free(s6);
  parse_result_free(&r6);

  PASS();
}

static void test_factor_polynomials(void) {
  TEST("factor: polynomial difference of squares and common factor");

  ParseResult r1 = parse("x^2 - 1");
  AstNode *f1 = sym_factor(ast_clone(r1.root));
  char *s1 = ast_to_string(f1);
  ASSERT_TRUE(strstr(s1, "x - 1") != NULL);
  ASSERT_TRUE(strstr(s1, "1 + x") != NULL || strstr(s1, "x + 1") != NULL);
  ASSERT_TRUE(strstr(s1, "*") != NULL);
  free(s1);
  ast_free(f1);
  parse_result_free(&r1);

  ParseResult r2 = parse("x^2 + x");
  AstNode *f2 = sym_factor(ast_clone(r2.root));
  char *s2 = ast_to_string(f2);
  ASSERT_TRUE(strstr(s2, "x*") != NULL || strstr(s2, "*x") != NULL);
  ASSERT_TRUE(strstr(s2, "1 + x") != NULL || strstr(s2, "x + 1") != NULL);
  free(s2);
  ast_free(f2);
  parse_result_free(&r2);

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

static void test_full_simplify_distribution(void) {
  TEST("simplify: (x + 1)*(x + 2) -> 2 + 3*x + x^2");
  ParseResult r = parse("(x + 1)*(x + 2)");
  AstNode *s = sym_full_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "2 + 3*x + x^2");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_full_simplify_abs_power_identity(void) {
  TEST("simplify: x^2*abs(x)^-3 -> abs(x)^-1");
  ParseResult r = parse("x^2*abs(x)^-3");
  AstNode *s = sym_full_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "abs(x)^-1");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_full_simplify_conjugate_radical_squares(void) {
  TEST("simplify: conjugate radical squares cancel");
  ParseResult r = parse("(x+sqrt(2))^2*(x-sqrt(2))^2 - (x^2-2)^2");
  AstNode *s = sym_full_simplify(ast_clone(r.root));
  ASSERT_TRUE(is_num_node(s, 0));
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_full_simplify_common_inverse_factor(void) {
  TEST("simplify: common inverse factor cancels");
  ParseResult r = parse("-2*(-2 + x^2)^-1 + x^2*(-2 + x^2)^-1");
  AstNode *s = sym_full_simplify(ast_clone(r.root));
  ASSERT_TRUE(is_num_node(s, 1));
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

static void test_poly_gcd_basic(void) {
  TEST("simplify: (x^5+2x^3+x)/(x^4+2x^2+1) -> x");
  ParseResult r = parse("(x^5 + 2*x^3 + x) / (x^4 + 2*x^2 + 1)");
  AstNode *s = sym_full_simplify(ast_clone(r.root));
  ASSERT_EQ(s->type, AST_VARIABLE);
  ASSERT_STR_EQ(s->as.variable, "x");
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_poly_gcd_linear(void) {
  TEST("simplify: (x^2-1)/(x-1) -> 1 + x");
  ParseResult r = parse("(x^2 - 1) / (x - 1)");
  AstNode *s = sym_full_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "1 + x");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_poly_gcd_quadratic(void) {
  TEST("simplify: (x^3+x)/(x^2+1) -> x");
  ParseResult r = parse("(x^3 + x) / (x^2 + 1)");
  AstNode *s = sym_full_simplify(ast_clone(r.root));
  ASSERT_EQ(s->type, AST_VARIABLE);
  ASSERT_STR_EQ(s->as.variable, "x");
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_common_denom_grouping(void) {
  TEST("simplify: d/dx(int(x/(x^2+1)^2))*(x^2+1)^2 -> x");
  ParseResult r = parse("x / (x^2 + 1)^2");
  ParseResult r2 = parse("(x^2 + 1)^2");
  AstNode *integral = sym_integrate(r.root, "x");
  AstNode *deriv = sym_diff(integral, "x");
  AstNode *expr = ast_binop(OP_MUL, deriv, ast_clone(r2.root));
  AstNode *s = sym_full_simplify(expr);
  ASSERT_EQ(s->type, AST_VARIABLE);
  ASSERT_STR_EQ(s->as.variable, "x");
  ast_free(s);
  ast_free(integral);
  parse_result_free(&r);
  parse_result_free(&r2);
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
  test_simplify_power_grouping();
  test_simplify_mul_reciprocal();
  test_simplify_cancel_div();
  test_simplify_exp_ln();
  test_simplify_sqrt_power_domain_guard();
  test_simplify_log_rules_with_domains();
  test_factor_polynomials();
  test_simplify_elementary_functions();
  test_simplify_distributive();
  test_full_simplify_distribution();
  test_full_simplify_abs_power_identity();
  test_full_simplify_conjugate_radical_squares();
  test_full_simplify_common_inverse_factor();
  test_expand();
  test_poly_gcd_basic();
  test_poly_gcd_linear();
  test_poly_gcd_quadratic();
  test_common_denom_grouping();
}
