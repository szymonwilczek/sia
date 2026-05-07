#include "../test_support.h"
#include "../test_suites.h"

static void test_eval_number_theory(void) {
  TEST("eval: gcd/lcm exact integers");

  ParseResult r1 = parse("gcd(24, 18)");
  EvalResult e1 = eval(r1.root, NULL);
  ASSERT_TRUE(e1.ok);
  ASSERT_TRUE(e1.value.exact);
  ASSERT_TRUE(fraction_is(e1.value.re_q, 6, 1));
  ASSERT_TRUE(fraction_is_zero(e1.value.im_q));
  eval_result_free(&e1);
  parse_result_free(&r1);

  ParseResult r2 = parse("lcm(6, 8)");
  EvalResult e2 = eval(r2.root, NULL);
  ASSERT_TRUE(e2.ok);
  ASSERT_TRUE(e2.value.exact);
  ASSERT_TRUE(fraction_is(e2.value.re_q, 24, 1));
  ASSERT_TRUE(fraction_is_zero(e2.value.im_q));
  eval_result_free(&e2);
  parse_result_free(&r2);

  ParseResult r3 = parse("gcd(12/3, 18/3)");
  EvalResult e3 = eval(r3.root, NULL);
  ASSERT_TRUE(e3.ok);
  ASSERT_TRUE(e3.value.exact);
  ASSERT_TRUE(fraction_is(e3.value.re_q, 2, 1));
  ASSERT_TRUE(fraction_is_zero(e3.value.im_q));
  eval_result_free(&e3);
  parse_result_free(&r3);

  PASS();
}


static void test_eval_number_theory_overflow(void) {
  TEST("eval: lcm overflow detection");
  ParseResult r = parse("lcm(3037000500, 3037000501)");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(!e.ok);
  ASSERT_TRUE(e.error != NULL);
  ASSERT_TRUE(strstr(e.error, "overflow") != NULL);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}


void tests_number_theory_functional(void) {
  test_eval_number_theory();
  test_eval_number_theory_overflow();
}
