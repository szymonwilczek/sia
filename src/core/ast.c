#include "sia/ast.h"
#include "sia/factorial.h"
#include "sia/fractions.h"
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
  n->as.number = c_real(val);
  return n;
}

AstNode *ast_number_complex(Complex value) {
  AstNode *n = xmalloc(sizeof *n);
  n->type = AST_NUMBER;
  n->as.number = value;
  return n;
}

AstNode *ast_complex(double re, double im) {
  AstNode *n = xmalloc(sizeof *n);
  n->type = AST_NUMBER;
  n->as.number = c_make(re, im);
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

AstNode *ast_limit(AstNode *expr, const char *var, size_t varlen,
                   AstNode *target) {
  return ast_limit_directed(expr, var, varlen, target, 0);
}

AstNode *ast_limit_directed(AstNode *expr, const char *var, size_t varlen,
                            AstNode *target, int direction) {
  AstNode *n = xmalloc(sizeof *n);
  n->type = AST_LIMIT;
  n->as.limit.expr = expr;
  n->as.limit.var = xstrndup(var, varlen);
  n->as.limit.target = target;
  n->as.limit.direction = direction < 0 ? -1 : (direction > 0 ? 1 : 0);
  return n;
}

AstNode *ast_matrix(AstNode **elements, size_t rows, size_t cols) {
  AstNode *n = xmalloc(sizeof *n);
  n->type = AST_MATRIX;
  n->as.matrix.rows = rows;
  n->as.matrix.cols = cols;
  size_t total = rows * cols;
  n->as.matrix.elements = xmalloc(total * sizeof(AstNode *));
  memcpy(n->as.matrix.elements, elements, total * sizeof(AstNode *));
  return n;
}

AstNode *ast_eq(AstNode *lhs, AstNode *rhs) {
  AstNode *n = xmalloc(sizeof *n);
  n->type = AST_EQ;
  n->as.eq.lhs = lhs;
  n->as.eq.rhs = rhs;
  return n;
}

AstNode *ast_infinity(int sign) {
  AstNode *n = xmalloc(sizeof *n);
  n->type = AST_INFINITY;
  n->as.infinity.sign = sign >= 0 ? 1 : -1;
  return n;
}

AstNode *ast_undefined(void) {
  AstNode *n = xmalloc(sizeof *n);
  n->type = AST_UNDEFINED;
  return n;
}

AstNode *ast_clone(const AstNode *node) {
  if (!node)
    return NULL;

  switch (node->type) {
  case AST_NUMBER:
    return ast_number_complex(node->as.number);
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
  case AST_LIMIT:
    return ast_limit_directed(ast_clone(node->as.limit.expr),
                              node->as.limit.var, strlen(node->as.limit.var),
                              ast_clone(node->as.limit.target),
                              node->as.limit.direction);
  case AST_MATRIX: {
    size_t total = node->as.matrix.rows * node->as.matrix.cols;
    AstNode **elems = xmalloc(total * sizeof(AstNode *));
    for (size_t i = 0; i < total; i++)
      elems[i] = ast_clone(node->as.matrix.elements[i]);
    AstNode *r = ast_matrix(elems, node->as.matrix.rows, node->as.matrix.cols);
    free(elems);
    return r;
  }
  case AST_EQ:
    return ast_eq(ast_clone(node->as.eq.lhs), ast_clone(node->as.eq.rhs));
  case AST_INFINITY:
    return ast_infinity(node->as.infinity.sign);
  case AST_UNDEFINED:
    return ast_undefined();
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
  case AST_LIMIT:
    ast_free(node->as.limit.expr);
    free(node->as.limit.var);
    ast_free(node->as.limit.target);
    break;
  case AST_MATRIX: {
    size_t total = node->as.matrix.rows * node->as.matrix.cols;
    for (size_t i = 0; i < total; i++)
      ast_free(node->as.matrix.elements[i]);
    free(node->as.matrix.elements);
    break;
  }
  case AST_EQ:
    ast_free(node->as.eq.lhs);
    ast_free(node->as.eq.rhs);
    break;
  case AST_INFINITY:
  case AST_UNDEFINED:
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

static void sb_append_fraction(StrBuf *sb, Fraction f) {
  if (f.den == 1) {
    sb_printf(sb, "%lld", f.num);
  } else {
    sb_printf(sb, "%lld/%lld", f.num, f.den);
  }
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
    Complex z = node->as.number;
    if (z.exact && fraction_is_zero(z.im_q)) {
      sb_append_fraction(sb, z.re_q);
    } else if (z.exact && fraction_is_zero(z.re_q)) {
      if (fraction_is_one(z.im_q)) {
        sb_append(sb, "i", 1);
      } else if (z.im_q.num == -z.im_q.den) {
        sb_append(sb, "(-i)", 4);
      } else if (z.im_q.den == 1) {
        sb_printf(sb, "%lldi", z.im_q.num);
      } else {
        sb_printf(sb, "%lld/%lldi", z.im_q.num, z.im_q.den);
      }
    } else if (z.exact) {
      sb_append(sb, "(", 1);
      sb_append_fraction(sb, z.re_q);
      if (z.im_q.num > 0) {
        sb_append(sb, " + ", 3);
        if (fraction_is_one(z.im_q))
          sb_append(sb, "i", 1);
        else if (z.im_q.den == 1)
          sb_printf(sb, "%lldi", z.im_q.num);
        else
          sb_printf(sb, "%lld/%lldi", z.im_q.num, z.im_q.den);
      } else {
        sb_append(sb, " - ", 3);
        Fraction aim = fraction_neg(z.im_q);
        if (fraction_is_one(aim))
          sb_append(sb, "i", 1);
        else if (aim.den == 1)
          sb_printf(sb, "%lldi", aim.num);
        else
          sb_printf(sb, "%lld/%lldi", aim.num, aim.den);
      }
      sb_append(sb, ")", 1);
    } else if (z.im == 0.0) {
      double v = z.re;
      Fraction f = fraction_from_double(v);
      if (f.den != 1) {
        sb_append_fraction(sb, f);
      } else if (v == (long long)v && fabs(v) < 1e15) {
        sb_printf(sb, "%lld", (long long)v);
      } else {
        sb_printf(sb, "%g", v);
      }
    } else if (z.re == 0.0) {
      if (z.im == 1.0)
        sb_append(sb, "i", 1);
      else if (z.im == -1.0)
        sb_append(sb, "(-i)", 4);
      else if (z.im == (long long)z.im && fabs(z.im) < 1e15)
        sb_printf(sb, "%lldi", (long long)z.im);
      else
        sb_printf(sb, "%gi", z.im);
    } else {
      sb_append(sb, "(", 1);
      if (z.re == (long long)z.re && fabs(z.re) < 1e15)
        sb_printf(sb, "%lld", (long long)z.re);
      else
        sb_printf(sb, "%g", z.re);
      if (z.im > 0) {
        sb_append(sb, " + ", 3);
        if (z.im == 1.0)
          sb_append(sb, "i", 1);
        else if (z.im == (long long)z.im && fabs(z.im) < 1e15)
          sb_printf(sb, "%lldi", (long long)z.im);
        else
          sb_printf(sb, "%gi", z.im);
      } else {
        sb_append(sb, " - ", 3);
        double aim = -z.im;
        if (aim == 1.0)
          sb_append(sb, "i", 1);
        else if (aim == (long long)aim && fabs(aim) < 1e15)
          sb_printf(sb, "%lldi", (long long)aim);
        else
          sb_printf(sb, "%gi", aim);
      }
      sb_append(sb, ")", 1);
    }
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
    if (factorial_is_call(node)) {
      const AstNode *arg = node->as.call.args[0];
      if (arg->type == AST_BINOP || arg->type == AST_UNARY_NEG) {
        sb_append(sb, "(", 1);
        ast_serialize(arg, sb, NULL, 0);
        sb_append(sb, ")", 1);
      } else {
        ast_serialize(arg, sb, NULL, 0);
      }
      sb_append(sb, "!", 1);
      break;
    }
    sb_append(sb, node->as.call.name, strlen(node->as.call.name));
    sb_append(sb, "(", 1);
    for (size_t i = 0; i < node->as.call.nargs; i++) {
      if (i > 0)
        sb_append(sb, ", ", 2);
      ast_serialize(node->as.call.args[i], sb, NULL, 0);
    }
    sb_append(sb, ")", 1);
    break;
  case AST_LIMIT:
    sb_append(sb, "lim(", 4);
    ast_serialize(node->as.limit.expr, sb, NULL, 0);
    sb_append(sb, ", ", 2);
    sb_append(sb, node->as.limit.var, strlen(node->as.limit.var));
    sb_append(sb, ", ", 2);
    ast_serialize(node->as.limit.target, sb, NULL, 0);
    if (node->as.limit.direction > 0)
      sb_append(sb, "+", 1);
    else if (node->as.limit.direction < 0)
      sb_append(sb, "-", 1);
    sb_append(sb, ")", 1);
    break;
  case AST_EQ:
    ast_serialize(node->as.eq.lhs, sb, NULL, 0);
    sb_append(sb, " = ", 3);
    ast_serialize(node->as.eq.rhs, sb, NULL, 0);
    break;
  case AST_INFINITY:
    if (node->as.infinity.sign < 0)
      sb_append(sb, "-inf", 4);
    else
      sb_append(sb, "inf", 3);
    break;
  case AST_UNDEFINED:
    sb_append(sb, "undefined", 9);
    break;
  case AST_MATRIX: {
    sb_append(sb, "[", 1);
    for (size_t r = 0; r < node->as.matrix.rows; r++) {
      if (r > 0)
        sb_append(sb, "; ", 2);
      for (size_t c = 0; c < node->as.matrix.cols; c++) {
        if (c > 0)
          sb_append(sb, ", ", 2);
        size_t idx = r * node->as.matrix.cols + c;
        ast_serialize(node->as.matrix.elements[idx], sb, NULL, 0);
      }
    }
    sb_append(sb, "]", 1);
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
