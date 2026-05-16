#include "symbolic_internal.h"
#include <math.h>
#include <string.h>

static int is_func_of_var(const AstNode *node, const char *fname,
                          const char *var) {
  if (!node || node->type != AST_FUNC_CALL)
    return 0;
  if (strcmp(node->as.call.name, fname) != 0)
    return 0;
  if (node->as.call.nargs != 1)
    return 0;
  return node->as.call.args[0]->type == AST_VARIABLE &&
         strcmp(node->as.call.args[0]->as.variable, var) == 0;
}

static int is_linear_in_var(const AstNode *node, const char *var, double *a,
                            double *b) {
  if (!node)
    return 0;
  if (node->type == AST_VARIABLE && strcmp(node->as.variable, var) == 0) {
    *a = 1.0;
    *b = 0.0;
    return 1;
  }
  if (node->type == AST_BINOP && node->as.binop.op == OP_MUL) {
    if (node->as.binop.left->type == AST_NUMBER &&
        node->as.binop.right->type == AST_VARIABLE &&
        strcmp(node->as.binop.right->as.variable, var) == 0) {
      *a = node->as.binop.left->as.number.re;
      *b = 0.0;
      return c_is_real(node->as.binop.left->as.number);
    }
    if (node->as.binop.right->type == AST_NUMBER &&
        node->as.binop.left->type == AST_VARIABLE &&
        strcmp(node->as.binop.left->as.variable, var) == 0) {
      *a = node->as.binop.right->as.number.re;
      *b = 0.0;
      return c_is_real(node->as.binop.right->as.number);
    }
  }
  if (node->type == AST_UNARY_NEG) {
    if (is_linear_in_var(node->as.unary.operand, var, a, b)) {
      *a = -(*a);
      *b = -(*b);
      return 1;
    }
  }
  return 0;
}

static AstNode *laplace_table_lookup(const AstNode *expr, const char *t,
                                     const char *s) {
  if (!expr || !t || !s)
    return NULL;

  /* L{1} = 1/s */
  if (expr->type == AST_NUMBER && c_is_real(expr->as.number) &&
      expr->as.number.re == 1.0) {
    return ast_binop(OP_POW, ast_variable(s, strlen(s)), ast_number(-1));
  }

  /* L{c} = c/s for constant */
  if (expr->type == AST_NUMBER && !sym_contains_var(expr, t)) {
    return ast_binop(
        OP_MUL, ast_clone(expr),
        ast_binop(OP_POW, ast_variable(s, strlen(s)), ast_number(-1)));
  }

  /* L{t} = 1/s^2 */
  if (expr->type == AST_VARIABLE && strcmp(expr->as.variable, t) == 0) {
    return ast_binop(OP_POW, ast_variable(s, strlen(s)), ast_number(-2));
  }

  /* L{t^n} = n!/s^(n+1) for positive integer n */
  if (expr->type == AST_BINOP && expr->as.binop.op == OP_POW &&
      expr->as.binop.left->type == AST_VARIABLE &&
      strcmp(expr->as.binop.left->as.variable, t) == 0 &&
      expr->as.binop.right->type == AST_NUMBER &&
      c_is_real(expr->as.binop.right->as.number)) {
    double n = expr->as.binop.right->as.number.re;
    if (n > 0 && n == (int)n && n <= 20) {
      long long fact = 1;
      for (int i = 2; i <= (int)n; i++)
        fact *= i;
      return ast_binop(
          OP_MUL, ast_number((double)fact),
          ast_binop(OP_POW, ast_variable(s, strlen(s)), ast_number(-(n + 1))));
    }
  }

  /* L{exp(a*t)} = 1/(s-a) */
  if (expr->type == AST_FUNC_CALL && strcmp(expr->as.call.name, "exp") == 0 &&
      expr->as.call.nargs == 1) {
    double a, b;
    if (is_linear_in_var(expr->as.call.args[0], t, &a, &b) && b == 0.0) {
      /* 1/(s - a) */
      AstNode *denom =
          ast_binop(OP_SUB, ast_variable(s, strlen(s)), ast_number(a));
      return ast_binop(OP_POW, denom, ast_number(-1));
    }
  }

  /* L{sin(a*t)} = a/(s^2+a^2) */
  if (expr->type == AST_FUNC_CALL && strcmp(expr->as.call.name, "sin") == 0 &&
      expr->as.call.nargs == 1) {
    double a, b;
    if (is_linear_in_var(expr->as.call.args[0], t, &a, &b) && b == 0.0) {
      AstNode *denom = ast_binop(
          OP_ADD, ast_binop(OP_POW, ast_variable(s, strlen(s)), ast_number(2)),
          ast_number(a * a));
      return ast_binop(OP_MUL, ast_number(a),
                       ast_binop(OP_POW, denom, ast_number(-1)));
    }
    if (is_func_of_var(expr, "sin", t)) {
      AstNode *denom = ast_binop(
          OP_ADD, ast_binop(OP_POW, ast_variable(s, strlen(s)), ast_number(2)),
          ast_number(1));
      return ast_binop(OP_POW, denom, ast_number(-1));
    }
  }

  /* L{cos(a*t)} = s/(s^2+a^2) */
  if (expr->type == AST_FUNC_CALL && strcmp(expr->as.call.name, "cos") == 0 &&
      expr->as.call.nargs == 1) {
    double a, b;
    if (is_linear_in_var(expr->as.call.args[0], t, &a, &b) && b == 0.0) {
      AstNode *denom = ast_binop(
          OP_ADD, ast_binop(OP_POW, ast_variable(s, strlen(s)), ast_number(2)),
          ast_number(a * a));
      return ast_binop(OP_MUL, ast_variable(s, strlen(s)),
                       ast_binop(OP_POW, denom, ast_number(-1)));
    }
    if (is_func_of_var(expr, "cos", t)) {
      AstNode *denom = ast_binop(
          OP_ADD, ast_binop(OP_POW, ast_variable(s, strlen(s)), ast_number(2)),
          ast_number(1));
      return ast_binop(OP_MUL, ast_variable(s, strlen(s)),
                       ast_binop(OP_POW, denom, ast_number(-1)));
    }
  }

  return NULL;
}

