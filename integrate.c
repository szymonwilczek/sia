#include "canonical.h"
#include "solve.h"
#include "symbolic_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static AstNode *integrate_trig(const AstNode *expr, const char *var) {
  if (expr->type != AST_FUNC_CALL || expr->as.call.nargs != 1)
    return NULL;

  const char *name = expr->as.call.name;
  const AstNode *inner = expr->as.call.args[0];
  AstNode *slope = NULL;
  AstNode *base = NULL;

  if (strcmp(name, "sin") == 0) {
    base = ast_unary_neg(
        ast_func_call("cos", 3, (AstNode *[]){ast_clone(inner)}, 1));
  } else if (strcmp(name, "cos") == 0) {
    base = ast_func_call("sin", 3, (AstNode *[]){ast_clone(inner)}, 1);
  } else if (strcmp(name, "exp") == 0) {
    base = ast_func_call("exp", 3, (AstNode *[]){ast_clone(inner)}, 1);
  } else if (strcmp(name, "sinh") == 0) {
    base = ast_func_call("cosh", 4, (AstNode *[]){ast_clone(inner)}, 1);
  } else if (strcmp(name, "cosh") == 0) {
    base = ast_func_call("sinh", 4, (AstNode *[]){ast_clone(inner)}, 1);
  } else if (strcmp(name, "tan") == 0) {
    base = ast_unary_neg(
        ast_func_call("ln", 2,
                      (AstNode *[]){ast_func_call(
                          "cos", 3, (AstNode *[]){ast_clone(inner)}, 1)},
                      1));
  } else if (strcmp(name, "ln") == 0) {
    if (!is_var(inner, var))
      return NULL;
    AstNode *x1 = ast_variable(var, strlen(var));
    AstNode *x2 = ast_variable(var, strlen(var));
    AstNode *lnx = ast_func_call(
        "ln", 2, (AstNode *[]){ast_variable(var, strlen(var))}, 1);
    base = ast_binop(OP_SUB, ast_binop(OP_MUL, x1, lnx), x2);
  } else {
    return NULL;
  }

  if (is_var(inner, var))
    return base;

  if (!sym_contains_var(inner, var)) {
    ast_free(base);
    return NULL;
  }

  slope = sym_diff(inner, var);
  if (!slope) {
    ast_free(base);
    return NULL;
  }
  slope = sym_full_simplify(slope);
  if (sym_contains_var(slope, var) || is_zero_node(slope)) {
    ast_free(slope);
    ast_free(base);
    return NULL;
  }

  {
    AstNode *linear_part = sym_full_simplify(
        ast_binop(OP_MUL, ast_clone(slope), ast_variable(var, strlen(var))));
    AstNode *offset =
        sym_full_simplify(ast_binop(OP_SUB, ast_clone(inner), linear_part));
    if (sym_contains_var(offset, var)) {
      ast_free(offset);
      ast_free(slope);
      ast_free(base);
      return NULL;
    }
    ast_free(offset);
  }

  if (slope->type == AST_NUMBER) {
    Complex scale = c_div(c_real(1.0), slope->as.number);
    ast_free(slope);
    return sym_full_simplify(
        ast_binop(OP_MUL, ast_number_complex(scale), base));
  }

  return sym_full_simplify(ast_binop(OP_DIV, base, slope));
}

AstNode *sym_integrate(const AstNode *expr, const char *var);
AstNode *sym_laplace(const AstNode *expr, const char *t, const char *s);

#define SYM_INTEGRATE_MAX_DEPTH 8

typedef struct {
  AstNode *term;
  AstNode *root_coeff;
} IntegralAffineResult;

static IntegralAffineResult integral_affine_fail(void) {
  IntegralAffineResult result = {0};
  return result;
}

static IntegralAffineResult integral_affine_term(AstNode *term) {
  IntegralAffineResult result = {term, ast_number(0)};
  return result;
}

static IntegralAffineResult integral_affine_root_coeff(AstNode *coeff) {
  IntegralAffineResult result = {ast_number(0), coeff};
  return result;
}

static int integral_affine_ok(const IntegralAffineResult *result) {
  return result && result->term && result->root_coeff;
}

static void integral_affine_free(IntegralAffineResult *result) {
  if (!result)
    return;
  ast_free(result->term);
  ast_free(result->root_coeff);
  result->term = NULL;
  result->root_coeff = NULL;
}

static void flatten_product_terms(const AstNode *node, NodeList *terms) {
  if (!node)
    return;
  if (node->type == AST_BINOP && node->as.binop.op == OP_MUL) {
    flatten_product_terms(node->as.binop.left, terms);
    flatten_product_terms(node->as.binop.right, terms);
    return;
  }
  node_list_push(terms, ast_clone(node));
}

static AstNode *build_product_excluding(const NodeList *terms, size_t skip) {
  AstNode *result = NULL;

  for (size_t i = 0; i < terms->count; i++) {
    if (i == skip)
      continue;
    if (!result) {
      result = ast_clone(terms->items[i]);
    } else {
      result = ast_binop(OP_MUL, result, ast_clone(terms->items[i]));
    }
  }

  if (!result)
    return ast_number(1);
  return result;
}

#define PFD_MAX_DEGREE 8
#define PFD_MAX_FACTORS 8

typedef struct {
  Complex coeffs[PFD_MAX_DEGREE + 1];
  int deg;
} Poly1D;

typedef struct {
  int base_degree;
  int power;
  Poly1D base;
} PfdFactor;

typedef struct {
  PfdFactor items[PFD_MAX_FACTORS];
  size_t count;
} PfdFactorList;

static int pfd_integer_node(const AstNode *node, int *out) {
  double rounded = 0.0;

  if (!node || node->type != AST_NUMBER || !c_is_real(node->as.number))
    return 0;

  rounded = round(node->as.number.re);
  if (fabs(node->as.number.re - rounded) > 1e-9)
    return 0;

  if (out)
    *out = (int)rounded;
  return 1;
}

static void poly1d_zero(Poly1D *poly) {
  memset(poly, 0, sizeof(*poly));
  poly->deg = 0;
}

static void poly1d_normalize(Poly1D *poly) {
  while (poly->deg > 0 && c_is_zero(poly->coeffs[poly->deg]))
    poly->deg--;
}

static int poly1d_is_zero(const Poly1D *poly) {
  for (int i = 0; i <= poly->deg; i++)
    if (!c_is_zero(poly->coeffs[i]))
      return 0;
  return 1;
}

