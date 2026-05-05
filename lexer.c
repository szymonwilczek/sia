#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void lexer_init(Lexer *lex, const char *source) {
  lex->source = source;
  lex->pos = 0;
}

static Token make_token(TokenType type, const char *start, size_t length) {
  Token t = {.type = type, .start = start, .length = length, .numval = 0.0};
  return t;
}

static Token make_number(const char *start, size_t length, double val) {
  Token t = {
      .type = TOK_NUMBER, .start = start, .length = length, .numval = val};
  return t;
}

static Token make_error(const char *start) {
  Token t = {.type = TOK_ERROR, .start = start, .length = 1, .numval = 0.0};
  return t;
}

Token lexer_next(Lexer *lex) {
  const char *s = lex->source;

  while (s[lex->pos] && isspace((unsigned char)s[lex->pos]))
    lex->pos++;

  if (!s[lex->pos])
    return make_token(TOK_EOF, s + lex->pos, 0);

  const char *start = s + lex->pos;
  char c = s[lex->pos];

  if (isdigit((unsigned char)c) || c == '.') {
    char *end;
    double val = strtod(start, &end);
    size_t len = (size_t)(end - start);
    lex->pos += len;
    return make_number(start, len, val);
  }

  if (isalpha((unsigned char)c) || c == '_') {
    size_t begin = lex->pos;
    while (isalnum((unsigned char)s[lex->pos]) || s[lex->pos] == '_')
      lex->pos++;
    return make_token(TOK_IDENT, start, lex->pos - begin);
  }

  lex->pos++;
  switch (c) {
  case '+':
    return make_token(TOK_PLUS, start, 1);
  case '-':
    return make_token(TOK_MINUS, start, 1);
  case '*':
    return make_token(TOK_STAR, start, 1);
  case '!':
    return make_token(TOK_BANG, start, 1);
  case '/':
    return make_token(TOK_SLASH, start, 1);
  case '^':
    return make_token(TOK_CARET, start, 1);
  case '(':
    return make_token(TOK_LPAREN, start, 1);
  case ')':
    return make_token(TOK_RPAREN, start, 1);
  case ',':
    return make_token(TOK_COMMA, start, 1);
  case ';':
    return make_token(TOK_SEMICOLON, start, 1);
  case '[':
    return make_token(TOK_LBRACKET, start, 1);
  case ']':
    return make_token(TOK_RBRACKET, start, 1);
  default:
    return make_error(start);
  }
}

const char *token_type_name(TokenType type) {
  switch (type) {
  case TOK_NUMBER:
    return "NUMBER";
  case TOK_IDENT:
    return "IDENT";
  case TOK_PLUS:
    return "PLUS";
  case TOK_MINUS:
    return "MINUS";
  case TOK_STAR:
    return "STAR";
  case TOK_BANG:
    return "BANG";
  case TOK_SLASH:
    return "SLASH";
  case TOK_CARET:
    return "CARET";
  case TOK_LPAREN:
    return "LPAREN";
  case TOK_RPAREN:
    return "RPAREN";
  case TOK_COMMA:
    return "COMMA";
  case TOK_SEMICOLON:
    return "SEMICOLON";
  case TOK_LBRACKET:
    return "LBRACKET";
  case TOK_RBRACKET:
    return "RBRACKET";
  case TOK_EOF:
    return "EOF";
  case TOK_ERROR:
    return "ERROR";
  }
  return "UNKNOWN";
}
