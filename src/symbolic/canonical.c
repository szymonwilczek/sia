#include "sia/canonical.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int node_sort_key(const AstNode *n) {
  switch (n->type) {
  case AST_NUMBER:
    return 0;
  case AST_VARIABLE:
    return 1;
  case AST_UNARY_NEG:
    return 2;
  case AST_BINOP:
    return 3;
  case AST_FUNC_CALL:
    return 4;
  case AST_LIMIT:
    return 5;
  case AST_MATRIX:
    return 6;
  case AST_EQ:
    return 7;
  }
  return 7;
}

static int node_compare(const AstNode *a, const AstNode *b) {
  int ka = node_sort_key(a);
  int kb = node_sort_key(b);
  if (ka != kb)
    return ka - kb;

  switch (a->type) {
  case AST_NUMBER:
    if (a->as.number.re < b->as.number.re)
      return -1;
    if (a->as.number.re > b->as.number.re)
      return 1;
    if (a->as.number.im < b->as.number.im)
      return -1;
    if (a->as.number.im > b->as.number.im)
      return 1;
    return 0;
  case AST_VARIABLE:
    return strcmp(a->as.variable, b->as.variable);
  case AST_FUNC_CALL: {
    int c = strcmp(a->as.call.name, b->as.call.name);
    if (c != 0)
      return c;
    if (a->as.call.nargs != b->as.call.nargs)
      return (int)a->as.call.nargs - (int)b->as.call.nargs;
    for (size_t i = 0; i < a->as.call.nargs; i++) {
      c = node_compare(a->as.call.args[i], b->as.call.args[i]);
      if (c != 0)
        return c;
    }
    return 0;
  }
  case AST_LIMIT: {
    int c = strcmp(a->as.limit.var, b->as.limit.var);
    if (c != 0)
      return c;
    c = node_compare(a->as.limit.target, b->as.limit.target);
    if (c != 0)
      return c;
    return node_compare(a->as.limit.expr, b->as.limit.expr);
  }
  case AST_UNARY_NEG:
    return node_compare(a->as.unary.operand, b->as.unary.operand);
  case AST_BINOP: {
    if (a->as.binop.op != b->as.binop.op)
      return (int)a->as.binop.op - (int)b->as.binop.op;
    int c = node_compare(a->as.binop.left, b->as.binop.left);
    if (c != 0)
      return c;
    return node_compare(a->as.binop.right, b->as.binop.right);
  }
  case AST_EQ: {
    int c = node_compare(a->as.eq.lhs, b->as.eq.lhs);
    if (c != 0)
      return c;
    return node_compare(a->as.eq.rhs, b->as.eq.rhs);
  }
  case AST_MATRIX: {
    size_t n = (a->as.matrix.rows < b->as.matrix.rows)   ? -1
               : (a->as.matrix.rows > b->as.matrix.rows) ? 1
                                                         : 0;
    if (n)
      return (int)n;
    n = (a->as.matrix.cols < b->as.matrix.cols)   ? -1
        : (a->as.matrix.cols > b->as.matrix.cols) ? 1
                                                  : 0;
    if (n)
      return (int)n;
    for (size_t i = 0; i < a->as.matrix.rows * a->as.matrix.cols; i++) {
      int c = node_compare(a->as.matrix.elements[i], b->as.matrix.elements[i]);
      if (c != 0)
        return c;
    }
    return 0;
  }
  }
  return 0;
}

static void sort_commutative(AstNode *node) {
  if (!node || node->type != AST_BINOP)
    return;
  if (node->as.binop.op != OP_ADD && node->as.binop.op != OP_MUL)
    return;

  if (node_compare(node->as.binop.left, node->as.binop.right) > 0) {
    AstNode *tmp = node->as.binop.left;
    node->as.binop.left = node->as.binop.right;
    node->as.binop.right = tmp;
  }
}