static int poly1d_extract(const AstNode *expr, const char *var, int max_degree,
                          Poly1D *poly) {
  const AstNode *stack[256];
  AstNode *expanded = NULL;
  int sp = 0;

  if (max_degree > PFD_MAX_DEGREE)
    return 0;

  poly1d_zero(poly);
  expanded = sym_expand(ast_clone(expr));
  expanded = ast_canonicalize(expanded);
  expanded = sym_collect_terms(expanded);
  expanded = sym_simplify(expanded);

  stack[sp++] = expanded;
  while (sp > 0) {
    const AstNode *node = stack[--sp];
    const AstNode *term = node;
    Complex coeff = c_real(1.0);
    int degree = 0;

    if (!node)
      continue;

    if (node->type == AST_BINOP && node->as.binop.op == OP_ADD) {
      if (sp + 2 > 256) {
        ast_free(expanded);
        return 0;
      }
      stack[sp++] = node->as.binop.left;
      stack[sp++] = node->as.binop.right;
      continue;
    }

    if (node->type == AST_UNARY_NEG) {
      coeff = c_real(-1.0);
      term = node->as.unary.operand;
    }

    if (term->type == AST_NUMBER) {
      poly->coeffs[0] = c_add(poly->coeffs[0], c_mul(coeff, term->as.number));
      continue;
    }

    if (term->type == AST_VARIABLE && strcmp(term->as.variable, var) == 0) {
      if (max_degree < 1) {
        ast_free(expanded);
        return 0;
      }
      poly->coeffs[1] = c_add(poly->coeffs[1], coeff);
      if (poly->deg < 1)
        poly->deg = 1;
      continue;
    }

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

    if (term->type == AST_VARIABLE && strcmp(term->as.variable, var) == 0) {
      if (max_degree < 1) {
        ast_free(expanded);
        return 0;
      }
      poly->coeffs[1] = c_add(poly->coeffs[1], coeff);
      if (poly->deg < 1)
        poly->deg = 1;
      continue;
    }

    if (term->type == AST_BINOP && term->as.binop.op == OP_POW &&
        is_var(term->as.binop.left, var) &&
        pfd_integer_node(term->as.binop.right, &degree)) {
      if (degree < 0 || degree > max_degree) {
        ast_free(expanded);
        return 0;
      }
      poly->coeffs[degree] = c_add(poly->coeffs[degree], coeff);
      if (poly->deg < degree)
        poly->deg = degree;
      continue;
    }

    if (sym_contains_var(term, var)) {
      ast_free(expanded);
      return 0;
    }

    poly->coeffs[0] = c_add(poly->coeffs[0], coeff);
  }

  ast_free(expanded);
  poly1d_normalize(poly);
  return 1;
}

static AstNode *poly1d_to_ast(const Poly1D *poly, const char *var) {
  AstNode *result = NULL;

  for (int i = 0; i <= poly->deg; i++) {
    AstNode *term = NULL;

    if (c_is_zero(poly->coeffs[i]))
      continue;

    if (i == 0) {
      term = ast_number_complex(poly->coeffs[0]);
    } else {
      AstNode *power = ast_variable(var, strlen(var));
      if (i != 1)
        power = ast_binop(OP_POW, power, ast_number(i));

      if (c_is_one(poly->coeffs[i])) {
        term = power;
      } else {
        term = ast_binop(OP_MUL, ast_number_complex(poly->coeffs[i]), power);
      }
    }

    if (!result)
      result = term;
    else
      result = ast_binop(OP_ADD, result, term);
  }

  if (!result)
    return ast_number(0);
  return sym_full_simplify(result);
}

static int poly1d_shift_mul(const Poly1D *poly, int shift, Poly1D *out) {
  poly1d_zero(out);
  if (poly->deg + shift > PFD_MAX_DEGREE)
    return 0;

  for (int i = 0; i <= poly->deg; i++)
    out->coeffs[i + shift] = poly->coeffs[i];
  out->deg = poly->deg + shift;
  poly1d_normalize(out);
  return 1;
}

static int poly1d_divmod(const Poly1D *numer, const Poly1D *denom, Poly1D *quot,
                         Poly1D *rem) {
  Poly1D work;

  if (poly1d_is_zero(denom))
    return 0;

  poly1d_zero(quot);
  work = *numer;
  poly1d_normalize(&work);

  while (!poly1d_is_zero(&work) && work.deg >= denom->deg) {
    int shift = work.deg - denom->deg;
    Complex factor = c_div(work.coeffs[work.deg], denom->coeffs[denom->deg]);

    if (shift > PFD_MAX_DEGREE)
      return 0;

    quot->coeffs[shift] = c_add(quot->coeffs[shift], factor);
    if (quot->deg < shift)
      quot->deg = shift;

    for (int i = 0; i <= denom->deg; i++) {
      int target = i + shift;
      work.coeffs[target] =
          c_sub(work.coeffs[target], c_mul(factor, denom->coeffs[i]));
    }
    poly1d_normalize(&work);
  }

  *rem = work;
  poly1d_normalize(quot);
  poly1d_normalize(rem);
  return 1;
}

static int poly1d_equal(const Poly1D *lhs, const Poly1D *rhs) {
  if (lhs->deg != rhs->deg)
    return 0;
  for (int i = 0; i <= lhs->deg; i++) {
    if (c_abs(c_sub(lhs->coeffs[i], rhs->coeffs[i])) > 1e-9)
      return 0;
  }
  return 1;
}

static int poly1d_mul(const Poly1D *lhs, const Poly1D *rhs, Poly1D *out) {
  poly1d_zero(out);
  if (lhs->deg + rhs->deg > PFD_MAX_DEGREE)
    return 0;

  out->deg = lhs->deg + rhs->deg;
  for (int i = 0; i <= lhs->deg; i++) {
    for (int j = 0; j <= rhs->deg; j++) {
      out->coeffs[i + j] =
          c_add(out->coeffs[i + j], c_mul(lhs->coeffs[i], rhs->coeffs[j]));
    }
  }
  poly1d_normalize(out);
  return 1;
}

static int poly1d_pow(const Poly1D *base, int power, Poly1D *out) {
  Poly1D result;

  if (power < 0)
    return 0;

  poly1d_zero(&result);
  result.coeffs[0] = c_real(1.0);

  for (int i = 0; i < power; i++) {
    Poly1D next;
    if (!poly1d_mul(&result, base, &next))
      return 0;
    result = next;
  }

  *out = result;
  return 1;
}

static int pfd_factor_list_push(PfdFactorList *list, const Poly1D *poly,
                                int base_degree, int power) {
  for (size_t i = 0; i < list->count; i++) {
    if (list->items[i].base_degree == base_degree &&
        poly1d_equal(&list->items[i].base, poly)) {
      list->items[i].power += power;
      return 1;
    }
  }

  if (list->count >= PFD_MAX_FACTORS)
    return 0;
  list->items[list->count].base_degree = base_degree;
  list->items[list->count].power = power;
  list->items[list->count].base = *poly;
  list->count++;
  return 1;
}

