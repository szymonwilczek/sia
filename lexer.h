#ifndef SIA_LEXER_H
#define SIA_LEXER_H

#include <stddef.h>

typedef enum {
  TOK_NUMBER,
  TOK_IDENT,
  TOK_PLUS,
  TOK_MINUS,
  TOK_STAR,
  TOK_BANG,
  TOK_SLASH,
  TOK_CARET,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_COMMA,
  TOK_SEMICOLON,
  TOK_LBRACKET,
  TOK_RBRACKET,
  TOK_EOF,
  TOK_ERROR
} TokenType;

typedef struct {
  TokenType type;
  const char *start;
  size_t length;
  double numval;
} Token;

typedef struct {
  const char *source;
  size_t pos;
} Lexer;

void lexer_init(Lexer *lex, const char *source);
Token lexer_next(Lexer *lex);
const char *token_type_name(TokenType type);

#endif
