#include "symbolic.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int is_number(const AstNode *n, double v) {
  return n && n->type == AST_NUMBER && n->as.number == v;
}

static int is_num(const AstNode *n) { return n && n->type == AST_NUMBER; }

static int is_var(const AstNode *n, const char *var) {
  return n && n->type == AST_VARIABLE && strcmp(n->as.variable, var) == 0;
}

static int contains_var(const AstNode *n, const char *var) {
  if (!n)
    return 0;
  switch (n->type) {
  case AST_NUMBER:
    return 0;
  case AST_VARIABLE:
    return strcmp(n->as.variable, var) == 0;
  case AST_BINOP:
    return contains_var(n->as.binop.left, var) ||
           contains_var(n->as.binop.right, var);
  case AST_UNARY_NEG:
    return contains_var(n->as.unary.operand, var);
  case AST_FUNC_CALL:
    for (size_t i = 0; i < n->as.call.nargs; i++)
      if (contains_var(n->as.call.args[i], var))
        return 1;
    return 0;
  }
  return 0;
}

AstNode *sym_simplify(AstNode *node) {
  if (!node)
    return NULL;

  switch (node->type) {
  case AST_NUMBER:
  case AST_VARIABLE:
    return node;

  case AST_UNARY_NEG:
    node->as.unary.operand = sym_simplify(node->as.unary.operand);
    if (is_num(node->as.unary.operand)) {
      double v = -node->as.unary.operand->as.number;
      ast_free(node);
      return ast_number(v);
    }
    return node;

  case AST_FUNC_CALL:
    for (size_t i = 0; i < node->as.call.nargs; i++)
      node->as.call.args[i] = sym_simplify(node->as.call.args[i]);
    return node;

  case AST_BINOP:
    break;
  }

  node->as.binop.left = sym_simplify(node->as.binop.left);
  node->as.binop.right = sym_simplify(node->as.binop.right);

  AstNode *L = node->as.binop.left;
  AstNode *R = node->as.binop.right;
  BinOpKind op = node->as.binop.op;

  /* constant folding */
  if (is_num(L) && is_num(R)) {
    double result;
    switch (op) {
    case OP_ADD:
      result = L->as.number + R->as.number;
      break;
    case OP_SUB:
      result = L->as.number - R->as.number;
      break;
    case OP_MUL:
      result = L->as.number * R->as.number;
      break;
    case OP_DIV:
      if (R->as.number == 0.0)
        return node;
      result = L->as.number / R->as.number;
      break;
    case OP_POW:
      result = pow(L->as.number, R->as.number);
      break;
    default:
      return node;
    }
    ast_free(node);
    return ast_number(result);
  }

  switch (op) {
  case OP_ADD:
    /* x + 0 -> x */
    if (is_number(R, 0)) {
      node->as.binop.left = NULL;
      ast_free(node);
      return L;
    }
    /* 0 + x -> x */
    if (is_number(L, 0)) {
      node->as.binop.right = NULL;
      ast_free(node);
      return R;
    }
    break;

  case OP_SUB:
    /* x - 0 -> x */
    if (is_number(R, 0)) {
      node->as.binop.left = NULL;
      ast_free(node);
      return L;
    }
    break;

  case OP_MUL:
    /* x * 0 or 0 * x -> 0 */
    if (is_number(L, 0) || is_number(R, 0)) {
      ast_free(node);
      return ast_number(0);
    }
    /* x * 1 -> x */
    if (is_number(R, 1)) {
      node->as.binop.left = NULL;
      ast_free(node);
      return L;
    }
    /* 1 * x -> x */
    if (is_number(L, 1)) {
      node->as.binop.right = NULL;
      ast_free(node);
      return R;
    }
    break;

  case OP_DIV:
    /* x / 1 -> x */
    if (is_number(R, 1)) {
      node->as.binop.left = NULL;
      ast_free(node);
      return L;
    }
    /* 0 / x -> 0 */
    if (is_number(L, 0)) {
      ast_free(node);
      return ast_number(0);
    }
    break;

  case OP_POW:
    /* x ^ 0 -> 1 */
    if (is_number(R, 0)) {
      ast_free(node);
      return ast_number(1);
    }
    /* x ^ 1 -> x */
    if (is_number(R, 1)) {
      node->as.binop.left = NULL;
      ast_free(node);
      return L;
    }
    /* 0 ^ x -> 0 (for x > 0, good enough for now) */
    if (is_number(L, 0)) {
      ast_free(node);
      return ast_number(0);
    }
    break;
  }

  return node;
}