static int poly1d_try_real_root(const Poly1D *poly, const char *var,
                                Complex *root_out) {
  static const double seeds[] = {0.0, 1.0,  -1.0, 2.0, -2.0,
                                 0.5, -0.5, 3.0,  -3.0};
  SymTab empty = {0};
  AstNode *poly_ast = poly1d_to_ast(poly, var);

  for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); i++) {
    SolveResult roots = sym_solve(poly_ast, var, c_real(seeds[i]), &empty);
    if (roots.ok && roots.count >= 1 && c_is_real(roots.roots[0]) &&
        fabs(roots.roots[0].im) < 1e-9) {
      *root_out = roots.roots[0];
      solve_result_free(&roots);
      ast_free(poly_ast);
      return 1;
    }
    solve_result_free(&roots);
  }

  ast_free(poly_ast);
  return 0;
}

static int pfd_factor_poly(const Poly1D *poly, const char *var,
                           PfdFactorList *list);

static int pfd_factor_even_substitution(const Poly1D *poly, const char *var,
                                        PfdFactorList *list) {
  Poly1D u_poly = {0};
  u_poly.deg = poly->deg / 2;
  for (int i = 0; i <= u_poly.deg; i++)
    u_poly.coeffs[i] = poly->coeffs[2 * i];

  while (u_poly.deg >= 1) {
    Poly1D quad = {0};
    Complex u_root = c_real(0.0);
    int found = 0;

    if (u_poly.deg == 1) {
      Complex ua = u_poly.coeffs[1];
      quad.coeffs[0] = c_div(u_poly.coeffs[0], ua);
      quad.coeffs[2] = c_real(1.0);
      quad.deg = 2;
      return pfd_factor_poly(&quad, var, list);
    }

    {
      static const double seeds[] = {0.0, 1.0,  -1.0, 2.0,  -2.0, 0.5, -0.5,
                                     3.0, -3.0, 4.0,  -4.0, 5.0,  -5.0};
      for (size_t s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
        Complex val = c_real(seeds[s]);
        Complex eval = c_real(0.0);
        Complex power = c_real(1.0);
        for (int i = 0; i <= u_poly.deg; i++) {
          eval = c_add(eval, c_mul(u_poly.coeffs[i], power));
          if (i < u_poly.deg)
            power = c_mul(power, val);
        }
        if (c_abs(eval) < 1e-9) {
          u_root = val;
          found = 1;
          break;
        }
      }
    }

    if (!found && u_poly.deg == 2) {
      Complex a = u_poly.coeffs[2];
      Complex b = u_poly.coeffs[1];
      Complex c = u_poly.coeffs[0];
      Complex disc = c_sub(c_mul(b, b), c_mul(c_real(4.0), c_mul(a, c)));
      if (c_is_real(disc) && disc.re >= -1e-12) {
        if (fabs(disc.re) < 1e-12)
          disc = c_real(0.0);
        u_root = c_div(c_sub(c_neg(b), c_sqrt(disc)), c_mul(c_real(2.0), a));
        found = 1;
      }
    }

    if (!found)
      return 0;

    {
      Poly1D u_linear = {0};
      Poly1D u_quot = {0};
      Poly1D u_rem = {0};

      u_linear.coeffs[0] = c_neg(u_root);
      u_linear.coeffs[1] = c_real(1.0);
      u_linear.deg = 1;

      if (!poly1d_divmod(&u_poly, &u_linear, &u_quot, &u_rem))
        return 0;
      for (int i = 0; i <= u_rem.deg; i++) {
        if (c_abs(u_rem.coeffs[i]) > 1e-8)
          return 0;
      }

      quad.coeffs[0] = c_neg(u_root);
      quad.coeffs[2] = c_real(1.0);
      quad.deg = 2;

      if (!pfd_factor_poly(&quad, var, list))
        return 0;

      u_poly = u_quot;
    }
  }
  return 1;
}

static int pfd_factor_poly(const Poly1D *poly, const char *var,
                           PfdFactorList *list) {
  if (poly->deg < 1)
    return 0;

  if (poly->deg == 1)
    return pfd_factor_list_push(list, poly, 1, 1);

  if (poly->deg == 2) {
    Complex qa = poly->coeffs[2];
    Complex qb = poly->coeffs[1];
    Complex qc = poly->coeffs[0];
    Complex disc = c_sub(c_mul(qb, qb), c_mul(c_real(4.0), c_mul(qa, qc)));
    Fraction disc_frac;

    if (c_is_real(disc) && disc.re >= -1e-12 && disc.exact &&
        c_real_fraction(disc, &disc_frac) && disc_frac.num >= 0) {
      long long dp = disc_frac.num;
      long long dq = disc_frac.den;
      long long sp = (long long)round(sqrt((double)dp));
      long long sq = (long long)round(sqrt((double)dq));

      if (sp * sp == dp && sq * sq == dq) {
        Complex sqrt_disc =
            c_from_fractions(fraction_make(sp, sq), fraction_make(0, 1));
        Complex r1 = c_div(c_sub(c_neg(qb), sqrt_disc), c_mul(c_real(2.0), qa));
        Complex r2 = c_div(c_add(c_neg(qb), sqrt_disc), c_mul(c_real(2.0), qa));
        Poly1D linear = {0};

        linear.coeffs[0] = c_neg(r1);
        linear.coeffs[1] = c_real(1.0);
        linear.deg = 1;
        if (c_abs(c_sub(r1, r2)) <= 1e-9)
          return pfd_factor_list_push(list, &linear, 1, 2);

        if (!pfd_factor_list_push(list, &linear, 1, 1))
          return 0;
        poly1d_zero(&linear);
        linear.coeffs[0] = c_neg(r2);
        linear.coeffs[1] = c_real(1.0);
        linear.deg = 1;
        return pfd_factor_list_push(list, &linear, 1, 1);
      }
    }

    return pfd_factor_list_push(list, poly, 2, 1);
  }

  if (c_is_zero(poly->coeffs[0])) {
    Poly1D reduced = {0};
    Poly1D linear = {0};

    linear.coeffs[0] = c_real(0.0);
    linear.coeffs[1] = c_real(1.0);
    linear.deg = 1;

    for (int i = 1; i <= poly->deg; i++)
      reduced.coeffs[i - 1] = poly->coeffs[i];
    reduced.deg = poly->deg - 1;
    poly1d_normalize(&reduced);

    return pfd_factor_list_push(list, &linear, 1, 1) &&
           pfd_factor_poly(&reduced, var, list);
  }

  {
    Complex root = c_real(0.0);
    Poly1D linear = {0};
    Poly1D quotient = {0};
    Poly1D remainder = {0};

    if (!poly1d_try_real_root(poly, var, &root)) {
      int only_even = 1;
      for (int i = 1; i <= poly->deg; i += 2)
        if (!c_is_zero(poly->coeffs[i])) {
          only_even = 0;
          break;
        }
      if (!only_even || poly->deg < 4 || poly->deg % 2 != 0)
        return 0;
      return pfd_factor_even_substitution(poly, var, list);
    }

    {
      int root_snapped = 0;
      static const int snap_denoms[] = {1, 2, 3, 4, 5, 6, 8, 10, 12};
      for (size_t s = 0; s < sizeof(snap_denoms) / sizeof(snap_denoms[0]);
           s++) {
        int d = snap_denoms[s];
        double approx = root.re * d;
        long long n = (long long)round(approx);
        if (fabs(approx - (double)n) < 1e-6 * d) {
          Complex snapped =
              c_from_fractions(fraction_make(n, d), fraction_make(0, 1));
          Poly1D tl = {0};
          Poly1D tq = {0};
          Poly1D tr = {0};
          tl.coeffs[0] = c_neg(snapped);
          tl.coeffs[1] = c_real(1.0);
          tl.deg = 1;
          if (poly1d_divmod(poly, &tl, &tq, &tr) && poly1d_is_zero(&tr)) {
            root = snapped;
            root_snapped = 1;
            break;
          }
        }
      }

      if (!root_snapped && poly->deg >= 4 && poly->deg % 2 == 0) {
        int only_even = 1;
        for (int i = 1; i <= poly->deg; i += 2)
          if (!c_is_zero(poly->coeffs[i])) {
            only_even = 0;
            break;
          }
        if (only_even)
          return pfd_factor_even_substitution(poly, var, list);
      }
    }

    linear.coeffs[0] = c_neg(root);
    linear.coeffs[1] = c_real(1.0);
    linear.deg = 1;

    if (!poly1d_divmod(poly, &linear, &quotient, &remainder))
      return 0;
    for (int i = 0; i <= remainder.deg; i++) {
      if (c_abs(remainder.coeffs[i]) > 1e-8)
        return 0;
      remainder.coeffs[i] = c_real(0.0);
    }
    poly1d_normalize(&quotient);

    return pfd_factor_list_push(list, &linear, 1, 1) &&
           pfd_factor_poly(&quotient, var, list);
  }
}

