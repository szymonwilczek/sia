#include "../test_support.h"
#include "../test_suites.h"

static void test_simplify_number_theory(void) {
  TEST("simplify: gcd/lcm exact folding");

  ParseResult r1 = parse("gcd(24, 18)");
  AstNode *s1 = sym_simplify(ast_clone(r1.root));
  ASSERT_TRUE(is_num_node(s1, 6));
  ast_free(s1);
  parse_result_free(&r1);

  ParseResult r2 = parse("lcm(6, 8)");
  AstNode *s2 = sym_simplify(ast_clone(r2.root));
  ASSERT_TRUE(is_num_node(s2, 24));
  ast_free(s2);
  parse_result_free(&r2);

  ParseResult r3 = parse("gcd(12/3, 18/3)");
  AstNode *s3 = sym_simplify(ast_clone(r3.root));
  ASSERT_TRUE(is_num_node(s3, 2));
  ast_free(s3);
  parse_result_free(&r3);

  PASS();
}


static void test_latex_number_theory(void) {
  TEST("latex: gcd/lcm render as operators");

  ParseResult r1 = parse("gcd(24, 18)");
  char *s1 = ast_to_latex(r1.root);
  ASSERT_STR_EQ(s1, "\\gcd\\left(24, 18\\right)");
  free(s1);
  parse_result_free(&r1);

  ParseResult r2 = parse("lcm(6, 8)");
  char *s2 = ast_to_latex(r2.root);
  ASSERT_STR_EQ(s2, "\\operatorname{lcm}\\left(6, 8\\right)");
  free(s2);
  parse_result_free(&r2);

  PASS();
}


void tests_number_theory_mathtest(void) {
  test_simplify_number_theory();
  test_latex_number_theory();
}
