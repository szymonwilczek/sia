#include "ast.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *xmalloc(size_t n) {
  void *p = malloc(n);
  if (!p) {
    fprintf(stderr, "sia: out of memory\n");
    abort();
  }
  return p;
}

static char *xstrndup(const char *s, size_t n) {
  char *p = xmalloc(n + 1);
  memcpy(p, s, n);
  p[n] = '\0';
  return p;
}

AstNode *ast_number(double val) {
  AstNode *n = xmalloc(sizeof *n);
  n->type = AST_NUMBER;
  n->as.number = val;
  return n;
}

AstNode *ast_variable(const char *name, size_t len) {
  AstNode *n = xmalloc(sizeof *n);
  n->type = AST_VARIABLE;
  n->as.variable = xstrndup(name, len);
  return n;
}

AstNode *ast_binop(BinOpKind op, AstNode *left, AstNode *right) {
  AstNode *n = xmalloc(sizeof *n);
  n->type = AST_BINOP;
  n->as.binop.op = op;
  n->as.binop.left = left;
  n->as.binop.right = right;
  return n;
}

AstNode *ast_unary_neg(AstNode *operand) {
  AstNode *n = xmalloc(sizeof *n);
  n->type = AST_UNARY_NEG;
  n->as.unary.operand = operand;
  return n;
}

AstNode *ast_func_call(const char *name, size_t namelen, AstNode **args,
                       size_t nargs) {
  AstNode *n = xmalloc(sizeof *n);
  n->type = AST_FUNC_CALL;
  n->as.call.name = xstrndup(name, namelen);
  n->as.call.args = NULL;
  n->as.call.nargs = nargs;
  if (nargs > 0) {
    n->as.call.args = xmalloc(nargs * sizeof(AstNode *));
    memcpy(n->as.call.args, args, nargs * sizeof(AstNode *));
  }
  return n;
}

AstNode *ast_matrix(Matrix *m) {
  AstNode *n = xmalloc(sizeof *n);
  n->type = AST_MATRIX;
  n->as.matrix = m;
  return n;
}

AstNode *ast_clone(const AstNode *node) {
  if (!node)
    return NULL;

  switch (node->type) {
  case AST_NUMBER:
    return ast_number(node->as.number);
  case AST_VARIABLE: {
    size_t len = strlen(node->as.variable);
    return ast_variable(node->as.variable, len);
  }
  case AST_BINOP:
    return ast_binop(node->as.binop.op, ast_clone(node->as.binop.left),
                     ast_clone(node->as.binop.right));
  case AST_UNARY_NEG:
    return ast_unary_neg(ast_clone(node->as.unary.operand));
  case AST_FUNC_CALL: {
    AstNode **args = NULL;
    size_t n = node->as.call.nargs;
    if (n > 0) {
      args = xmalloc(n * sizeof(AstNode *));
      for (size_t i = 0; i < n; i++)
        args[i] = ast_clone(node->as.call.args[i]);
    }
    size_t namelen = strlen(node->as.call.name);
    AstNode *r = ast_func_call(node->as.call.name, namelen, args, n);
    free(args);
    return r;
  }
  case AST_MATRIX:
    return ast_matrix(matrix_clone(node->as.matrix));
  }
  return NULL;
}

void ast_free(AstNode *node) {
  if (!node)
    return;

  switch (node->type) {
  case AST_NUMBER:
    break;
  case AST_VARIABLE:
    free(node->as.variable);
    break;
  case AST_BINOP:
    ast_free(node->as.binop.left);
    ast_free(node->as.binop.right);
    break;
  case AST_UNARY_NEG:
    ast_free(node->as.unary.operand);
    break;
  case AST_FUNC_CALL:
    for (size_t i = 0; i < node->as.call.nargs; i++)
      ast_free(node->as.call.args[i]);
    free(node->as.call.args);
    free(node->as.call.name);
    break;
  case AST_MATRIX:
    matrix_free(node->as.matrix);
    break;
  }
  free(node);
}

/* AST serialization */
typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} StrBuf;

static void sb_init(StrBuf *sb) {
  sb->cap = 64;
  sb->len = 0;
  sb->buf = xmalloc(sb->cap);
  sb->buf[0] = '\0';
}

static void sb_append(StrBuf *sb, const char *s, size_t n) {
  while (sb->len + n + 1 > sb->cap) {
    sb->cap *= 2;
    sb->buf = realloc(sb->buf, sb->cap);
    if (!sb->buf)
      abort();
  }
  memcpy(sb->buf + sb->len, s, n);
  sb->len += n;
  sb->buf[sb->len] = '\0';
}

static void sb_printf(StrBuf *sb, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

static void sb_printf(StrBuf *sb, const char *fmt, ...) {
  char tmp[128];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
  va_end(ap);
  if (n > 0)
    sb_append(sb, tmp, (size_t)n);
}

static int needs_parens(const AstNode *child, const AstNode *parent,
                        int is_right) {
  if (child->type != AST_BINOP || parent->type != AST_BINOP)
    return 0;

  static const int prec[] = {
      [OP_ADD] = 1, [OP_SUB] = 1, [OP_MUL] = 2, [OP_DIV] = 2, [OP_POW] = 3};

  int cp = prec[child->as.binop.op];
  int pp = prec[parent->as.binop.op];

  if (cp < pp)
    return 1;
  if (cp == pp && is_right &&
      (parent->as.binop.op == OP_SUB || parent->as.binop.op == OP_DIV))
    return 1;
  return 0;
}

static void ast_serialize(const AstNode *node, StrBuf *sb,
                          const AstNode *parent, int is_right) {
  if (!node)
    return;

  switch (node->type) {
  case AST_NUMBER: {
    double v = node->as.number;
    if (v == (long long)v && fabs(v) < 1e15)
      sb_printf(sb, "%lld", (long long)v);
    else
      sb_printf(sb, "%g", v);
    break;
  }
  case AST_VARIABLE:
    sb_append(sb, node->as.variable, strlen(node->as.variable));
    break;
  case AST_BINOP: {
    int parens = parent && needs_parens(node, parent, is_right);
    if (parens)
      sb_append(sb, "(", 1);

    ast_serialize(node->as.binop.left, sb, node, 0);

    const char *ops[] = {[OP_ADD] = " + ",
                         [OP_SUB] = " - ",
                         [OP_MUL] = "*",
                         [OP_DIV] = "/",
                         [OP_POW] = "^"};
    const char *op = ops[node->as.binop.op];
    sb_append(sb, op, strlen(op));

    ast_serialize(node->as.binop.right, sb, node, 1);

    if (parens)
      sb_append(sb, ")", 1);
    break;
  }
  case AST_UNARY_NEG:
    sb_append(sb, "(-", 2);
    ast_serialize(node->as.unary.operand, sb, node, 0);
    sb_append(sb, ")", 1);
    break;
  case AST_FUNC_CALL:
    sb_append(sb, node->as.call.name, strlen(node->as.call.name));
    sb_append(sb, "(", 1);
    for (size_t i = 0; i < node->as.call.nargs; i++) {
      if (i > 0)
        sb_append(sb, ", ", 2);
      ast_serialize(node->as.call.args[i], sb, NULL, 0);
    }
    sb_append(sb, ")", 1);
    break;
  case AST_MATRIX: {
    char *ms = matrix_to_string(node->as.matrix);
    sb_append(sb, ms, strlen(ms));
    free(ms);
    break;
  }
  }
}

char *ast_to_string(const AstNode *node) {
  StrBuf sb;
  sb_init(&sb);
  ast_serialize(node, &sb, NULL, 0);
  return sb.buf;
}
