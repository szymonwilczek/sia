#include "ast.h"
#include "eval.h"
#include "lexer.h"
#include "parser.h"
#include "symbolic.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                                             \
  do {                                                                         \
    tests_run++;                                                               \
    printf("  %-50s", name);                                                   \
  } while (0)
#define PASS()                                                                 \
  do {                                                                         \
    tests_passed++;                                                            \
    printf("OK\n");                                                            \
  } while (0)
#define FAIL(msg)                                                              \
  do {                                                                         \
    printf("FAIL: %s\n", msg);                                                 \
  } while (0)

#define ASSERT_EQ(a, b)                                                        \
  do {                                                                         \
    if ((a) != (b)) {                                                          \
      FAIL(#a " != " #b);                                                      \
      return;                                                                  \
    }                                                                          \
  } while (0)
#define ASSERT_NEAR(a, b, eps)                                                 \
  do {                                                                         \
    if (fabs((a) - (b)) > (eps)) {                                             \
      char _buf[128];                                                          \
      snprintf(_buf, sizeof _buf, "%g != %g", (double)(a), (double)(b));       \
      FAIL(_buf);                                                              \
      return;                                                                  \
    }                                                                          \
  } while (0)
#define ASSERT_STR_EQ(a, b)                                                    \
  do {                                                                         \
    if (strcmp((a), (b)) != 0) {                                               \
      char _buf[256];                                                          \
      snprintf(_buf, sizeof _buf, "'%s' != '%s'", (a), (b));                   \
      FAIL(_buf);                                                              \
      return;                                                                  \
    }                                                                          \
  } while (0)
#define ASSERT_TRUE(x)                                                         \
  do {                                                                         \
    if (!(x)) {                                                                \
      FAIL(#x " is false");                                                    \
      return;                                                                  \
    }                                                                          \
  } while (0)

static void test_lexer_basic(void) {
  TEST("lexer: basic tokens");
  Lexer lex;
  lexer_init(&lex, "2 + 3 * 4");

  Token t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_NUMBER);
  ASSERT_NEAR(t.numval, 2.0, 1e-9);
  t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_PLUS);
  t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_NUMBER);
  ASSERT_NEAR(t.numval, 3.0, 1e-9);
  t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_STAR);
  t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_NUMBER);
  ASSERT_NEAR(t.numval, 4.0, 1e-9);
  t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_EOF);

  PASS();
}

static void test_lexer_float(void) {
  TEST("lexer: floating point number");
  Lexer lex;
  lexer_init(&lex, "3.14159");
  Token t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_NUMBER);
  ASSERT_NEAR(t.numval, 3.14159, 1e-6);
  PASS();
}

static void test_lexer_identifiers(void) {
  TEST("lexer: identifiers and functions");
  Lexer lex;
  lexer_init(&lex, "sin(x)");
  Token t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_IDENT);
  ASSERT_EQ(t.length, 3);
  t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_LPAREN);
  t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_IDENT);
  ASSERT_EQ(t.length, 1);
  t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_RPAREN);
  PASS();
}

static void test_lexer_operators(void) {
  TEST("lexer: all operators");
  Lexer lex;
  lexer_init(&lex, "+-*/^(),");
  TokenType expected[] = {TOK_PLUS,   TOK_MINUS, TOK_STAR,
                          TOK_SLASH,  TOK_CARET, TOK_LPAREN,
                          TOK_RPAREN, TOK_COMMA, TOK_EOF};
  for (int i = 0; i < 9; i++) {
    Token t = lexer_next(&lex);
    ASSERT_EQ(t.type, expected[i]);
  }
  PASS();
}

static void test_lexer_error(void) {
  TEST("lexer: error on unknown character");
  Lexer lex;
  lexer_init(&lex, "@");
  Token t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_ERROR);
  PASS();
}

static void test_parser_number(void) {
  TEST("parser: simple number");
  ParseResult r = parse("42");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_TRUE(r.error == NULL);
  ASSERT_EQ(r.root->type, AST_NUMBER);
  ASSERT_NEAR(r.root->as.number, 42.0, 1e-9);
  parse_result_free(&r);
  PASS();
}

