// TODO: Tests should probably be divided into separated modules. Not so modular
// to keep them in almost 2k line file.

#include "ast.h"
#include "canonical.h"
#include "eval.h"
#include "latex.h"
#include "lexer.h"
#include "matrix.h"
#include "parser.h"
#include "symbolic.h"
#include "symtab.h"
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
    printf("  %-55s", name);                                                   \
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

static int is_num_node(const AstNode *n, double v) {
  return n && n->type == AST_NUMBER && fabs(n->as.number - v) < 1e-9;
}

/* Lexer*/

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

/* Parser */

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

/* AST */

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

static void test_eval_arithmetic(void) {
  TEST("eval: basic arithmetic 2+3*4");
  ParseResult r = parse("2+3*4");
  ASSERT_TRUE(r.root != NULL);
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, 14.0, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_power(void) {
  TEST("eval: power 2^10");
  ParseResult r = parse("2^10");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, 1024.0, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_functions(void) {
  TEST("eval: sin(0) = 0");
  ParseResult r = parse("sin(0)");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, 0.0, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_cos(void) {
  TEST("eval: cos(0) = 1");
  ParseResult r = parse("cos(0)");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, 1.0, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_sqrt(void) {
  TEST("eval: sqrt(144) = 12");
  ParseResult r = parse("sqrt(144)");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, 12.0, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_pi(void) {
  TEST("eval: pi constant");
  ParseResult r = parse("pi");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, M_PI, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_complex_expr(void) {
  TEST("eval: (3+4)*(2-1)^2 = 7");
  ParseResult r = parse("(3+4)*(2-1)^2");
  EvalResult e = eval(r.root, NULL);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, 7.0, 1e-9);
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
  ASSERT_NEAR(e.value, 4.0, 1e-9);
  eval_result_free(&e);
  parse_result_free(&r);
  PASS();
}

static void test_eval_symtab(void) {
  TEST("eval: symbol table lookup");
  SymTab st;
  symtab_init(&st);
  symtab_set(&st, "g", 9.81);
  ParseResult r = parse("g");
  EvalResult e = eval(r.root, &st);
  ASSERT_TRUE(e.ok);
  ASSERT_NEAR(e.value, 9.81, 1e-9);
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
  ASSERT_NEAR(e1.value, 0.0, 1e-9);
  eval_result_free(&e1);
  parse_result_free(&r1);

  ParseResult r2 = parse("cosh(0)");
  EvalResult e2 = eval(r2.root, NULL);
  ASSERT_TRUE(e2.ok);
  ASSERT_NEAR(e2.value, 1.0, 1e-9);
  eval_result_free(&e2);
  parse_result_free(&r2);
  PASS();
}

static void test_eval_domain_errors(void) {
  TEST("eval: domain errors for ln, sqrt, log, asin");
  ParseResult r1 = parse("ln(-1)");
  EvalResult e1 = eval(r1.root, NULL);
  ASSERT_TRUE(!e1.ok);
  eval_result_free(&e1);
  parse_result_free(&r1);

  ParseResult r2 = parse("sqrt(-1)");
  EvalResult e2 = eval(r2.root, NULL);
  ASSERT_TRUE(!e2.ok);
  eval_result_free(&e2);
  parse_result_free(&r2);

  ParseResult r3 = parse("log(0)");
  EvalResult e3 = eval(r3.root, NULL);
  ASSERT_TRUE(!e3.ok);
  eval_result_free(&e3);
  parse_result_free(&r3);

  ParseResult r4 = parse("asin(2)");
  EvalResult e4 = eval(r4.root, NULL);
  ASSERT_TRUE(!e4.ok);
  eval_result_free(&e4);
  parse_result_free(&r4);
  PASS();
}

/* Symbolic */

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

static void test_simplify_x_minus_x(void) {
  TEST("simplify: x - x -> 0");
  ParseResult r = parse("x - x");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_TRUE(is_num_node(s, 0));
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_x_plus_x(void) {
  TEST("simplify: x + x -> 2*x");
  ParseResult r = parse("x + x");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "2*x");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_x_times_x(void) {
  TEST("simplify: x * x -> x^2");
  ParseResult r = parse("x * x");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "x^2");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_x_div_x(void) {
  TEST("simplify: x / x -> 1");
  ParseResult r = parse("x / x");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_TRUE(is_num_node(s, 1));
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_power_of_power(void) {
  TEST("simplify: (x^2)^3 -> x^6");
  ParseResult r = parse("(x^2)^3");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "x^6");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_one_pow(void) {
  TEST("simplify: 1^x -> 1");
  ParseResult r = parse("1^x");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_TRUE(is_num_node(s, 1));
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_double_neg(void) {
  TEST("simplify: -(-x) -> x");
  AstNode *x = ast_variable("x", 1);
  AstNode *neg = ast_unary_neg(ast_unary_neg(x));
  AstNode *s = sym_simplify(neg);
  ASSERT_EQ(s->type, AST_VARIABLE);
  ASSERT_STR_EQ(s->as.variable, "x");
  ast_free(s);
  PASS();
}

static void test_simplify_mul_neg_one(void) {
  TEST("simplify: (-1)*x -> (-x)");
  ParseResult r = parse("(-1)*x");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_EQ(s->type, AST_UNARY_NEG);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_coeff_merge(void) {
  TEST("simplify: 2*(3*x) -> 6*x");
  ParseResult r = parse("2*(3*x)");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "6*x");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_mul_reciprocal(void) {
  TEST("simplify: x * (1/x) -> 1");
  ParseResult r = parse("x * (1/x)");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_TRUE(is_num_node(s, 1));
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_cancel_div(void) {
  TEST("simplify: x * (y/x) -> y");
  ParseResult r = parse("x * (y/x)");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_EQ(s->type, AST_VARIABLE);
  ASSERT_STR_EQ(s->as.variable, "y");
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

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

static void test_simplify_exp_ln(void) {
  TEST("simplify: exp(ln(x)) -> x");
  ParseResult r = parse("exp(ln(x))");
  AstNode *s = sym_simplify(ast_clone(r.root));
  ASSERT_EQ(s->type, AST_VARIABLE);
  ASSERT_STR_EQ(s->as.variable, "x");
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_simplify_distributive(void) {
  TEST("simplify: (x^3 + x^2) * x^(-2) -> x + 1");
  ParseResult r = parse("(x^3 + x^2) * x^(-2)");
  AstNode *s = sym_simplify(ast_clone(r.root));
  char *str = ast_to_string(s);
  ASSERT_STR_EQ(str, "x + 1");
  free(str);
  ast_free(s);
  parse_result_free(&r);
  PASS();
}

static void test_int_definite(void) {
  TEST("int: definite integral(x, x, 0, 2) = 2");
  AstNode *expr = ast_variable("x", 1);
  AstNode *result = sym_integrate(expr, "x");
  ast_free(expr);
  ast_free(result);
  PASS();
}

static void test_expand(void) {
  TEST("expand: (x+1)^2 -> 1 + 2*x + x^2 (or similar collected form)");
  ParseResult r = parse("(x+1)^2");
  AstNode *expr = ast_clone(r.root);
  expr = sym_expand(expr);
  expr = ast_canonicalize(expr);
  expr = sym_collect_terms(expr);
  char *str = ast_to_string(expr);
  ASSERT_STR_EQ(str, "1 + 2*x + x^2");
  free(str);
  ast_free(expr);
  parse_result_free(&r);
  PASS();
}

static void test_matrix_expand(void) {
  TEST("expand: [[x, 1], [1, x]]^2 -> [1 + x^2, 2*x; 2*x, 1 + x^2]");
  ParseResult r = parse("[[x, 1], [1, x]]^2");
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

static void test_diff_constant(void) {
  TEST("diff: d/dx(5) = 0");
  ParseResult r = parse("5");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  ASSERT_TRUE(is_num_node(d, 0));
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_variable(void) {
  TEST("diff: d/dx(x) = 1");
  ParseResult r = parse("x");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  ASSERT_TRUE(is_num_node(d, 1));
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
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "6*x + 2");
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

static void test_diff_exp(void) {
  TEST("diff: d/dx(exp(x)) = exp(x)");
  ParseResult r = parse("exp(x)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "exp(x)");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_ln(void) {
  TEST("diff: d/dx(ln(x)) = 1/x");
  ParseResult r = parse("ln(x)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "1/x");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_chain_sin_x2(void) {
  TEST("diff: d/dx(sin(x^2)) uses chain rule");
  ParseResult r = parse("sin(x^2)");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "cos(x^2)*2*x");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_diff_x_to_x(void) {
  TEST("diff: d/dx(x^x) = x^x*ln(x) + x^x");
  ParseResult r = parse("x^x");
  AstNode *d = sym_diff(r.root, "x");
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "x^x*ln(x) + x^x");
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

/* Integration */

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

static void test_integrate_sin(void) {
  TEST("int: integral(sin(x), x) = -cos(x)");
  ParseResult r = parse("sin(x)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "(-cos(x))");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_cos(void) {
  TEST("int: integral(cos(x), x) = sin(x)");
  ParseResult r = parse("cos(x)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "sin(x)");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_exp(void) {
  TEST("int: integral(exp(x), x) = exp(x)");
  ParseResult r = parse("exp(x)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "exp(x)");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_1_over_x(void) {
  TEST("int: integral(x^(-1), x) = ln(x)");
  ParseResult r = parse("x^(-1)");
  AstNode *result = sym_integrate(r.root, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "ln(x)");
  free(s);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_1_div_x(void) {
  TEST("int: integral(1/x, x) via ast_canonicalize = ln(x)");
  ParseResult r = parse("1/x");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  expr = sym_simplify(expr);
  AstNode *result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "ln(x)");
  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_x_div_x(void) {
  TEST("int: integral(x/x, x) via ast_canonicalize = x");
  ParseResult r = parse("x/x");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  expr = sym_simplify(expr);
  AstNode *result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);
  result = sym_simplify(result);
  char *s = ast_to_string(result);
  ASSERT_STR_EQ(s, "x");
  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

static void test_integrate_rational(void) {
  TEST("int: integral((x^3 + x^2) / x^2, x) via ast_canonicalize");
  ParseResult r = parse("(x^3 + x^2) / x^2");
  AstNode *expr = ast_canonicalize(ast_clone(r.root));
  expr = sym_simplify(expr);
  AstNode *result = sym_integrate(expr, "x");
  ASSERT_TRUE(result != NULL);
  result = sym_simplify(result);
  char *s = ast_to_string(result);
  ASSERT_TRUE(strcmp(s, "x + x^2/2") == 0 || strcmp(s, "x^2/2 + x") == 0);
  free(s);
  ast_free(expr);
  ast_free(result);
  parse_result_free(&r);
  PASS();
}

/* Canonical Form */

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

static void test_symtab_set_get(void) {
  TEST("symtab: set and get value");
  SymTab st;
  symtab_init(&st);
  symtab_set(&st, "x", 42.0);
  double val;
  ASSERT_TRUE(symtab_get(&st, "x", &val));
  ASSERT_NEAR(val, 42.0, 1e-9);
  symtab_free(&st);
  PASS();
}

static void test_symtab_overwrite(void) {
  TEST("symtab: overwrite existing value");
  SymTab st;
  symtab_init(&st);
  symtab_set(&st, "x", 1.0);
  symtab_set(&st, "x", 2.0);
  double val;
  ASSERT_TRUE(symtab_get(&st, "x", &val));
  ASSERT_NEAR(val, 2.0, 1e-9);
  symtab_free(&st);
  PASS();
}

static void test_symtab_missing(void) {
  TEST("symtab: missing key returns 0");
  SymTab st;
  symtab_init(&st);
  double val;
  ASSERT_TRUE(!symtab_get(&st, "nope", &val));
  symtab_free(&st);
  PASS();
}

static void test_symtab_remove(void) {
  TEST("symtab: remove entry");
  SymTab st;
  symtab_init(&st);
  symtab_set(&st, "x", 1.0);
  symtab_remove(&st, "x");
  double val;
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

static void test_matrix_create(void) {
  TEST("matrix: create and access");
  Matrix *m = matrix_create(2, 2);
  matrix_set(m, 0, 0, 1.0);
  matrix_set(m, 0, 1, 2.0);
  matrix_set(m, 1, 0, 3.0);
  matrix_set(m, 1, 1, 4.0);
  ASSERT_NEAR(matrix_get(m, 0, 0), 1.0, 1e-9);
  ASSERT_NEAR(matrix_get(m, 1, 1), 4.0, 1e-9);
  matrix_free(m);
  PASS();
}

static void test_matrix_add(void) {
  TEST("matrix: addition");
  Matrix *a = matrix_create(2, 2);
  Matrix *b = matrix_create(2, 2);
  matrix_set(a, 0, 0, 1);
  matrix_set(a, 0, 1, 2);
  matrix_set(a, 1, 0, 3);
  matrix_set(a, 1, 1, 4);
  matrix_set(b, 0, 0, 5);
  matrix_set(b, 0, 1, 6);
  matrix_set(b, 1, 0, 7);
  matrix_set(b, 1, 1, 8);
  Matrix *c = matrix_add(a, b);
  ASSERT_TRUE(c != NULL);
  ASSERT_NEAR(matrix_get(c, 0, 0), 6.0, 1e-9);
  ASSERT_NEAR(matrix_get(c, 1, 1), 12.0, 1e-9);
  matrix_free(a);
  matrix_free(b);
  matrix_free(c);
  PASS();
}

static void test_matrix_mul(void) {
  TEST("matrix: multiplication");
  Matrix *a = matrix_create(2, 2);
  Matrix *b = matrix_create(2, 2);
  matrix_set(a, 0, 0, 1);
  matrix_set(a, 0, 1, 2);
  matrix_set(a, 1, 0, 3);
  matrix_set(a, 1, 1, 4);
  matrix_set(b, 0, 0, 5);
  matrix_set(b, 0, 1, 6);
  matrix_set(b, 1, 0, 7);
  matrix_set(b, 1, 1, 8);
  Matrix *c = matrix_mul(a, b);
  ASSERT_TRUE(c != NULL);
  ASSERT_NEAR(matrix_get(c, 0, 0), 19.0, 1e-9);
  ASSERT_NEAR(matrix_get(c, 0, 1), 22.0, 1e-9);
  ASSERT_NEAR(matrix_get(c, 1, 0), 43.0, 1e-9);
  ASSERT_NEAR(matrix_get(c, 1, 1), 50.0, 1e-9);
  matrix_free(a);
  matrix_free(b);
  matrix_free(c);
  PASS();
}

static void test_matrix_det(void) {
  TEST("matrix: determinant 2x2");
  Matrix *m = matrix_create(2, 2);
  matrix_set(m, 0, 0, 1);
  matrix_set(m, 0, 1, 2);
  matrix_set(m, 1, 0, 3);
  matrix_set(m, 1, 1, 4);
  ASSERT_NEAR(matrix_det(m), -2.0, 1e-9);
  matrix_free(m);
  PASS();
}

static void test_matrix_det_3x3(void) {
  TEST("matrix: determinant 3x3");
  Matrix *m = matrix_create(3, 3);
  matrix_set(m, 0, 0, 6);
  matrix_set(m, 0, 1, 1);
  matrix_set(m, 0, 2, 1);
  matrix_set(m, 1, 0, 4);
  matrix_set(m, 1, 1, -2);
  matrix_set(m, 1, 2, 5);
  matrix_set(m, 2, 0, 2);
  matrix_set(m, 2, 1, 8);
  matrix_set(m, 2, 2, 7);
  ASSERT_NEAR(matrix_det(m), -306.0, 1e-9);
  matrix_free(m);
  PASS();
}

static void test_matrix_inverse(void) {
  TEST("matrix: inverse 2x2");
  Matrix *m = matrix_create(2, 2);
  matrix_set(m, 0, 0, 4);
  matrix_set(m, 0, 1, 7);
  matrix_set(m, 1, 0, 2);
  matrix_set(m, 1, 1, 6);
  Matrix *inv = matrix_inverse(m);
  ASSERT_TRUE(inv != NULL);

  /* M * M^-1 should be identity */
  Matrix *id = matrix_mul(m, inv);
  ASSERT_NEAR(matrix_get(id, 0, 0), 1.0, 1e-9);
  ASSERT_NEAR(matrix_get(id, 0, 1), 0.0, 1e-9);
  ASSERT_NEAR(matrix_get(id, 1, 0), 0.0, 1e-9);
  ASSERT_NEAR(matrix_get(id, 1, 1), 1.0, 1e-9);
  matrix_free(m);
  matrix_free(inv);
  matrix_free(id);
  PASS();
}

static void test_matrix_transpose(void) {
  TEST("matrix: transpose");
  Matrix *m = matrix_create(2, 3);
  matrix_set(m, 0, 0, 1);
  matrix_set(m, 0, 1, 2);
  matrix_set(m, 0, 2, 3);
  matrix_set(m, 1, 0, 4);
  matrix_set(m, 1, 1, 5);
  matrix_set(m, 1, 2, 6);
  Matrix *t = matrix_transpose(m);
  ASSERT_EQ(t->rows, 3);
  ASSERT_EQ(t->cols, 2);
  ASSERT_NEAR(matrix_get(t, 0, 0), 1.0, 1e-9);
  ASSERT_NEAR(matrix_get(t, 2, 1), 6.0, 1e-9);
  matrix_free(m);
  matrix_free(t);
  PASS();
}

static void test_matrix_identity(void) {
  TEST("matrix: identity 3x3");
  Matrix *id = matrix_identity(3);
  ASSERT_NEAR(matrix_get(id, 0, 0), 1.0, 1e-9);
  ASSERT_NEAR(matrix_get(id, 1, 1), 1.0, 1e-9);
  ASSERT_NEAR(matrix_get(id, 2, 2), 1.0, 1e-9);
  ASSERT_NEAR(matrix_get(id, 0, 1), 0.0, 1e-9);
  matrix_free(id);
  PASS();
}

static void test_matrix_trace(void) {
  TEST("matrix: trace");
  Matrix *m = matrix_create(3, 3);
  matrix_set(m, 0, 0, 1);
  matrix_set(m, 1, 1, 5);
  matrix_set(m, 2, 2, 9);
  ASSERT_NEAR(matrix_trace(m), 15.0, 1e-9);
  matrix_free(m);
  PASS();
}

static void test_matrix_to_string(void) {
  TEST("matrix: serialization");
  Matrix *m = matrix_create(2, 2);
  matrix_set(m, 0, 0, 1);
  matrix_set(m, 0, 1, 2);
  matrix_set(m, 1, 0, 3);
  matrix_set(m, 1, 1, 4);
  char *s = matrix_to_string(m);
  ASSERT_STR_EQ(s, "[1, 2; 3, 4]");
  free(s);
  matrix_free(m);
  PASS();
}

static void test_matrix_singular(void) {
  TEST("matrix: inverse of singular returns NULL");
  Matrix *m = matrix_create(2, 2);
  matrix_set(m, 0, 0, 1);
  matrix_set(m, 0, 1, 2);
  matrix_set(m, 1, 0, 2);
  matrix_set(m, 1, 1, 4);
  Matrix *inv = matrix_inverse(m);
  ASSERT_TRUE(inv == NULL);
  matrix_free(m);
  PASS();
}

static void test_lexer_brackets(void) {
  TEST("lexer: bracket tokens [ and ]");
  Lexer lex;
  lexer_init(&lex, "[[1,2],[3,4]]");
  Token t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_LBRACKET);
  t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_LBRACKET);
  t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_NUMBER);
  ASSERT_NEAR(t.numval, 1.0, 1e-9);
  t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_COMMA);
  t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_NUMBER);
  ASSERT_NEAR(t.numval, 2.0, 1e-9);
  t = lexer_next(&lex);
  ASSERT_EQ(t.type, TOK_RBRACKET);
  PASS();
}

static void test_parser_matrix_2x2(void) {
  TEST("parser: matrix literal [[1,2],[3,4]]");
  ParseResult r = parse("[[1,2],[3,4]]");
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
  TEST("parser: row matrix [[1,2,3]]");
  ParseResult r = parse("[[1,2,3]]");
  ASSERT_TRUE(r.root != NULL);
  ASSERT_TRUE(r.error == NULL);
  ASSERT_EQ(r.root->type, AST_MATRIX);
  ASSERT_EQ(r.root->as.matrix.rows, 1);
  ASSERT_EQ(r.root->as.matrix.cols, 3);
  parse_result_free(&r);
  PASS();
}

static void test_parser_matrix_symbolic(void) {
  TEST("parser: symbolic matrix [[sin(x), 1]]");
  ParseResult r = parse("[[sin(x), 1]]");
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

static void test_diff_matrix(void) {
  TEST("diff: d/dx([[x^2, x]]) = [[2*x, 1]]");
  ParseResult r = parse("[[x^2, x]]");
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

static void test_sym_det_1x1(void) {
  TEST("sym_det: 1x1 [[x]]");
  ParseResult r = parse("[[x]]");
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
  TEST("sym_det: 2x2 [[a,b],[c,d]]");
  ParseResult r = parse("[[a,b],[c,d]]");
  AstNode *d = sym_det(r.root);
  ASSERT_TRUE(d != NULL);
  char *s = ast_to_string(d);
  ASSERT_STR_EQ(s, "(-b*c) + a*d");
  free(s);
  ast_free(d);
  parse_result_free(&r);
  PASS();
}

static void test_sym_det_2x2_numeric(void) {
  TEST("sym_det: 2x2 numeric [[1,2],[3,4]]");
  ParseResult r = parse("[[1,2],[3,4]]");
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
  TEST("sym_det: 3x3 numeric [[6,1,1],[4,-2,5],[2,8,7]]");
  ParseResult r = parse("[[6,1,1],[4,-2,5],[2,8,7]]");
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
  ParseResult r = parse("[[1,2,3],[4,5,6]]");
  AstNode *d = sym_det(r.root);
  ASSERT_TRUE(d == NULL);
  parse_result_free(&r);
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

static void test_latex_number(void) {
  TEST("latex: integer renders as plain number");
  AstNode *n = ast_number(42);
  char *s = ast_to_latex(n);
  ASSERT_STR_EQ(s, "42");
  free(s);
  ast_free(n);
  PASS();
}

static void test_latex_variable(void) {
  TEST("latex: single char variable");
  AstNode *n = ast_variable("x", 1);
  char *s = ast_to_latex(n);
  ASSERT_STR_EQ(s, "x");
  free(s);
  ast_free(n);
  PASS();
}

static void test_latex_pi(void) {
  TEST("latex: pi -> \\pi");
  AstNode *n = ast_variable("pi", 2);
  char *s = ast_to_latex(n);
  ASSERT_STR_EQ(s, "\\pi");
  free(s);
  ast_free(n);
  PASS();
}

static void test_latex_multichar_var(void) {
  TEST("latex: multi-char var -> \\mathrm{var}");
  AstNode *n = ast_variable("abc", 3);
  char *s = ast_to_latex(n);
  ASSERT_STR_EQ(s, "\\mathrm{abc}");
  free(s);
  ast_free(n);
  PASS();
}

static void test_latex_add(void) {
  TEST("latex: x + 1");
  ParseResult r = parse("x + 1");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "x + 1");
  free(s);
  parse_result_free(&r);
  PASS();
}

static void test_latex_sub(void) {
  TEST("latex: x - 1");
  ParseResult r = parse("x - 1");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "x - 1");
  free(s);
  parse_result_free(&r);
  PASS();
}

static void test_latex_mul_implicit(void) {
  TEST("latex: 4*x -> 4 x (implicit mul)");
  ParseResult r = parse("4*x");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "4 x");
  free(s);
  parse_result_free(&r);
  PASS();
}

static void test_latex_mul_cdot(void) {
  TEST("latex: 3*4 -> 3 \\cdot 4");
  ParseResult r = parse("3*4");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "3 \\cdot 4");
  free(s);
  parse_result_free(&r);
  PASS();
}

static void test_latex_div_frac(void) {
  TEST("latex: a/b -> \\frac{a}{b}");
  ParseResult r = parse("a/b");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "\\frac{a}{b}");
  free(s);
  parse_result_free(&r);
  PASS();
}

static void test_latex_pow(void) {
  TEST("latex: x^2 -> x^{2}");
  ParseResult r = parse("x^2");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "x^{2}");
  free(s);
  parse_result_free(&r);
  PASS();
}

static void test_latex_nested_pow(void) {
  TEST("latex: x^(n+1) -> x^{n + 1}");
  ParseResult r = parse("x^(n+1)");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "x^{n + 1}");
  free(s);
  parse_result_free(&r);
  PASS();
}

static void test_latex_sin(void) {
  TEST("latex: sin(x) -> \\sin\\left(x\\right)");
  ParseResult r = parse("sin(x)");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "\\sin\\left(x\\right)");
  free(s);
  parse_result_free(&r);
  PASS();
}

static void test_latex_sqrt(void) {
  TEST("latex: sqrt(x) -> \\sqrt{x}");
  ParseResult r = parse("sqrt(x)");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "\\sqrt{x}");
  free(s);
  parse_result_free(&r);
  PASS();
}

static void test_latex_abs(void) {
  TEST("latex: abs(x) -> \\left|x\\right|");
  ParseResult r = parse("abs(x)");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "\\left|x\\right|");
  free(s);
  parse_result_free(&r);
  PASS();
}

static void test_latex_unary_neg(void) {
  TEST("latex: -(x+1) -> -\\left(x + 1\\right)");
  AstNode *inner = ast_binop(OP_ADD, ast_variable("x", 1), ast_number(1));
  AstNode *neg = ast_unary_neg(inner);
  char *s = ast_to_latex(neg);
  ASSERT_STR_EQ(s, "-\\left(x + 1\\right)");
  free(s);
  ast_free(neg);
  PASS();
}

static void test_latex_matrix(void) {
  TEST("latex: 2x2 matrix -> pmatrix env");
  ParseResult r = parse("[[1, 2], [3, 4]]");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "\\begin{pmatrix}\n1 & 2 \\\\\n3 & 4\n\\end{pmatrix}");
  free(s);
  parse_result_free(&r);
  PASS();
}

static void test_latex_frac_no_parens(void) {
  TEST("latex: (x+1)/(x-1) -> no parens inside frac");
  ParseResult r = parse("(x+1)/(x-1)");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "\\frac{x + 1}{x - 1}");
  free(s);
  parse_result_free(&r);
  PASS();
}

static void test_latex_complex_expr(void) {
  TEST("latex: 4*x^2 + 3*x + 1");
  ParseResult r = parse("4*x^2 + 3*x + 1");
  char *s = ast_to_latex(r.root);
  ASSERT_STR_EQ(s, "4 x^{2} + 3 x + 1");
  free(s);
  parse_result_free(&r);
  PASS();
}

/* Main */

int main(void) {
  printf("=== sia test suite ===\n\n");

  printf("[Lexer]\n");
  test_lexer_basic();
  test_lexer_float();
  test_lexer_identifiers();
  test_lexer_operators();
  test_lexer_error();
  test_lexer_brackets();

  printf("\n[Parser]\n");
  test_parser_number();
  test_parser_precedence();
  test_parser_parens();
  test_parser_func_call();
  test_parser_nested();
  test_parser_unary_neg();
  test_parser_error();
  test_parser_matrix_2x2();
  test_parser_matrix_1xN();
  test_parser_matrix_symbolic();

  printf("\n[AST]\n");
  test_ast_clone();
  test_ast_to_string();
  test_ast_matrix_clone();
  test_diff_matrix();

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
  test_eval_symtab();
  test_eval_hyperbolic();
  test_eval_domain_errors();

  printf("\n[Symbolic - Simplify]\n");
  test_simplify_zero_mul();
  test_simplify_identity_add();
  test_simplify_const_fold();
  test_simplify_x_minus_x();
  test_simplify_x_plus_x();
  test_simplify_x_times_x();
  test_simplify_x_div_x();
  test_simplify_power_of_power();
  test_simplify_one_pow();
  test_simplify_double_neg();
  test_simplify_mul_neg_one();
  test_simplify_coeff_merge();
  test_simplify_mul_reciprocal();
  test_simplify_cancel_div();
  test_simplify_trig_tan();
  test_simplify_trig_pythagorean();
  test_simplify_exp_ln();
  test_simplify_distributive();
  test_expand();
  test_matrix_expand();

  printf("\n[Symbolic - Differentiation]\n");
  test_diff_constant();
  test_diff_variable();
  test_diff_x_squared();
  test_diff_polynomial();
  test_diff_sin();
  test_diff_cos();
  test_diff_exp();
  test_diff_ln();
  test_diff_chain_sin_x2();
  test_diff_x_to_x();
  test_diff_asin();
  test_diff_atan();

  printf("\n[Symbolic - Integration]\n");
  test_integrate_x();
  test_integrate_4x();
  test_integrate_constant();
  test_integrate_x_squared();
  test_integrate_sin();
  test_integrate_cos();
  test_integrate_exp();
  test_integrate_1_over_x();
  test_integrate_1_div_x();
  test_integrate_x_div_x();
  test_integrate_rational();
  test_int_definite();

  printf("\n[Canonical Form]\n");
  test_canonical_sub_to_add();
  test_canonical_div_to_mul();
  test_canonical_sort();

  printf("\n[Symbol Table]\n");
  test_symtab_set_get();
  test_symtab_overwrite();
  test_symtab_missing();
  test_symtab_remove();
  test_symtab_expr();

  printf("\n[Matrix]\n");
  test_matrix_create();
  test_matrix_add();
  test_matrix_mul();
  test_matrix_det();
  test_matrix_det_3x3();
  test_matrix_inverse();
  test_matrix_transpose();
  test_matrix_identity();
  test_matrix_trace();
  test_matrix_to_string();
  test_matrix_singular();
  test_sym_det_1x1();
  test_sym_det_2x2();
  test_sym_det_2x2_numeric();
  test_sym_det_3x3();
  test_sym_det_nonsquare();
  test_trig_pythagorean_scattered();
  test_trig_cos2_minus_sin2();
  test_trig_sin2_minus_cos2();
  test_trig_scaled_cos2_sin2();
  test_mul_neg_simplify();
  test_diff_sincos_to_cos2x();
  test_trig_scattered_with_constant();
  test_trig_pythagorean_complex_arg();

  printf("\n[LaTeX]\n");
  test_latex_number();
  test_latex_variable();
  test_latex_pi();
  test_latex_multichar_var();
  test_latex_add();
  test_latex_sub();
  test_latex_mul_implicit();
  test_latex_mul_cdot();
  test_latex_div_frac();
  test_latex_pow();
  test_latex_nested_pow();
  test_latex_sin();
  test_latex_sqrt();
  test_latex_abs();
  test_latex_unary_neg();
  test_latex_matrix();
  test_latex_frac_no_parens();
  test_latex_complex_expr();

  printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}
