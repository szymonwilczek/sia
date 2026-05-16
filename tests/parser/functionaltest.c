#include "../test_suites.h"
#include "../test_support.h"

static void test_parser_number(void) {
  TEST("parser: simple number");
  ParseResult r = parse("42");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_TRUE(r.error == NULL);
  ASSERT_EQ(r.root->type, AST_NUMBER);
  ASSERT_CNEAR(r.root->as.number, c_real(42.0), 1e-9);
  parse_result_free(&r);
  PASS();
}

static void test_parser_precedence(void) {
  TEST("parser: operator precedence 2+2*2");
  ParseResult r = parse("2+2*2");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_EQ(r.root->type, AST_BINOP);
  ASSERT_EQ(r.root->as.binop.op, OP_ADD);
  ASSERT_EQ(r.root->as.binop.left->type, AST_NUMBER);
  ASSERT_EQ(r.root->as.binop.right->type, AST_BINOP);
  ASSERT_EQ(r.root->as.binop.right->as.binop.op, OP_MUL);
  parse_result_free(&r);
  PASS();
}

static void test_parser_parens(void) {
  TEST("parser: parentheses (2+2)*2");
  ParseResult r = parse("(2+2)*2");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_EQ(r.root->type, AST_BINOP);
  ASSERT_EQ(r.root->as.binop.op, OP_MUL);
  ASSERT_EQ(r.root->as.binop.left->type, AST_BINOP);
  ASSERT_EQ(r.root->as.binop.left->as.binop.op, OP_ADD);
  parse_result_free(&r);
  PASS();
}

static void test_parser_func_call(void) {
  TEST("parser: function call sin(3.14)");
  ParseResult r = parse("sin(3.14)");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_EQ(r.root->type, AST_FUNC_CALL);
  ASSERT_STR_EQ(r.root->as.call.name, "sin");
  ASSERT_EQ(r.root->as.call.nargs, 1);
  parse_result_free(&r);
  PASS();
}

static void test_parser_nested(void) {
  TEST("parser: nested expression 2^3+1");
  ParseResult r = parse("2^3+1");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_EQ(r.root->type, AST_BINOP);
  ASSERT_EQ(r.root->as.binop.op, OP_ADD);
  ASSERT_EQ(r.root->as.binop.left->type, AST_BINOP);
  ASSERT_EQ(r.root->as.binop.left->as.binop.op, OP_POW);
  parse_result_free(&r);
  PASS();
}

static void test_parser_power_right_associative(void) {
  TEST("parser: exponentiation is right-associative");
  ParseResult r = parse("2^3^2");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_TRUE(r.error == NULL);
  ASSERT_EQ(r.root->type, AST_BINOP);
  ASSERT_EQ(r.root->as.binop.op, OP_POW);
  ASSERT_TRUE(is_num_node(r.root->as.binop.left, 2.0));
  ASSERT_EQ(r.root->as.binop.right->type, AST_BINOP);
  ASSERT_EQ(r.root->as.binop.right->as.binop.op, OP_POW);
  ASSERT_TRUE(is_num_node(r.root->as.binop.right->as.binop.left, 3.0));
  ASSERT_TRUE(is_num_node(r.root->as.binop.right->as.binop.right, 2.0));
  parse_result_free(&r);
  PASS();
}

static void test_parser_unary_neg(void) {
  TEST("parser: unary negation -5");
  ParseResult r = parse("-5");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_EQ(r.root->type, AST_NUMBER);
  ASSERT_CNEAR(r.root->as.number, c_real(-5.0), 1e-9);
  parse_result_free(&r);
  PASS();
}

static void test_parser_error(void) {
  TEST("parser: error on malformed input");
  ParseResult r = parse("2 +");
  ASSERT_TRUE(r.root == NULL);
  ASSERT_TRUE(r.error != NULL);
  parse_result_free(&r);
  PASS();
}

/* AST */

static void test_parser_matrix_2x2(void) {
  TEST("parser: matrix literal [1,2;3,4]");
  ParseResult r = parse("[1,2;3,4]");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_TRUE(r.error == NULL);
  ASSERT_EQ(r.root->type, AST_MATRIX);
  ASSERT_EQ(r.root->as.matrix.rows, 2);
  ASSERT_EQ(r.root->as.matrix.cols, 2);
  ASSERT_TRUE(is_num_node(r.root->as.matrix.elements[0], 1.0));
  ASSERT_TRUE(is_num_node(r.root->as.matrix.elements[1], 2.0));
  ASSERT_TRUE(is_num_node(r.root->as.matrix.elements[2], 3.0));
  ASSERT_TRUE(is_num_node(r.root->as.matrix.elements[3], 4.0));
  parse_result_free(&r);
  PASS();
}

static void test_parser_matrix_1xN(void) {
  TEST("parser: row matrix [1,2,3]");
  ParseResult r = parse("[1,2,3]");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_TRUE(r.error == NULL);
  ASSERT_EQ(r.root->type, AST_MATRIX);
  ASSERT_EQ(r.root->as.matrix.rows, 1);
  ASSERT_EQ(r.root->as.matrix.cols, 3);
  parse_result_free(&r);
  PASS();
}

static void test_parser_matrix_symbolic(void) {
  TEST("parser: symbolic matrix [sin(x), 1]");
  ParseResult r = parse("[sin(x), 1]");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_TRUE(r.error == NULL);
  ASSERT_EQ(r.root->type, AST_MATRIX);
  ASSERT_EQ(r.root->as.matrix.rows, 1);
  ASSERT_EQ(r.root->as.matrix.cols, 2);
  ASSERT_EQ(r.root->as.matrix.elements[0]->type, AST_FUNC_CALL);
  ASSERT_TRUE(is_num_node(r.root->as.matrix.elements[1], 1.0));
  parse_result_free(&r);
  PASS();
}

static void test_parser_factorial(void) {
  TEST("parser: postfix factorial 5!");
  ParseResult r = parse("5!");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_EQ(r.root->type, AST_FUNC_CALL);
  ASSERT_STR_EQ(r.root->as.call.name, "factorial");
  ASSERT_EQ(r.root->as.call.nargs, 1);
  parse_result_free(&r);
  PASS();
}

static void test_parser_limit(void) {
  TEST("parser: limit expression lim(sin(x)/x, x, 0)");
  ParseResult r = parse("lim(sin(x)/x, x, 0)");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_TRUE(r.error == NULL);
  ASSERT_EQ(r.root->type, AST_LIMIT);
  ASSERT_STR_EQ(r.root->as.limit.var, "x");
  ASSERT_TRUE(is_num_node(r.root->as.limit.target, 0.0));
  parse_result_free(&r);
  PASS();
}

void tests_parser_functional(void) {
  test_parser_number();
  test_parser_precedence();
  test_parser_parens();
  test_parser_func_call();
  test_parser_nested();
  test_parser_power_right_associative();
  test_parser_unary_neg();
  test_parser_error();
  test_parser_matrix_2x2();
  test_parser_matrix_1xN();
  test_parser_matrix_symbolic();
  test_parser_factorial();
  test_parser_limit();
}
