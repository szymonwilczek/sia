#include "../test_support.h"
#include "../test_suites.h"

static void test_matrix_create(void) {
  TEST("matrix: create and access");
  Matrix *m = matrix_create(2, 2);
  matrix_set(m, 0, 0, c_real(1.0));
  matrix_set(m, 0, 1, c_real(2.0));
  matrix_set(m, 1, 0, c_real(3.0));
  matrix_set(m, 1, 1, c_real(4.0));
  ASSERT_CNEAR(matrix_get(m, 0, 0), c_real(1.0), 1e-9);
  ASSERT_CNEAR(matrix_get(m, 1, 1), c_real(4.0), 1e-9);
  matrix_free(m);
  PASS();
}


static void test_matrix_add(void) {
  TEST("matrix: addition");
  Matrix *a = matrix_create(2, 2);
  Matrix *b = matrix_create(2, 2);
  matrix_set(a, 0, 0, c_real(1));
  matrix_set(a, 0, 1, c_real(2));
  matrix_set(a, 1, 0, c_real(3));
  matrix_set(a, 1, 1, c_real(4));
  matrix_set(b, 0, 0, c_real(5));
  matrix_set(b, 0, 1, c_real(6));
  matrix_set(b, 1, 0, c_real(7));
  matrix_set(b, 1, 1, c_real(8));
  Matrix *c = matrix_add(a, b);
  ASSERT_TRUE(c != NULL);
  ASSERT_CNEAR(matrix_get(c, 0, 0), c_real(6.0), 1e-9);
  ASSERT_CNEAR(matrix_get(c, 1, 1), c_real(12.0), 1e-9);
  matrix_free(a);
  matrix_free(b);
  matrix_free(c);
  PASS();
}


static void test_matrix_mul(void) {
  TEST("matrix: multiplication");
  Matrix *a = matrix_create(2, 2);
  Matrix *b = matrix_create(2, 2);
  matrix_set(a, 0, 0, c_real(1));
  matrix_set(a, 0, 1, c_real(2));
  matrix_set(a, 1, 0, c_real(3));
  matrix_set(a, 1, 1, c_real(4));
  matrix_set(b, 0, 0, c_real(5));
  matrix_set(b, 0, 1, c_real(6));
  matrix_set(b, 1, 0, c_real(7));
  matrix_set(b, 1, 1, c_real(8));
  Matrix *c = matrix_mul(a, b);
  ASSERT_TRUE(c != NULL);
  ASSERT_CNEAR(matrix_get(c, 0, 0), c_real(19.0), 1e-9);
  ASSERT_CNEAR(matrix_get(c, 0, 1), c_real(22.0), 1e-9);
  ASSERT_CNEAR(matrix_get(c, 1, 0), c_real(43.0), 1e-9);
  ASSERT_CNEAR(matrix_get(c, 1, 1), c_real(50.0), 1e-9);
  matrix_free(a);
  matrix_free(b);
  matrix_free(c);
  PASS();
}


static void test_matrix_inverse(void) {
  TEST("matrix: inverse 2x2");
  Matrix *m = matrix_create(2, 2);
  matrix_set(m, 0, 0, c_real(4));
  matrix_set(m, 0, 1, c_real(7));
  matrix_set(m, 1, 0, c_real(2));
  matrix_set(m, 1, 1, c_real(6));
  Matrix *inv = matrix_inverse(m);
  ASSERT_TRUE(inv != NULL);

  /* M * M^-1 should be identity */
  Matrix *id = matrix_mul(m, inv);
  ASSERT_CNEAR(matrix_get(id, 0, 0), c_real(1.0), 1e-9);
  ASSERT_CNEAR(matrix_get(id, 0, 1), c_real(0.0), 1e-9);
  ASSERT_CNEAR(matrix_get(id, 1, 0), c_real(0.0), 1e-9);
  ASSERT_CNEAR(matrix_get(id, 1, 1), c_real(1.0), 1e-9);
  matrix_free(m);
  matrix_free(inv);
  matrix_free(id);
  PASS();
}


