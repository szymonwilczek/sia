#include "canonical.h"
#include "factorial.h"
#include "symbolic_internal.h"
#include <stdlib.h>
#include <string.h>

int sym_contains_var(const AstNode *n, const char *var) {
  if (!n)
    return 0;
  switch (n->type) {
  case AST_NUMBER:
    return 0;
  case AST_VARIABLE:
    return strcmp(n->as.variable, var) == 0;
  case AST_BINOP:
    return sym_contains_var(n->as.binop.left, var) ||
           sym_contains_var(n->as.binop.right, var);
  case AST_UNARY_NEG:
    return sym_contains_var(n->as.unary.operand, var);
  case AST_FUNC_CALL:
    for (size_t i = 0; i < n->as.call.nargs; i++)
      if (sym_contains_var(n->as.call.args[i], var))
        return 1;
    return 0;
  case AST_LIMIT:
    if (sym_contains_var(n->as.limit.target, var))
      return 1;
    if (strcmp(n->as.limit.var, var) == 0)
      return 0;
    return sym_contains_var(n->as.limit.expr, var);
  case AST_MATRIX: {
    size_t total = n->as.matrix.rows * n->as.matrix.cols;
    for (size_t i = 0; i < total; i++)
      if (sym_contains_var(n->as.matrix.elements[i], var))
        return 1;
    return 0;
  }
  }
  return 0;
}

AstNode *sym_subs(const AstNode *expr, const char *var, const AstNode *val) {
  if (!expr || !var || !val)
    return NULL;

  switch (expr->type) {
  case AST_NUMBER:
    return ast_clone(expr);
  case AST_VARIABLE:
    if (strcmp(expr->as.variable, var) == 0)
      return ast_clone(val);
    return ast_clone(expr);
  case AST_BINOP:
    return ast_binop(expr->as.binop.op, sym_subs(expr->as.binop.left, var, val),
                     sym_subs(expr->as.binop.right, var, val));
  case AST_UNARY_NEG:
    return ast_unary_neg(sym_subs(expr->as.unary.operand, var, val));
  case AST_FUNC_CALL: {
    AstNode **args = NULL;
    if (expr->as.call.nargs > 0) {
      args = calloc(expr->as.call.nargs, sizeof(AstNode *));
      if (!args)
        return NULL;
      for (size_t i = 0; i < expr->as.call.nargs; i++) {
        args[i] = sym_subs(expr->as.call.args[i], var, val);
        if (!args[i]) {
          for (size_t j = 0; j < i; j++)
            ast_free(args[j]);
          free(args);
          return NULL;
        }
      }
    }
    AstNode *result =
        ast_func_call(expr->as.call.name, strlen(expr->as.call.name), args,
                      expr->as.call.nargs);
    free(args);
    return result;
  }
  case AST_LIMIT: {
    AstNode *target = sym_subs(expr->as.limit.target, var, val);
    AstNode *body = NULL;
    if (strcmp(expr->as.limit.var, var) == 0)
      body = ast_clone(expr->as.limit.expr);
    else
      body = sym_subs(expr->as.limit.expr, var, val);
    if (!body || !target) {
      ast_free(body);
      ast_free(target);
      return NULL;
    }
    return ast_limit(body, expr->as.limit.var, strlen(expr->as.limit.var),
                     target);
  }
  case AST_MATRIX: {
    size_t total = expr->as.matrix.rows * expr->as.matrix.cols;
    AstNode **elems = calloc(total, sizeof(AstNode *));
    if (!elems)
      return NULL;
    for (size_t i = 0; i < total; i++) {
      elems[i] = sym_subs(expr->as.matrix.elements[i], var, val);
      if (!elems[i]) {
        for (size_t j = 0; j < i; j++)
          ast_free(elems[j]);
        free(elems);
        return NULL;
      }
    }
    AstNode *result =
        ast_matrix(elems, expr->as.matrix.rows, expr->as.matrix.cols);
    free(elems);
    return result;
  }
  }

  return NULL;
}