static int pfd_extract_rational_expr(const AstNode *expr, const char *var,
                                     AstNode **numer_out, AstNode **denom_out) {
  NodeList factors = {0};
  AstNode *numer = ast_number(1);
  AstNode *denom = ast_number(1);

  *numer_out = NULL;
  *denom_out = NULL;

  if (expr->type == AST_BINOP && expr->as.binop.op == OP_DIV) {
    *numer_out = sym_full_simplify(ast_clone(expr->as.binop.left));
    *denom_out = sym_full_simplify(ast_clone(expr->as.binop.right));
    return *numer_out && *denom_out;
  }

  flatten_product_terms(expr, &factors);
  for (size_t i = 0; i < factors.count; i++) {
    AstNode *factor = factors.items[i];
    AstNode *normalized_base = NULL;
    Poly1D poly;
    int exp = 0;

    if (factor->type == AST_BINOP && factor->as.binop.op == OP_POW &&
        pfd_integer_node(factor->as.binop.right, &exp) && exp < 0) {
      normalized_base = sym_full_simplify(ast_clone(factor->as.binop.left));
      if (normalized_base &&
          poly1d_extract(normalized_base, var, PFD_MAX_DEGREE, &poly) &&
          poly.deg >= 1) {
        for (int k = 0; k < -exp; k++)
          denom = sym_full_simplify(
              ast_binop(OP_MUL, denom, ast_clone(normalized_base)));
        ast_free(normalized_base);
        continue;
      }
      ast_free(normalized_base);
    }

    if (factor->type == AST_BINOP && factor->as.binop.op == OP_POW &&
        pfd_integer_node(factor->as.binop.right, &exp) && exp < 0 &&
        poly1d_extract(factor->as.binop.left, var, PFD_MAX_DEGREE, &poly) &&
        poly.deg >= 1) {
      for (int k = 0; k < -exp; k++)
        denom = sym_full_simplify(
            ast_binop(OP_MUL, denom, ast_clone(factor->as.binop.left)));
    } else {
      numer = sym_full_simplify(ast_binop(OP_MUL, numer, ast_clone(factor)));
    }
  }

  node_list_free(&factors);
  if (!sym_contains_var(denom, var)) {
    ast_free(numer);
    ast_free(denom);
    return 0;
  }

  *numer_out = numer;
  *denom_out = denom;
  return 1;
}

static int pfd_solve_coefficients(const Poly1D *numer, const Poly1D *denom,
                                  const PfdFactorList *factors,
                                  Complex *solution, size_t unknown_count) {
  Matrix *system = NULL;
  Matrix *inverse = NULL;
  Complex rhs[PFD_MAX_DEGREE + 1] = {0};
  size_t col = 0;

  if (unknown_count == 0 || unknown_count > (size_t)denom->deg)
    return 0;

  system = matrix_create(unknown_count, unknown_count);
  for (size_t row = 0; row < unknown_count; row++)
    rhs[row] = (row <= (size_t)numer->deg) ? numer->coeffs[row] : c_real(0.0);

  for (size_t i = 0; i < factors->count; i++) {
    for (int power = 1; power <= factors->items[i].power; power++) {
      Poly1D base_power = {0};
      Poly1D quotient = {0};
      Poly1D remainder = {0};

      if (!poly1d_pow(&factors->items[i].base, power, &base_power) ||
          !poly1d_divmod(denom, &base_power, &quotient, &remainder) ||
          !poly1d_is_zero(&remainder)) {
        matrix_free(system);
        return 0;
      }

      if (factors->items[i].base_degree == 1) {
        for (size_t row = 0; row < unknown_count; row++) {
          Complex value = (row <= (size_t)quotient.deg) ? quotient.coeffs[row]
                                                        : c_real(0.0);
          matrix_set(system, row, col, value);
        }
        col++;
      } else {
        Poly1D shifted = {0};
        if (!poly1d_shift_mul(&quotient, 1, &shifted)) {
          matrix_free(system);
          return 0;
        }
        for (size_t row = 0; row < unknown_count; row++) {
          Complex av =
              (row <= (size_t)shifted.deg) ? shifted.coeffs[row] : c_real(0.0);
          Complex bv = (row <= (size_t)quotient.deg) ? quotient.coeffs[row]
                                                     : c_real(0.0);
          matrix_set(system, row, col, av);
          matrix_set(system, row, col + 1, bv);
        }
        col += 2;
      }
    }
  }

  inverse = matrix_inverse(system);
  matrix_free(system);
  if (!inverse)
    return 0;

  for (size_t row = 0; row < unknown_count; row++) {
    Complex sum = c_real(0.0);
    for (size_t k = 0; k < unknown_count; k++)
      sum = c_add(sum, c_mul(matrix_get(inverse, row, k), rhs[k]));
    solution[row] = sum;
  }

  matrix_free(inverse);
  return 1;
}

