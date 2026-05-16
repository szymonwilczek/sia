#include "sia/parser.h"
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
static AstNode *parse_primary(Parser *p);

static AstNode *parse_postfix(Parser *p) {
  AstNode *node = parse_primary(p);
  if (!node)
    return NULL;

  while (match(p, TOK_BANG)) {
    advance(p);
    AstNode *args[] = {node};
    node = ast_func_call("factorial", 10, args, 1);
  }

  return node;
}

static AstNode *parse_unary(Parser *p) {
  if (match(p, TOK_PLUS)) {
    advance(p);
    return parse_unary(p);
  }

  if (match(p, TOK_MINUS)) {
    advance(p);
    AstNode *operand = parse_unary(p);
    if (!operand)
      return NULL;
    if (operand->type == AST_NUMBER) {
      operand->as.number = c_neg(operand->as.number);
      return operand;
    }
    return ast_unary_neg(operand);
  }

  return parse_postfix(p);
}

static AstNode *parse_matrix(Parser *p) {
  /* parse [e00, e01; e10, e11] style matrices */
  AstNode *data[256];
  size_t total_parsed = 0;
  size_t rows = 0, cols = 0;
  size_t current_row_cols = 0;

  while (!match(p, TOK_RBRACKET) && !match(p, TOK_EOF)) {
    AstNode *elem = parse_expr(p);
    if (!elem)
      goto cleanup;

    if (total_parsed >= 256) {
      set_error(p, "matrix too large (max 256 elements)");
      ast_free(elem);
      goto cleanup;
    }

    data[total_parsed++] = elem;
    current_row_cols++;

    if (match(p, TOK_COMMA)) {
      advance(p);
    } else if (match(p, TOK_SEMICOLON) || match(p, TOK_RBRACKET)) {
      if (rows == 0) {
        cols = current_row_cols;
      } else if (current_row_cols != cols) {
        set_error(p, "matrix rows must have equal number of columns");
        goto cleanup;
      }
      rows++;
      current_row_cols = 0;
      if (match(p, TOK_SEMICOLON))
        advance(p);
    } else {
      set_error(p, "expected ',' or ';' or ']' in matrix");
      goto cleanup;
    }
  }

  if (!expect(p, TOK_RBRACKET))
    goto cleanup;

  if (rows == 0 && current_row_cols > 0) {
    rows = 1;
    cols = current_row_cols;
  }

  if (rows == 0) {
    set_error(p, "empty matrix");
    goto cleanup;
  }

  /* support for nested brackets: [[a,b],[c,d]] -> 2x2 matrix
   * If 1xN matrix where every element is a 1xM matrix -> flatten it */
  if (rows == 1 && total_parsed > 0 && data[0]->type == AST_MATRIX) {
    size_t inner_cols = data[0]->as.matrix.cols;
    size_t inner_rows = data[0]->as.matrix.rows;
    int all_match = (inner_rows == 1);
    for (size_t i = 1; i < total_parsed; i++) {
      if (data[i]->type != AST_MATRIX ||
          data[i]->as.matrix.cols != inner_cols ||
          data[i]->as.matrix.rows != 1) {
        all_match = 0;
        break;
      }
    }

    if (all_match) {
      size_t new_rows = total_parsed;
      size_t new_cols = inner_cols;
      AstNode **new_elements = malloc(new_rows * new_cols * sizeof(AstNode *));
      for (size_t i = 0; i < new_rows; i++) {
        for (size_t j = 0; j < new_cols; j++) {
          new_elements[i * new_cols + j] =
              ast_clone(data[i]->as.matrix.elements[j]);
        }
        ast_free(data[i]);
      }
      return ast_matrix(new_elements, new_rows, new_cols);
    }
  }

  AstNode **final_data = malloc(total_parsed * sizeof(AstNode *));
  memcpy(final_data, data, total_parsed * sizeof(AstNode *));
  return ast_matrix(final_data, rows, cols);

cleanup:
  /* free partial row in progress */
  for (size_t i = 0; i < total_parsed; i++) {
    ast_free(data[i]);
  }
  return NULL;
}

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

    if (len == 1 && name[0] == 'i') {
      return ast_complex(0.0, 1.0);
    }

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

      if ((len == 3 && strncmp(name, "lim", 3) == 0) ||
          (len == 5 && strncmp(name, "limit", 5) == 0)) {
        if (nargs != 3 || args[1]->type != AST_VARIABLE) {
          set_error(p, "lim expects arguments: lim(expr, var, target)");
          for (size_t i = 0; i < nargs; i++)
            ast_free(args[i]);
          return NULL;
        }

        AstNode *limit = ast_limit(args[0], args[1]->as.variable,
                                   strlen(args[1]->as.variable), args[2]);
        args[0] = NULL;
        args[2] = NULL;
        ast_free(args[1]);
        return limit;
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

  if (match(p, TOK_LBRACKET)) {
    advance(p); /* consume outer '[' */
    return parse_matrix(p);
  }

  char buf[64];
  snprintf(buf, sizeof buf, "unexpected token: %s",
           token_type_name(p->current.type));
  set_error(p, buf);
  return NULL;
}

static AstNode *parse_power(Parser *p) {
  AstNode *left = parse_unary(p);
  if (!left)
    return NULL;

  while (match(p, TOK_CARET)) {
    advance(p);
    AstNode *right = parse_unary(p);
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

  while (match(p, TOK_STAR) || match(p, TOK_SLASH) || match(p, TOK_IDENT) ||
         match(p, TOK_LPAREN) || match(p, TOK_LBRACKET)) {
    BinOpKind op = OP_MUL;
    if (match(p, TOK_STAR)) {
      advance(p);
    } else if (match(p, TOK_SLASH)) {
      op = OP_DIV;
      advance(p);
    }

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
