#include "sia/solve.h"
#include "sia/canonical.h"
#include "sia/eval.h"
#include "sia/logarithm.h"
#include "sia/symbolic.h"
#include "symbolic_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SolveResult ok_roots(Complex *roots, size_t count) {
  SolveResult r;
  r.roots = count ? malloc(count * sizeof(Complex)) : NULL;
  if (count)
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

typedef struct {
  AstNode **items;
  size_t count;
  size_t cap;
} DenomList;

typedef struct {
  Complex *items;
  size_t count;
  size_t cap;
} ComplexList;

static void denom_list_free(DenomList *l) {
  for (size_t i = 0; i < l->count; i++)
    ast_free(l->items[i]);
  free(l->items);
  l->items = NULL;
  l->count = l->cap = 0;
}

static void denom_list_push(DenomList *l, AstNode *node) {
  if (l->count == l->cap) {
    l->cap = l->cap ? l->cap * 2 : 4;
    l->items = realloc(l->items, l->cap * sizeof(AstNode *));
  }
  l->items[l->count++] = node;
}

static void cx_list_push(ComplexList *l, Complex v) {
  if (l->count == l->cap) {
    l->cap = l->cap ? l->cap * 2 : 4;
    l->items = realloc(l->items, l->cap * sizeof(Complex));
  }
  l->items[l->count++] = v;
}

static int is_negative_integer(const AstNode *n) {
  if (!n)
    return 0;
  if (n->type == AST_UNARY_NEG && n->as.unary.operand &&
      n->as.unary.operand->type == AST_NUMBER &&
      c_is_real(n->as.unary.operand->as.number)) {
    double v = n->as.unary.operand->as.number.re;
    return v == (double)(long long)v && v > 0;
  }
  if (n->type == AST_NUMBER && c_is_real(n->as.number)) {
    double v = n->as.number.re;
    return v < 0 && v == (double)(long long)v;
  }
  return 0;
}

static void collect_denominators(const AstNode *n, const char *var,
                                 DenomList *out) {
  if (!n)
    return;
  switch (n->type) {
  case AST_BINOP:
    if (n->as.binop.op == OP_DIV && sym_contains_var(n->as.binop.right, var))
      denom_list_push(out, ast_clone(n->as.binop.right));
    if (n->as.binop.op == OP_POW && is_negative_integer(n->as.binop.right) &&
        sym_contains_var(n->as.binop.left, var))
      denom_list_push(out, ast_clone(n->as.binop.left));
    collect_denominators(n->as.binop.left, var, out);
    collect_denominators(n->as.binop.right, var, out);
    break;
  case AST_UNARY_NEG:
    collect_denominators(n->as.unary.operand, var, out);
    break;
  case AST_FUNC_CALL:
    for (size_t i = 0; i < n->as.call.nargs; i++)
      collect_denominators(n->as.call.args[i], var, out);
    break;
  case AST_EQ:
    collect_denominators(n->as.eq.lhs, var, out);
    collect_denominators(n->as.eq.rhs, var, out);
    break;
  default:
    break;
  }
}

static void collect_forbidden(const AstNode *expr, const char *var,
                              ComplexList *out) {
  DenomList denoms = {0};
  collect_denominators(expr, var, &denoms);

  for (size_t i = 0; i < denoms.count; i++) {
    AstNode *d = sym_simplify(ast_clone(denoms.items[i]));
    Complex coeffs[3];
    int deg = extract_poly_coeffs(d, var, coeffs, 2);
    ast_free(d);
    if (deg <= 0)
      continue;
    if (deg == 1) {
      if (c_is_zero(coeffs[1]))
        continue;
      Complex root = c_div(c_neg(coeffs[0]), coeffs[1]);
      cx_list_push(out, root);
    } else if (deg == 2) {
      Complex b2 = c_mul(coeffs[1], coeffs[1]);
      Complex four_ac = c_mul(c_real(4.0), c_mul(coeffs[2], coeffs[0]));
      Complex disc = c_sub(b2, four_ac);
      Complex sq = c_sqrt(disc);
      Complex two_a = c_mul(c_real(2.0), coeffs[2]);
      cx_list_push(out, c_div(c_sub(c_neg(coeffs[1]), sq), two_a));
      cx_list_push(out, c_div(c_add(c_neg(coeffs[1]), sq), two_a));
    }
  }

  denom_list_free(&denoms);
}

