#include "../test_support.h"
#include "../test_suites.h"

static void test_canonical_sub_to_add(void) {
  TEST("canonical: A - B -> A + (-1)*B");
  ParseResult r = parse("x - y");
  AstNode *c = ast_canonicalize(ast_clone(r.root));
  ASSERT_EQ(c->type, AST_BINOP);
  ASSERT_EQ(c->as.binop.op, OP_ADD);
  ast_free(c);
  parse_result_free(&r);
  PASS();
}


static void test_canonical_div_to_mul(void) {
  TEST("canonical: A / B -> A * B^(-1)");
  ParseResult r = parse("x / y");
  AstNode *c = ast_canonicalize(ast_clone(r.root));
  ASSERT_EQ(c->type, AST_BINOP);
  ASSERT_EQ(c->as.binop.op, OP_MUL);
  ASSERT_EQ(c->as.binop.right->type, AST_BINOP);
  ASSERT_EQ(c->as.binop.right->as.binop.op, OP_POW);
  ast_free(c);
  parse_result_free(&r);
  PASS();
}


static void test_canonical_sort(void) {
  TEST("canonical: commutative sort x + 2 -> 2 + x");
  ParseResult r = parse("x + 2");
  AstNode *c = ast_canonicalize(ast_clone(r.root));
  /* after ast_canonicalize, constants sort before variables */
  ASSERT_EQ(c->type, AST_BINOP);
  ASSERT_EQ(c->as.binop.op, OP_ADD);
  /* left should be number (sort key 0), right variable (sort key 1) */
  ASSERT_EQ(c->as.binop.left->type, AST_NUMBER);
  ASSERT_EQ(c->as.binop.right->type, AST_VARIABLE);
  ast_free(c);
  parse_result_free(&r);
  PASS();
}

/* Symbol Table */


void tests_canonical_mathtest(void) {
  test_canonical_sub_to_add();
  test_canonical_div_to_mul();
  test_canonical_sort();
}
