#include "../test_support.h"
#include "../test_suites.h"

static void test_ast_clone(void) {
  TEST("ast: clone preserves structure");
  ParseResult r = parse("2*x + 1");
  ASSERT_TRUE(r.root != NULL);
  AstNode *clone = ast_clone(r.root);
  char *s1 = ast_to_string(r.root);
  char *s2 = ast_to_string(clone);
  ASSERT_STR_EQ(s1, s2);
  free(s1);
  free(s2);
  ast_free(clone);
  parse_result_free(&r);
  PASS();
}


static void test_ast_to_string(void) {
  TEST("ast: serialization round-trip");
  ParseResult r = parse("2 + 3*x");
  ASSERT_TRUE(r.root != NULL);
  char *s = ast_to_string(r.root);
  ASSERT_STR_EQ(s, "2 + 3*x");
  free(s);
  parse_result_free(&r);
  PASS();
}

/* Eval */


static void test_ast_matrix_clone(void) {
  TEST("ast: matrix clone round-trip");
  ParseResult r = parse("[[1,2],[3,4]]");
  ASSERT_TRUE(r.root != NULL);
  AstNode *clone = ast_clone(r.root);
  char *s1 = ast_to_string(r.root);
  char *s2 = ast_to_string(clone);
  ASSERT_STR_EQ(s1, s2);
  free(s1);
  free(s2);
  ast_free(clone);
  parse_result_free(&r);
  PASS();
}

/* LaTeX */


void tests_ast_functional(void) {
  test_ast_clone();
  test_ast_to_string();
  test_ast_matrix_clone();
}