static AstNode *pfd_integrate_linear_term(Complex coeff, const Poly1D *factor,
                                          int power, const char *var) {
  AstNode *factor_ast = poly1d_to_ast(factor, var);

  if (power == 1) {
    AstNode *abs_ast = ast_func_call("abs", 3, (AstNode *[]){factor_ast}, 1);
    AstNode *log_ast = ast_func_call("ln", 2, (AstNode *[]){abs_ast}, 1);
    Complex scale = c_div(coeff, factor->coeffs[1]);

    return sym_full_simplify(
        ast_binop(OP_MUL, ast_number_complex(scale), log_ast));
  }

  return sym_full_simplify(
      ast_binop(OP_DIV,
                ast_number_complex(c_div(
                    coeff, c_mul(c_real(1.0 - power), factor->coeffs[1]))),
                ast_binop(OP_POW, factor_ast, ast_number(power - 1))));
}

static AstNode *build_sqrt_node(Complex val) {
  if (val.exact && c_is_real(val) && val.re >= 0) {
    Fraction frac;
    if (c_real_fraction(val, &frac) && frac.num >= 0) {
      long long p = frac.num;
      long long q = frac.den;
      long long sp = (long long)round(sqrt((double)p));
      long long sq = (long long)round(sqrt((double)q));
      if (sp * sp == p && sq * sq == q)
        return ast_number_complex(
            c_from_fractions(fraction_make(sp, sq), fraction_make(0, 1)));
    }
  }
  return ast_func_call("sqrt", 4, (AstNode *[]){ast_number_complex(val)}, 1);
}

static AstNode *pfd_integrate_quadratic_term(Complex ax_coeff, Complex b_coeff,
                                             const Poly1D *factor,
                                             const char *var) {
  Complex a = factor->coeffs[2];
  Complex b = factor->coeffs[1];
  Complex c0 = factor->coeffs[0];
  Complex alpha = c_div(ax_coeff, c_mul(c_real(2.0), a));
  Complex beta = c_sub(b_coeff, c_mul(alpha, b));
  Complex delta = c_sub(c_mul(c_real(4.0), c_mul(a, c0)), c_mul(b, b));
  AstNode *result = NULL;

  if (!c_is_zero(alpha)) {
    AstNode *log_ast =
        ast_func_call("ln", 2, (AstNode *[]){poly1d_to_ast(factor, var)}, 1);
    result = sym_full_simplify(
        ast_binop(OP_MUL, ast_number_complex(alpha), log_ast));
  }

  if (!c_is_zero(beta)) {
    AstNode *scaled = NULL;

    if (c_is_real(delta) && delta.re < 0) {
      Complex neg_delta = c_neg(delta);
      AstNode *sqrt_nd = build_sqrt_node(neg_delta);
      AstNode *two_ax_b = sym_full_simplify(
          ast_binop(OP_ADD,
                    ast_binop(OP_MUL, ast_number_complex(c_mul(c_real(2.0), a)),
                              ast_variable(var, strlen(var))),
                    ast_number_complex(b)));
      AstNode *ln_sub =
          ast_func_call("ln", 2,
                        (AstNode *[]){ast_func_call(
                            "abs", 3,
                            (AstNode *[]){ast_binop(OP_SUB, ast_clone(two_ax_b),
                                                    ast_clone(sqrt_nd))},
                            1)},
                        1);
      AstNode *ln_add = ast_func_call(
          "ln", 2,
          (AstNode *[]){ast_func_call(
              "abs", 3,
              (AstNode *[]){ast_binop(OP_ADD, two_ax_b, ast_clone(sqrt_nd))},
              1)},
          1);
      AstNode *scale = sym_full_simplify(
          ast_binop(OP_DIV, ast_number_complex(beta), sqrt_nd));
      scaled = sym_full_simplify(
          ast_binop(OP_MUL, scale, ast_binop(OP_SUB, ln_sub, ln_add)));
    } else {
      AstNode *sqrt_delta = build_sqrt_node(delta);
      AstNode *x_term =
          ast_binop(OP_MUL, ast_number_complex(c_mul(c_real(2.0), a)),
                    ast_variable(var, strlen(var)));
      AstNode *atan_arg = sym_full_simplify(
          ast_binop(OP_DIV, ast_binop(OP_ADD, x_term, ast_number_complex(b)),
                    ast_clone(sqrt_delta)));
      AstNode *atan_ast = ast_func_call("atan", 4, (AstNode *[]){atan_arg}, 1);
      AstNode *scale = sym_full_simplify(ast_binop(
          OP_DIV, ast_number_complex(c_mul(c_real(2.0), beta)), sqrt_delta));
      scaled = sym_full_simplify(ast_binop(OP_MUL, scale, atan_ast));
    }

    if (!result)
      result = scaled;
    else
      result = sym_full_simplify(ast_binop(OP_ADD, result, scaled));
  }

  return result ? result : ast_number(0);
}

static AstNode *pfd_integrate_inv_quadratic_power(const Poly1D *factor, int n,
                                                  const char *var) {
  Complex a = factor->coeffs[2];
  Complex b = factor->coeffs[1];
  Complex c0 = factor->coeffs[0];
  Complex delta = c_sub(c_mul(c_real(4.0), c_mul(a, c0)), c_mul(b, b));

  if (n == 1) {
    if (c_is_real(delta) && delta.re < 0) {
      Complex neg_delta = c_neg(delta);
      AstNode *sqrt_nd = build_sqrt_node(neg_delta);
      AstNode *two_ax_b = sym_full_simplify(
          ast_binop(OP_ADD,
                    ast_binop(OP_MUL, ast_number_complex(c_mul(c_real(2.0), a)),
                              ast_variable(var, strlen(var))),
                    ast_number_complex(b)));
      AstNode *ln_sub =
          ast_func_call("ln", 2,
                        (AstNode *[]){ast_func_call(
                            "abs", 3,
                            (AstNode *[]){ast_binop(OP_SUB, ast_clone(two_ax_b),
                                                    ast_clone(sqrt_nd))},
                            1)},
                        1);
      AstNode *ln_add = ast_func_call(
          "ln", 2,
          (AstNode *[]){ast_func_call(
              "abs", 3,
              (AstNode *[]){ast_binop(OP_ADD, two_ax_b, ast_clone(sqrt_nd))},
              1)},
          1);
      AstNode *scale =
          sym_full_simplify(ast_binop(OP_DIV, ast_number(1), sqrt_nd));
      return sym_full_simplify(
          ast_binop(OP_MUL, scale, ast_binop(OP_SUB, ln_sub, ln_add)));
    }

    {
      AstNode *sqrt_d = build_sqrt_node(delta);
      AstNode *x_term =
          ast_binop(OP_MUL, ast_number_complex(c_mul(c_real(2.0), a)),
                    ast_variable(var, strlen(var)));
      AstNode *atan_arg = sym_full_simplify(
          ast_binop(OP_DIV, ast_binop(OP_ADD, x_term, ast_number_complex(b)),
                    ast_clone(sqrt_d)));
      AstNode *atan_ast = ast_func_call("atan", 4, (AstNode *[]){atan_arg}, 1);
      AstNode *scale =
          sym_full_simplify(ast_binop(OP_DIV, ast_number(2), sqrt_d));
      return sym_full_simplify(ast_binop(OP_MUL, scale, atan_ast));
    }
  }

  {
    AstNode *rec = pfd_integrate_inv_quadratic_power(factor, n - 1, var);
    AstNode *two_ax_b = NULL;
    AstNode *first = NULL;
    AstNode *second = NULL;
    Complex nm1 = c_real((double)(n - 1));
    Complex nm1_delta = c_mul(nm1, delta);
    Complex rec_coeff = c_div(
        c_mul(c_mul(c_real(2.0), a), c_real((double)(2 * n - 3))), nm1_delta);

    if (!rec)
      return NULL;

    two_ax_b = sym_full_simplify(
        ast_binop(OP_ADD,
                  ast_binop(OP_MUL, ast_number_complex(c_mul(c_real(2.0), a)),
                            ast_variable(var, strlen(var))),
                  ast_number_complex(b)));

    first = sym_full_simplify(
        ast_binop(OP_DIV, two_ax_b,
                  ast_binop(OP_MUL, ast_number_complex(nm1_delta),
                            ast_binop(OP_POW, poly1d_to_ast(factor, var),
                                      ast_number(n - 1)))));

    second = sym_full_simplify(
        ast_binop(OP_MUL, ast_number_complex(rec_coeff), rec));

    return sym_full_simplify(ast_binop(OP_ADD, first, second));
  }
}

