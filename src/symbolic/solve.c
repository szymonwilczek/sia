#include "sia/solve.h"
#include "sia/canonical.h"
#include "sia/eval.h"
#include "sia/logarithm.h"
#include "sia/symbolic.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SolveResult ok_roots(Complex *roots, size_t count) {
  SolveResult r;
  r.roots = malloc(count * sizeof(Complex));
  memcpy(r.roots, roots, count * sizeof(Complex));
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

static Complex eval_at(const AstNode *expr, const char *var, Complex val,
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
    return c_make(NAN, NAN);
  }
  Complex v = er.value;
  eval_result_free(&er);
  return v;
}

/*
 * Extract polynomial coefficients from a flattened/simplified expression.
 * Handles forms like: c + c*x + c*x^2 (after expand + canonicalize).
 * Returns degree, or -1 on failure. Fills coeffs[0..degree].
 */
static int extract_poly_coeffs(const AstNode *expr, const char *var,
                               Complex *coeffs, int max_degree) {
  for (int i = 0; i <= max_degree; i++)
    coeffs[i] = c_real(0.0);

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

    Complex coeff = c_real(1.0);
    int degree = 0;
    const AstNode *term = n;

    /* handle unary neg */
    if (term->type == AST_UNARY_NEG) {
      coeff = c_real(-1.0);
      term = term->as.unary.operand;
    }

    if (term->type == AST_NUMBER) {
      coeffs[0] = c_add(coeffs[0], c_mul(coeff, term->as.number));
      continue;
    }

    if (term->type == AST_VARIABLE && strcmp(term->as.variable, var) == 0) {
      if (1 > max_degree) {
        ast_free(expanded);
        return -1;
      }
      coeffs[1] = c_add(coeffs[1], coeff);
      continue;
    }

    /* c * expr */
    if (term->type == AST_BINOP && term->as.binop.op == OP_MUL) {
      const AstNode *left = term->as.binop.left;
      const AstNode *right = term->as.binop.right;

      if (left->type == AST_NUMBER) {
        coeff = c_mul(coeff, left->as.number);
        term = right;
      } else if (right->type == AST_NUMBER) {
        coeff = c_mul(coeff, right->as.number);
        term = left;
      }
    }

    /* bare variable after coefficient extraction */
    if (term->type == AST_VARIABLE && strcmp(term->as.variable, var) == 0) {
      if (1 > max_degree) {
        ast_free(expanded);
        return -1;
      }
      coeffs[1] = c_add(coeffs[1], coeff);
      continue;
    }

    /* x^n */
    if (term->type == AST_BINOP && term->as.binop.op == OP_POW &&
        term->as.binop.left->type == AST_VARIABLE &&
        strcmp(term->as.binop.left->as.variable, var) == 0 &&
        term->as.binop.right->type == AST_NUMBER &&
        c_is_real(term->as.binop.right->as.number)) {
      degree = (int)term->as.binop.right->as.number.re;
      if (degree < 0 || degree > max_degree ||
          (double)degree != term->as.binop.right->as.number.re) {
        ast_free(expanded);
        return -1;
      }
      coeffs[degree] = c_add(coeffs[degree], coeff);
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
    Complex val = eval_at(term, var, c_real(0), &empty);
    coeffs[0] = c_add(coeffs[0], c_mul(coeff, val));
    if (isnan(coeffs[0].re)) {
      ast_free(expanded);
      return -1;
    }
  }

  ast_free(expanded);

  int deg = 0;
  for (int i = max_degree; i >= 0; i--) {
    if (!c_is_zero(coeffs[i])) {
      deg = i;
      break;
    }
  }
  return deg;
}

static SolveResult solve_linear(Complex a, Complex b) {
  if (c_is_zero(a)) {
    if (c_is_zero(b))
      return fail("infinitely many solutions");
    return fail("no solution (contradiction)");
  }
  Complex root = c_div(c_neg(b), a);
  return ok_roots(&root, 1);
}

static SolveResult solve_quadratic(Complex a, Complex b, Complex c) {
  /* roots = (-b +/- sqrt(b^2 - 4ac)) / 2a */
  Complex b2 = c_mul(b, b);
  Complex four_ac = c_mul(c_real(4.0), c_mul(a, c));
  Complex disc = c_sub(b2, four_ac);
  Complex sq = c_sqrt(disc);

  Complex two_a = c_mul(c_real(2.0), a);
  Complex roots[2];
  roots[0] = c_div(c_sub(c_neg(b), sq), two_a);
  roots[1] = c_div(c_add(c_neg(b), sq), two_a);

  return ok_roots(roots, 2);
}

