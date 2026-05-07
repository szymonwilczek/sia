#include "../test_support.h"
#include "../test_suites.h"

static void test_eval_factorial(void) {
  TEST("eval: 5! and factorial(5) = 120");

  ParseResult r1 = parse("5!");
  EvalResult e1 = eval(r1.root, NULL);
  ASSERT_TRUE(e1.ok);
  ASSERT_TRUE(e1.value.exact);
  ASSERT_TRUE(fraction_is(e1.value.re_q, 120, 1));
  ASSERT_TRUE(fraction_is_zero(e1.value.im_q));
  eval_result_free(&e1);
  parse_result_free(&r1);

  ParseResult r2 = parse("factorial(5)");
  EvalResult e2 = eval(r2.root, NULL);
  ASSERT_TRUE(e2.ok);
  ASSERT_TRUE(e2.value.exact);
  ASSERT_TRUE(fraction_is(e2.value.re_q, 120, 1));
  ASSERT_TRUE(fraction_is_zero(e2.value.im_q));
  eval_result_free(&e2);
  parse_result_free(&r2);

  PASS();
}


static void test_eval_factorial_overflow(void) {
  TEST("eval: factorial overflow on 21!");
  ParseResult r = parse("21!");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(!e.ok);
  ASSERT_TRUE(e.error != NULL);
  ASSERT_TRUE(strstr(e.error, "overflow") != NULL);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}


static void test_simplify_factorial(void) {
  TEST("simplify: 5! and factorial(5) -> 120");

  ParseResult r1 = parse("5!");
  AstNode *s1 = sym_simplify(ast_clone(r1.root));
  ASSERT_TRUE(is_num_node(s1, 120));
  ast_free(s1);
  parse_result_free(&r1);

  ParseResult r2 = parse("factorial(5)");
  AstNode *s2 = sym_simplify(ast_clone(r2.root));
  ASSERT_TRUE(is_num_node(s2, 120));
  ast_free(s2);
  parse_result_free(&r2);

  PASS();
}


static void test_latex_factorial(void) {
  TEST("latex: factorial postfix");
  ParseResult r = parse("(x+1)!");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "\\left(x + 1\\right)!");
  free(s);
  parse_result_free(&r);
  PASS();
}


void tests_factorial_functional(void) {
  test_eval_factorial();
  test_eval_factorial_overflow();
  test_simplify_factorial();
  test_latex_factorial();
}
