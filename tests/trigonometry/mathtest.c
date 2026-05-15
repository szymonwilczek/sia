#include "../test_suites.h"
#include "../test_support.h"

static void test_simplify_trig_tan(void) {
  TEST("simplify: sin(x)/cos(x) -> tan(x)");
  ParseResult r = parse("sin(x)/cos(x)");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "tan(x)");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_trig_pythagorean(void) {
  TEST("simplify: sin(x)^2 + cos(x)^2 -> 1");
  ParseResult r = parse("sin(x)^2 + cos(x)^2");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_TRUE(is_num_node(s, 1));
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_inverse_trig(void) {
  TEST("simplify: inverse trig constants and identities");

  ParseResult r1 = parse("asin(0)");
  AstNode *s1 = sym_simplify(ast_clone(r1.root));
  ASSERT_TRUE(is_num_node(s1, 0));
  ast_free(s1);
  parse_result_free(&r1);

  ParseResult r2 = parse("acos(1)");
  AstNode *s2 = sym_simplify(ast_clone(r2.root));
  ASSERT_TRUE(is_num_node(s2, 0));
  ast_free(s2);
  parse_result_free(&r2);

  ParseResult r3 = parse("atan(0)");
  AstNode *s3 = sym_simplify(ast_clone(r3.root));
  ASSERT_TRUE(is_num_node(s3, 0));
  ast_free(s3);
  parse_result_free(&r3);

  ParseResult r4 = parse("asin(1)");
  AstNode *s4 = sym_simplify(ast_clone(r4.root));
  char *str4 = ast_to_string(s4);
  ASSERT_STR_EQ(str4, "pi/2");
  free(str4);
  ast_free(s4);
  parse_result_free(&r4);

  ParseResult r5 = parse("acos(0)");
  AstNode *s5 = sym_simplify(ast_clone(r5.root));
  char *str5 = ast_to_string(s5);
  ASSERT_STR_EQ(str5, "pi/2");
  free(str5);
  ast_free(s5);
  parse_result_free(&r5);

  ParseResult r6 = parse("atan(1)");
  AstNode *s6 = sym_simplify(ast_clone(r6.root));
  char *str6 = ast_to_string(s6);
  ASSERT_STR_EQ(str6, "pi/4");
  free(str6);
  ast_free(s6);
  parse_result_free(&r6);

  ParseResult r7 = parse("sin(asin(x))");
  AstNode *s7 = sym_simplify(ast_clone(r7.root));
  char *str7 = ast_to_string(s7);
  ASSERT_STR_EQ(str7, "x");
  free(str7);
  ast_free(s7);
  parse_result_free(&r7);

  ParseResult r8 = parse("cos(acos(x))");
  AstNode *s8 = sym_simplify(ast_clone(r8.root));
  char *str8 = ast_to_string(s8);
  ASSERT_STR_EQ(str8, "x");
  free(str8);
  ast_free(s8);
  parse_result_free(&r8);

  ParseResult r9 = parse("tan(atan(x))");
  AstNode *s9 = sym_simplify(ast_clone(r9.root));
  char *str9 = ast_to_string(s9);
  ASSERT_STR_EQ(str9, "x");
  free(str9);
  ast_free(s9);
  parse_result_free(&r9);

  ParseResult r10 = parse("asin(sin(x))");
  AstNode *s10 = sym_simplify(ast_clone(r10.root));
  char *str10 = ast_to_string(s10);
  ASSERT_STR_EQ(str10, "x");
  free(str10);
  ast_free(s10);
  parse_result_free(&r10);

  ParseResult r11 = parse("acos(cos(x))");
  AstNode *s11 = sym_simplify(ast_clone(r11.root));
  char *str11 = ast_to_string(s11);
  ASSERT_STR_EQ(str11, "x");
  free(str11);
  ast_free(s11);
  parse_result_free(&r11);

  ParseResult r12 = parse("atan(tan(x))");
  AstNode *s12 = sym_simplify(ast_clone(r12.root));
  char *str12 = ast_to_string(s12);
  ASSERT_STR_EQ(str12, "x");
  free(str12);
  ast_free(s12);
  parse_result_free(&r12);

  ParseResult r13 = parse("asin(-x)");
  AstNode *s13 = sym_simplify(ast_clone(r13.root));
  char *str13 = ast_to_string(s13);
  ASSERT_STR_EQ(str13, "(-asin(x))");
  free(str13);
  ast_free(s13);
  parse_result_free(&r13);

  ParseResult r14 = parse("atan(-x)");
  AstNode *s14 = sym_simplify(ast_clone(r14.root));
  char *str14 = ast_to_string(s14);
  ASSERT_STR_EQ(str14, "(-atan(x))");
  free(str14);
  ast_free(s14);
  parse_result_free(&r14);

  PASS();
}

static void test_diff_sin(void) {
  TEST("diff: d/dx(sin(x)) = cos(x)");
  ParseResult r = parse("sin(x)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "cos(x)");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_cos(void) {
  TEST("diff: d/dx(cos(x)) = -sin(x)");
  ParseResult r = parse("cos(x)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "(-sin(x))");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_asin(void) {
  TEST("diff: d/dx(asin(x))");
  ParseResult r = parse("asin(x)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "1/sqrt(1 - x^2)");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_acos(void) {
  TEST("diff: d/dx(acos(x))");
  ParseResult r = parse("acos(x)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "(-1/sqrt(1 - x^2))");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_atan(void) {
  TEST("diff: d/dx(atan(x)) = 1/(1+x^2)");
  ParseResult r = parse("atan(x)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "1/(1 + x^2)");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_hyperbolic(void) {
  TEST("simplify: hyperbolic constants and parity");

  ParseResult r1 = parse("sinh(0)");
  AstNode *s1 = sym_simplify(ast_clone(r1.root));
  ASSERT_TRUE(is_num_node(s1, 0));
  ast_free(s1);
  parse_result_free(&r1);

  ParseResult r2 = parse("cosh(0)");
  AstNode *s2 = sym_simplify(ast_clone(r2.root));
  ASSERT_TRUE(is_num_node(s2, 1));
  ast_free(s2);
  parse_result_free(&r2);

  ParseResult r3 = parse("tanh(0)");
  AstNode *s3 = sym_simplify(ast_clone(r3.root));
  ASSERT_TRUE(is_num_node(s3, 0));
  ast_free(s3);
  parse_result_free(&r3);

  ParseResult r4 = parse("sinh(-x)");
  AstNode *s4 = sym_simplify(ast_clone(r4.root));
  char *str4 = ast_to_string(s4);
  ASSERT_STR_EQ(str4, "(-sinh(x))");
  free(str4);
  ast_free(s4);
  parse_result_free(&r4);

  ParseResult r5 = parse("cosh(-x)");
  AstNode *s5 = sym_simplify(ast_clone(r5.root));
  char *str5 = ast_to_string(s5);
  ASSERT_STR_EQ(str5, "cosh(x)");
  free(str5);
  ast_free(s5);
  parse_result_free(&r5);

  ParseResult r6 = parse("tanh(-x)");
  AstNode *s6 = sym_simplify(ast_clone(r6.root));
  char *str6 = ast_to_string(s6);
  ASSERT_STR_EQ(str6, "(-tanh(x))");
  free(str6);
  ast_free(s6);
  parse_result_free(&r6);

  PASS();
}

static void test_simplify_hyperbolic_identities(void) {
  TEST("simplify: hyperbolic identities");

  ParseResult r1 = parse("sinh(x)/cosh(x)");
  AstNode *s1 = sym_simplify(ast_clone(r1.root));
  char *str1 = ast_to_string(s1);
  ASSERT_STR_EQ(str1, "tanh(x)");
  free(str1);
  ast_free(s1);
  parse_result_free(&r1);

  ParseResult r2 = parse("cosh(x)^2 - sinh(x)^2");
  AstNode *s2 = sym_simplify(ast_clone(r2.root));
  ASSERT_TRUE(is_num_node(s2, 1));
  ast_free(s2);
  parse_result_free(&r2);

  ParseResult r3 = parse("sinh(x)^2 - cosh(x)^2");
  AstNode *s3 = sym_simplify(ast_clone(r3.root));
  ASSERT_TRUE(is_num_node(s3, -1));
  ast_free(s3);
  parse_result_free(&r3);

  ParseResult r4 = parse("sinh(i*x)");
  AstNode *s4 = sym_simplify(ast_clone(r4.root));
  char *str4 = ast_to_string(s4);
  ASSERT_STR_EQ(str4, "i*sin(x)");
  free(str4);
  ast_free(s4);
  parse_result_free(&r4);

  ParseResult r5 = parse("cosh(i*x)");
  AstNode *s5 = sym_simplify(ast_clone(r5.root));
  char *str5 = ast_to_string(s5);
  ASSERT_STR_EQ(str5, "cos(x)");
  free(str5);
  ast_free(s5);
  parse_result_free(&r5);

  ParseResult r6 = parse("tanh(i*x)");
  AstNode *s6 = sym_simplify(ast_clone(r6.root));
  char *str6 = ast_to_string(s6);
  ASSERT_STR_EQ(str6, "i*tan(x)");
  free(str6);
  ast_free(s6);
  parse_result_free(&r6);

  PASS();
}

static void test_diff_hyperbolic(void) {
  TEST("diff: d/dx(sinh/cosh/tanh)");

  ParseResult r1 = parse("sinh(x)");
  AstNode *d1 = sym_diff(r1.root, "x");
  ASSERT_TRUE(d1 != NULL);
  char *s1 = ast_to_string(d1);
  ASSERT_STR_EQ(s1, "cosh(x)");
  free(s1);
  ast_free(d1);
  parse_result_free(&r1);

  ParseResult r2 = parse("cosh(x)");
  AstNode *d2 = sym_diff(r2.root, "x");
  ASSERT_TRUE(d2 != NULL);
  char *s2 = ast_to_string(d2);
  ASSERT_STR_EQ(s2, "sinh(x)");
  free(s2);
  ast_free(d2);
  parse_result_free(&r2);

  ParseResult r3 = parse("tanh(x)");
  AstNode *d3 = sym_diff(r3.root, "x");
  ASSERT_TRUE(d3 != NULL);
  char *s3 = ast_to_string(d3);
  ASSERT_STR_EQ(s3, "1/cosh(x)^2");
  free(s3);
  ast_free(d3);
  parse_result_free(&r3);

  PASS();
}

static void test_trig_numeric_roundtrip_identities(void) {
  TEST("eval: trig inverse and hyperbolic numeric identities");

  ParseResult r1 = parse("sin(asin(0.25))");
  EvalResult e1 = eval(r1.root, NULL);
  ASSERT_TRUE(e1.ok);
  ASSERT_CNEAR(e1.value, c_real(0.25), 1e-9);
  eval_result_free(&e1);
  parse_result_free(&r1);

  ParseResult r2 = parse("cos(acos(0.25))");
  EvalResult e2 = eval(r2.root, NULL);
  ASSERT_TRUE(e2.ok);
  ASSERT_CNEAR(e2.value, c_real(0.25), 1e-9);
  eval_result_free(&e2);
  parse_result_free(&r2);

  ParseResult r3 = parse("tan(atan(0.25))");
  EvalResult e3 = eval(r3.root, NULL);
  ASSERT_TRUE(e3.ok);
  ASSERT_CNEAR(e3.value, c_real(0.25), 1e-9);
  eval_result_free(&e3);
  parse_result_free(&r3);

  ParseResult r4 = parse("sinh(1)/cosh(1)");
  EvalResult e4 = eval(r4.root, NULL);
  ASSERT_TRUE(e4.ok);
  ASSERT_CNEAR(e4.value, c_real(tanh(1.0)), 1e-9);
  eval_result_free(&e4);
  parse_result_free(&r4);

  ParseResult r5 = parse("cosh(1)^2 - sinh(1)^2");
  EvalResult e5 = eval(r5.root, NULL);
  ASSERT_TRUE(e5.ok);
  ASSERT_CNEAR(e5.value, c_real(1.0), 1e-9);
  eval_result_free(&e5);
  parse_result_free(&r5);

  PASS();
}

static void test_trig_complex_bridge_identities(void) {
  TEST("eval: circular and hyperbolic complex bridge identities");

  ParseResult r1 = parse("sin(i)");
  EvalResult e1 = eval(r1.root, NULL);
  ASSERT_TRUE(e1.ok);
  ASSERT_CNEAR(e1.value, c_make(0.0, sinh(1.0)), 1e-9);
  eval_result_free(&e1);
  parse_result_free(&r1);

  ParseResult r2 = parse("cos(i)");
  EvalResult e2 = eval(r2.root, NULL);
  ASSERT_TRUE(e2.ok);
  ASSERT_CNEAR(e2.value, c_real(cosh(1.0)), 1e-9);
  eval_result_free(&e2);
  parse_result_free(&r2);

  ParseResult r3 = parse("sinh(i)");
  EvalResult e3 = eval(r3.root, NULL);
  ASSERT_TRUE(e3.ok);
  ASSERT_CNEAR(e3.value, c_make(0.0, sin(1.0)), 1e-9);
  eval_result_free(&e3);
  parse_result_free(&r3);

  ParseResult r4 = parse("cosh(i)");
  EvalResult e4 = eval(r4.root, NULL);
  ASSERT_TRUE(e4.ok);
  ASSERT_CNEAR(e4.value, c_real(cos(1.0)), 1e-9);
  eval_result_free(&e4);
  parse_result_free(&r4);

  PASS();
}

static void test_trig_pythagorean_scattered(void) {
  TEST("trig: sin(x)^2 + y + cos(x)^2 -> 1 + y");
  ParseResult r = parse("sin(x)^2 + y + cos(x)^2");
  AstNode *e = sym_expand(r.root);
  e = ast_canonicalize(e);
  e = sym_collect_terms(e);
  char *s = ast_to_string(e);
  ASSERT_STR_EQ(s, "1 + y");
  free(s);
  ast_free(e);
  parse_result_free(&r);
  PASS();
}

static void test_trig_cos2_minus_sin2(void) {
  TEST("trig: cos(x)^2 - sin(x)^2 -> cos(2*x)");
  ParseResult r = parse("cos(x)^2 - sin(x)^2");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "cos(2*x)");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_trig_sin2_minus_cos2(void) {
  TEST("trig: sin(x)^2 - cos(x)^2 -> -cos(2*x)");
  ParseResult r = parse("sin(x)^2 - cos(x)^2");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "(-cos(2*x))");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_trig_scaled_cos2_sin2(void) {
  TEST("trig: 3*cos(x)^2 - 3*sin(x)^2 -> 3*cos(2*x)");
  ParseResult r = parse("3*cos(x)^2 - 3*sin(x)^2");
  AstNode *e = sym_expand(r.root);
  e = ast_canonicalize(e);
  e = sym_collect_terms(e);
  char *s = ast_to_string(e);
  ASSERT_STR_EQ(s, "3*cos(2*x)");
  free(s);
  ast_free(e);
  parse_result_free(&r);
  PASS();
}

static void test_mul_neg_simplify(void) {
  TEST("simplify: A * (-B) -> -(A*B)");
  ParseResult r = parse("sin(x)*(-sin(x))");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "(-sin(x)^2)");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_diff_sincos_to_cos2x(void) {
  TEST("expand: d/dx(sin(x)*cos(x)) -> cos(2*x)");
  ParseResult r = parse("sin(x)*cos(x)");
  AstNode *d = sym_diff(r.root, "x");
  d = sym_simplify(d);
  AstNode *e = sym_expand(d);
  e = ast_canonicalize(e);
  e = sym_collect_terms(e);
  char *s = ast_to_string(e);
  ASSERT_STR_EQ(s, "cos(2*x)");
  free(s);
  ast_free(e);
  parse_result_free(&r);
  PASS();
}

static void test_trig_scattered_with_constant(void) {
  TEST("trig: sin(x)^2 + y + cos(x)^2 + 5 -> 6 + y");
  ParseResult r = parse("sin(x)^2 + y + cos(x)^2 + 5");
  AstNode *e = sym_expand(r.root);
  e = ast_canonicalize(e);
  e = sym_collect_terms(e);
  char *s = ast_to_string(e);
  ASSERT_STR_EQ(s, "6 + y");
  free(s);
  ast_free(e);
  parse_result_free(&r);
  PASS();
}

static void test_trig_pythagorean_complex_arg(void) {
  TEST("trig: sin(x^2+1)^2 + cos(x^2+1)^2 -> 1");
  ParseResult r = parse("sin(x^2+1)^2 + cos(x^2+1)^2");
  AstNode *e = sym_expand(r.root);
  e = ast_canonicalize(e);
  e = sym_collect_terms(e);
  char *s = ast_to_string(e);
  ASSERT_STR_EQ(s, "1");
  free(s);
  ast_free(e);
  parse_result_free(&r);
  PASS();
}

void tests_trigonometry_mathtest(void) {
  test_simplify_trig_tan();
  test_simplify_trig_pythagorean();
  test_simplify_inverse_trig();
  test_diff_sin();
  test_diff_cos();
  test_diff_asin();
  test_diff_acos();
  test_diff_atan();
  test_simplify_hyperbolic();
  test_simplify_hyperbolic_identities();
  test_diff_hyperbolic();
  test_trig_numeric_roundtrip_identities();
  test_trig_complex_bridge_identities();
  test_trig_pythagorean_scattered();
  test_trig_cos2_minus_sin2();
  test_trig_sin2_minus_cos2();
  test_trig_scaled_cos2_sin2();
  test_mul_neg_simplify();
  test_diff_sincos_to_cos2x();
  test_trig_scattered_with_constant();
  test_trig_pythagorean_complex_arg();
}
