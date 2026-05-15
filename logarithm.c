#include "logarithm.h"
#include "symbolic.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int is_number(const AstNode *node, double value) {
  return node && node->type == AST_NUMBER && c_is_real(node->as.number) &&
         node->as.number.re == value;
}

static int ast_equal(const AstNode *a, const AstNode *b) {
  if (!a && !b)
    return 1;
  if (!a || !b || a->type != b->type)
    return 0;

  switch (a->type) {
  case AST_NUMBER:
    return c_eq(a->as.number, b->as.number);
  case AST_VARIABLE:
    return strcmp(a->as.variable, b->as.variable) == 0;
  case AST_BINOP:
    return a->as.binop.op == b->as.binop.op &&
           ast_equal(a->as.binop.left, b->as.binop.left) &&
           ast_equal(a->as.binop.right, b->as.binop.right);
  case AST_UNARY_NEG:
    return ast_equal(a->as.unary.operand, b->as.unary.operand);
  case AST_FUNC_CALL:
    if (strcmp(a->as.call.name, b->as.call.name) != 0 ||
        a->as.call.nargs != b->as.call.nargs)
      return 0;
    for (size_t i = 0; i < a->as.call.nargs; i++) {
      if (!ast_equal(a->as.call.args[i], b->as.call.args[i]))
        return 0;
    }
    return 1;
  case AST_MATRIX:
    if (a->as.matrix.rows != b->as.matrix.rows ||
        a->as.matrix.cols != b->as.matrix.cols)
      return 0;
    for (size_t i = 0; i < a->as.matrix.rows * a->as.matrix.cols; i++) {
      if (!ast_equal(a->as.matrix.elements[i], b->as.matrix.elements[i]))
        return 0;
    }
    return 1;
  }
  return 0;
}

LogKind log_kind(const AstNode *node) {
  if (!node || node->type != AST_FUNC_CALL)
    return LOG_KIND_NONE;

  const char *name = node->as.call.name;
  size_t nargs = node->as.call.nargs;

  if (strcmp(name, "ln") == 0 && nargs == 1)
    return LOG_KIND_LN;
  if (strcmp(name, "log10") == 0 && nargs == 1)
    return LOG_KIND_BASE10;
  if (strcmp(name, "log2") == 0 && nargs == 1)
    return LOG_KIND_BASE2;
  if (strcmp(name, "log") == 0 && nargs == 1)
    return LOG_KIND_BASE10;
  if (strcmp(name, "log") == 0 && nargs == 2)
    return LOG_KIND_GENERIC;
  return LOG_KIND_NONE;
}

Complex log_eval_value_base(Complex value, Complex base, int *ok,
                            char **error) {
  if (c_is_zero(value)) {
    if (ok)
      *ok = 0;
    if (error)
      *error = strdup("domain error: log of zero");
    return c_real(0.0);
  }

  if (c_is_real(base) && base.re == 1.0) {
    if (ok)
      *ok = 0;
    if (error)
      *error = strdup("domain error: log base 1");
    return c_real(0.0);
  }

  if (c_is_real(value) && c_is_real(base) && value.re > 0 && base.re > 0 &&
      base.re != 1.0) {
    if (base.re == 10.0) {
      if (ok)
        *ok = 1;
      return c_real(log10(value.re));
    }
    if (base.re == 2.0) {
      if (ok)
        *ok = 1;
      return c_real(log2(value.re));
    }
    if (ok)
      *ok = 1;
    return c_real(log(value.re) / log(base.re));
  }

  Complex denom = c_log(base);
  if (c_is_zero(denom)) {
    if (ok)
      *ok = 0;
    if (error)
      *error = strdup("domain error: invalid log base");
    return c_real(0.0);
  }

  if (ok)
    *ok = 1;
  return c_div(c_log(value), denom);
}

AstNode *log_simplify_call(AstNode *node) {
  LogKind kind = log_kind(node);
  if (kind == LOG_KIND_NONE)
    return node;

  if (kind == LOG_KIND_LN) {
    if (is_number(node->as.call.args[0], 1.0)) {
      ast_free(node);
      return ast_number(0);
    }
    return node;
  }

  AstNode *value = node->as.call.args[0];
  AstNode *base = NULL;
  if (kind == LOG_KIND_BASE10) {
    base = ast_number(10);
  } else if (kind == LOG_KIND_BASE2) {
    base = ast_number(2);
  } else {
    base = node->as.call.args[1];
    node->as.call.args[1] = NULL;
  }

  node->as.call.args[0] = NULL;
  ast_free(node);

  AstNode *canonical = ast_func_call("log", 3, (AstNode *[]){value, base}, 2);
  value = canonical->as.call.args[0];
  base = canonical->as.call.args[1];

  if (is_number(value, 1.0)) {
    canonical->as.call.args[0] = NULL;
    canonical->as.call.args[1] = NULL;
    ast_free(canonical);
    return ast_number(0);
  }

  if (ast_equal(value, base)) {
    canonical->as.call.args[0] = NULL;
    canonical->as.call.args[1] = NULL;
    ast_free(canonical);
    return ast_number(1);
  }

  if (value->type == AST_NUMBER && base->type == AST_NUMBER) {
    int ok = 0;
    char *error = NULL;
    Complex result =
        log_eval_value_base(value->as.number, base->as.number, &ok, &error);
    free(error);
    if (ok) {
      canonical->as.call.args[0] = NULL;
      canonical->as.call.args[1] = NULL;
      ast_free(canonical);
      return ast_number_complex(result);
    }
  }

  return canonical;
}