static AstNode *pfd_integrate_quadratic_term_power(Complex ax_coeff,
                                                   Complex b_coeff,
                                                   const Poly1D *factor,
                                                   int power, const char *var) {
  Complex a = factor->coeffs[2];
  Complex b = factor->coeffs[1];
  Complex alpha = c_div(ax_coeff, c_mul(c_real(2.0), a));
  Complex beta = c_sub(b_coeff, c_mul(alpha, b));
  AstNode *result = NULL;

  if (power == 1)
    return pfd_integrate_quadratic_term(ax_coeff, b_coeff, factor, var);

  if (!c_is_zero(alpha)) {
    Complex scale = c_neg(c_div(alpha, c_real((double)(power - 1))));
    result = sym_full_simplify(ast_binop(
        OP_DIV, ast_number_complex(scale),
        ast_binop(OP_POW, poly1d_to_ast(factor, var), ast_number(power - 1))));
  }

  if (!c_is_zero(beta)) {
    AstNode *inv_int = pfd_integrate_inv_quadratic_power(factor, power, var);
    AstNode *term = NULL;
    if (!inv_int) {
      ast_free(result);
      return NULL;
    }
    term =
        sym_full_simplify(ast_binop(OP_MUL, ast_number_complex(beta), inv_int));
    if (result)
      result = sym_full_simplify(ast_binop(OP_ADD, result, term));
    else
      result = term;
  }

  return result ? result : ast_number(0);
}

static AstNode *integrate_partial_fractions(const AstNode *expr,
                                            const char *var) {
  AstNode *numer_ast = NULL;
  AstNode *denom_ast = NULL;
  AstNode *quotient_ast = NULL;
  AstNode *quotient_integral = NULL;
  AstNode *proper_integral = NULL;
  AstNode *result = NULL;
  Poly1D numer = {0};
  Poly1D denom = {0};
  Poly1D quotient = {0};
  Poly1D remainder = {0};
  PfdFactorList factors = {0};
  Complex coeffs[PFD_MAX_DEGREE + 1] = {0};
  size_t unknown_count = 0;
  size_t coeff_index = 0;

  if (!pfd_extract_rational_expr(expr, var, &numer_ast, &denom_ast))
    return NULL;
  if (!poly1d_extract(numer_ast, var, PFD_MAX_DEGREE, &numer) ||
      !poly1d_extract(denom_ast, var, PFD_MAX_DEGREE, &denom) ||
      denom.deg < 1) {
    ast_free(numer_ast);
    ast_free(denom_ast);
    return NULL;
  }

  if (!poly1d_divmod(&numer, &denom, &quotient, &remainder)) {
    ast_free(numer_ast);
    ast_free(denom_ast);
    return NULL;
  }

  if (!poly1d_is_zero(&quotient)) {
    quotient_ast = poly1d_to_ast(&quotient, var);
    quotient_integral = sym_integrate(quotient_ast, var);
    ast_free(quotient_ast);
    if (!quotient_integral) {
      ast_free(numer_ast);
      ast_free(denom_ast);
      return NULL;
    }
  }

  if (poly1d_is_zero(&remainder)) {
    ast_free(numer_ast);
    ast_free(denom_ast);
    return quotient_integral ? quotient_integral : ast_number(0);
  }

  if (!pfd_factor_poly(&denom, var, &factors)) {
    ast_free(numer_ast);
    ast_free(denom_ast);
    ast_free(quotient_integral);
    return NULL;
  }

  for (size_t i = 0; i < factors.count; i++)
    unknown_count +=
        (size_t)(factors.items[i].base_degree * factors.items[i].power);

  if (unknown_count == 0 || unknown_count > PFD_MAX_DEGREE ||
      unknown_count != (size_t)denom.deg ||
      !pfd_solve_coefficients(&remainder, &denom, &factors, coeffs,
                              unknown_count)) {
    ast_free(numer_ast);
    ast_free(denom_ast);
    ast_free(quotient_integral);
    return NULL;
  }

  for (size_t i = 0; i < factors.count; i++) {
    for (int power = 1; power <= factors.items[i].power; power++) {
      AstNode *term = NULL;

      if (factors.items[i].base_degree == 1) {
        term = pfd_integrate_linear_term(coeffs[coeff_index],
                                         &factors.items[i].base, power, var);
        coeff_index++;
      } else {
        term = pfd_integrate_quadratic_term_power(
            coeffs[coeff_index], coeffs[coeff_index + 1],
            &factors.items[i].base, power, var);
        coeff_index += 2;
      }

      if (!term) {
        ast_free(numer_ast);
        ast_free(denom_ast);
        ast_free(quotient_integral);
        ast_free(proper_integral);
        return NULL;
      }

      if (!proper_integral)
        proper_integral = term;
      else
        proper_integral =
            sym_full_simplify(ast_binop(OP_ADD, proper_integral, term));
    }
  }

  if (quotient_integral && proper_integral)
    result = sym_full_simplify(
        ast_binop(OP_ADD, quotient_integral, proper_integral));
  else if (quotient_integral)
    result = quotient_integral;
  else
    result = proper_integral;

  ast_free(numer_ast);
  ast_free(denom_ast);
  return result;
}