static void test_parser_precedence(void) {
  TEST("parser: operator precedence 2+2*2");
  ParseResult r = parse("2+2*2");
  ASSERT_TRUE(r.root != NULL);
  /* should be ADD(2, MUL(2, 2)) */
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
  /* should be MUL(ADD(2,2), 2) */
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

static void test_parser_unary_neg(void) {
  TEST("parser: unary negation -5");
  ParseResult r = parse("-5");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_EQ(r.root->type, AST_NUMBER);
  ASSERT_NEAR(r.root->as.number, -5.0, 1e-9);
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

static void test_eval_arithmetic(void) {
  TEST("eval: basic arithmetic 2+3*4");
  ParseResult r = parse("2+3*4");
  ASSERT_TRUE(r.root != NULL);
  EvalResult e = eval(r.root);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, 14.0, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_power(void) {
  TEST("eval: power 2^10");
  ParseResult r = parse("2^10");
  EvalResult e = eval(r.root);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, 1024.0, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_functions(void) {
  TEST("eval: sin(0) = 0");
  ParseResult r = parse("sin(0)");
  EvalResult e = eval(r.root);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, 0.0, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_cos(void) {
  TEST("eval: cos(0) = 1");
  ParseResult r = parse("cos(0)");
  EvalResult e = eval(r.root);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, 1.0, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_sqrt(void) {
  TEST("eval: sqrt(144) = 12");
  ParseResult r = parse("sqrt(144)");
  EvalResult e = eval(r.root);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, 12.0, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_pi(void) {
  TEST("eval: pi constant");
  ParseResult r = parse("pi");
  EvalResult e = eval(r.root);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, M_PI, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_complex_expr(void) {
  TEST("eval: (3+4)*(2-1)^2 = 7");
  ParseResult r = parse("(3+4)*(2-1)^2");
  EvalResult e = eval(r.root);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, 7.0, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_div_zero(void) {
  TEST("eval: division by zero error");
  ParseResult r = parse("1/0");
  EvalResult e = eval(r.root);
  ASSERT_TRUE(!e.ok);
  ASSERT_TRUE(e.error != NULL);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_nested_func(void) {
  TEST("eval: sqrt(abs(-16)) = 4");
  ParseResult r = parse("sqrt(abs(-16))");
  EvalResult e = eval(r.root);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, 4.0, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

/* helper for simplify tests */
static int is_num_node(const AstNode *n, double v) {
  return n && n->type == AST_NUMBER && fabs(n->as.number - v) < 1e-9;
}

static void test_simplify_zero_mul(void) {
  TEST("simplify: 0 * x -> 0");
  ParseResult r = parse("0 * x");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_TRUE(is_num_node(s, 0));
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_identity_add(void) {
  TEST("simplify: x + 0 -> x");
  ParseResult r = parse("x + 0");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_EQ(s->type, AST_VARIABLE);
  ASSERT_STR_EQ(s->as.variable, "x");
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_const_fold(void) {
  TEST("simplify: 2 * 3 -> 6");
  ParseResult r = parse("2 * 3");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_EQ(s->type, AST_NUMBER);
  ASSERT_NEAR(s->as.number, 6.0, 1e-9);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_diff_constant(void) {
  TEST("diff: d/dx(5) = 0");
  ParseResult r = parse("5");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  ASSERT_EQ(d->type, AST_NUMBER);
  ASSERT_NEAR(d->as.number, 0.0, 1e-9);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_variable(void) {
  TEST("diff: d/dx(x) = 1");
  ParseResult r = parse("x");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  ASSERT_EQ(d->type, AST_NUMBER);
  ASSERT_NEAR(d->as.number, 1.0, 1e-9);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_x_squared(void) {
  TEST("diff: d/dx(x^2) = 2*x");
  ParseResult r = parse("x^2");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "2*x");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_polynomial(void) {
  TEST("diff: d/dx(3*x^2 + 2*x + 1)");
  ParseResult r = parse("3*x^2 + 2*x + 1");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  /* should simplify to 6*x + 2 */
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "3*2*x + 2");
  free(s);
  ast_free(d);
  parse_result_free(&r);
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

static void test_integrate_x(void) {
  TEST("int: integral(x, x) = x^2/2");
  ParseResult r = parse("x");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "x^2/2");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_4x(void) {
  TEST("int: integral(4*x, x) = 4*x^2/2");
  ParseResult r = parse("4*x");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "4*x^2/2");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_constant(void) {
  TEST("int: integral(5, x) = 5*x");
  ParseResult r = parse("5");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "5*x");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_x_squared(void) {
  TEST("int: integral(x^2, x) = x^3/3");
  ParseResult r = parse("x^2");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "x^3/3");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

int main(void) {
  printf("=== sia test suite ===\n\n");

  printf("[Lexer]\n");
  test_lexer_basic();
  test_lexer_float();
  test_lexer_identifiers();
  test_lexer_operators();
  test_lexer_error();

  printf("\n[Parser]\n");
  test_parser_number();
  test_parser_precedence();
  test_parser_parens();
  test_parser_func_call();
  test_parser_nested();
  test_parser_unary_neg();
  test_parser_error();

  printf("\n[AST]\n");
  test_ast_clone();
  test_ast_to_string();

  printf("\n[Eval]\n");
  test_eval_arithmetic();
  test_eval_power();
  test_eval_functions();
  test_eval_cos();
  test_eval_sqrt();
  test_eval_pi();
  test_eval_complex_expr();
  test_eval_div_zero();
  test_eval_nested_func();

  printf("\n[Symbolic]\n");
  test_simplify_zero_mul();
  test_simplify_identity_add();
  test_simplify_const_fold();
  test_diff_constant();
  test_diff_variable();
  test_diff_x_squared();
  test_diff_polynomial();
  test_diff_sin();
  test_integrate_x();
  test_integrate_4x();
  test_integrate_constant();
  test_integrate_x_squared();

  printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}