AstNode *ast_canonicalize(AstNode *node) {
  if (!node)
    return NULL;

  switch (node->type) {
  case AST_NUMBER:
  case AST_VARIABLE:
    return node;

  case AST_MATRIX: {
    size_t total = node->as.matrix.rows * node->as.matrix.cols;
    for (size_t i = 0; i < total; i++)
      node->as.matrix.elements[i] =
          ast_canonicalize(node->as.matrix.elements[i]);
    return node;
  }

  case AST_UNARY_NEG:
    node->as.unary.operand = ast_canonicalize(node->as.unary.operand);
    return node;

  case AST_FUNC_CALL:
    for (size_t i = 0; i < node->as.call.nargs; i++)
      node->as.call.args[i] = ast_canonicalize(node->as.call.args[i]);
    return node;

  case AST_LIMIT:
    node->as.limit.expr = ast_canonicalize(node->as.limit.expr);
    node->as.limit.target = ast_canonicalize(node->as.limit.target);
    return node;

  case AST_EQ:
    node->as.eq.lhs = ast_canonicalize(node->as.eq.lhs);
    node->as.eq.rhs = ast_canonicalize(node->as.eq.rhs);
    return node;

  case AST_BINOP:
    break;
  }

  /* A - B  =>  A + (-1 * B) */
  if (node->as.binop.op == OP_SUB) {
    node->as.binop.op = OP_ADD;
    node->as.binop.right =
        ast_binop(OP_MUL, ast_number(-1), node->as.binop.right);
  }

  /* A / B  =>  A * B^(-1) */
  if (node->as.binop.op == OP_DIV) {
    node->as.binop.op = OP_MUL;
    node->as.binop.right =
        ast_binop(OP_POW, node->as.binop.right, ast_number(-1));
  }

  node->as.binop.left = ast_canonicalize(node->as.binop.left);
  node->as.binop.right = ast_canonicalize(node->as.binop.right);

  sort_commutative(node);

  return node;
}

typedef struct {
  char *var;
  int degree;
} CanonicalDegree;

typedef struct {
  Complex coeff;
  CanonicalDegree *degrees;
  size_t degree_count;
  size_t degree_cap;
} CanonicalTerm;

typedef struct {
  CanonicalTerm *terms;
  size_t count;
  size_t cap;
} CanonicalPolynomial;

static void canonical_term_free(CanonicalTerm *term) {
  if (!term)
    return;
  for (size_t i = 0; i < term->degree_count; i++)
    free(term->degrees[i].var);
  free(term->degrees);
  term->degrees = NULL;
  term->degree_count = 0;
  term->degree_cap = 0;
}

static void canonical_polynomial_free(CanonicalPolynomial *poly) {
  if (!poly)
    return;
  for (size_t i = 0; i < poly->count; i++)
    canonical_term_free(&poly->terms[i]);
  free(poly->terms);
  poly->terms = NULL;
  poly->count = 0;
  poly->cap = 0;
}

static int canonical_nonnegative_int(const AstNode *node, int *out) {
  double rounded = 0.0;

  if (!node || node->type != AST_NUMBER || !c_is_real(node->as.number))
    return 0;

  rounded = round(node->as.number.re);
  if (fabs(node->as.number.re - rounded) > 1e-9 || rounded < 0.0)
    return 0;

  if (out)
    *out = (int)rounded;
  return 1;
}

static int canonical_term_add_degree(CanonicalTerm *term, const char *var,
                                     int degree) {
  size_t pos = 0;

  if (degree == 0)
    return 1;

  while (pos < term->degree_count && strcmp(term->degrees[pos].var, var) < 0)
    pos++;

  if (pos < term->degree_count && strcmp(term->degrees[pos].var, var) == 0) {
    term->degrees[pos].degree += degree;
    return 1;
  }

  if (term->degree_count == term->degree_cap) {
    size_t new_cap = term->degree_cap ? term->degree_cap * 2 : 4;
    CanonicalDegree *degrees =
        realloc(term->degrees, new_cap * sizeof(CanonicalDegree));
    if (!degrees)
      return 0;
    term->degrees = degrees;
    term->degree_cap = new_cap;
  }

  for (size_t i = term->degree_count; i > pos; i--)
    term->degrees[i] = term->degrees[i - 1];

  term->degrees[pos].var = strdup(var);
  if (!term->degrees[pos].var)
    return 0;
  term->degrees[pos].degree = degree;
  term->degree_count++;
  return 1;
}

static int canonical_term_copy(CanonicalTerm *dst, const CanonicalTerm *src) {
  dst->coeff = src->coeff;
  dst->degrees = NULL;
  dst->degree_count = 0;
  dst->degree_cap = 0;

  for (size_t i = 0; i < src->degree_count; i++) {
    if (!canonical_term_add_degree(dst, src->degrees[i].var,
                                   src->degrees[i].degree)) {
      canonical_term_free(dst);
      return 0;
    }
  }

  return 1;
}

