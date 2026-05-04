#include "parser.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  Lexer lex;
  Token current;
  char *error;
} Parser;

static void advance(Parser *p) { p->current = lexer_next(&p->lex); }

static void set_error(Parser *p, const char *msg) {
  if (!p->error)
    p->error = strdup(msg);
}

static int match(Parser *p, TokenType type) { return p->current.type == type; }

static int expect(Parser *p, TokenType type) {
  if (p->current.type != type) {
    char buf[128];
    snprintf(buf, sizeof buf, "expected %s, got %s", token_type_name(type),
             token_type_name(p->current.type));
    set_error(p, buf);
    return 0;
  }
  advance(p);
  return 1;
}

static AstNode *parse_expr(Parser *p);

static AstNode *parse_primary(Parser *p) {
  if (p->error)
    return NULL;

  if (match(p, TOK_NUMBER)) {
    double val = p->current.numval;
    advance(p);
    return ast_number(val);
  }

  if (match(p, TOK_IDENT)) {
    const char *name = p->current.start;
    size_t len = p->current.length;
    advance(p);

    if (match(p, TOK_LPAREN)) {
      advance(p);
      AstNode *args[16];
      size_t nargs = 0;

      if (!match(p, TOK_RPAREN)) {
        args[nargs] = parse_expr(p);
        if (!args[nargs])
          return NULL;
        nargs++;

        while (match(p, TOK_COMMA)) {
          advance(p);
          if (nargs >= 16) {
            set_error(p, "too many function arguments (max 16)");
            for (size_t i = 0; i < nargs; i++)
              ast_free(args[i]);
            return NULL;
          }
          args[nargs] = parse_expr(p);
          if (!args[nargs]) {
            for (size_t i = 0; i < nargs; i++)
              ast_free(args[i]);
            return NULL;
          }
          nargs++;
        }
      }

      if (!expect(p, TOK_RPAREN)) {
        for (size_t i = 0; i < nargs; i++)
          ast_free(args[i]);
        return NULL;
      }

      return ast_func_call(name, len, args, nargs);
    }

    return ast_variable(name, len);
  }

  if (match(p, TOK_LPAREN)) {
    advance(p);
    AstNode *expr = parse_expr(p);
    if (!expr)
      return NULL;
    if (!expect(p, TOK_RPAREN)) {
      ast_free(expr);
      return NULL;
    }
    return expr;
  }

  if (match(p, TOK_MINUS)) {
    advance(p);
    AstNode *operand = parse_primary(p);
    if (!operand)
      return NULL;
    if (operand->type == AST_NUMBER) {
      operand->as.number = -operand->as.number;
      return operand;
    }
    return ast_unary_neg(operand);
  }

  if (match(p, TOK_PLUS)) {
    advance(p);
    return parse_primary(p);
  }

  char buf[64];
  snprintf(buf, sizeof buf, "unexpected token: %s",
           token_type_name(p->current.type));
  set_error(p, buf);
  return NULL;
}

static AstNode *parse_power(Parser *p) {
  AstNode *left = parse_primary(p);
  if (!left)
    return NULL;

  while (match(p, TOK_CARET)) {
    advance(p);
    AstNode *right = parse_primary(p);
    if (!right) {
      ast_free(left);
      return NULL;
    }
    left = ast_binop(OP_POW, left, right);
  }
  return left;
}

static AstNode *parse_factor(Parser *p) {
  AstNode *left = parse_power(p);
  if (!left)
    return NULL;

  while (match(p, TOK_STAR) || match(p, TOK_SLASH)) {
    BinOpKind op = match(p, TOK_STAR) ? OP_MUL : OP_DIV;
    advance(p);
    AstNode *right = parse_power(p);
    if (!right) {
      ast_free(left);
      return NULL;
    }
    left = ast_binop(op, left, right);
  }
  return left;
}

static AstNode *parse_expr(Parser *p) {
  AstNode *left = parse_factor(p);
  if (!left)
    return NULL;

  while (match(p, TOK_PLUS) || match(p, TOK_MINUS)) {
    BinOpKind op = match(p, TOK_PLUS) ? OP_ADD : OP_SUB;
    advance(p);
    AstNode *right = parse_factor(p);
    if (!right) {
      ast_free(left);
      return NULL;
    }
    left = ast_binop(op, left, right);
  }
  return left;
}

ParseResult parse(const char *input) {
  Parser p;
  lexer_init(&p.lex, input);
  p.error = NULL;
  advance(&p);

  AstNode *root = parse_expr(&p);

  if (!p.error && !match(&p, TOK_EOF)) {
    set_error(&p, "unexpected trailing input");
    ast_free(root);
    root = NULL;
  }

  if (p.error && root) {
    ast_free(root);
    root = NULL;
  }

  ParseResult r = {.root = root, .error = p.error};
  return r;
}

void parse_result_free(ParseResult *r) {
  ast_free(r->root);
  r->root = NULL;
  free(r->error);
  r->error = NULL;
}
