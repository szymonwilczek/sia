#include "../test_suites.h"
#include "../test_support.h"

static void test_matrix_det(void) {
  TEST("matrix: determinant 2x2");
  Matrix *m = matrix_create(2, 2);
  matrix_set(m, 0, 0, c_real(1));
  matrix_set(m, 0, 1, c_real(2));
  matrix_set(m, 1, 0, c_real(3));
  matrix_set(m, 1, 1, c_real(4));
  ASSERT_CNEAR(matrix_det(m), c_real(-2.0), 1e-9);
  matrix_free(m);
  PASS();
}

static void test_matrix_det_3x3(void) {
  TEST("matrix: determinant 3x3");
  Matrix *m = matrix_create(3, 3);
  matrix_set(m, 0, 0, c_real(6));
  matrix_set(m, 0, 1, c_real(1));
  matrix_set(m, 0, 2, c_real(1));
  matrix_set(m, 1, 0, c_real(4));
  matrix_set(m, 1, 1, c_real(-2));
  matrix_set(m, 1, 2, c_real(5));
  matrix_set(m, 2, 0, c_real(2));
  matrix_set(m, 2, 1, c_real(8));
  matrix_set(m, 2, 2, c_real(7));
  ASSERT_CNEAR(matrix_det(m), c_real(-306.0), 1e-9);
  matrix_free(m);
  PASS();
}

static void test_sym_det_1x1(void) {
  TEST("sym_det: 1x1 [x]");
  ParseResult r = parse("[x]");
  AstNode *d = sym_det(r.root);
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "x");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_sym_det_2x2(void) {
  TEST("sym_det: 2x2 [a,b;c,d]");
  ParseResult r = parse("[a,b;c,d]");
  AstNode *d = sym_det(r.root);
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_TRUE(strcmp(s, "(-b*c) + a*d") == 0 || strcmp(s, "a*d + (-c*b)") == 0);
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_sym_det_2x2_numeric(void) {
  TEST("sym_det: 2x2 numeric [1,2;3,4]");
  ParseResult r = parse("[1,2;3,4]");
  AstNode *d = sym_det(r.root);
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "-2");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_sym_det_3x3(void) {
  TEST("sym_det: 3x3 numeric [6,1,1;4,-2,5;2,8,7]");
  ParseResult r = parse("[6,1,1;4,-2,5;2,8,7]");
  AstNode *d = sym_det(r.root);
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "-306");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_sym_det_nonsquare(void) {
  TEST("sym_det: non-square returns NULL");
  ParseResult r = parse("[1,2,3;4,5,6]");
  AstNode *d = sym_det(r.root);
  ASSERT_TRUE(d == NULL);
  parse_result_free(&r);
  PASS();
}

static void test_matrix_det_multiplicative(void) {
  TEST("matrix: det(A*B) = det(A)*det(B)");
  Matrix *a = matrix_create(2, 2);
  matrix_set(a, 0, 0, c_real(1));
  matrix_set(a, 0, 1, c_real(2));
  matrix_set(a, 1, 0, c_real(3));
  matrix_set(a, 1, 1, c_real(5));

  Matrix *b = matrix_create(2, 2);
  matrix_set(b, 0, 0, c_real(-2));
  matrix_set(b, 0, 1, c_real(4));
  matrix_set(b, 1, 0, c_real(1));
  matrix_set(b, 1, 1, c_real(3));

  Matrix *ab = matrix_mul(a, b);
  Complex det_ab = matrix_det(ab);
  Complex det_product = c_mul(matrix_det(a), matrix_det(b));
  ASSERT_CNEAR(det_ab, det_product, 1e-9);

  matrix_free(ab);
  matrix_free(b);
  matrix_free(a);
  PASS();
}

static void test_matrix_inverse_identity_product(void) {
  TEST("matrix: A * inverse(A) = I");
  Matrix *a = matrix_create(2, 2);
  matrix_set(a, 0, 0, c_real(4));
  matrix_set(a, 0, 1, c_real(7));
  matrix_set(a, 1, 0, c_real(2));
  matrix_set(a, 1, 1, c_real(6));

  Matrix *inv = matrix_inverse(a);
  ASSERT_TRUE(inv != NULL);
  Matrix *product = matrix_mul(a, inv);
  ASSERT_TRUE(product != NULL);

  ASSERT_CNEAR(matrix_get(product, 0, 0), c_real(1), 1e-9);
  ASSERT_CNEAR(matrix_get(product, 0, 1), c_real(0), 1e-9);
  ASSERT_CNEAR(matrix_get(product, 1, 0), c_real(0), 1e-9);
  ASSERT_CNEAR(matrix_get(product, 1, 1), c_real(1), 1e-9);

  matrix_free(product);
  matrix_free(inv);
  matrix_free(a);
  PASS();
}

void tests_matrix_mathtest(void) {
  test_matrix_det();
  test_matrix_det_3x3();
  test_sym_det_1x1();
  test_sym_det_2x2();
  test_sym_det_2x2_numeric();
  test_sym_det_3x3();
  test_sym_det_nonsquare();
  test_matrix_det_multiplicative();
  test_matrix_inverse_identity_product();
}