static int canonical_poly_append_owned(CanonicalPolynomial *poly,
                                       CanonicalTerm *term) {
  if (poly->count == poly->cap) {
    size_t new_cap = poly->cap ? poly->cap * 2 : 4;
    CanonicalTerm *terms =
        realloc(poly->terms, new_cap * sizeof(CanonicalTerm));
    if (!terms)
      return 0;
    poly->terms = terms;
    poly->cap = new_cap;
  }

  poly->terms[poly->count++] = *term;
  term->degrees = NULL;
  term->degree_count = 0;
  term->degree_cap = 0;
  return 1;
}

static int canonical_terms_equal(const CanonicalTerm *a,
                                 const CanonicalTerm *b) {
  if (a->degree_count != b->degree_count)
    return 0;
  for (size_t i = 0; i < a->degree_count; i++) {
    if (strcmp(a->degrees[i].var, b->degrees[i].var) != 0 ||
        a->degrees[i].degree != b->degrees[i].degree)
      return 0;
  }
  return 1;
}

static int canonical_total_degree(const CanonicalTerm *term) {
  int total = 0;
  for (size_t i = 0; i < term->degree_count; i++)
    total += term->degrees[i].degree;
  return total;
}

static int canonical_term_compare(const void *lhs, const void *rhs) {
  const CanonicalTerm *a = lhs;
  const CanonicalTerm *b = rhs;
  int total_a = canonical_total_degree(a);
  int total_b = canonical_total_degree(b);

  if (total_a != total_b)
    return total_a - total_b;

  for (size_t i = 0; i < a->degree_count && i < b->degree_count; i++) {
    int cmp = strcmp(a->degrees[i].var, b->degrees[i].var);
    if (cmp != 0)
      return cmp;
    if (a->degrees[i].degree != b->degrees[i].degree)
      return b->degrees[i].degree - a->degrees[i].degree;
  }

  if (a->degree_count < b->degree_count)
    return -1;
  if (a->degree_count > b->degree_count)
    return 1;
  return 0;
}

static void canonical_poly_normalize(CanonicalPolynomial *poly) {
  size_t out = 0;

  for (size_t i = 0; i < poly->count; i++) {
    if (c_is_zero(poly->terms[i].coeff)) {
      canonical_term_free(&poly->terms[i]);
      continue;
    }

    size_t write = out++;
    if (write != i)
      poly->terms[write] = poly->terms[i];
  }
  poly->count = out;

  for (size_t i = 0; i < poly->count; i++) {
    for (size_t j = i + 1; j < poly->count; j++) {
      if (!canonical_terms_equal(&poly->terms[i], &poly->terms[j]))
        continue;
      poly->terms[i].coeff = c_add(poly->terms[i].coeff, poly->terms[j].coeff);
      canonical_term_free(&poly->terms[j]);
      poly->terms[j].coeff = c_real(0.0);
    }
  }

  out = 0;
  for (size_t i = 0; i < poly->count; i++) {
    if (c_is_zero(poly->terms[i].coeff)) {
      canonical_term_free(&poly->terms[i]);
      continue;
    }

    size_t write = out++;
    if (write != i)
      poly->terms[write] = poly->terms[i];
  }
  poly->count = out;

  qsort(poly->terms, poly->count, sizeof(CanonicalTerm),
        canonical_term_compare);
}

static int canonical_poly_constant(CanonicalPolynomial *poly, Complex coeff) {
  CanonicalTerm term = {0};
  term.coeff = coeff;
  return canonical_poly_append_owned(poly, &term);
}

static int canonical_poly_variable(CanonicalPolynomial *poly,
                                   const char *name) {
  CanonicalTerm term = {0};
  term.coeff = c_real(1.0);
  if (!canonical_term_add_degree(&term, name, 1)) {
    canonical_term_free(&term);
    return 0;
  }
  return canonical_poly_append_owned(poly, &term);
}

static int canonical_poly_multiply_terms(CanonicalTerm *dst,
                                         const CanonicalTerm *lhs,
                                         const CanonicalTerm *rhs) {
  dst->coeff = c_mul(lhs->coeff, rhs->coeff);
  dst->degrees = NULL;
  dst->degree_count = 0;
  dst->degree_cap = 0;

  for (size_t i = 0; i < lhs->degree_count; i++) {
    if (!canonical_term_add_degree(dst, lhs->degrees[i].var,
                                   lhs->degrees[i].degree)) {
      canonical_term_free(dst);
      return 0;
    }
  }

  for (size_t i = 0; i < rhs->degree_count; i++) {
    if (!canonical_term_add_degree(dst, rhs->degrees[i].var,
                                   rhs->degrees[i].degree)) {
      canonical_term_free(dst);
      return 0;
    }
  }

  return 1;
}