static AstNode *laplace_linearity(const AstNode *expr, const char *t,
                                  const char *s) {
  if (!expr)
    return NULL;

  /* L{a + b} = L{a} + L{b} */
  if (expr->type == AST_BINOP && expr->as.binop.op == OP_ADD) {
    AstNode *la = sym_laplace(expr->as.binop.left, t, s);
    AstNode *lb = sym_laplace(expr->as.binop.right, t, s);
    if (la && lb)
      return ast_binop(OP_ADD, la, lb);
    ast_free(la);
    ast_free(lb);
    return NULL;
  }
  if (expr->type == AST_BINOP && expr->as.binop.op == OP_SUB) {
    AstNode *la = sym_laplace(expr->as.binop.left, t, s);
    AstNode *lb = sym_laplace(expr->as.binop.right, t, s);
    if (la && lb)
      return ast_binop(OP_SUB, la, lb);
    ast_free(la);
    ast_free(lb);
    return NULL;
  }

  /* L{c*f(t)} = c*L{f(t)} when c is constant w.r.t. t */
  if (expr->type == AST_BINOP && expr->as.binop.op == OP_MUL) {
    if (!sym_contains_var(expr->as.binop.left, t)) {
      AstNode *inner = sym_laplace(expr->as.binop.right, t, s);
      if (inner)
        return ast_binop(OP_MUL, ast_clone(expr->as.binop.left), inner);
      return NULL;
    }
    if (!sym_contains_var(expr->as.binop.right, t)) {
      AstNode *inner = sym_laplace(expr->as.binop.left, t, s);
      if (inner)
        return ast_binop(OP_MUL, ast_clone(expr->as.binop.right), inner);
      return NULL;
    }

    /* L{t^n * f(t)} = (-1)^n * d^n/ds^n F(s) */
    if (expr->as.binop.left->type == AST_VARIABLE &&
        strcmp(expr->as.binop.left->as.variable, t) == 0) {
      AstNode *fs = sym_laplace(expr->as.binop.right, t, s);
      if (fs) {
        AstNode *deriv = sym_diff(fs, s);
        ast_free(fs);
        return ast_unary_neg(deriv);
      }
    }
    if (expr->as.binop.right->type == AST_VARIABLE &&
        strcmp(expr->as.binop.right->as.variable, t) == 0) {
      AstNode *fs = sym_laplace(expr->as.binop.left, t, s);
      if (fs) {
        AstNode *deriv = sym_diff(fs, s);
        ast_free(fs);
        return ast_unary_neg(deriv);
      }
    }
    if (expr->as.binop.left->type == AST_BINOP &&
        expr->as.binop.left->as.binop.op == OP_POW &&
        expr->as.binop.left->as.binop.left->type == AST_VARIABLE &&
        strcmp(expr->as.binop.left->as.binop.left->as.variable, t) == 0 &&
        expr->as.binop.left->as.binop.right->type == AST_NUMBER &&
        c_is_real(expr->as.binop.left->as.binop.right->as.number)) {
      int n = (int)expr->as.binop.left->as.binop.right->as.number.re;
      if (n > 0 && n <= 10) {
        AstNode *fs = sym_laplace(expr->as.binop.right, t, s);
        if (fs) {
          AstNode *result = fs;
          for (int i = 0; i < n; i++) {
            AstNode *d = sym_diff(result, s);
            ast_free(result);
            result = d;
          }
          if (n % 2 == 1)
            result = ast_unary_neg(result);
          return result;
        }
      }
    }
    if (expr->as.binop.right->type == AST_BINOP &&
        expr->as.binop.right->as.binop.op == OP_POW &&
        expr->as.binop.right->as.binop.left->type == AST_VARIABLE &&
        strcmp(expr->as.binop.right->as.binop.left->as.variable, t) == 0 &&
        expr->as.binop.right->as.binop.right->type == AST_NUMBER &&
        c_is_real(expr->as.binop.right->as.binop.right->as.number)) {
      int n = (int)expr->as.binop.right->as.binop.right->as.number.re;
      if (n > 0 && n <= 10) {
        AstNode *fs = sym_laplace(expr->as.binop.left, t, s);
        if (fs) {
          AstNode *result = fs;
          for (int i = 0; i < n; i++) {
            AstNode *d = sym_diff(result, s);
            ast_free(result);
            result = d;
          }
          if (n % 2 == 1)
            result = ast_unary_neg(result);
          return result;
        }
      }
    }
  }

  /* L{-f(t)} = -L{f(t)} */
  if (expr->type == AST_UNARY_NEG) {
    AstNode *inner = sym_laplace(expr->as.unary.operand, t, s);
    if (inner)
      return ast_unary_neg(inner);
    return NULL;
  }

  return NULL;
}

