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
  SolveResult r = {0};
  r.roots = count ? malloc(count * sizeof(Complex)) : NULL;
  if (count)
    memcpy(r.roots, roots, count * sizeof(Complex));
  r.count = count;
  r.ok = 1;
  return r;
}

static SolveResult ok_symbolic_single(AstNode *expr) {
  SolveResult r = {0};
  r.symbolic_roots = malloc(sizeof(AstNode *));
  r.symbolic_roots[0] = expr;
  r.symbolic_count = 1;
  r.ok = 1;
  return r;
}

static SolveResult fail(const char *msg) {
  SolveResult r = {0};
  r.error = strdup(msg);
  return r;
}

void solve_result_free(SolveResult *r) {
  free(r->roots);
  r->roots = NULL;
  if (r->symbolic_roots) {
    for (size_t i = 0; i < r->symbolic_count; i++)
      ast_free(r->symbolic_roots[i]);
    free(r->symbolic_roots);
    r->symbolic_roots = NULL;
  }
  r->symbolic_count = 0;
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

static int has_foreign_symbol(const AstNode *n, const char *var) {
  if (!n)
    return 0;
  switch (n->type) {
  case AST_NUMBER:
    return 0;
  case AST_VARIABLE: {
    const char *s = n->as.variable;
    if (strcmp(s, var) == 0)
      return 0;
    if (strcmp(s, "pi") == 0 || strcmp(s, "e") == 0 || strcmp(s, "i") == 0)
      return 0;
    return 1;
  }
  case AST_BINOP:
    return has_foreign_symbol(n->as.binop.left, var) ||
           has_foreign_symbol(n->as.binop.right, var);
  case AST_UNARY_NEG:
    return has_foreign_symbol(n->as.unary.operand, var);
  case AST_FUNC_CALL:
    for (size_t i = 0; i < n->as.call.nargs; i++)
      if (has_foreign_symbol(n->as.call.args[i], var))
        return 1;
    return 0;
  case AST_EQ:
    return has_foreign_symbol(n->as.eq.lhs, var) ||
           has_foreign_symbol(n->as.eq.rhs, var);
  default:
    return 0;
  }
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
   * Cross-multiply any top-level DIV on either side first to clear the
   * outermost denominator without relying on the simplifier to cancel
   * factors across a sum (e.g. 1/(1+1/x) = 2 becomes 1 = 2*(1+1/x)).
   * Then reduce rational subexpressions so (x^2-1)/(x-1) collapses to
   * 1+x before structure-mangling canonicalization. */
  if (expr->type == AST_EQ) {
    AstNode *l = ast_clone(expr->as.eq.lhs);
    AstNode *r = ast_clone(expr->as.eq.rhs);
    while (l && l->type == AST_BINOP && l->as.binop.op == OP_DIV) {
      AstNode *num = l->as.binop.left;
      AstNode *den = l->as.binop.right;
      l->as.binop.left = NULL;
      l->as.binop.right = NULL;
      ast_free(l);
      l = num;
      r = ast_binop(OP_MUL, r, den);
    }
    while (r && r->type == AST_BINOP && r->as.binop.op == OP_DIV) {
      AstNode *num = r->as.binop.left;
      AstNode *den = r->as.binop.right;
      r->as.binop.left = NULL;
      r->as.binop.right = NULL;
      ast_free(r);
      r = num;
      l = ast_binop(OP_MUL, l, den);
    }
    AstNode *lr = sym_reduce_rational_subexprs(l);
    AstNode *rr = sym_reduce_rational_subexprs(r);
    ast_free(l);
    ast_free(r);
    normalized = ast_binop(OP_SUB, lr, rr);
  } else {
    normalized = sym_reduce_rational_subexprs(expr);
  }

  simplified = sym_simplify(normalized);
  if (!simplified)
    return fail("failed to simplify expression before solving");
  /* log_b(f(x)) = c  =>  f(x) = b^c  (recursively solve the inner form so
   * f(x) need not be the bare variable). Detected here, BEFORE the expand
   * pipeline rewrites SUB(log,...) into a sum form that the log_kind
   * pattern can no longer match. */
  {
    const AstNode *log_node = NULL;
    const AstNode *other = NULL;
    if (simplified->type == AST_BINOP && simplified->as.binop.op == OP_SUB) {
      const AstNode *lhs = simplified->as.binop.left;
      const AstNode *rhs = simplified->as.binop.right;
      if (log_kind(lhs) != LOG_KIND_NONE && !sym_contains_var(rhs, var)) {
        log_node = lhs;
        other = rhs;
      } else if (log_kind(rhs) != LOG_KIND_NONE &&
                 !sym_contains_var(lhs, var)) {
        log_node = rhs;
        other = lhs;
      }
    } else if (log_kind(simplified) != LOG_KIND_NONE) {
      log_node = simplified;
      other = NULL;
    }

    if (log_node) {
      LogKind k = log_kind(log_node);
      AstNode *value = ast_clone(log_node->as.call.args[0]);
      AstNode *base = NULL;
      int base_has_var = 0;
      switch (k) {
      case LOG_KIND_LN:
        base = NULL;
        break;
      case LOG_KIND_BASE10:
        base = ast_number(10);
        break;
      case LOG_KIND_BASE2:
        base = ast_number(2);
        break;
      case LOG_KIND_GENERIC:
        base = ast_clone(log_node->as.call.args[1]);
        base_has_var = sym_contains_var(base, var);
        break;
      default:
        ast_free(value);
        value = NULL;
        break;
      }

      if (value && !base_has_var) {
        /* log_b(f(x)) = c  =>  f(x) = b^c  */
        AstNode *other_clone = other ? ast_clone(other) : ast_number(0);
        AstNode *rhs_pow;
        if (!base) {
          rhs_pow = ast_func_call("exp", 3, (AstNode *[]){other_clone}, 1);
        } else {
          rhs_pow = ast_binop(OP_POW, base, other_clone);
        }
        AstNode *new_expr = ast_binop(OP_SUB, value, rhs_pow);
        ast_free(simplified);
        SolveResult sub = sym_solve_core(new_expr, var, x0, st);
        ast_free(new_expr);
        return sub;
      }
      if (value && base && base_has_var && !sym_contains_var(value, var)) {
        /* log_x(value) = c  =>  x^c = value  =>  x = value^(1/c)  */
        AstNode *other_clone = other ? ast_clone(other) : ast_number(0);
        AstNode *inv = ast_binop(OP_DIV, ast_number(1), other_clone);
        AstNode *new_value = ast_binop(OP_POW, value, inv);
        AstNode *new_expr = ast_binop(OP_SUB, base, new_value);
        ast_free(simplified);
        SolveResult sub = sym_solve_core(new_expr, var, x0, st);
        ast_free(new_expr);
        return sub;
      }
      ast_free(value);
      ast_free(base);
    }
  }

  /* Cross-multiplication leaves nested SUBs (e.g. (x^2-1) - 2*(x-1)) that
   * extract_poly_coeffs cannot flatten through its OP_ADD-only walker.
   * Run the full expand pipeline once so terms become a flat polynomial. */
  {
    AstNode *expanded = sym_expand(simplified);
    if (expanded) {
      ast_free(simplified);
      simplified = expanded;
    }
    simplified = ast_canonicalize(simplified);
    simplified = sym_collect_terms(simplified);
    simplified = sym_full_simplify(simplified);
  }

  /* If the simplified form still has var-dependent denominators, multiply
   * through with cancellation so the rational equation becomes a
   * polynomial. Iterate because a single pass may leave a nested rational
   * intact (e.g. (x+1)*(1+x^-1)^-1 — the bases x+1 and 1+x^-1 don't
   * structurally match for power cancellation). Each pass clears another
   * level; bail at 8 to avoid runaway cost. Extraneous roots are stripped
   * later by filter_extraneous. */
  for (int pass = 0; pass < 8; pass++) {
    DenomList denoms = {0};
    collect_denominators(simplified, var, &denoms);
    if (denoms.count == 0) {
      denom_list_free(&denoms);
      break;
    }

    AstNode *product = ast_clone(denoms.items[0]);
    for (size_t i = 1; i < denoms.count; i++)
      product = ast_binop(OP_MUL, product, ast_clone(denoms.items[i]));
    simplified = ast_binop(OP_MUL, simplified, product);

    AstNode *expanded = sym_expand(simplified);
    if (expanded) {
      ast_free(simplified);
      simplified = expanded;
    }
    simplified = ast_canonicalize(simplified);
    simplified = sym_collect_terms(simplified);
    simplified = sym_full_simplify(simplified);
    denom_list_free(&denoms);
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

  /* Symbolic polynomial isolators: when extract_poly_coeffs cannot read the
   * numeric coefficients (because A/B/C carry free symbols other than
   * `var`), recover them via evaluation-point interpolation:
   *   linear   A*x + B           -> B = f(0), A = f(1) - f(0)
   *   quadratic A*x^2 + B*x + C  -> C = f(0),
   *                                 A = (f(1) + f(-1))/2 - C,
   *                                 B = (f(1) - f(-1))/2
   * Each isolator verifies via an extra sample point and returns symbolic
   * AST roots (eval'd to Complex when the symtab can supply numeric
   * bindings, otherwise reported as ast_to_string-able expressions). */
  {
    AstNode *node_zero = ast_number(0);
    AstNode *node_one = ast_number(1);
    AstNode *node_mone = ast_number(-1);
    AstNode *node_two = ast_number(2);
    AstNode *f0 = sym_full_simplify(sym_subs(simplified, var, node_zero));
    AstNode *f1 = sym_full_simplify(sym_subs(simplified, var, node_one));
    AstNode *fm1 = sym_full_simplify(sym_subs(simplified, var, node_mone));
    AstNode *f2 = sym_full_simplify(sym_subs(simplified, var, node_two));
    ast_free(node_zero);
    ast_free(node_one);
    ast_free(node_mone);
    ast_free(node_two);

    /* ---- linear ---- */
    if (f0 && f1 && f2) {
      AstNode *A =
          sym_full_simplify(ast_binop(OP_SUB, ast_clone(f1), ast_clone(f0)));
      AstNode *B = ast_clone(f0);
      AstNode *predicted_f2 = sym_full_simplify(
          ast_binop(OP_ADD, ast_binop(OP_MUL, ast_number(2), ast_clone(A)),
                    ast_clone(B)));
      AstNode *residual =
          sym_full_simplify(ast_binop(OP_SUB, ast_clone(f2), predicted_f2));
      int is_linear = residual && residual->type == AST_NUMBER &&
                      c_is_zero(residual->as.number);
      ast_free(residual);

      if (is_linear && !is_zero_node(A)) {
        AstNode *root_expr = sym_full_simplify(
            ast_binop(OP_DIV, ast_unary_neg(ast_clone(B)), ast_clone(A)));
        ast_free(A);
        ast_free(B);
        ast_free(f0);
        ast_free(f1);
        ast_free(fm1);
        ast_free(f2);
        ast_free(simplified);
        EvalResult er = eval(root_expr, st);
        if (er.ok) {
          ast_free(root_expr);
          SolveResult r = ok_roots(&er.value, 1);
          eval_result_free(&er);
          return r;
        }
        eval_result_free(&er);
        return ok_symbolic_single(root_expr);
      }
      ast_free(A);
      ast_free(B);
    }

    /* ---- quadratic ---- */
    if (f0 && f1 && fm1 && f2) {
      AstNode *sum =
          sym_full_simplify(ast_binop(OP_ADD, ast_clone(f1), ast_clone(fm1)));
      AstNode *A = sym_full_simplify(ast_binop(
          OP_SUB, ast_binop(OP_DIV, sum, ast_number(2)), ast_clone(f0)));
      AstNode *diff =
          sym_full_simplify(ast_binop(OP_SUB, ast_clone(f1), ast_clone(fm1)));
      AstNode *B = sym_full_simplify(ast_binop(OP_DIV, diff, ast_number(2)));
      AstNode *C = ast_clone(f0);

      /* verify with f(2) = 4A + 2B + C */
      AstNode *predicted_f2 = sym_full_simplify(ast_binop(
          OP_ADD,
          ast_binop(OP_ADD, ast_binop(OP_MUL, ast_number(4), ast_clone(A)),
                    ast_binop(OP_MUL, ast_number(2), ast_clone(B))),
          ast_clone(C)));
      AstNode *residual =
          sym_full_simplify(ast_binop(OP_SUB, ast_clone(f2), predicted_f2));
      int is_quad = residual && residual->type == AST_NUMBER &&
                    c_is_zero(residual->as.number);
      ast_free(residual);

      if (is_quad && !is_zero_node(A)) {
        /* discriminant = B^2 - 4AC */
        AstNode *disc = sym_full_simplify(ast_binop(
            OP_SUB, ast_binop(OP_POW, ast_clone(B), ast_number(2)),
            ast_binop(OP_MUL, ast_binop(OP_MUL, ast_number(4), ast_clone(A)),
                      ast_clone(C))));
        AstNode *sqrt_disc =
            ast_func_call("sqrt", 4, (AstNode *[]){ast_clone(disc)}, 1);
        ast_free(disc);
        AstNode *two_a =
            sym_full_simplify(ast_binop(OP_MUL, ast_number(2), ast_clone(A)));
        AstNode *neg_B = ast_unary_neg(ast_clone(B));
        AstNode *root_plus = sym_full_simplify(ast_binop(
            OP_DIV, ast_binop(OP_ADD, ast_clone(neg_B), ast_clone(sqrt_disc)),
            ast_clone(two_a)));
        AstNode *root_minus = sym_full_simplify(
            ast_binop(OP_DIV, ast_binop(OP_SUB, neg_B, sqrt_disc), two_a));
        ast_free(A);
        ast_free(B);
        ast_free(C);
        ast_free(f0);
        ast_free(f1);
        ast_free(fm1);
        ast_free(f2);
        ast_free(simplified);

        /* try eval — if both eval cleanly, return as numeric roots */
        EvalResult er_p = eval(root_plus, st);
        EvalResult er_m = eval(root_minus, st);
        if (er_p.ok && er_m.ok) {
          ast_free(root_plus);
          ast_free(root_minus);
          Complex pair[2] = {er_p.value, er_m.value};
          SolveResult r = ok_roots(pair, 2);
          eval_result_free(&er_p);
          eval_result_free(&er_m);
          return r;
        }
        eval_result_free(&er_p);
        eval_result_free(&er_m);
        SolveResult sr = {0};
        sr.symbolic_roots = malloc(2 * sizeof(AstNode *));
        sr.symbolic_roots[0] = root_plus;
        sr.symbolic_roots[1] = root_minus;
        sr.symbolic_count = 2;
        sr.ok = 1;
        return sr;
      }
      ast_free(A);
      ast_free(B);
      ast_free(C);
    }
    ast_free(f0);
    ast_free(f1);
    ast_free(fm1);
    ast_free(f2);
  }

  /* Newton only handles purely numeric expressions in `var`; refuse if
   * any other free symbol leaked through. */
  if (has_foreign_symbol(simplified, var)) {
    ast_free(simplified);
    return fail("cannot solve: equation contains unbound symbols other "
                "than the solve variable");
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