AstNode *sym_diff(const AstNode *expr, const char *var) {
  if (!expr)
    return ast_number(0);

  switch (expr->type) {
  case AST_NUMBER:
    return ast_number(0);

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
      /* L'R + LR' */
      return sym_simplify(
          ast_binop(OP_ADD, ast_binop(OP_MUL, sym_diff(L, var), ast_clone(R)),
                    ast_binop(OP_MUL, ast_clone(L), sym_diff(R, var))));

    case OP_DIV:
      /* (L'R - LR') / R^2 */
      return sym_simplify(ast_binop(
          OP_DIV,
          ast_binop(OP_SUB, ast_binop(OP_MUL, sym_diff(L, var), ast_clone(R)),
                    ast_binop(OP_MUL, ast_clone(L), sym_diff(R, var))),
          ast_binop(OP_POW, ast_clone(R), ast_number(2))));

    case OP_POW:
      /* x^n where n is constant: n*x^(n-1) * x' */
      if (!contains_var(R, var)) {
        return sym_simplify(ast_binop(
            OP_MUL,
            ast_binop(
                OP_MUL, ast_clone(R),
                ast_binop(OP_POW, ast_clone(L),
                          ast_binop(OP_SUB, ast_clone(R), ast_number(1)))),
            sym_diff(L, var)));
      }
      /* a^x where a is constant: a^x * ln(a) * x' */
      if (!contains_var(L, var)) {
        AstNode *lna_args[1] = {ast_clone(L)};
        return sym_simplify(
            ast_binop(OP_MUL,
                      ast_binop(OP_MUL, ast_clone(expr),
                                ast_func_call("ln", 2, lna_args, 1)),
                      sym_diff(R, var)));
      }
      return NULL;
    }
    break;
  }

  case AST_FUNC_CALL: {
    const char *name = expr->as.call.name;
    if (expr->as.call.nargs != 1)
      return NULL;
    const AstNode *inner = expr->as.call.args[0];
    AstNode *inner_d = sym_diff(inner, var);

    AstNode *outer_d = NULL;
    if (strcmp(name, "sin") == 0) {
      AstNode *cos_args[1] = {ast_clone(inner)};
      outer_d = ast_func_call("cos", 3, cos_args, 1);
    } else if (strcmp(name, "cos") == 0) {
      AstNode *sin_args[1] = {ast_clone(inner)};
      outer_d = ast_unary_neg(ast_func_call("sin", 3, sin_args, 1));
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
    } else if (strcmp(name, "tan") == 0) {
      /* sec^2(u) = 1/cos^2(u) */
      AstNode *cos_args[1] = {ast_clone(inner)};
      outer_d =
          ast_binop(OP_DIV, ast_number(1),
                    ast_binop(OP_POW, ast_func_call("cos", 3, cos_args, 1),
                              ast_number(2)));
    } else {
      ast_free(inner_d);
      return NULL;
    }

    /* f'(g(x)) * g'(x) */
    return sym_simplify(ast_binop(OP_MUL, outer_d, inner_d));
  }
  }

  return NULL;
}

static AstNode *integrate_monomial(const AstNode *expr, const char *var) {
  /* of constant -> c*x */
  if (!contains_var(expr, var)) {
    return ast_binop(OP_MUL, ast_clone(expr), ast_variable(var, strlen(var)));
  }

  /* of x -> x^2/2 */
  if (is_var(expr, var)) {
    return ast_binop(
        OP_DIV,
        ast_binop(OP_POW, ast_variable(var, strlen(var)), ast_number(2)),
        ast_number(2));
  }

  /* of x^n -> x^(n+1)/(n+1) */
  if (expr->type == AST_BINOP && expr->as.binop.op == OP_POW &&
      is_var(expr->as.binop.left, var) && is_num(expr->as.binop.right)) {
    double n = expr->as.binop.right->as.number;
    if (n == -1.0) {
      /* of x^(-1) -> ln(x) */
      AstNode *args[1] = {ast_variable(var, strlen(var))};
      return ast_func_call("ln", 2, args, 1);
    }
    double n1 = n + 1.0;
    return ast_binop(
        OP_DIV,
        ast_binop(OP_POW, ast_variable(var, strlen(var)), ast_number(n1)),
        ast_number(n1));
  }

  /* of c*f(x) -> c * integral(f(x)) */
  if (expr->type == AST_BINOP && expr->as.binop.op == OP_MUL) {
    if (!contains_var(expr->as.binop.left, var)) {
      AstNode *inner = integrate_monomial(expr->as.binop.right, var);
      if (inner)
        return ast_binop(OP_MUL, ast_clone(expr->as.binop.left), inner);
    }
    if (!contains_var(expr->as.binop.right, var)) {
      AstNode *inner = integrate_monomial(expr->as.binop.left, var);
      if (inner)
        return ast_binop(OP_MUL, ast_clone(expr->as.binop.right), inner);
    }
  }

  return NULL;
}

AstNode *sym_integrate(const AstNode *expr, const char *var) {
  if (!expr)
    return NULL;

  /* int(a + b) = int(a) + int(b) */
  if (expr->type == AST_BINOP &&
      (expr->as.binop.op == OP_ADD || expr->as.binop.op == OP_SUB)) {
    AstNode *li = sym_integrate(expr->as.binop.left, var);
    AstNode *ri = sym_integrate(expr->as.binop.right, var);
    if (li && ri)
      return sym_simplify(ast_binop(expr->as.binop.op, li, ri));
    ast_free(li);
    ast_free(ri);
    return NULL;
  }

  /* negation */
  if (expr->type == AST_UNARY_NEG) {
    AstNode *inner = sym_integrate(expr->as.unary.operand, var);
    if (inner)
      return sym_simplify(ast_unary_neg(inner));
    return NULL;
  }

  AstNode *result = integrate_monomial(expr, var);
  if (result)
    return sym_simplify(result);

  return NULL;
}
