#include "solve.h"
#include "canonical.h"
#include "eval.h"
#include "symbolic.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SolveResult ok_roots(double *roots, size_t count) {
  SolveResult r;
  r.roots = malloc(count * sizeof(double));
  memcpy(r.roots, roots, count * sizeof(double));
  r.count = count;
  r.ok = 1;
  r.error = NULL;
  return r;
}

static SolveResult fail(const char *msg) {
  SolveResult r;
  r.roots = NULL;
  r.count = 0;
  r.ok = 0;
  r.error = strdup(msg);
  return r;
}

void solve_result_free(SolveResult *r) {
  free(r->roots);
  r->roots = NULL;
  free(r->error);
  r->error = NULL;
  r->count = 0;
  r->ok = 0;
}

static double eval_at(const AstNode *expr, const char *var, double val,
                      const SymTab *st) {
  SymTab local;
  memcpy(&local, st, sizeof(SymTab));

  SymEntry entry;
  entry.name = (char *)var;
  entry.value = val;
  entry.expr = NULL;
  entry.params = NULL;
  entry.num_params = 0;

  unsigned hash = 5381;
  for (const char *p = var; *p; p++)
    hash = hash * 33 + (unsigned char)*p;
  hash %= SYMTAB_BUCKETS;

  entry.next = local.buckets[hash];
  local.buckets[hash] = &entry;

  EvalResult er = eval(expr, &local);
  local.buckets[hash] = entry.next;

  if (!er.ok) {
    eval_result_free(&er);
    return NAN;
  }
  double v = er.value;
  eval_result_free(&er);
  return v;
}

/*
 * Extract polynomial coefficients from a flattened/simplified expression.
 * Handles forms like: c + c*x + c*x^2 (after expand + canonicalize).
 * Returns degree, or -1 on failure. Fills coeffs[0..degree].
 */
static int extract_poly_coeffs(const AstNode *expr, const char *var,
                               double *coeffs, int max_degree) {
  for (int i = 0; i <= max_degree; i++)
    coeffs[i] = 0;

  AstNode *expanded = sym_expand(ast_clone(expr));
  expanded = ast_canonicalize(expanded);
  expanded = sym_collect_terms(expanded);
  expanded = sym_simplify(expanded);

  /* flatten sum into terms */
  const AstNode *stack[128];
  int sp = 0;
  stack[sp++] = expanded;

  while (sp > 0) {
    const AstNode *n = stack[--sp];
    if (!n)
      continue;

    if (n->type == AST_BINOP && n->as.binop.op == OP_ADD) {
      if (sp + 2 > 128) {
        ast_free(expanded);
        return -1;
      }
      stack[sp++] = n->as.binop.left;
      stack[sp++] = n->as.binop.right;
      continue;
    }

    double coeff = 1.0;
    int degree = 0;
    const AstNode *term = n;

    /* handle unary neg */
    if (term->type == AST_UNARY_NEG) {
      coeff = -1.0;
      term = term->as.unary.operand;
    }

    if (term->type == AST_NUMBER) {
      coeffs[0] += coeff * term->as.number;
      continue;
    }

    if (term->type == AST_VARIABLE && strcmp(term->as.variable, var) == 0) {
      if (1 > max_degree) {
        ast_free(expanded);
        return -1;
      }
      coeffs[1] += coeff;
      continue;
    }

    /* c * expr */
    if (term->type == AST_BINOP && term->as.binop.op == OP_MUL) {
      const AstNode *left = term->as.binop.left;
      const AstNode *right = term->as.binop.right;

      if (left->type == AST_NUMBER) {
        coeff *= left->as.number;
        term = right;
      } else if (right->type == AST_NUMBER) {
        coeff *= right->as.number;
        term = left;
      }
    }

    /* bare variable after coefficient extraction */
    if (term->type == AST_VARIABLE && strcmp(term->as.variable, var) == 0) {
      if (1 > max_degree) {
        ast_free(expanded);
        return -1;
      }
      coeffs[1] += coeff;
      continue;
    }

    /* x^n */
    if (term->type == AST_BINOP && term->as.binop.op == OP_POW &&
        term->as.binop.left->type == AST_VARIABLE &&
        strcmp(term->as.binop.left->as.variable, var) == 0 &&
        term->as.binop.right->type == AST_NUMBER) {
      degree = (int)term->as.binop.right->as.number;
      if (degree < 0 || degree > max_degree ||
          degree != (int)term->as.binop.right->as.number) {
        ast_free(expanded);
        return -1;
      }
      coeffs[degree] += coeff;
      continue;
    }

    /* term contains the variable but in unsupported form */
    if (sym_contains_var(term, var)) {
      ast_free(expanded);
      return -1;
    }

    /* constant term (variable/expression not containing var) */
    SymTab empty;
    memset(&empty, 0, sizeof(empty));
    coeffs[0] += coeff * eval_at(term, var, 0, &empty);
    if (isnan(coeffs[0])) {
      ast_free(expanded);
      return -1;
    }
  }

  ast_free(expanded);

  int deg = 0;
  for (int i = max_degree; i >= 0; i--) {
    if (coeffs[i] != 0) {
      deg = i;
      break;
    }
  }
  return deg;
}

