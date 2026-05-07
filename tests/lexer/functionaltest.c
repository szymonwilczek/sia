#include "../test_support.h"
#include "../test_suites.h"

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


void tests_lexer_functional(void) {
  test_lexer_basic();
  test_lexer_float();
  test_lexer_identifiers();
  test_lexer_operators();
  test_lexer_error();
  test_lexer_brackets();
}