static int liate_priority(const AstNode *expr, const char *var) {
  if (!expr || !sym_contains_var(expr, var))
    return -1;

  if (is_call1(expr, "ln"))
    return 50;
  if (is_call1(expr, "asin") || is_call1(expr, "acos") ||
      is_call1(expr, "atan"))
    return 40;
  if (is_var(expr, var))
    return 30;
  if (expr->type == AST_BINOP && expr->as.binop.op == OP_POW &&
      is_var(expr->as.binop.left, var) &&
      !sym_contains_var(expr->as.binop.right, var))
    return 30;
  if (is_call1(expr, "sin") || is_call1(expr, "cos") || is_call1(expr, "tan"))
    return 20;
  if (is_call1(expr, "exp") || is_call1(expr, "sinh") ||
      is_call1(expr, "cosh") || is_call1(expr, "tanh"))
    return 10;
  return 0;
}

static int extract_root_multiple(const AstNode *expr, const AstNode *root,
                                 AstNode **coeff_out) {
  Complex expr_coeff = c_real(1.0);
  Complex root_coeff = c_real(1.0);
  AstNode *expr_base = NULL;
  AstNode *root_base = NULL;
  int ok = 0;

  *coeff_out = NULL;

  if (!sym_extract_coeff_and_base(expr, &expr_coeff, &expr_base) ||
      !sym_extract_coeff_and_base(root, &root_coeff, &root_base))
    goto cleanup;

  if (c_is_zero(root_coeff))
    goto cleanup;

  if ((!expr_base && !root_base) ||
      (expr_base && root_base && ast_equal(expr_base, root_base))) {
    *coeff_out = ast_number_complex(c_div(expr_coeff, root_coeff));
    ok = 1;
  }

cleanup:
  ast_free(expr_base);
  ast_free(root_base);
  return ok;
}

static AstNode *scale_affine_term(const AstNode *factor, AstNode *term) {
  AstNode *scaled = NULL;

  if (!term)
    return NULL;
  if (is_zero_node(term)) {
    ast_free(term);
    return ast_number(0);
  }

  scaled = sym_full_simplify(ast_binop(OP_MUL, ast_clone(factor), term));
  return scaled;
}

static AstNode *negate_additive_expr(const AstNode *expr) {
  if (!expr)
    return NULL;

  if (expr->type == AST_UNARY_NEG)
    return ast_clone(expr->as.unary.operand);

  if (expr->type == AST_BINOP && expr->as.binop.op == OP_ADD) {
    AstNode *left = negate_additive_expr(expr->as.binop.left);
    AstNode *right = negate_additive_expr(expr->as.binop.right);
    return sym_full_simplify(ast_binop(OP_ADD, left, right));
  }

  if (expr->type == AST_BINOP && expr->as.binop.op == OP_SUB) {
    AstNode *left = negate_additive_expr(expr->as.binop.left);
    AstNode *right = ast_clone(expr->as.binop.right);
    return sym_full_simplify(ast_binop(OP_ADD, left, right));
  }

  return sym_full_simplify(ast_unary_neg(ast_clone(expr)));
}

static IntegralAffineResult sym_integrate_affine(const AstNode *expr,
                                                 const char *var,
                                                 const AstNode *root, int depth,
                                                 int detect_root_loop);

static IntegralAffineResult integrate_by_parts(const AstNode *expr,
                                               const char *var,
                                               const AstNode *root, int depth) {
  IntegralAffineResult result = integral_affine_fail();
  NodeList factors = {0};
  unsigned char *used = NULL;
  int best_priority = -1;
  size_t best_index = 0;
  int found = 0;

  if (depth >= SYM_INTEGRATE_MAX_DEPTH)
    return result;

  if (expr->type == AST_BINOP && expr->as.binop.op == OP_MUL) {
    flatten_product_terms(expr, &factors);
  } else if (liate_priority(expr, var) > 0) {
    if (!node_list_push(&factors, ast_clone(expr)))
      return result;
  } else {
    return result;
  }

  used = calloc(factors.count ? factors.count : 1, sizeof(unsigned char));
  if (!used) {
    node_list_free(&factors);
    return result;
  }

  while (1) {
    IntegralAffineResult right = integral_affine_fail();
    IntegralAffineResult v = integral_affine_fail();
    AstNode *u = NULL;
    AstNode *dv = NULL;
    AstNode *du = NULL;
    AstNode *uv = NULL;
    AstNode *right_integrand = NULL;
    AstNode *neg_coeff = NULL;

    best_priority = -1;
    found = 0;

    for (size_t i = 0; i < factors.count; i++) {
      int priority = liate_priority(factors.items[i], var);
      if (used[i])
        continue;
      if (priority > best_priority) {
        best_priority = priority;
        best_index = i;
        found = 1;
      }
    }

    if (!found || best_priority < 0)
      break;

    used[best_index] = 1;
    u = ast_clone(factors.items[best_index]);
    dv = build_product_excluding(&factors, best_index);
    dv = sym_full_simplify(dv);
    if (!u || !dv) {
      ast_free(u);
      ast_free(dv);
      continue;
    }

    v = sym_integrate_affine(dv, var, root, depth + 1, 0);
    if (!integral_affine_ok(&v) || !is_zero_node(v.root_coeff)) {
      integral_affine_free(&v);
      ast_free(u);
      ast_free(dv);
      continue;
    }

    du = sym_diff(u, var);
    if (!du) {
      integral_affine_free(&v);
      ast_free(u);
      ast_free(dv);
      continue;
    }
    du = sym_full_simplify(du);
    uv = sym_full_simplify(ast_binop(OP_MUL, ast_clone(u), ast_clone(v.term)));
    right_integrand =
        sym_full_simplify(ast_binop(OP_MUL, ast_clone(v.term), du));
    right = sym_integrate_affine(right_integrand, var, root, depth + 1, 1);
    ast_free(right_integrand);
    ast_free(dv);
    ast_free(u);
    if (!integral_affine_ok(&right)) {
      integral_affine_free(&right);
      integral_affine_free(&v);
      ast_free(uv);
      continue;
    }

    neg_coeff = sym_full_simplify(ast_unary_neg(right.root_coeff));
    right.root_coeff = NULL;
    result.term = sym_full_simplify(
        ast_binop(OP_ADD, uv, negate_additive_expr(right.term)));
    right.term = NULL;
    result.root_coeff = neg_coeff;

    integral_affine_free(&right);
    integral_affine_free(&v);
    node_list_free(&factors);
    free(used);
    return result;
  }

  node_list_free(&factors);
  free(used);
  return integral_affine_fail();
}

