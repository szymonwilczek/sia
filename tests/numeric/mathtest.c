#include "../test_support.h"
#include "../test_suites.h"

static void test_eval_exact_rational(void) {
  TEST("eval: 1/2 + 1/3 = 5/6");
  ParseResult r = parse("1/2 + 1/3");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_TRUE(e.value.exact);
  ASSERT_TRUE(fraction_is(e.value.re_q, 5, 6));
  ASSERT_TRUE(fraction_is_zero(e.value.im_q));
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}


static void test_fraction_arithmetic(void) {
  TEST("fraction: exact arithmetic");
  Fraction half = fraction_make(1, 2);
  Fraction third = fraction_make(1, 3);
  ASSERT_TRUE(fraction_eq(fraction_add(half, third), fraction_make(5, 6)));
  ASSERT_TRUE(fraction_eq(fraction_sub(half, third), fraction_make(1, 6)));
  ASSERT_TRUE(fraction_eq(fraction_mul(half, third), fraction_make(1, 6)));
  ASSERT_TRUE(fraction_eq(fraction_div(half, third), fraction_make(3, 2)));
  PASS();
}


static void test_complex_exact_arithmetic(void) {
  TEST("complex: exact rational propagation");
  Complex left = c_from_fractions(fraction_make(1, 2), fraction_make(1, 2));
  Complex right = c_from_fractions(fraction_make(1, 2), fraction_make(-1, 2));
  Complex prod = c_mul(left, right);
  ASSERT_TRUE(prod.exact);
  ASSERT_TRUE(fraction_is(prod.re_q, 1, 2));
  ASSERT_TRUE(fraction_is_zero(prod.im_q));
  Complex sum =
      c_add(c_from_fractions(fraction_make(1, 2), fraction_make(0, 1)),
            c_from_fractions(fraction_make(1, 3), fraction_make(0, 1)));
  ASSERT_TRUE(sum.exact);
  ASSERT_TRUE(fraction_is(sum.re_q, 5, 6));
  ASSERT_TRUE(fraction_is_zero(sum.im_q));
  PASS();
}


static void test_simplify_rational_add(void) {
  TEST("simplify: 1/2 + 1/3 -> 5/6");
  ParseResult r = parse("1/2 + 1/3");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "5/6");
  ASSERT_TRUE(s->type == AST_NUMBER && s->as.number.exact);
  ASSERT_TRUE(fraction_is(s->as.number.re_q, 5, 6));
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}


static void test_simplify_rational_pow(void) {
  TEST("simplify: (1/2)^2 -> 1/4");
  ParseResult r = parse("(1/2)^2");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "1/4");
  ASSERT_TRUE(s->type == AST_NUMBER && s->as.number.exact);
  ASSERT_TRUE(fraction_is(s->as.number.re_q, 1, 4));
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}


static void test_simplify_exact_complex_product(void) {
  TEST("simplify: (1/2 + i/2) * (1/2 - i/2) -> 1/2");
  ParseResult r = parse("(1/2 + i/2) * (1/2 - i/2)");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "1/2");
  ASSERT_TRUE(s->type == AST_NUMBER && s->as.number.exact);
  ASSERT_TRUE(fraction_is(s->as.number.re_q, 1, 2));
  ASSERT_TRUE(fraction_is_zero(s->as.number.im_q));
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}


void tests_numeric_mathtest(void) {
  test_eval_exact_rational();
  test_fraction_arithmetic();
  test_complex_exact_arithmetic();
  test_simplify_rational_add();
  test_simplify_rational_pow();
  test_simplify_exact_complex_product();
}