static SolveResult solve_linear(double a, double b) {
  if (a == 0) {
    if (b == 0)
      return fail("infinitely many solutions");
    return fail("no solution (contradiction)");
  }
  double root = -b / a;
  return ok_roots(&root, 1);
}

static SolveResult solve_quadratic(double a, double b, double c) {
  double disc = b * b - 4 * a * c;
  if (disc < -1e-12)
    return fail("no real roots (discriminant < 0)");

  if (fabs(disc) < 1e-12) {
    double root = -b / (2 * a);
    return ok_roots(&root, 1);
  }

  double sq = sqrt(disc);
  double roots[2];
  roots[0] = (-b - sq) / (2 * a);
  roots[1] = (-b + sq) / (2 * a);
  return ok_roots(roots, 2);
}

static SolveResult solve_newton(const AstNode *f, const AstNode *df,
                                const char *var, double x0, const SymTab *st) {
  double x = x0;
  for (int i = 0; i < 200; i++) {
    double fv = eval_at(f, var, x, st);
    if (isnan(fv))
      return fail("evaluation error during Newton iteration");

    if (fabs(fv) < 1e-12) {
      return ok_roots(&x, 1);
    }

    double dfv = eval_at(df, var, x, st);
    if (isnan(dfv) || fabs(dfv) < 1e-15)
      return fail("derivative zero or undefined during Newton iteration");

    x = x - fv / dfv;

    if (isinf(x))
      return fail("Newton iteration diverged");
  }

  double fv = eval_at(f, var, x, st);
  if (fabs(fv) < 1e-8) {
    return ok_roots(&x, 1);
  }

  return fail("Newton method did not converge (try different initial guess)");
}

SolveResult sym_solve(const AstNode *expr, const char *var, double x0,
                      const SymTab *st) {
  if (!expr || !var)
    return fail("null expression or variable");

  /* try polynomial extraction up to degree 2 */
  double coeffs[3];
  int deg = extract_poly_coeffs(expr, var, coeffs, 2);

  if (deg == 0) {
    if (coeffs[0] == 0)
      return fail("expression is identically zero");
    return fail("no solution (constant non-zero expression)");
  }
  if (deg == 1)
    return solve_linear(coeffs[1], coeffs[0]);
  if (deg == 2)
    return solve_quadratic(coeffs[2], coeffs[1], coeffs[0]);

  /* fallback: Newton-Raphson */
  AstNode *df = sym_diff(expr, var);
  if (!df)
    return fail("cannot differentiate expression for Newton method");
  df = sym_simplify(df);

  SolveResult r = solve_newton(expr, df, var, x0, st);
  ast_free(df);
  return r;
}
