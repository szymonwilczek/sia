#include "../test_support.h"
#include "../test_suites.h"

static void test_eval_arithmetic(void) {
  TEST("eval: basic arithmetic 2+3*4");
  ParseResult r = parse("2+3*4");
  ASSERT_TRUE(r.root != NULL);
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_CNEAR(e.value, c_real(14.0), 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}


static void test_eval_power(void) {
  TEST("eval: power 2^10");
  ParseResult r = parse("2^10");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_CNEAR(e.value, c_real(1024.0), 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}


static void test_eval_functions(void) {
  TEST("eval: sin(0) = 0");
  ParseResult r = parse("sin(0)");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_CNEAR(e.value, c_real(0.0), 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}


static void test_eval_cos(void) {
  TEST("eval: cos(0) = 1");
  ParseResult r = parse("cos(0)");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_CNEAR(e.value, c_real(1.0), 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}


static void test_eval_sqrt(void) {
  TEST("eval: sqrt(144) = 12");
  ParseResult r = parse("sqrt(144)");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_CNEAR(e.value, c_real(12.0), 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}


static void test_eval_inverse_trig(void) {
  TEST("eval: asin(0), acos(1), atan(0)");

  ParseResult r1 = parse("asin(0)");
  EvalResult e1 = eval(r1.root, NULL);
  ASSERT_TRUE(e1.ok);
  ASSERT_TRUE(e1.value.exact);
  ASSERT_TRUE(fraction_is_zero(e1.value.re_q));
  ASSERT_TRUE(fraction_is_zero(e1.value.im_q));
  eval_result_free(&e1);
  parse_result_free(&r1);

  ParseResult r2 = parse("acos(1)");
  EvalResult e2 = eval(r2.root, NULL);
  ASSERT_TRUE(e2.ok);
  ASSERT_TRUE(e2.value.exact);
  ASSERT_TRUE(fraction_is_zero(e2.value.re_q));
  ASSERT_TRUE(fraction_is_zero(e2.value.im_q));
  eval_result_free(&e2);
  parse_result_free(&r2);

  ParseResult r3 = parse("atan(0)");
  EvalResult e3 = eval(r3.root, NULL);
  ASSERT_TRUE(e3.ok);
  ASSERT_TRUE(e3.value.exact);
  ASSERT_TRUE(fraction_is_zero(e3.value.re_q));
  ASSERT_TRUE(fraction_is_zero(e3.value.im_q));
  eval_result_free(&e3);
  parse_result_free(&r3);

  PASS();
}


static void test_eval_elementary_functions(void) {
  TEST("eval: abs(-16), log(1000, 10), log(8, 2)");

  ParseResult r1 = parse("abs(-16)");
  EvalResult e1 = eval(r1.root, NULL);
  ASSERT_TRUE(e1.ok);
  ASSERT_CNEAR(e1.value, c_real(16.0), 1e-9);
  eval_result_free(&e1);
  parse_result_free(&r1);

  ParseResult r2 = parse("log(1000, 10)");
  EvalResult e2 = eval(r2.root, NULL);
  ASSERT_TRUE(e2.ok);
  ASSERT_CNEAR(e2.value, c_real(3.0), 1e-9);
  eval_result_free(&e2);
  parse_result_free(&r2);

  ParseResult r3 = parse("log(8, 2)");
  EvalResult e3 = eval(r3.root, NULL);
  ASSERT_TRUE(e3.ok);
  ASSERT_CNEAR(e3.value, c_real(3.0), 1e-9);
  eval_result_free(&e3);
  parse_result_free(&r3);

  PASS();
}


static void test_eval_pi(void) {
  TEST("eval: pi constant");
  ParseResult r = parse("pi");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_CNEAR(e.value, c_real(M_PI), 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}


static void test_eval_complex_expr(void) {
  TEST("eval: (3+4)*(2-1)^2 = 7");
  ParseResult r = parse("(3+4)*(2-1)^2");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_CNEAR(e.value, c_real(7.0), 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}


static void test_eval_div_zero(void) {
  TEST("eval: division by zero error");
  ParseResult r = parse("1/0");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(!e.ok);
  ASSERT_TRUE(e.error != NULL);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}


static void test_eval_nested_func(void) {
  TEST("eval: sqrt(abs(-16)) = 4");
  ParseResult r = parse("sqrt(abs(-16))");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_CNEAR(e.value, c_real(4.0), 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}


static void test_eval_symtab(void) {
  TEST("eval: symbol table lookup");
  SymTab st;
  symtab_init(&st);
  symtab_set(&st, "g", c_real(9.81));
  ParseResult r = parse("g");
  EvalResult e = eval(r.root, &st);
  ASSERT_TRUE(e.ok);
  ASSERT_CNEAR(e.value, c_real(9.81), 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  symtab_free(&st);
  PASS();
}


static void test_eval_hyperbolic(void) {
  TEST("eval: sinh(0) = 0, cosh(0) = 1");
  ParseResult r1 = parse("sinh(0)");
  EvalResult e1 = eval(r1.root, NULL);
  ASSERT_TRUE(e1.ok);
  ASSERT_CNEAR(e1.value, c_real(0.0), 1e-9);
  eval_result_free(&e1);
  parse_result_free(&r1);

  ParseResult r2 = parse("cosh(0)");
  EvalResult e2 = eval(r2.root, NULL);
  ASSERT_TRUE(e2.ok);
  ASSERT_CNEAR(e2.value, c_real(1.0), 1e-9);
  eval_result_free(&e2);
  parse_result_free(&r2);
  PASS();
}


static void test_eval_complex_domain(void) {
  TEST("eval: complex results for ln(-1), sqrt(-1), asin(2)");
  ParseResult r1 = parse("ln(-1)");
  EvalResult e1 = eval(r1.root, NULL);
  ASSERT_TRUE(e1.ok);
  ASSERT_NEAR(e1.value.re, 0.0, 1e-9);
  ASSERT_NEAR(fabs(e1.value.im), M_PI, 1e-9);
  eval_result_free(&e1);
  parse_result_free(&r1);

  ParseResult r2 = parse("sqrt(-1)");
  EvalResult e2 = eval(r2.root, NULL);
  ASSERT_TRUE(e2.ok);
  ASSERT_NEAR(e2.value.re, 0.0, 1e-9);
  ASSERT_NEAR(e2.value.im, 1.0, 1e-9);
  eval_result_free(&e2);
  parse_result_free(&r2);

  ParseResult r3 = parse("log(0)");
  EvalResult e3 = eval(r3.root, NULL);
  ASSERT_TRUE(!e3.ok); /* log(0) is still -inf error */
  eval_result_free(&e3);
  parse_result_free(&r3);

  ParseResult r4 = parse("asin(2)");
  EvalResult e4 = eval(r4.root, NULL);
  ASSERT_TRUE(e4.ok);
  /* asin(2) = pi/2 - i*ln(2+sqrt(3)) */
  ASSERT_NEAR(e4.value.re, M_PI / 2.0, 1e-9);
  ASSERT_NEAR(e4.value.im, -1.3169578969, 1e-7);
  eval_result_free(&e4);
  parse_result_free(&r4);
  PASS();
}

/* Symbolic */


void tests_evaluation_functional(void) {
  test_eval_arithmetic();
  test_eval_power();
  test_eval_functions();
  test_eval_cos();
  test_eval_sqrt();
  test_eval_inverse_trig();
  test_eval_elementary_functions();
  test_eval_pi();
  test_eval_complex_expr();
  test_eval_div_zero();
  test_eval_nested_func();
  test_eval_symtab();
  test_eval_hyperbolic();
  test_eval_complex_domain();
}