AstNode *sym_taylor(const AstNode *expr, const char *var, const AstNode *a,
                    int order) {
  AstNode *sum = NULL;
  AstNode *current = NULL;
  AstNode *base = NULL;

  if (!expr || !var || !a || order < 0)
    return NULL;

  base = sym_simplify(
      ast_binop(OP_SUB, ast_variable(var, strlen(var)), ast_clone(a)));
  current = ast_clone(expr);
  if (!base || !current) {
    ast_free(base);
    ast_free(current);
    return NULL;
  }

  for (int k = 0; k <= order; k++) {
    AstNode *coeff = sym_subs(current, var, a);
    if (!coeff) {
      ast_free(base);
      ast_free(current);
      ast_free(sum);
      return NULL;
    }
    coeff = sym_simplify(coeff);

    if (!is_zero_node(coeff)) {
      AstNode *term = coeff;

      if (k > 0) {
        AstNode *power = NULL;
        Complex factorial_value;
        int ok = 0;
        char *error = NULL;

        if (k == 1) {
          power = ast_clone(base);
        } else {
          power = ast_binop(OP_POW, ast_clone(base), ast_number(k));
        }
        term = sym_simplify(ast_binop(OP_MUL, term, power));

        factorial_value = factorial_eval_value(
            c_from_fractions(fraction_make(k, 1), fraction_make(0, 1)), &ok,
            &error);
        free(error);
        if (!ok) {
          ast_free(term);
          ast_free(base);
          ast_free(current);
          ast_free(sum);
          return NULL;
        }

        term = sym_simplify(
            ast_binop(OP_DIV, term, ast_number_complex(factorial_value)));
      }

      if (!sum)
        sum = term;
      else
        sum = sym_simplify(ast_binop(OP_ADD, sum, term));
    } else {
      ast_free(coeff);
    }

    if (k < order) {
      AstNode *next = sym_diff(current, var);
      ast_free(current);
      if (!next) {
        ast_free(base);
        ast_free(sum);
        return NULL;
      }
      current = sym_simplify(next);
    }
  }

  ast_free(base);
  ast_free(current);

  if (!sum)
    return ast_number(0);

  sum = ast_canonicalize(sum);
  sum = sym_collect_terms(sum);
  sum = sym_simplify(sum);
  return sum;
}

static AstNode *sym_det_recursive(AstNode **elems, size_t n, size_t stride) {
  if (n == 1)
    return ast_clone(elems[0]);

  if (n == 2) {
    AstNode *ad =
        ast_binop(OP_MUL, ast_clone(elems[0]), ast_clone(elems[stride + 1]));
    AstNode *bc =
        ast_binop(OP_MUL, ast_clone(elems[1]), ast_clone(elems[stride]));
    AstNode *res = ast_binop(OP_SUB, ad, bc);
    res = sym_simplify(res);
    res = sym_expand(res);
    res = ast_canonicalize(res);
    res = sym_collect_terms(res);
    return sym_simplify(res);
  }

  AstNode **sub = malloc((n - 1) * (n - 1) * sizeof(AstNode *));
  AstNode *det = NULL;

  for (size_t col = 0; col < n; col++) {
    size_t si = 0;
    for (size_t r = 1; r < n; r++)
      for (size_t c = 0; c < n; c++) {
        if (c == col)
          continue;
        sub[si++] = elems[r * stride + c];
      }

    AstNode *cofactor = sym_det_recursive(sub, n - 1, n - 1);
    AstNode *term = ast_binop(OP_MUL, ast_clone(elems[col]), cofactor);
    term = sym_simplify(term);

    if (!det) {
      det = col % 2 == 0 ? term : sym_simplify(ast_unary_neg(term));
    } else {
      if (col % 2 == 0)
        det = ast_binop(OP_ADD, det, term);
      else
        det = ast_binop(OP_SUB, det, term);
    }
  }

  free(sub);

  det = sym_simplify(det);
  det = sym_expand(det);
  det = ast_canonicalize(det);
  det = sym_collect_terms(det);
  return sym_simplify(det);
}

AstNode *sym_det(const AstNode *matrix) {
  if (!matrix || matrix->type != AST_MATRIX)
    return NULL;
  if (matrix->as.matrix.rows != matrix->as.matrix.cols)
    return NULL;

  return sym_det_recursive(matrix->as.matrix.elements, matrix->as.matrix.rows,
                           matrix->as.matrix.cols);
}