static int canonical_poly_from_ast(const AstNode *node,
                                   CanonicalPolynomial *out);

static int canonical_poly_pow(const CanonicalPolynomial *base, int exp,
                              CanonicalPolynomial *out) {
  CanonicalPolynomial current = {0};

  if (exp == 0)
    return canonical_poly_constant(out, c_real(1.0));

  for (size_t i = 0; i < base->count; i++) {
    CanonicalTerm copy = {0};
    if (!canonical_term_copy(&copy, &base->terms[i]) ||
        !canonical_poly_append_owned(&current, &copy)) {
      canonical_term_free(&copy);
      canonical_polynomial_free(&current);
      return 0;
    }
  }

  for (int i = 1; i < exp; i++) {
    CanonicalPolynomial next = {0};
    for (size_t l = 0; l < current.count; l++) {
      for (size_t r = 0; r < base->count; r++) {
        CanonicalTerm product = {0};
        if (!canonical_poly_multiply_terms(&product, &current.terms[l],
                                           &base->terms[r]) ||
            !canonical_poly_append_owned(&next, &product)) {
          canonical_term_free(&product);
          canonical_polynomial_free(&next);
          canonical_polynomial_free(&current);
          return 0;
        }
      }
    }
    canonical_poly_normalize(&next);
    canonical_polynomial_free(&current);
    current = next;
  }

  *out = current;
  return 1;
}

static int canonical_poly_from_ast(const AstNode *node,
                                   CanonicalPolynomial *out) {
  if (!node)
    return 0;

  switch (node->type) {
  case AST_NUMBER:
    return canonical_poly_constant(out, node->as.number);
  case AST_VARIABLE:
    return canonical_poly_variable(out, node->as.variable);
  case AST_UNARY_NEG: {
    if (!canonical_poly_from_ast(node->as.unary.operand, out))
      return 0;
    for (size_t i = 0; i < out->count; i++)
      out->terms[i].coeff = c_neg(out->terms[i].coeff);
    return 1;
  }
  case AST_BINOP:
    break;
  default:
    return 0;
  }

  if (node->as.binop.op == OP_ADD || node->as.binop.op == OP_SUB) {
    CanonicalPolynomial lhs = {0};
    CanonicalPolynomial rhs = {0};

    if (!canonical_poly_from_ast(node->as.binop.left, &lhs) ||
        !canonical_poly_from_ast(node->as.binop.right, &rhs)) {
      canonical_polynomial_free(&lhs);
      canonical_polynomial_free(&rhs);
      return 0;
    }

    if (node->as.binop.op == OP_SUB) {
      for (size_t i = 0; i < rhs.count; i++)
        rhs.terms[i].coeff = c_neg(rhs.terms[i].coeff);
    }

    for (size_t i = 0; i < lhs.count; i++) {
      CanonicalTerm copy = {0};
      if (!canonical_term_copy(&copy, &lhs.terms[i]) ||
          !canonical_poly_append_owned(out, &copy)) {
        canonical_term_free(&copy);
        canonical_polynomial_free(&lhs);
        canonical_polynomial_free(&rhs);
        canonical_polynomial_free(out);
        return 0;
      }
    }
    for (size_t i = 0; i < rhs.count; i++) {
      CanonicalTerm copy = {0};
      if (!canonical_term_copy(&copy, &rhs.terms[i]) ||
          !canonical_poly_append_owned(out, &copy)) {
        canonical_term_free(&copy);
        canonical_polynomial_free(&lhs);
        canonical_polynomial_free(&rhs);
        canonical_polynomial_free(out);
        return 0;
      }
    }

    canonical_polynomial_free(&lhs);
    canonical_polynomial_free(&rhs);
    canonical_poly_normalize(out);
    return 1;
  }

  if (node->as.binop.op == OP_MUL) {
    CanonicalPolynomial lhs = {0};
    CanonicalPolynomial rhs = {0};

    if (!canonical_poly_from_ast(node->as.binop.left, &lhs) ||
        !canonical_poly_from_ast(node->as.binop.right, &rhs)) {
      canonical_polynomial_free(&lhs);
      canonical_polynomial_free(&rhs);
      return 0;
    }

    for (size_t l = 0; l < lhs.count; l++) {
      for (size_t r = 0; r < rhs.count; r++) {
        CanonicalTerm product = {0};
        if (!canonical_poly_multiply_terms(&product, &lhs.terms[l],
                                           &rhs.terms[r]) ||
            !canonical_poly_append_owned(out, &product)) {
          canonical_term_free(&product);
          canonical_polynomial_free(&lhs);
          canonical_polynomial_free(&rhs);
          canonical_polynomial_free(out);
          return 0;
        }
      }
    }

    canonical_polynomial_free(&lhs);
    canonical_polynomial_free(&rhs);
    canonical_poly_normalize(out);
    return 1;
  }

  if (node->as.binop.op == OP_POW) {
    CanonicalPolynomial base = {0};
    int exp = 0;

    if (!canonical_nonnegative_int(node->as.binop.right, &exp) ||
        !canonical_poly_from_ast(node->as.binop.left, &base)) {
      canonical_polynomial_free(&base);
      return 0;
    }

    if (!canonical_poly_pow(&base, exp, out)) {
      canonical_polynomial_free(&base);
      return 0;
    }

    canonical_polynomial_free(&base);
    canonical_poly_normalize(out);
    return 1;
  }

  return 0;
}