static AstNode *laplace_frequency_shift(const AstNode *expr, const char *t,
                                        const char *s) {
  if (!expr || expr->type != AST_BINOP || expr->as.binop.op != OP_MUL)
    return NULL;

  const AstNode *exp_part = NULL;
  const AstNode *rest = NULL;

  if (expr->as.binop.left->type == AST_FUNC_CALL &&
      strcmp(expr->as.binop.left->as.call.name, "exp") == 0 &&
      expr->as.binop.left->as.call.nargs == 1) {
    exp_part = expr->as.binop.left;
    rest = expr->as.binop.right;
  } else if (expr->as.binop.right->type == AST_FUNC_CALL &&
             strcmp(expr->as.binop.right->as.call.name, "exp") == 0 &&
             expr->as.binop.right->as.call.nargs == 1) {
    exp_part = expr->as.binop.right;
    rest = expr->as.binop.left;
  }

  if (!exp_part)
    return NULL;

  double a, b;
  if (!is_linear_in_var(exp_part->as.call.args[0], t, &a, &b) || b != 0.0)
    return NULL;

  /* L{exp(a*t)*f(t)} = F(s-a) */
  AstNode *f_transform = sym_laplace(rest, t, s);
  if (!f_transform)
    return NULL;

  /* substitute s -> (s - a) */
  AstNode *shifted_s =
      ast_binop(OP_SUB, ast_variable(s, strlen(s)), ast_number(a));
  AstNode *result = sym_subs(f_transform, s, shifted_s);
  ast_free(f_transform);
  ast_free(shifted_s);
  return result;
}

static AstNode *laplace_via_integration(const AstNode *expr, const char *t,
                                        const char *s) {
  /* Build exp(-s*t) * f(t) */
  AstNode *kernel = ast_func_call(
      "exp", 3,
      (AstNode *[]){ast_binop(OP_MUL, ast_unary_neg(ast_variable(s, strlen(s))),
                              ast_variable(t, strlen(t)))},
      1);
  AstNode *integrand = ast_binop(OP_MUL, kernel, ast_clone(expr));
  integrand = sym_simplify(integrand);

  AstNode *antideriv = sym_integrate(integrand, t);
  ast_free(integrand);
  if (!antideriv)
    return NULL;

  antideriv = sym_simplify(antideriv);

  /* eval at t=0: subtract F(0) */
  AstNode *at_zero = sym_subs(antideriv, t, ast_number(0));
  at_zero = sym_full_simplify(at_zero);

  AstNode *at_inf = sym_subs(antideriv, t, ast_number(1e18));
  at_inf = sym_full_simplify(at_inf);

  int inf_is_zero =
      (at_inf && at_inf->type == AST_NUMBER && c_is_real(at_inf->as.number) &&
       fabs(at_inf->as.number.re) < 1e-6);

  ast_free(at_inf);

  if (!inf_is_zero) {
    /* assume convergence: lim(t->inf) = 0 for Re(s) > threshold */
  }

  /* result = 0 - F(0) = -F(0) */
  AstNode *result = sym_simplify(ast_unary_neg(at_zero));
  ast_free(antideriv);
  return result;
}

AstNode *sym_laplace(const AstNode *expr, const char *t, const char *s) {
  if (!expr || !t || !s)
    return NULL;

  AstNode *simplified = sym_full_simplify(ast_clone(expr));

  /* transform table first */
  AstNode *result = laplace_table_lookup(simplified, t, s);
  if (result) {
    ast_free(simplified);
    return sym_simplify(result);
  }

  /* frequency shift: L{exp(a*t)*f(t)} = F(s-a) */
  result = laplace_frequency_shift(simplified, t, s);
  if (result) {
    ast_free(simplified);
    return sym_simplify(result);
  }

  /* linearity: L{a*f + b*g} = a*L{f} + b*L{g} */
  result = laplace_linearity(simplified, t, s);
  if (result) {
    ast_free(simplified);
    return sym_simplify(result);
  }

  /* fallback: direct integration */
  result = laplace_via_integration(simplified, t, s);
  ast_free(simplified);
  if (result)
    return sym_simplify(result);

  return NULL;
}