static SolveResult solve_newton(const AstNode *f, const AstNode *df,
                                const char *var, Complex x0, const SymTab *st) {
  Complex x = x0;
  for (int i = 0; i < 200; i++) {
    Complex fv = eval_at(f, var, x, st);
    if (isnan(fv.re))
      return fail("evaluation error during Newton iteration");

    if (c_abs(fv) < 1e-12) {
      return ok_roots(&x, 1);
    }

    Complex dfv = eval_at(df, var, x, st);
    if (isnan(dfv.re) || c_abs(dfv) < 1e-15)
      return fail("derivative zero or undefined during Newton iteration");

    x = c_sub(x, c_div(fv, dfv));

    if (isinf(x.re) || isinf(x.im))
      return fail("Newton iteration diverged");
  }

  Complex fv = eval_at(f, var, x, st);
  if (c_abs(fv) < 1e-8) {
    return ok_roots(&x, 1);
  }

  return fail("Newton method did not converge (try different initial guess)");
}

SolveResult sym_solve(const AstNode *expr, const char *var, Complex x0,
                      const SymTab *st) {
  SolveResult result;
  AstNode *simplified = NULL;

  if (!expr || !var)
    return fail("null expression or variable");

  simplified = sym_simplify(ast_clone(expr));
  if (!simplified)
    return fail("failed to simplify expression before solving");

  if (simplified->type == AST_BINOP && simplified->as.binop.op == OP_SUB) {
    const AstNode *lhs = simplified->as.binop.left;
    const AstNode *rhs = simplified->as.binop.right;

    if (log_kind(lhs) != LOG_KIND_NONE && !sym_contains_var(rhs, var)) {
      AstNode *root_expr = log_solve_call(lhs, rhs, var);
      if (root_expr) {
        root_expr = sym_simplify(root_expr);
        EvalResult er = eval(root_expr, st);
        ast_free(root_expr);
        if (!er.ok) {
          ast_free(simplified);
          return fail(er.error ? er.error : "evaluation error during solve");
        }
        SolveResult r = ok_roots(&er.value, 1);
        eval_result_free(&er);
        ast_free(simplified);
        return r;
      }
    }

    if (log_kind(rhs) != LOG_KIND_NONE && !sym_contains_var(lhs, var)) {
      AstNode *root_expr = log_solve_call(rhs, lhs, var);
      if (root_expr) {
        root_expr = sym_simplify(root_expr);
        EvalResult er = eval(root_expr, st);
        ast_free(root_expr);
        if (!er.ok) {
          ast_free(simplified);
          return fail(er.error ? er.error : "evaluation error during solve");
        }
        SolveResult r = ok_roots(&er.value, 1);
        eval_result_free(&er);
        ast_free(simplified);
        return r;
      }
    }
  }

  if (log_kind(simplified) != LOG_KIND_NONE &&
      !sym_contains_var(simplified, var)) {
    AstNode *zero = ast_number(0);
    AstNode *root_expr = log_solve_call(simplified, zero, var);
    ast_free(zero);
    if (root_expr) {
      root_expr = sym_simplify(root_expr);
      EvalResult er = eval(root_expr, st);
      ast_free(root_expr);
      if (!er.ok) {
        ast_free(simplified);
        return fail(er.error ? er.error : "evaluation error during solve");
      }
      SolveResult r = ok_roots(&er.value, 1);
      eval_result_free(&er);
      ast_free(simplified);
      return r;
    }
  }

  /* try polynomial extraction up to degree 2 */
  Complex coeffs[3];
  int deg = extract_poly_coeffs(simplified, var, coeffs, 2);

  if (deg == 0) {
    ast_free(simplified);
    if (c_is_zero(coeffs[0]))
      return fail("expression is identically zero");
    return fail("no solution (constant non-zero expression)");
  }
  if (deg == 1) {
    result = solve_linear(coeffs[1], coeffs[0]);
    ast_free(simplified);
    return result;
  }
  if (deg == 2) {
    result = solve_quadratic(coeffs[2], coeffs[1], coeffs[0]);
    ast_free(simplified);
    return result;
  }

  /* fallback: Newton-Raphson */
  AstNode *df = sym_diff(simplified, var);
  if (!df) {
    ast_free(simplified);
    return fail("cannot differentiate expression for Newton method");
  }
  df = sym_simplify(df);

  result = solve_newton(simplified, df, var, x0, st);
  ast_free(df);
  ast_free(simplified);
  return result;
}