static AstNode *canonical_term_to_ast(const CanonicalTerm *term) {
  AstNode *result = NULL;
  int has_vars = 0;

  for (size_t i = 0; i < term->degree_count; i++) {
    if (term->degrees[i].degree == 0)
      continue;
    has_vars = 1;
  }

  if (!c_is_one(term->coeff) || !has_vars)
    result = ast_number_complex(term->coeff);

  for (size_t i = 0; i < term->degree_count; i++) {
    if (term->degrees[i].degree == 0)
      continue;
    AstNode *factor =
        ast_variable(term->degrees[i].var, strlen(term->degrees[i].var));
    if (term->degrees[i].degree != 1) {
      factor = ast_binop(OP_POW, factor,
                         ast_number((double)term->degrees[i].degree));
    }

    if (!result)
      result = factor;
    else
      result = ast_binop(OP_MUL, result, factor);
  }

  if (!result)
    return ast_number(0);
  return result;
}

AstNode *ast_polynomial_canonicalize(const AstNode *node) {
  CanonicalPolynomial poly = {0};
  AstNode *result = NULL;

  if (!node || !canonical_poly_from_ast(node, &poly)) {
    canonical_polynomial_free(&poly);
    return NULL;
  }

  canonical_poly_normalize(&poly);
  if (poly.count == 0) {
    canonical_polynomial_free(&poly);
    return ast_number(0);
  }

  for (size_t i = 0; i < poly.count; i++) {
    AstNode *term = canonical_term_to_ast(&poly.terms[i]);
    if (!result)
      result = term;
    else
      result = ast_binop(OP_ADD, result, term);
  }

  canonical_polynomial_free(&poly);
  return result;
}

static int poly_is_univariate(const CanonicalPolynomial *p, const char **var) {
  *var = NULL;
  for (size_t i = 0; i < p->count; i++) {
    if (p->terms[i].degree_count > 1)
      return 0;
    if (p->terms[i].degree_count == 1) {
      if (!*var)
        *var = p->terms[i].degrees[0].var;
      else if (strcmp(*var, p->terms[i].degrees[0].var) != 0)
        return 0;
    }
  }
  if (!*var)
    *var = "x";
  return 1;
}

static int poly_degree(const CanonicalPolynomial *p, const char *var) {
  int deg = 0;
  for (size_t i = 0; i < p->count; i++) {
    for (size_t j = 0; j < p->terms[i].degree_count; j++) {
      if (strcmp(p->terms[i].degrees[j].var, var) == 0 &&
          p->terms[i].degrees[j].degree > deg)
        deg = p->terms[i].degrees[j].degree;
    }
  }
  return deg;
}

static Complex poly_get_coeff(const CanonicalPolynomial *p, const char *var,
                              int degree) {
  for (size_t i = 0; i < p->count; i++) {
    int term_deg = 0;
    for (size_t j = 0; j < p->terms[i].degree_count; j++) {
      if (strcmp(p->terms[i].degrees[j].var, var) == 0)
        term_deg = p->terms[i].degrees[j].degree;
    }
    if (term_deg == degree && p->terms[i].degree_count <= 1)
      return p->terms[i].coeff;
  }
  return c_real(0.0);
}

static int coeffs_from_poly(const CanonicalPolynomial *p, const char *var,
                            Complex **out, int *deg) {
  *deg = poly_degree(p, var);
  *out = calloc((size_t)(*deg + 1), sizeof(Complex));
  if (!*out)
    return 0;
  for (int i = 0; i <= *deg; i++)
    (*out)[i] = poly_get_coeff(p, var, i);
  return 1;
}

