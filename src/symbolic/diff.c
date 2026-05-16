#include "sia/logarithm.h"
#include "sia/trigonometry.h"
#include "symbolic_internal.h"
#include <stdlib.h>
#include <string.h>

AstNode *sym_diff(const AstNode *expr, const char *var) {
  if (!expr)
    return ast_number(0);

  if (expr->type == AST_BINOP &&
      (expr->as.binop.op == OP_DIV || expr->as.binop.op == OP_MUL)) {
    AstNode *pre = sym_reduce_rational_subexprs(expr);
    if (!ast_equal(pre, expr)) {
      AstNode *result = sym_diff(pre, var);
      ast_free(pre);
      return result;
    }
    ast_free(pre);
  }

  switch (expr->type) {
  case AST_NUMBER:
    return ast_number(0);

  case AST_MATRIX: {
    size_t total = expr->as.matrix.rows * expr->as.matrix.cols;
    AstNode **elems = malloc(total * sizeof(AstNode *));
    for (size_t i = 0; i < total; i++)
      elems[i] = sym_diff(expr->as.matrix.elements[i], var);
    AstNode *r = ast_matrix(elems, expr->as.matrix.rows, expr->as.matrix.cols);
    free(elems);
    return sym_simplify(r);
  }

  case AST_VARIABLE:
    return ast_number(is_var(expr, var) ? 1.0 : 0.0);

  case AST_UNARY_NEG:
    return sym_simplify(ast_unary_neg(sym_diff(expr->as.unary.operand, var)));

  case AST_BINOP: {
    const AstNode *L = expr->as.binop.left;
    const AstNode *R = expr->as.binop.right;

    switch (expr->as.binop.op) {
    case OP_ADD:
      return sym_simplify(
          ast_binop(OP_ADD, sym_diff(L, var), sym_diff(R, var)));

    case OP_SUB:
      return sym_simplify(
          ast_binop(OP_SUB, sym_diff(L, var), sym_diff(R, var)));

    case OP_MUL:
      return sym_simplify(
          ast_binop(OP_ADD, ast_binop(OP_MUL, sym_diff(L, var), ast_clone(R)),
                    ast_binop(OP_MUL, ast_clone(L), sym_diff(R, var))));

    case OP_DIV:
      return sym_simplify(ast_binop(
          OP_DIV,
          ast_binop(OP_SUB, ast_binop(OP_MUL, sym_diff(L, var), ast_clone(R)),
                    ast_binop(OP_MUL, ast_clone(L), sym_diff(R, var))),
          ast_binop(OP_POW, ast_clone(R), ast_number(2))));

    case OP_POW:
      if (!sym_contains_var(R, var) && !sym_contains_var(L, var)) {
        return ast_number(0);
      }
      /* x^n where n constant: n*x^(n-1)*x' */
      if (!sym_contains_var(R, var)) {
        return sym_simplify(ast_binop(
            OP_MUL,
            ast_binop(
                OP_MUL, ast_clone(R),
                ast_binop(OP_POW, ast_clone(L),
                          ast_binop(OP_SUB, ast_clone(R), ast_number(1)))),
            sym_diff(L, var)));
      }
      /* a^x where a constant: a^x * ln(a) * x' */
      if (!sym_contains_var(L, var)) {
        AstNode *lna_args[1] = {ast_clone(L)};
        return sym_simplify(
            ast_binop(OP_MUL,
                      ast_binop(OP_MUL, ast_clone(expr),
                                ast_func_call("ln", 2, lna_args, 1)),
                      sym_diff(R, var)));
      }
      /* f(x)^g(x): rewrite as e^(g*ln(f)), differentiate that
       * d/dx[f^g] = f^g * (g'*ln(f) + g*f'/f) */
      {
        AstNode *ln_f_args[1] = {ast_clone(L)};
        AstNode *ln_f = ast_func_call("ln", 2, ln_f_args, 1);

        AstNode *term1 = ast_binop(OP_MUL, sym_diff(R, var), ast_clone(ln_f));

        AstNode *term2 =
            ast_binop(OP_MUL, ast_clone(R),
                      ast_binop(OP_DIV, sym_diff(L, var), ast_clone(L)));

        ast_free(ln_f);

        return sym_simplify(ast_binop(OP_MUL, ast_clone(expr),
                                      ast_binop(OP_ADD, term1, term2)));
      }
    }
    break;
  }

  case AST_FUNC_CALL: {
    if (!sym_contains_var(expr, var))
      return ast_number(0);

    if ((strcmp(expr->as.call.name, "int") == 0 ||
         strcmp(expr->as.call.name, "integrate") == 0) &&
        expr->as.call.nargs == 2 &&
        expr->as.call.args[1]->type == AST_VARIABLE &&
        strcmp(expr->as.call.args[1]->as.variable, var) == 0) {
      return sym_full_simplify(ast_clone(expr->as.call.args[0]));
    }

    if (log_kind(expr) != LOG_KIND_NONE)
      return log_diff_call(expr, var);

    if (trig_kind(expr) != TRIG_KIND_NONE)
      return trig_diff_call(expr, var);

    const char *name = expr->as.call.name;
    if (expr->as.call.nargs != 1)
      return NULL;
    const AstNode *inner = expr->as.call.args[0];
    AstNode *inner_d = sym_diff(inner, var);

    AstNode *outer_d = NULL;
    if (strcmp(name, "sin") == 0) {
      outer_d = ast_func_call("cos", 3, (AstNode *[]){ast_clone(inner)}, 1);
    } else if (strcmp(name, "cos") == 0) {
      outer_d = ast_unary_neg(
          ast_func_call("sin", 3, (AstNode *[]){ast_clone(inner)}, 1));
    } else if (strcmp(name, "tan") == 0) {
      outer_d = ast_binop(
          OP_DIV, ast_number(1),
          ast_binop(OP_POW,
                    ast_func_call("cos", 3, (AstNode *[]){ast_clone(inner)}, 1),
                    ast_number(2)));
    } else if (strcmp(name, "sinh") == 0) {
      outer_d = ast_func_call("cosh", 4, (AstNode *[]){ast_clone(inner)}, 1);
    } else if (strcmp(name, "cosh") == 0) {
      outer_d = ast_func_call("sinh", 4, (AstNode *[]){ast_clone(inner)}, 1);
    } else if (strcmp(name, "tanh") == 0) {
      outer_d =
          ast_binop(OP_DIV, ast_number(1),
                    ast_binop(OP_POW,
                              ast_func_call("cosh", 4,
                                            (AstNode *[]){ast_clone(inner)}, 1),
                              ast_number(2)));
    } else if (strcmp(name, "ln") == 0) {
      outer_d = ast_binop(OP_DIV, ast_number(1), ast_clone(inner));
    } else if (strcmp(name, "exp") == 0) {
      outer_d = ast_clone(expr);
    } else if (strcmp(name, "sqrt") == 0) {
      outer_d = ast_binop(
          OP_DIV, ast_number(1),
          ast_binop(
              OP_MUL, ast_number(2),
              ast_func_call("sqrt", 4, (AstNode *[]){ast_clone(inner)}, 1)));
    } else if (strcmp(name, "abs") == 0) {
      outer_d = ast_binop(
          OP_DIV, ast_clone(inner),
          ast_func_call("abs", 3, (AstNode *[]){ast_clone(inner)}, 1));
    } else if (strcmp(name, "asin") == 0) {
      /* 1/sqrt(1-u^2) */
      outer_d = ast_binop(OP_DIV, ast_number(1),
                          ast_func_call("sqrt", 4,
                                        (AstNode *[]){ast_binop(
                                            OP_SUB, ast_number(1),
                                            ast_binop(OP_POW, ast_clone(inner),
                                                      ast_number(2)))},
                                        1));
    } else if (strcmp(name, "acos") == 0) {
      /* -1/sqrt(1-u^2) */
      outer_d = ast_unary_neg(ast_binop(
          OP_DIV, ast_number(1),
          ast_func_call(
              "sqrt", 4,
              (AstNode *[]){ast_binop(
                  OP_SUB, ast_number(1),
                  ast_binop(OP_POW, ast_clone(inner), ast_number(2)))},
              1)));
    } else if (strcmp(name, "atan") == 0) {
      /* 1/(1+u^2) */
      outer_d = ast_binop(
          OP_DIV, ast_number(1),
          ast_binop(OP_ADD, ast_number(1),
                    ast_binop(OP_POW, ast_clone(inner), ast_number(2))));
    } else {
      ast_free(inner_d);
      return NULL;
    }

    return sym_simplify(ast_binop(OP_MUL, outer_d, inner_d));
  }

  case AST_LIMIT:
    if (!sym_contains_var(expr, var))
      return ast_number(0);
    return NULL;
  }

  return NULL;
}