static int complex_close(Complex a, Complex b) {
  return c_abs(c_sub(a, b)) < 1e-9;
}

static SolveResult filter_extraneous(SolveResult sr,
                                     const ComplexList *forbidden) {
  if (!sr.ok || sr.count == 0 || forbidden->count == 0)
    return sr;

  Complex *kept = malloc(sr.count * sizeof(Complex));
  size_t keep = 0;
  for (size_t i = 0; i < sr.count; i++) {
    int bad = 0;
    for (size_t j = 0; j < forbidden->count; j++) {
      if (complex_close(sr.roots[i], forbidden->items[j])) {
        bad = 1;
        break;
      }
    }
    if (!bad)
      kept[keep++] = sr.roots[i];
  }

  free(sr.roots);
  sr.roots = keep ? kept : NULL;
  if (!keep)
    free(kept);
  sr.count = keep;
  return sr;
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

static SolveResult sym_solve_core(const AstNode *expr, const char *var,
                                  Complex x0, const SymTab *st);

SolveResult sym_solve(const AstNode *expr, const char *var, Complex x0,
                      const SymTab *st) {
  SolveResult result = sym_solve_core(expr, var, x0, st);

  if (result.ok && result.count > 0) {
    ComplexList forbidden = {0};
    collect_forbidden(expr, var, &forbidden);
    result = filter_extraneous(result, &forbidden);
    free(forbidden.items);
  }

  return result;
}

static SolveResult sym_solve_core(const AstNode *expr, const char *var,
                                  Complex x0, const SymTab *st) {
  SolveResult result;
  AstNode *simplified = NULL;
  AstNode *normalized = NULL;

  if (!expr || !var)
    return fail("null expression or variable");

  /* AST_EQ(lhs, rhs)  =>  lhs - rhs (set to zero implicitly).
   * Reduce rational subexpressions on each side first so (x^2-1)/(x-1)
   * collapses to 1+x before structure-mangling canonicalization. */
  if (expr->type == AST_EQ) {
    AstNode *l = sym_reduce_rational_subexprs(expr->as.eq.lhs);
    AstNode *r = sym_reduce_rational_subexprs(expr->as.eq.rhs);
    normalized = ast_binop(OP_SUB, l, r);
  } else {
    normalized = sym_reduce_rational_subexprs(expr);
  }

  simplified = sym_simplify(normalized);
  if (!simplified)
    return fail("failed to simplify expression before solving");

  /* if the simplified form still has var-dependent denominators, multiply
   * through with cancellation so the rational equation becomes a
   * polynomial. Extraneous roots are stripped later by filter_extraneous. */
  {
    DenomList denoms = {0};
    collect_denominators(simplified, var, &denoms);
    if (denoms.count > 0) {
      /* multiply through by the product of all collected denominators,
       * then expand and simplify so paired POW(d, -1)*d cancel. */
      AstNode *product = ast_clone(denoms.items[0]);
      for (size_t i = 1; i < denoms.count; i++)
        product = ast_binop(OP_MUL, product, ast_clone(denoms.items[i]));
      simplified = ast_binop(OP_MUL, simplified, product);

      AstNode *expanded = sym_expand(simplified);
      if (expanded) {
        ast_free(simplified);
        simplified = expanded;
      }
      simplified = sym_full_simplify(simplified);
    }
    denom_list_free(&denoms);
  }

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