static int poly_from_coeffs(const Complex *coeffs, int deg, const char *var,
                            CanonicalPolynomial *out) {
  *out = (CanonicalPolynomial){0};
  for (int i = 0; i <= deg; i++) {
    if (c_is_zero(coeffs[i]))
      continue;
    CanonicalTerm term = {0};
    term.coeff = coeffs[i];
    if (i > 0 && !canonical_term_add_degree(&term, var, i)) {
      canonical_term_free(&term);
      canonical_polynomial_free(out);
      return 0;
    }
    if (!canonical_poly_append_owned(out, &term)) {
      canonical_term_free(&term);
      canonical_polynomial_free(out);
      return 0;
    }
  }
  canonical_poly_normalize(out);
  return 1;
}

static int actual_degree(const Complex *c, int deg) {
  while (deg > 0 && c_is_zero(c[deg]))
    deg--;
  return deg;
}

static void poly_div_rem(const Complex *a, int deg_a, const Complex *b,
                         int deg_b, Complex **quot, int *deg_q, Complex **rem,
                         int *deg_r) {
  int i;
  Complex *r = malloc((size_t)(deg_a + 1) * sizeof(Complex));
  Complex *q = calloc((size_t)(deg_a - deg_b + 1), sizeof(Complex));

  for (i = 0; i <= deg_a; i++)
    r[i] = a[i];

  *deg_q = deg_a - deg_b;

  for (i = deg_a - deg_b; i >= 0; i--) {
    q[i] = c_div(r[i + deg_b], b[deg_b]);
    for (int j = 0; j <= deg_b; j++) {
      r[i + j] = c_sub(r[i + j], c_mul(q[i], b[j]));
    }
  }

  *quot = q;
  *rem = r;
  *deg_r = actual_degree(r, deg_b - 1);
}

static void poly_gcd_coeffs(const Complex *a, int deg_a, const Complex *b,
                            int deg_b, Complex **gcd_out, int *deg_gcd) {
  Complex *p = malloc((size_t)(deg_a + 1) * sizeof(Complex));
  Complex *q = malloc((size_t)(deg_b + 1) * sizeof(Complex));
  int dp, dq;

  for (int i = 0; i <= deg_a; i++)
    p[i] = a[i];
  for (int i = 0; i <= deg_b; i++)
    q[i] = b[i];
  dp = actual_degree(p, deg_a);
  dq = actual_degree(q, deg_b);

  while (dq > 0 || (dq == 0 && !c_is_zero(q[0]))) {
    Complex *quot = NULL, *rem = NULL;
    int dquot, drem;
    if (dp < dq) {
      Complex *tmp = p;
      p = q;
      q = tmp;
      int td = dp;
      dp = dq;
      dq = td;
    }
    poly_div_rem(p, dp, q, dq, &quot, &dquot, &rem, &drem);
    free(quot);
    free(p);
    p = q;
    dp = dq;
    q = rem;
    dq = drem;
  }

  /* make monic */
  if (dp > 0 || !c_is_zero(p[0])) {
    Complex lc = p[dp];
    if (!c_is_one(lc)) {
      for (int i = 0; i <= dp; i++)
        p[i] = c_div(p[i], lc);
    }
  }

  free(q);
  *gcd_out = p;
  *deg_gcd = dp;
}