AstNode *sym_diff_n(const AstNode *expr, const char *var, int order) {
  AstNode *current = NULL;

  if (!expr || !var || order < 0)
    return NULL;

  current = ast_clone(expr);
  if (!current)
    return NULL;

  if (order == 0)
    return sym_full_simplify(current);

  for (int i = 0; i < order; i++) {
    AstNode *next = sym_diff(current, var);
    ast_free(current);
    if (!next)
      return NULL;
    current = sym_full_simplify(next);
  }

  return current;
}

AstNode *sym_grad(const AstNode *expr, const AstNode *vars_matrix) {
  size_t total = 0;
  AstNode **elems = NULL;
  AstNode *result = NULL;

  if (!expr || !vars_matrix || vars_matrix->type != AST_MATRIX)
    return NULL;

  total = vars_matrix->as.matrix.rows * vars_matrix->as.matrix.cols;
  elems = calloc(total, sizeof(AstNode *));
  if (!elems)
    return NULL;

  for (size_t i = 0; i < total; i++) {
    const AstNode *var_node = vars_matrix->as.matrix.elements[i];
    if (!var_node || var_node->type != AST_VARIABLE) {
      for (size_t j = 0; j < i; j++)
        ast_free(elems[j]);
      free(elems);
      return NULL;
    }

    elems[i] = sym_diff(expr, var_node->as.variable);
    if (!elems[i]) {
      for (size_t j = 0; j <= i; j++)
        ast_free(elems[j]);
      free(elems);
      return NULL;
    }
    elems[i] = sym_simplify(elems[i]);
  }

  result = ast_matrix(elems, vars_matrix->as.matrix.rows,
                      vars_matrix->as.matrix.cols);
  free(elems);
  return result;
}