AstNode *log_diff_call(const AstNode *node, const char *var) {
  LogKind kind = log_kind(node);
  if (kind == LOG_KIND_NONE)
    return NULL;

  const AstNode *value = node->as.call.args[0];
  const AstNode *base = NULL;
  AstNode *db = NULL;

  if (kind == LOG_KIND_LN) {
    AstNode *dv = sym_diff(value, var);
    return sym_simplify(ast_binop(OP_DIV, dv, ast_clone(value)));
  }

  if (kind == LOG_KIND_BASE10) {
    base = ast_number(10);
    db = ast_number(0);
  } else if (kind == LOG_KIND_BASE2) {
    base = ast_number(2);
    db = ast_number(0);
  } else {
    base = node->as.call.args[1];
    if (!sym_contains_var(base, var)) {
      db = ast_number(0);
    } else {
      db = sym_diff(base, var);
    }
  }

  AstNode *dv = sym_diff(value, var);
  AstNode *ln_value =
      ast_func_call("ln", 2, (AstNode *[]){ast_clone(value)}, 1);
  AstNode *ln_base = ast_func_call("ln", 2, (AstNode *[]){ast_clone(base)}, 1);

  if (!sym_contains_var(base, var)) {
    AstNode *denom = ast_binop(OP_MUL, ast_clone(value), ln_base);
    if (kind == LOG_KIND_BASE10 || kind == LOG_KIND_BASE2)
      ast_free((AstNode *)base);
    ast_free(ln_value);
    ast_free(db);
    return sym_simplify(ast_binop(OP_DIV, dv, denom));
  }

  AstNode *term1 = ast_binop(OP_MUL, ast_binop(OP_DIV, dv, ast_clone(value)),
                             ast_clone(ln_base));
  AstNode *term2 =
      ast_binop(OP_MUL, ln_value, ast_binop(OP_DIV, db, ast_clone(base)));
  AstNode *numerator = ast_binop(OP_SUB, term1, term2);
  AstNode *denominator = ast_binop(OP_POW, ln_base, ast_number(2));

  if (kind == LOG_KIND_BASE10 || kind == LOG_KIND_BASE2)
    ast_free((AstNode *)base);

  return sym_simplify(ast_binop(OP_DIV, numerator, denominator));
}

AstNode *log_solve_call(const AstNode *node, const AstNode *rhs,
                        const char *var) {
  LogKind kind = log_kind(node);
  if (kind == LOG_KIND_NONE || !rhs || sym_contains_var(rhs, var))
    return NULL;

  const AstNode *value = node->as.call.args[0];
  const AstNode *base = NULL;
  int value_has_var = sym_contains_var(value, var);
  int base_has_var = 0;

  if (kind == LOG_KIND_LN) {
    if (!value_has_var)
      return NULL;
    return ast_func_call("exp", 3, (AstNode *[]){ast_clone(rhs)}, 1);
  }

  if (kind == LOG_KIND_BASE10) {
    base = ast_number(10);
  } else if (kind == LOG_KIND_BASE2) {
    base = ast_number(2);
  } else {
    base = node->as.call.args[1];
    base_has_var = sym_contains_var(base, var);
  }

  if (value_has_var && !base_has_var) {
    AstNode *res = ast_binop(OP_POW, ast_clone(base), ast_clone(rhs));
    if (kind == LOG_KIND_BASE10 || kind == LOG_KIND_BASE2)
      ast_free((AstNode *)base);
    return res;
  }

  if (!value_has_var && base_has_var) {
    if (is_number(rhs, 0.0)) {
      if (kind == LOG_KIND_BASE10 || kind == LOG_KIND_BASE2)
        ast_free((AstNode *)base);
      return NULL;
    }
    AstNode *inv = ast_binop(OP_DIV, ast_number(1), ast_clone(rhs));
    AstNode *res = ast_binop(OP_POW, ast_clone(value), inv);
    if (kind == LOG_KIND_BASE10 || kind == LOG_KIND_BASE2)
      ast_free((AstNode *)base);
    return res;
  }

  if (kind == LOG_KIND_BASE10 || kind == LOG_KIND_BASE2)
    ast_free((AstNode *)base);
  return NULL;
}