static int canonical_poly_exact_div(const CanonicalPolynomial *numer,
                                    const CanonicalPolynomial *denom,
                                    const char *var,
                                    CanonicalPolynomial *quotient) {
  CanonicalPolynomial rem = {0};
  int deg_n, deg_d;

  for (size_t i = 0; i < numer->count; i++) {
    CanonicalTerm copy = {0};
    if (!canonical_term_copy(&copy, &numer->terms[i]) ||
        !canonical_poly_append_owned(&rem, &copy)) {
      canonical_term_free(&copy);
      canonical_polynomial_free(&rem);
      return 0;
    }
  }
  canonical_poly_normalize(&rem);

  deg_d = poly_degree(denom, var);
  if (deg_d == 0 && denom->count == 1 && !c_is_zero(denom->terms[0].coeff)) {
    Complex inv = c_div(c_real(1.0), denom->terms[0].coeff);
    for (size_t i = 0; i < rem.count; i++)
      rem.terms[i].coeff = c_mul(rem.terms[i].coeff, inv);
    *quotient = rem;
    return 1;
  }

  *quotient = (CanonicalPolynomial){0};

  for (;;) {
    canonical_poly_normalize(&rem);
    if (rem.count == 0)
      break;

    deg_n = poly_degree(&rem, var);
    if (deg_n < deg_d) {
      canonical_polynomial_free(&rem);
      canonical_polynomial_free(quotient);
      return 0;
    }

    /* find leading term of remainder (highest degree in var) */
    int lt_idx = -1;
    int lt_deg = -1;
    for (size_t i = 0; i < rem.count; i++) {
      int d = 0;
      for (size_t j = 0; j < rem.terms[i].degree_count; j++) {
        if (strcmp(rem.terms[i].degrees[j].var, var) == 0)
          d = rem.terms[i].degrees[j].degree;
      }
      if (d > lt_deg) {
        lt_deg = d;
        lt_idx = (int)i;
      }
    }
    if (lt_idx < 0)
      break;

    /* find leading term of divisor */
    int ld_idx = -1;
    int ld_deg = -1;
    for (size_t i = 0; i < denom->count; i++) {
      int d = 0;
      for (size_t j = 0; j < denom->terms[i].degree_count; j++) {
        if (strcmp(denom->terms[i].degrees[j].var, var) == 0)
          d = denom->terms[i].degrees[j].degree;
      }
      if (d > ld_deg) {
        ld_deg = d;
        ld_idx = (int)i;
      }
    }
    if (ld_idx < 0)
      break;

    /* quotient term = lt_rem / lt_denom */
    CanonicalTerm qt = {0};
    qt.coeff = c_div(rem.terms[lt_idx].coeff, denom->terms[ld_idx].coeff);

    /* degrees = rem_degrees - denom_degrees */
    for (size_t j = 0; j < rem.terms[lt_idx].degree_count; j++) {
      if (!canonical_term_add_degree(&qt, rem.terms[lt_idx].degrees[j].var,
                                     rem.terms[lt_idx].degrees[j].degree)) {
        canonical_term_free(&qt);
        canonical_polynomial_free(&rem);
        canonical_polynomial_free(quotient);
        return 0;
      }
    }
    for (size_t j = 0; j < denom->terms[ld_idx].degree_count; j++) {
      if (!canonical_term_add_degree(&qt, denom->terms[ld_idx].degrees[j].var,
                                     -denom->terms[ld_idx].degrees[j].degree)) {
        canonical_term_free(&qt);
        canonical_polynomial_free(&rem);
        canonical_polynomial_free(quotient);
        return 0;
      }
    }

    /* check no negative degrees in quotient term */
    for (size_t j = 0; j < qt.degree_count; j++) {
      if (qt.degrees[j].degree < 0) {
        canonical_term_free(&qt);
        canonical_polynomial_free(&rem);
        canonical_polynomial_free(quotient);
        return 0;
      }
    }

    /* subtract qt * denom from remainder */
    for (size_t i = 0; i < denom->count; i++) {
      CanonicalTerm product = {0};
      if (!canonical_poly_multiply_terms(&product, &qt, &denom->terms[i])) {
        canonical_term_free(&product);
        canonical_term_free(&qt);
        canonical_polynomial_free(&rem);
        canonical_polynomial_free(quotient);
        return 0;
      }
      product.coeff = c_neg(product.coeff);
      if (!canonical_poly_append_owned(&rem, &product)) {
        canonical_term_free(&product);
        canonical_term_free(&qt);
        canonical_polynomial_free(&rem);
        canonical_polynomial_free(quotient);
        return 0;
      }
    }

    if (!canonical_poly_append_owned(quotient, &qt)) {
      canonical_term_free(&qt);
      canonical_polynomial_free(&rem);
      canonical_polynomial_free(quotient);
      return 0;
    }

    canonical_poly_normalize(&rem);
  }

  canonical_polynomial_free(&rem);
  canonical_poly_normalize(quotient);
  return 1;
}

