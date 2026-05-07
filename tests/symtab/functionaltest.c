#include "../test_support.h"
#include "../test_suites.h"

static void test_symtab_set_get(void) {
  TEST("symtab: set and get value");
  SymTab st;
  symtab_init(&st);
  symtab_set(&st, "x", c_real(42.0));
  Complex val;
  ASSERT_TRUE(symtab_get(&st, "x", &val));
  ASSERT_CNEAR(val, c_real(42.0), 1e-9);
  symtab_free(&st);
  PASS();
}


static void test_symtab_overwrite(void) {
  TEST("symtab: overwrite existing value");
  SymTab st;
  symtab_init(&st);
  symtab_set(&st, "x", c_real(1.0));
  symtab_set(&st, "x", c_real(2.0));
  Complex val;
  ASSERT_TRUE(symtab_get(&st, "x", &val));
  ASSERT_CNEAR(val, c_real(2.0), 1e-9);
  symtab_free(&st);
  PASS();
}


static void test_symtab_missing(void) {
  TEST("symtab: missing key returns 0");
  SymTab st;
  symtab_init(&st);
  Complex val;
  ASSERT_TRUE(!symtab_get(&st, "nope", &val));
  symtab_free(&st);
  PASS();
}


static void test_symtab_remove(void) {
  TEST("symtab: remove entry");
  SymTab st;
  symtab_init(&st);
  symtab_set(&st, "x", c_real(1.0));
  symtab_remove(&st, "x");
  Complex val;
  ASSERT_TRUE(!symtab_get(&st, "x", &val));
  symtab_free(&st);
  PASS();
}


static void test_symtab_expr(void) {
  TEST("symtab: store and retrieve expression");
  SymTab st;
  symtab_init(&st);
  AstNode *expr = ast_binop(OP_ADD, ast_variable("x", 1), ast_number(1));
  symtab_set_expr(&st, "f", expr);
  AstNode *got = symtab_get_expr(&st, "f");
  ASSERT_TRUE(got != NULL);
  char *s = ast_to_string(got);
  ASSERT_STR_EQ(s, "x + 1");
  free(s);
  symtab_free(&st);
  PASS();
}

/* Matrix */


void tests_symtab_functional(void) {
  test_symtab_set_get();
  test_symtab_overwrite();
  test_symtab_missing();
  test_symtab_remove();
  test_symtab_expr();
}