static void test_matrix_transpose(void) {
  TEST("matrix: transpose");
  Matrix *m = matrix_create(2, 3);
  matrix_set(m, 0, 0, c_real(1));
  matrix_set(m, 0, 1, c_real(2));
  matrix_set(m, 0, 2, c_real(3));
  matrix_set(m, 1, 0, c_real(4));
  matrix_set(m, 1, 1, c_real(5));
  matrix_set(m, 1, 2, c_real(6));
  Matrix *t = matrix_transpose(m);
  ASSERT_EQ(t->rows, 3);
  ASSERT_EQ(t->cols, 2);
  ASSERT_CNEAR(matrix_get(t, 0, 0), c_real(1.0), 1e-9);
  ASSERT_CNEAR(matrix_get(t, 2, 1), c_real(6.0), 1e-9);
  matrix_free(m);
  matrix_free(t);
  PASS();
}


static void test_matrix_identity(void) {
  TEST("matrix: identity 3x3");
  Matrix *id = matrix_identity(3);
  ASSERT_CNEAR(matrix_get(id, 0, 0), c_real(1.0), 1e-9);
  ASSERT_CNEAR(matrix_get(id, 1, 1), c_real(1.0), 1e-9);
  ASSERT_CNEAR(matrix_get(id, 2, 2), c_real(1.0), 1e-9);
  ASSERT_CNEAR(matrix_get(id, 0, 1), c_real(0.0), 1e-9);
  matrix_free(id);
  PASS();
}


static void test_matrix_trace(void) {
  TEST("matrix: trace");
  Matrix *m = matrix_create(3, 3);
  matrix_set(m, 0, 0, c_real(1));
  matrix_set(m, 1, 1, c_real(5));
  matrix_set(m, 2, 2, c_real(9));
  ASSERT_CNEAR(matrix_trace(m), c_real(15.0), 1e-9);
  matrix_free(m);
  PASS();
}


static void test_matrix_to_string(void) {
  TEST("matrix: serialization");
  Matrix *m = matrix_create(2, 2);
  matrix_set(m, 0, 0, c_real(1));
  matrix_set(m, 0, 1, c_real(2));
  matrix_set(m, 1, 0, c_real(3));
  matrix_set(m, 1, 1, c_real(4));
  char *s = matrix_to_string(m);
  ASSERT_STR_EQ(s, "[1, 2; 3, 4]");
  free(s);
  matrix_free(m);
  PASS();
}


static void test_matrix_singular(void) {
  TEST("matrix: inverse of singular returns NULL");
  Matrix *m = matrix_create(2, 2);
  matrix_set(m, 0, 0, c_real(1));
  matrix_set(m, 0, 1, c_real(2));
  matrix_set(m, 1, 0, c_real(2));
  matrix_set(m, 1, 1, c_real(4));
  Matrix *inv = matrix_inverse(m);
  ASSERT_TRUE(inv == NULL);
  matrix_free(m);
  PASS();
}


static void test_matrix_expand(void) {
  TEST("expand: [x, 1; 1, x]^2 -> [1 + x^2, 2*x; 2*x, 1 + x^2]");
  ParseResult r = parse("[x, 1; 1, x]^2");
  AstNode *expr = ast_clone(r.root);
  expr = sym_expand(expr);
  expr = ast_canonicalize(expr);
  expr = sym_collect_terms(expr);
  char *str = ast_to_string(expr);
  ASSERT_STR_EQ(str, "[1 + x^2, 2*x; 2*x, 1 + x^2]");
  free(str);
  ast_free(expr);
  parse_result_free(&r);
  PASS();
}

/* Differentiation */


static void test_diff_matrix(void) {
  TEST("diff: d/dx([x^2, x]) = [2*x, 1]");
  ParseResult r = parse("[x^2, x]");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  ASSERT_EQ(d->type, AST_MATRIX);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "[2*x, 1]");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}


void tests_matrix_functional(void) {
  test_matrix_create();
  test_matrix_add();
  test_matrix_mul();
  test_matrix_inverse();
  test_matrix_transpose();
  test_matrix_identity();
  test_matrix_trace();
  test_matrix_to_string();
  test_matrix_singular();
  test_matrix_expand();
  test_diff_matrix();
}