AstNode *ast_poly_gcd_reduce(const AstNode *numer, const AstNode *denom) {
  CanonicalPolynomial pn = {0}, pd = {0};
  const char *var_n = NULL, *var_d = NULL;
  Complex *cn = NULL, *cd = NULL, *gcd = NULL;
  Complex *qn = NULL, *qd = NULL, *rem = NULL;
  int dn, dd, dg, dqn, dqd, drem;
  CanonicalPolynomial poly_num = {0}, poly_den = {0};
  AstNode *result = NULL;

  if (!canonical_poly_from_ast(numer, &pn) ||
      !canonical_poly_from_ast(denom, &pd))
    goto fail;

  canonical_poly_normalize(&pn);
  canonical_poly_normalize(&pd);

  if (!poly_is_univariate(&pd, &var_d)) {
    /* multivariate denominator: find lead variable and try exact division */
    const char *lead_var = NULL;
    int lead_deg = 0;
    for (size_t i = 0; i < pd.count; i++) {
      for (size_t j = 0; j < pd.terms[i].degree_count; j++) {
        if (pd.terms[i].degrees[j].degree > lead_deg) {
          lead_deg = pd.terms[i].degrees[j].degree;
          lead_var = pd.terms[i].degrees[j].var;
        }
      }
    }
    if (!lead_var)
      goto fail;
    CanonicalPolynomial quot = {0};
    if (!canonical_poly_exact_div(&pn, &pd, lead_var, &quot))
      goto fail;
    if (quot.count == 0) {
      result = ast_number(0);
    } else {
      for (size_t i = 0; i < quot.count; i++) {
        AstNode *t = canonical_term_to_ast(&quot.terms[i]);
        result = result ? ast_binop(OP_ADD, result, t) : t;
      }
    }
    canonical_polynomial_free(&quot);
    goto fail;
  }

  if (!poly_is_univariate(&pn, &var_n)) {
    /* multivariate numerator, univariate denominator: try exact division */
    CanonicalPolynomial quot = {0};
    if (!canonical_poly_exact_div(&pn, &pd, var_d, &quot))
      goto fail;
    if (quot.count == 0) {
      result = ast_number(0);
    } else {
      for (size_t i = 0; i < quot.count; i++) {
        AstNode *t = canonical_term_to_ast(&quot.terms[i]);
        result = result ? ast_binop(OP_ADD, result, t) : t;
      }
    }
    canonical_polynomial_free(&quot);
    goto fail; /* cleanup pn, pd */
  }

  if (var_n && var_d && strcmp(var_n, var_d) != 0)
    goto fail;

  const char *var = var_n ? var_n : var_d;

  if (!coeffs_from_poly(&pn, var, &cn, &dn) ||
      !coeffs_from_poly(&pd, var, &cd, &dd))
    goto fail;

  if (dn == 0 && dd == 0)
    goto fail;

  poly_gcd_coeffs(cn, dn, cd, dd, &gcd, &dg);

  if (dg == 0 && c_is_one(gcd[0]))
    goto fail;

  poly_div_rem(cn, dn, gcd, dg, &qn, &dqn, &rem, &drem);
  for (int i = 0; i <= drem; i++) {
    if (!c_is_zero(rem[i]))
      goto fail;
  }
  free(rem);
  rem = NULL;

  poly_div_rem(cd, dd, gcd, dg, &qd, &dqd, &rem, &drem);
  for (int i = 0; i <= drem; i++) {
    if (!c_is_zero(rem[i]))
      goto fail;
  }

  if (!poly_from_coeffs(qn, dqn, var, &poly_num) ||
      !poly_from_coeffs(qd, dqd, var, &poly_den))
    goto fail;

  {
    AstNode *num_ast = NULL, *den_ast = NULL;

    if (poly_num.count == 0) {
      num_ast = ast_number(0);
    } else {
      for (size_t i = 0; i < poly_num.count; i++) {
        AstNode *t = canonical_term_to_ast(&poly_num.terms[i]);
        num_ast = num_ast ? ast_binop(OP_ADD, num_ast, t) : t;
      }
    }

    if (poly_den.count == 1 && c_is_one(poly_den.terms[0].coeff) &&
        poly_den.terms[0].degree_count == 0) {
      result = num_ast;
    } else {
      den_ast = NULL;
      for (size_t i = 0; i < poly_den.count; i++) {
        AstNode *t = canonical_term_to_ast(&poly_den.terms[i]);
        den_ast = den_ast ? ast_binop(OP_ADD, den_ast, t) : t;
      }
      result = ast_binop(OP_MUL, num_ast,
                         ast_binop(OP_POW, den_ast, ast_number(-1)));
    }
  }

fail:
  canonical_polynomial_free(&pn);
  canonical_polynomial_free(&pd);
  canonical_polynomial_free(&poly_num);
  canonical_polynomial_free(&poly_den);
  free(cn);
  free(cd);
  free(gcd);
  free(qn);
  free(qd);
  free(rem);
  return result;
}