static AstNode *integrate_monomial(const AstNode *expr, const char *var) {
  /* constant -> c*x */
  if (!sym_contains_var(expr, var)) {
    return ast_binop(OP_MUL, ast_clone(expr), ast_variable(var, strlen(var)));
  }

  /* x -> x^2/2 */
  if (is_var(expr, var)) {
    return ast_binop(
        OP_DIV,
        ast_binop(OP_POW, ast_variable(var, strlen(var)), ast_number(2)),
        ast_number(2));
  }

  /* x^n -> x^(n+1)/(n+1) */
  if (expr->type == AST_BINOP && expr->as.binop.op == OP_POW &&
      is_var(expr->as.binop.left, var) && is_num(expr->as.binop.right) &&
      c_is_real(expr->as.binop.right->as.number)) {
    double n = expr->as.binop.right->as.number.re;
    if (n == -1.0) {
      return ast_func_call("ln", 2,
                           (AstNode *[]){ast_variable(var, strlen(var))}, 1);
    }
    double n1 = n + 1.0;
    return ast_binop(
        OP_DIV,
        ast_binop(OP_POW, ast_variable(var, strlen(var)), ast_number(n1)),
        ast_number(n1));
  }

  /* sqrt(x) -> x^(3/2)/(3/2) */
  if (is_call1(expr, "sqrt") && is_var(expr->as.call.args[0], var)) {
    return ast_binop(
        OP_DIV,
        ast_binop(OP_POW, ast_variable(var, strlen(var)), ast_rational(3, 2)),
        ast_rational(3, 2));
  }

  return NULL;
}

static AstNode *sym_integrate_direct(const AstNode *expr, const char *var) {
  AstNode *result = integrate_trig(expr, var);
  if (result)
    return sym_simplify(result);

  result = integrate_monomial(expr, var);
  if (result)
    return sym_simplify(result);

  return NULL;
}

static IntegralAffineResult sym_integrate_affine(const AstNode *expr,
                                                 const char *var,
                                                 const AstNode *root, int depth,
                                                 int detect_root_loop) {
  IntegralAffineResult result = integral_affine_fail();
  AstNode *direct = NULL;
  AstNode *root_multiple = NULL;

  if (!expr)
    return result;
  if (depth > SYM_INTEGRATE_MAX_DEPTH)
    return result;

  if (detect_root_loop && depth > 0 &&
      extract_root_multiple(expr, root, &root_multiple))
    return integral_affine_root_coeff(root_multiple);

  /* linearity: int(a +/- b) = int(a) +/- int(b) */
  if (expr->type == AST_BINOP &&
      (expr->as.binop.op == OP_ADD || expr->as.binop.op == OP_SUB)) {
    IntegralAffineResult left = sym_integrate_affine(
        expr->as.binop.left, var, root, depth + 1, detect_root_loop);
    IntegralAffineResult right = sym_integrate_affine(
        expr->as.binop.right, var, root, depth + 1, detect_root_loop);
    if (!integral_affine_ok(&left) || !integral_affine_ok(&right)) {
      integral_affine_free(&left);
      integral_affine_free(&right);
      return result;
    }

    result.term =
        sym_full_simplify(ast_binop(expr->as.binop.op, left.term, right.term));
    left.term = NULL;
    right.term = NULL;
    result.root_coeff = sym_full_simplify(
        ast_binop(expr->as.binop.op, left.root_coeff, right.root_coeff));
    left.root_coeff = NULL;
    right.root_coeff = NULL;

    integral_affine_free(&left);
    integral_affine_free(&right);
    return result;
  }

  /* negation */
  if (expr->type == AST_UNARY_NEG) {
    IntegralAffineResult inner = sym_integrate_affine(
        expr->as.unary.operand, var, root, depth + 1, detect_root_loop);
    if (!integral_affine_ok(&inner))
      return result;

    result.term = sym_full_simplify(ast_unary_neg(inner.term));
    inner.term = NULL;
    result.root_coeff = sym_full_simplify(ast_unary_neg(inner.root_coeff));
    inner.root_coeff = NULL;
    integral_affine_free(&inner);
    return result;
  }

  /* c*f(x) -> c * int(f(x)) after global product normalization */
  if (expr->type == AST_BINOP && expr->as.binop.op == OP_MUL) {
    if (!sym_contains_var(expr->as.binop.left, var)) {
      IntegralAffineResult inner = sym_integrate_affine(
          expr->as.binop.right, var, root, depth + 1, detect_root_loop);
      if (integral_affine_ok(&inner)) {
        result.term = scale_affine_term(expr->as.binop.left, inner.term);
        inner.term = NULL;
        result.root_coeff =
            scale_affine_term(expr->as.binop.left, inner.root_coeff);
        inner.root_coeff = NULL;
        integral_affine_free(&inner);
        return result;
      }
      integral_affine_free(&inner);
    }
    if (!sym_contains_var(expr->as.binop.right, var)) {
      IntegralAffineResult inner = sym_integrate_affine(
          expr->as.binop.left, var, root, depth + 1, detect_root_loop);
      if (integral_affine_ok(&inner)) {
        result.term = scale_affine_term(expr->as.binop.right, inner.term);
        inner.term = NULL;
        result.root_coeff =
            scale_affine_term(expr->as.binop.right, inner.root_coeff);
        inner.root_coeff = NULL;
        integral_affine_free(&inner);
        return result;
      }
      integral_affine_free(&inner);
    }
  }

  direct = sym_integrate_direct(expr, var);
  if (direct)
    return integral_affine_term(direct);

  direct = integrate_partial_fractions(expr, var);
  if (direct)
    return integral_affine_term(direct);

  return integrate_by_parts(expr, var, root, depth);
}

AstNode *sym_integrate(const AstNode *expr, const char *var) {
  IntegralAffineResult affine = integral_affine_fail();
  AstNode *normalized = NULL;
  AstNode *result = NULL;
  AstNode *denominator = NULL;

  if (!expr)
    return NULL;

  normalized = sym_full_simplify(ast_clone(expr));
  if (!normalized)
    return NULL;

  affine = sym_integrate_affine(normalized, var, normalized, 0, 0);
  if (!integral_affine_ok(&affine)) {
    ast_free(normalized);
    return NULL;
  }

  if (is_zero_node(affine.root_coeff)) {
    result = affine.term;
    affine.term = NULL;
  } else {
    denominator =
        sym_full_simplify(ast_binop(OP_SUB, ast_number(1), affine.root_coeff));
    affine.root_coeff = NULL;
    if (!denominator || is_zero_node(denominator)) {
      ast_free(denominator);
      integral_affine_free(&affine);
      ast_free(normalized);
      return NULL;
    }
    result = sym_full_simplify(ast_binop(OP_DIV, affine.term, denominator));
    affine.term = NULL;
  }

  integral_affine_free(&affine);
  ast_free(normalized);
  if (!result)
    return NULL;

  result = ast_canonicalize(result);
  result = sym_collect_terms(result);
  return sym_full_simplify(result);
}
