#include "sia/assumptions.h"
#include "sia/canonical.h"
#include "sia/factorial.h"
#include "sia/limits.h"
#include "sia/logarithm.h"
#include "sia/number_theory.h"
#include "sia/trigonometry.h"
#include "symbolic_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int is_laplace_call(const AstNode *n) {
  return n && n->type == AST_FUNC_CALL &&
         strcmp(n->as.call.name, "laplace") == 0 && n->as.call.nargs == 3 &&
         n->as.call.args[1]->type == AST_VARIABLE &&
         n->as.call.args[2]->type == AST_VARIABLE;
}

static int is_nonnegative_integer_node(const AstNode *n, long long *out) {
  double rounded = 0.0;

  if (!n || n->type != AST_NUMBER || !c_is_real(n->as.number))
    return 0;

  rounded = round(n->as.number.re);
  if (fabs(n->as.number.re - rounded) > 1e-9 || rounded < 0.0)
    return 0;

  if (out)
    *out = (long long)rounded;
  return 1;
}

/* check if n is f(u)^2 for a given function name f */
static int is_call1_squared(const AstNode *n, const char *fname) {
  return n && n->type == AST_BINOP && n->as.binop.op == OP_POW &&
         is_number(n->as.binop.right, 2) && is_call1(n->as.binop.left, fname);
}

static int complex_imaginary_unit_sign(Complex z) {
  if (z.exact) {
    if (!fraction_is_zero(z.re_q) || !fraction_is_one(z.im_q))
      return z.im_q.num == -z.im_q.den && fraction_is_zero(z.re_q) ? -1 : 0;
    return 1;
  }
  if (z.re != 0.0)
    return 0;
  if (z.im == 1.0)
    return 1;
  if (z.im == -1.0)
    return -1;
  return 0;
}

static int node_imaginary_unit_sign(const AstNode *n) {
  if (!n || n->type != AST_NUMBER)
    return 0;
  return complex_imaginary_unit_sign(n->as.number);
}

static int extract_imaginary_multiple(const AstNode *n, const AstNode **inner,
                                      int *sign) {
  int unit_sign = 0;

  if (!n)
    return 0;
  if (n->type == AST_UNARY_NEG) {
    if (!extract_imaginary_multiple(n->as.unary.operand, inner, sign))
      return 0;
    *sign = -*sign;
    return 1;
  }
  if (n->type != AST_BINOP || n->as.binop.op != OP_MUL)
    return 0;

  unit_sign = node_imaginary_unit_sign(n->as.binop.left);
  if (unit_sign != 0) {
    *inner = n->as.binop.right;
    *sign = unit_sign;
    return 1;
  }

  unit_sign = node_imaginary_unit_sign(n->as.binop.right);
  if (unit_sign != 0) {
    *inner = n->as.binop.left;
    *sign = unit_sign;
    return 1;
  }

  return 0;
}

static AstNode *build_imaginary_scaled_call(const char *name,
                                            const AstNode *arg, int sign) {
  AstNode *call =
      ast_func_call(name, strlen(name), (AstNode *[]){ast_clone(arg)}, 1);

  if (sign > 0)
    return ast_binop(OP_MUL, ast_complex(0.0, 1.0), call);
  return ast_binop(OP_MUL, ast_complex(0.0, -1.0), call);
}

static int fold_unary_numeric_call(const char *name, Complex arg,
                                   Complex *out) {
  if (strcmp(name, "abs") == 0) {
    *out = c_real(c_abs(arg));
    return 1;
  }
  if (strcmp(name, "sqrt") == 0) {
    if (arg.exact && c_is_real(arg) && arg.re >= 0) {
      Fraction frac;
      if (c_real_fraction(arg, &frac) && frac.num >= 0) {
        long long p = frac.num;
        long long q = frac.den;
        long long sp = (long long)round(sqrt((double)p));
        long long sq = (long long)round(sqrt((double)q));
        if (sp * sp == p && sq * sq == q) {
          *out = c_from_fractions(fraction_make(sp, sq), fraction_make(0, 1));
          return 1;
        }
        return 0;
      }
    }
    *out = c_sqrt(arg);
    return 1;
  }
  if (strcmp(name, "sin") == 0) {
    *out = c_sin(arg);
    return 1;
  }
  if (strcmp(name, "cos") == 0) {
    *out = c_cos(arg);
    return 1;
  }
  if (strcmp(name, "tan") == 0) {
    *out = c_tan(arg);
    return 1;
  }
  if (strcmp(name, "sinh") == 0) {
    *out = c_sinh(arg);
    return 1;
  }
  if (strcmp(name, "cosh") == 0) {
    *out = c_cosh(arg);
    return 1;
  }
  if (strcmp(name, "tanh") == 0) {
    *out = c_tanh(arg);
    return 1;
  }
  if (strcmp(name, "asin") == 0) {
    *out = c_asin(arg);
    return 1;
  }
  if (strcmp(name, "acos") == 0) {
    *out = c_acos(arg);
    return 1;
  }
  if (strcmp(name, "atan") == 0) {
    *out = c_atan(arg);
    return 1;
  }
  if (strcmp(name, "ln") == 0) {
    *out = c_log(arg);
    return 1;
  }
  if (strcmp(name, "exp") == 0) {
    *out = c_exp(arg);
    return 1;
  }
  return 0;
}

static int is_positive_inf_node(const AstNode *n) {
  return n && ((n->type == AST_INFINITY && n->as.infinity.sign > 0) ||
               (n->type == AST_VARIABLE && strcmp(n->as.variable, "inf") == 0));
}

static int is_negative_inf_node(const AstNode *n) {
  return n && ((n->type == AST_INFINITY && n->as.infinity.sign < 0) ||
               (n->type == AST_UNARY_NEG &&
                is_positive_inf_node(n->as.unary.operand)));
}

static int is_inf_node(const AstNode *n) {
  return is_positive_inf_node(n) || is_negative_inf_node(n);
}

static int infinity_sign(const AstNode *n) {
  if (is_negative_inf_node(n))
    return -1;
  if (is_positive_inf_node(n))
    return 1;
  return 0;
}

static int positive_real_number_value(const AstNode *n, double *out) {
  if (!n || n->type != AST_NUMBER || !c_is_real(n->as.number) ||
      !isfinite(n->as.number.re) || n->as.number.re <= 0.0)
    return 0;
  if (out)
    *out = n->as.number.re;
  return 1;
}

typedef struct {
  AstNode *base;
  AstNode *exp;
} PowerFactor;

typedef struct {
  PowerFactor *items;
  size_t count;
  size_t cap;
} PowerFactorList;

typedef struct {
  Complex coeff;
  PowerFactorList factors;
} FactorizedTerm;

static int power_factor_list_push(PowerFactorList *list, AstNode *base,
                                  AstNode *exp) {
  if (list->count == list->cap) {
    size_t new_cap = list->cap ? list->cap * 2 : 4;
    PowerFactor *items = realloc(list->items, new_cap * sizeof(PowerFactor));
    if (!items)
      return 0;
    list->items = items;
    list->cap = new_cap;
  }

  list->items[list->count].base = base;
  list->items[list->count].exp = exp;
  list->count++;
  return 1;
}

static void power_factor_list_free(PowerFactorList *list) {
  if (!list)
    return;
  for (size_t i = 0; i < list->count; i++) {
    ast_free(list->items[i].base);
    ast_free(list->items[i].exp);
  }
  free(list->items);
  list->items = NULL;
  list->count = 0;
  list->cap = 0;
}

static void factorized_terms_free(FactorizedTerm *terms, size_t count) {
  if (!terms)
    return;
  for (size_t i = 0; i < count; i++)
    power_factor_list_free(&terms[i].factors);
  free(terms);
}

static int flatten_mul_factors(const AstNode *node, Complex *coeff,
                               PowerFactorList *factors) {
  if (!node)
    return 1;

  if (node->type == AST_NUMBER) {
    *coeff = c_mul(*coeff, node->as.number);
    return 1;
  }

  if (node->type == AST_UNARY_NEG) {
    *coeff = c_neg(*coeff);
    return flatten_mul_factors(node->as.unary.operand, coeff, factors);
  }

  if (node->type == AST_BINOP && node->as.binop.op == OP_MUL) {
    return flatten_mul_factors(node->as.binop.left, coeff, factors) &&
           flatten_mul_factors(node->as.binop.right, coeff, factors);
  }

  if (node->type == AST_BINOP && node->as.binop.op == OP_POW) {
    long long exp_value = 0;

    if (node->as.binop.left->type == AST_UNARY_NEG &&
        is_nonnegative_integer_node(node->as.binop.right, &exp_value)) {
      if (exp_value % 2 == 1)
        *coeff = c_neg(*coeff);
      return power_factor_list_push(
          factors, ast_clone(node->as.binop.left->as.unary.operand),
          ast_clone(node->as.binop.right));
    }

    return power_factor_list_push(factors, ast_clone(node->as.binop.left),
                                  ast_clone(node->as.binop.right));
  }

  return power_factor_list_push(factors, ast_clone(node), ast_number(1));
}

static AstNode *build_mul_chain(Complex coeff, NodeList *factors) {
  AstNode *result = NULL;

  if (c_is_zero(coeff))
    return ast_number(0);

  if (!c_is_one(coeff) || factors->count == 0)
    result = ast_number_complex(coeff);

  for (size_t i = 0; i < factors->count; i++) {
    AstNode *factor = factors->items[i];
    factors->items[i] = NULL;
    if (!result) {
      result = factor;
    } else {
      result = ast_binop(OP_MUL, result, factor);
    }
  }

  if (!result)
    return ast_number(1);
  return result;
}

static AstNode *build_factor_node(PowerFactor *factor) {
  AstNode *base = factor->base;
  AstNode *exp = factor->exp;

  factor->base = NULL;
  factor->exp = NULL;

  if (is_number(exp, 0)) {
    ast_free(base);
    ast_free(exp);
    return ast_number(1);
  }

  if (is_number(exp, 1)) {
    ast_free(exp);
    return base;
  }

  return ast_binop(OP_POW, base, exp);
}

static AstNode *build_factor_node_clone(const PowerFactor *factor) {
  if (is_number(factor->exp, 0))
    return ast_number(1);
  if (is_number(factor->exp, 1))
    return ast_clone(factor->base);
  return ast_binop(OP_POW, ast_clone(factor->base), ast_clone(factor->exp));
}

static void merge_like_factors(PowerFactorList *factors) {
  for (size_t i = 0; i < factors->count; i++) {
    if (!factors->items[i].base)
      continue;
    for (size_t j = i + 1; j < factors->count; j++) {
      AstNode *exp = NULL;
      if (!factors->items[j].base)
        continue;
      if (!ast_equal(factors->items[i].base, factors->items[j].base))
        continue;

      exp = sym_simplify(
          ast_binop(OP_ADD, factors->items[i].exp, factors->items[j].exp));
      factors->items[i].exp = exp;
      factors->items[j].exp = NULL;
      ast_free(factors->items[j].base);
      factors->items[j].base = NULL;
    }
  }
}

static void fold_abs_power_identities(PowerFactorList *factors) {
  for (size_t i = 0; i < factors->count; i++) {
    long long exp_value = 0;
    long long even_part = 0;

    if (!factors->items[i].base || factors->items[i].base->type != AST_VARIABLE)
      continue;
    if (!is_nonnegative_integer_node(factors->items[i].exp, &exp_value))
      continue;

    even_part = exp_value - (exp_value % 2);
    if (even_part == 0)
      continue;

    for (size_t j = 0; j < factors->count; j++) {
      AstNode *new_abs_exp = NULL;

      if (i == j || !factors->items[j].base)
        continue;
      if (!is_call1(factors->items[j].base, "abs"))
        continue;
      if (!ast_equal(factors->items[j].base->as.call.args[0],
                     factors->items[i].base))
        continue;

      new_abs_exp = sym_simplify(ast_binop(OP_ADD, factors->items[j].exp,
                                           ast_number((double)even_part)));
      factors->items[j].exp = new_abs_exp;
      ast_free(factors->items[i].exp);
      factors->items[i].exp = ast_number((double)(exp_value - even_part));
      break;
    }
  }
}

static int append_factor_nodes(PowerFactorList *factors, NodeList *nodes) {
  for (size_t i = 0; i < factors->count; i++) {
    AstNode *factor_node = NULL;
    if (!factors->items[i].base)
      continue;
    factor_node = build_factor_node(&factors->items[i]);
    if (!is_number(factor_node, 1) || nodes->count == 0) {
      if (!node_list_push(nodes, factor_node)) {
        ast_free(factor_node);
        return 0;
      }
    } else {
      ast_free(factor_node);
    }
  }

  return 1;
}

static int find_power_factor(const PowerFactorList *factors,
                             const AstNode *base, const AstNode *exp) {
  for (size_t i = 0; i < factors->count; i++) {
    if (!factors->items[i].base || !factors->items[i].exp)
      continue;
    if (ast_equal(factors->items[i].base, base) &&
        ast_equal(factors->items[i].exp, exp))
      return (int)i;
  }

  return -1;
}

static AstNode *clone_factor_node(const AstNode *base, const AstNode *exp) {
  if (is_number(exp, 0))
    return ast_number(1);
  if (is_number(exp, 1))
    return ast_clone(base);
  return ast_binop(OP_POW, ast_clone(base), ast_clone(exp));
}

static AstNode *build_factorized_term(Complex coeff, PowerFactorList *factors) {
  NodeList nodes = {0};
  AstNode *result = NULL;

  if (!append_factor_nodes(factors, &nodes)) {
    node_list_free(&nodes);
    return NULL;
  }

  result = build_mul_chain(coeff, &nodes);
  node_list_free(&nodes);
  return result;
}

static AstNode *normalize_mul_factors(const AstNode *node) {
  Complex coeff = c_real(1.0);
  PowerFactorList factors = {0};
  NodeList nodes = {0};
  AstNode *result = NULL;

  if (!flatten_mul_factors(node, &coeff, &factors)) {
    power_factor_list_free(&factors);
    return NULL;
  }

  merge_like_factors(&factors);
  fold_abs_power_identities(&factors);
  if (!append_factor_nodes(&factors, &nodes)) {
    node_list_free(&nodes);
    power_factor_list_free(&factors);
    return NULL;
  }

  result = build_mul_chain(coeff, &nodes);
  node_list_free(&nodes);
  power_factor_list_free(&factors);
  return result;
}

int sym_extract_coeff_and_base(const AstNode *node, Complex *coeff,
                               AstNode **base) {
  PowerFactorList factors = {0};
  NodeList nodes = {0};

  *coeff = c_real(1.0);
  *base = NULL;

  if (!flatten_mul_factors(node, coeff, &factors))
    return 0;

  merge_like_factors(&factors);
  fold_abs_power_identities(&factors);
  if (!append_factor_nodes(&factors, &nodes)) {
    node_list_free(&nodes);
    power_factor_list_free(&factors);
    return 0;
  }

  *base = build_mul_chain(c_real(1.0), &nodes);
  node_list_free(&nodes);
  power_factor_list_free(&factors);

  if (*base && is_number(*base, 1)) {
    ast_free(*base);
    *base = NULL;
  }

  return 1;
}

AstNode *sym_simplify(AstNode *node) {
  if (!node)
    return NULL;

  switch (node->type) {
  case AST_NUMBER:
  case AST_VARIABLE:
  case AST_INFINITY:
  case AST_UNDEFINED:
    return node;

  case AST_MATRIX: {
    size_t total = node->as.matrix.rows * node->as.matrix.cols;
    for (size_t i = 0; i < total; i++)
      node->as.matrix.elements[i] = sym_simplify(node->as.matrix.elements[i]);
    return node;
  }

  case AST_EQ:
    node->as.eq.lhs = sym_simplify(node->as.eq.lhs);
    node->as.eq.rhs = sym_simplify(node->as.eq.rhs);
    return node;

  case AST_LIMIT: {
    AstNode *result = NULL;
    node->as.limit.expr = sym_simplify(node->as.limit.expr);
    node->as.limit.target = sym_simplify(node->as.limit.target);
    result =
        sym_limit_directed(node->as.limit.expr, node->as.limit.var,
                           node->as.limit.target, node->as.limit.direction);
    if (result) {
      ast_free(node);
      return result;
    }
    return node;
  }

  case AST_UNARY_NEG:
    node->as.unary.operand = sym_simplify(node->as.unary.operand);
    if (node->as.unary.operand->type == AST_INFINITY) {
      int sign = -node->as.unary.operand->as.infinity.sign;
      ast_free(node);
      return ast_infinity(sign);
    }
    if (is_num(node->as.unary.operand)) {
      Complex z = c_neg(node->as.unary.operand->as.number);
      ast_free(node);
      return ast_number_complex(z);
    }
    /* -(-x) -> x */
    if (node->as.unary.operand->type == AST_UNARY_NEG) {
      AstNode *inner = node->as.unary.operand->as.unary.operand;
      node->as.unary.operand->as.unary.operand = NULL;
      ast_free(node);
      return inner;
    }
    /* -((-x)^n) -> x^n when n is odd integer */
    if (node->as.unary.operand->type == AST_BINOP &&
        node->as.unary.operand->as.binop.op == OP_POW &&
        node->as.unary.operand->as.binop.left->type == AST_UNARY_NEG &&
        node->as.unary.operand->as.binop.right->type == AST_NUMBER &&
        c_is_real(node->as.unary.operand->as.binop.right->as.number)) {
      double n = node->as.unary.operand->as.binop.right->as.number.re;
      if (n == (int)n && ((int)n % 2 != 0)) {
        AstNode *base = node->as.unary.operand->as.binop.left->as.unary.operand;
        AstNode *exp = node->as.unary.operand->as.binop.right;
        node->as.unary.operand->as.binop.left->as.unary.operand = NULL;
        node->as.unary.operand->as.binop.right = NULL;
        ast_free(node);
        return ast_binop(OP_POW, base, exp);
      }
    }
    return node;

  case AST_FUNC_CALL:
    for (size_t i = 0; i < node->as.call.nargs; i++)
      node->as.call.args[i] = sym_simplify(node->as.call.args[i]);
    if (is_laplace_call(node)) {
      AstNode *result =
          sym_laplace(node->as.call.args[0], node->as.call.args[1]->as.variable,
                      node->as.call.args[2]->as.variable);
      if (result) {
        ast_free(node);
        return result;
      }
    }
    if (factorial_is_call(node))
      return factorial_simplify_call(node);
    if (trig_kind(node) != TRIG_KIND_NONE)
      return trig_simplify_call(node);
    if (number_theory_kind(node) != NT_KIND_NONE)
      return number_theory_simplify_call(node);
    if (is_call1(node, "ln") && is_call1(node->as.call.args[0], "exp")) {
      AstNode *inner = ast_clone(node->as.call.args[0]->as.call.args[0]);
      ast_free(node);
      return inner;
    }
    if (is_call1(node, "ln") && is_var(node->as.call.args[0], "e")) {
      ast_free(node);
      return ast_number(1);
    }
    if (log_kind(node) != LOG_KIND_NONE)
      return log_simplify_call(node);
    if (node->as.call.nargs == 1) {
      const AstNode *inner = NULL;
      int sign = 0;

      if (extract_imaginary_multiple(node->as.call.args[0], &inner, &sign)) {
        const char *name = node->as.call.name;
        AstNode *rewritten = NULL;

        if (strcmp(name, "sinh") == 0) {
          rewritten = build_imaginary_scaled_call("sin", inner, sign);
        } else if (strcmp(name, "cosh") == 0) {
          rewritten =
              ast_func_call("cos", 3, (AstNode *[]){ast_clone(inner)}, 1);
        } else if (strcmp(name, "tanh") == 0) {
          rewritten = build_imaginary_scaled_call("tan", inner, sign);
        } else if (strcmp(name, "sin") == 0) {
          rewritten = build_imaginary_scaled_call("sinh", inner, sign);
        } else if (strcmp(name, "cos") == 0) {
          rewritten =
              ast_func_call("cosh", 4, (AstNode *[]){ast_clone(inner)}, 1);
        } else if (strcmp(name, "tan") == 0) {
          rewritten = build_imaginary_scaled_call("tanh", inner, sign);
        }

        if (rewritten) {
          ast_free(node);
          return sym_simplify(rewritten);
        }
      }
    }
    if (node->as.call.nargs == 1 && is_num(node->as.call.args[0])) {
      Complex folded;
      if (strcmp(node->as.call.name, "exp") == 0 &&
          c_is_real(node->as.call.args[0]->as.number)) {
        AstNode *exponent = ast_clone(node->as.call.args[0]);
        AstNode *e = ast_variable("e", 1);
        if (is_number(exponent, 0)) {
          ast_free(exponent);
          ast_free(e);
          ast_free(node);
          return ast_number(1);
        }
        if (is_number(exponent, 1)) {
          ast_free(exponent);
          ast_free(node);
          return e;
        }
        ast_free(node);
        return ast_binop(OP_POW, e, exponent);
      }
      if (fold_unary_numeric_call(node->as.call.name,
                                  node->as.call.args[0]->as.number, &folded)) {
        ast_free(node);
        return ast_number_complex(folded);
      }
    }
    /* sqrt(c * A) -> sqrt_extract(c) * sqrt(A) where the numeric factor c has
     * its perfect-square part pulled out.
     */
    if (is_call1(node, "sqrt") && node->as.call.args[0]->type == AST_BINOP &&
        node->as.call.args[0]->as.binop.op == OP_MUL) {
      AstNode *arg = node->as.call.args[0];
      AstNode *num_factor = NULL;
      AstNode *rest = NULL;
      if (is_num(arg->as.binop.left)) {
        num_factor = arg->as.binop.left;
        rest = arg->as.binop.right;
      } else if (is_num(arg->as.binop.right)) {
        num_factor = arg->as.binop.right;
        rest = arg->as.binop.left;
      }
      if (num_factor && c_is_real(num_factor->as.number) &&
          num_factor->as.number.re > 0) {
        Complex c = num_factor->as.number;
        Fraction f;
        if (c_real_fraction(c, &f) && f.num > 0) {
          long long p = f.num, q = f.den;
          long long sp = 1, sq = 1;
          for (long long i = 2; i * i <= p; i++)
            while (p % (i * i) == 0) {
              sp *= i;
              p /= (i * i);
            }
          for (long long i = 2; i * i <= q; i++)
            while (q % (i * i) == 0) {
              sq *= i;
              q /= (i * i);
            }
          if (sp > 1 || sq > 1) {
            Fraction outer_f = fraction_make(sp, sq);
            Fraction inner_f = fraction_make(p, q);
            AstNode *outer = ast_number_complex(
                c_from_fractions(outer_f, fraction_make(0, 1)));
            AstNode *rest_clone = ast_clone(rest);
            AstNode *inner_arg;
            if (inner_f.num == 1 && inner_f.den == 1) {
              inner_arg = rest_clone;
            } else {
              AstNode *inner_num = ast_number_complex(
                  c_from_fractions(inner_f, fraction_make(0, 1)));
              inner_arg = ast_binop(OP_MUL, inner_num, rest_clone);
            }
            AstNode *new_sqrt =
                ast_func_call("sqrt", 4, (AstNode *[]){inner_arg}, 1);
            ast_free(node);
            return sym_simplify(ast_binop(OP_MUL, outer, new_sqrt));
          }
        }
      }
    }
    /* sqrt(POW(A, 2*n)) -> A^n (for real A>=0)
     * Handled here for non-bare powers so the quadratic formula's
     * sqrt(B^2 - 4AC) shrinks sqrt(x^2) -> x within nested forms. */
    if (is_call1(node, "sqrt") && node->as.call.args[0]->type == AST_BINOP &&
        node->as.call.args[0]->as.binop.op == OP_POW &&
        is_num(node->as.call.args[0]->as.binop.right) &&
        c_is_real(node->as.call.args[0]->as.binop.right->as.number)) {
      double e = node->as.call.args[0]->as.binop.right->as.number.re;
      if (e == 2.0) {
        AstNode *base = node->as.call.args[0]->as.binop.left;
        AstNode *abs_call =
            ast_func_call("abs", 3, (AstNode *[]){ast_clone(base)}, 1);
        ast_free(node);
        return sym_simplify(abs_call);
      }
    }
    /* sqrt(k^2 * m) -> k * sqrt(m) */
    if (is_call1(node, "sqrt") && is_num(node->as.call.args[0])) {
      Complex arg = node->as.call.args[0]->as.number;
      Fraction frac;
      if (arg.exact && c_is_real(arg) && arg.re > 0 &&
          c_real_fraction(arg, &frac) && frac.num > 0) {
        long long p = frac.num, q = frac.den;
        long long sp = 1, sq = 1;
        for (long long i = 2; i * i <= p; i++)
          while (p % (i * i) == 0) {
            sp *= i;
            p /= (i * i);
          }
        for (long long i = 2; i * i <= q; i++)
          while (q % (i * i) == 0) {
            sq *= i;
            q /= (i * i);
          }
        if (sp > 1 || sq > 1) {
          Fraction outer_f = fraction_make(sp, sq);
          Fraction inner_f = fraction_make(p, q);
          AstNode *outer = ast_number_complex(
              c_from_fractions(outer_f, fraction_make(0, 1)));
          if (inner_f.num == 1 && inner_f.den == 1) {
            ast_free(node);
            return outer;
          }
          AstNode *inner = ast_number_complex(
              c_from_fractions(inner_f, fraction_make(0, 1)));
          AstNode *sqrt_inner =
              ast_func_call("sqrt", 4, (AstNode *[]){inner}, 1);
          ast_free(node);
          return sym_simplify(ast_binop(OP_MUL, outer, sqrt_inner));
        }
      }
    }
    /* exp(X) -> 0 when X tends to -inf (contains inf with negative sign) */
    if (is_call1(node, "exp")) {
      AstNode *arg = node->as.call.args[0];
      int neg_inf = 0;
      if (arg->type == AST_UNARY_NEG &&
          arg->as.unary.operand->type == AST_VARIABLE &&
          strcmp(arg->as.unary.operand->as.variable, "inf") == 0) {
        neg_inf = 1;
      } else if (arg->type == AST_UNARY_NEG &&
                 arg->as.unary.operand->type == AST_BINOP &&
                 arg->as.unary.operand->as.binop.op == OP_MUL) {
        AstNode *prod = arg->as.unary.operand;
        if ((prod->as.binop.left->type == AST_VARIABLE &&
             strcmp(prod->as.binop.left->as.variable, "inf") == 0) ||
            (prod->as.binop.right->type == AST_VARIABLE &&
             strcmp(prod->as.binop.right->as.variable, "inf") == 0))
          neg_inf = 1;
      } else if (arg->type == AST_BINOP && arg->as.binop.op == OP_MUL) {
        int has_inf = 0, has_neg = 0;
        AstNode *l = arg->as.binop.left;
        AstNode *r = arg->as.binop.right;
        if (l->type == AST_VARIABLE && strcmp(l->as.variable, "inf") == 0)
          has_inf = 1;
        if (r->type == AST_VARIABLE && strcmp(r->as.variable, "inf") == 0)
          has_inf = 1;
        if (l->type == AST_UNARY_NEG) {
          has_neg = 1;
          if (l->as.unary.operand->type == AST_VARIABLE &&
              strcmp(l->as.unary.operand->as.variable, "inf") == 0)
            has_inf = 1;
        }
        if (r->type == AST_UNARY_NEG) {
          has_neg = 1;
          if (r->as.unary.operand->type == AST_VARIABLE &&
              strcmp(r->as.unary.operand->as.variable, "inf") == 0)
            has_inf = 1;
        }
        if (l->type == AST_NUMBER && l->as.number.re < 0)
          has_neg = 1;
        if (r->type == AST_NUMBER && r->as.number.re < 0)
          has_neg = 1;
        if (has_inf && has_neg)
          neg_inf = 1;
      }
      if (neg_inf) {
        ast_free(node);
        return ast_number(0);
      }
    }
    /* exp(ln(x)) -> x */
    if (is_call1(node, "exp") && is_call1(node->as.call.args[0], "ln")) {
      AstNode *inner = ast_clone(node->as.call.args[0]->as.call.args[0]);
      ast_free(node);
      return inner;
    }
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
    Complex result;
    switch (op) {
    case OP_ADD:
      result = c_add(L->as.number, R->as.number);
      break;
    case OP_SUB:
      result = c_sub(L->as.number, R->as.number);
      break;
    case OP_MUL:
      result = c_mul(L->as.number, R->as.number);
      break;
    case OP_DIV:
      if (c_is_zero(R->as.number))
        return node;
      result = c_div(L->as.number, R->as.number);
      break;
    case OP_POW:
      result = c_pow(L->as.number, R->as.number);
      break;
    default:
      return node;
    }
    ast_free(node);
    return ast_number_complex(result);
  }

  switch (op) {
  case OP_ADD:
    if (is_inf_node(L) || is_inf_node(R)) {
      int ls = infinity_sign(L);
      int rs = infinity_sign(R);
      ast_free(node);
      if (ls != 0 && rs != 0)
        return ls == rs ? ast_infinity(ls) : ast_undefined();
      return ast_infinity(ls != 0 ? ls : rs);
    }
    if (is_number(R, 0)) {
      node->as.binop.left = NULL;
      ast_free(node);
      return L;
    }
    if (is_number(L, 0)) {
      node->as.binop.right = NULL;
      ast_free(node);
      return R;
    }
    /* x + x -> 2*x */
    if (ast_equal(L, R)) {
      node->as.binop.left = NULL;
      ast_free(node);
      return ast_binop(OP_MUL, ast_number(2), L);
    }
    /* c1*E + c2*E -> (c1+c2)*E */
    if (L->type == AST_BINOP && L->as.binop.op == OP_MUL &&
        is_num(L->as.binop.left) && R->type == AST_BINOP &&
        R->as.binop.op == OP_MUL && is_num(R->as.binop.left) &&
        ast_equal(L->as.binop.right, R->as.binop.right)) {
      Complex c =
          c_add(L->as.binop.left->as.number, R->as.binop.left->as.number);
      AstNode *base = ast_clone(L->as.binop.right);
      ast_free(node);
      return sym_simplify(ast_binop(OP_MUL, ast_number_complex(c), base));
    }
    /* c1*E + E -> (c1+1)*E */
    if (L->type == AST_BINOP && L->as.binop.op == OP_MUL &&
        is_num(L->as.binop.left) && ast_equal(L->as.binop.right, R)) {
      Complex c = c_add(L->as.binop.left->as.number, c_real(1.0));
      node->as.binop.right = NULL;
      AstNode *base = L->as.binop.right;
      L->as.binop.right = NULL;
      ast_free(node);
      return sym_simplify(ast_binop(OP_MUL, ast_number_complex(c), base));
    }
    /* E + c*E -> (c+1)*E */
    if (R->type == AST_BINOP && R->as.binop.op == OP_MUL &&
        is_num(R->as.binop.left) && ast_equal(L, R->as.binop.right)) {
      Complex c = c_add(R->as.binop.left->as.number, c_real(1.0));
      node->as.binop.left = NULL;
      AstNode *base = ast_clone(L);
      ast_free(node);
      return sym_simplify(ast_binop(OP_MUL, ast_number_complex(c), base));
    }
    /* sin(u)^2 + cos(u)^2 -> 1 */
    if (is_call1_squared(L, "sin") && is_call1_squared(R, "cos") &&
        ast_equal(L->as.binop.left->as.call.args[0],
                  R->as.binop.left->as.call.args[0])) {
      ast_free(node);
      return ast_number(1);
    }
    if (is_call1_squared(L, "cos") && is_call1_squared(R, "sin") &&
        ast_equal(L->as.binop.left->as.call.args[0],
                  R->as.binop.left->as.call.args[0])) {
      ast_free(node);
      return ast_number(1);
    }
    break;

  case OP_SUB:
    if (is_inf_node(L) || is_inf_node(R)) {
      int ls = infinity_sign(L);
      int rs = infinity_sign(R);
      ast_free(node);
      if (ls != 0 && rs != 0)
        return ls == rs ? ast_undefined() : ast_infinity(ls);
      return ast_infinity(ls != 0 ? ls : -rs);
    }
    if (is_number(R, 0)) {
      node->as.binop.left = NULL;
      ast_free(node);
      return L;
    }
    if (is_number(L, 0)) {
      node->as.binop.right = NULL;
      ast_free(node);
      return sym_simplify(ast_unary_neg(R));
    }
    /* x - x -> 0 */
    if (ast_equal(L, R)) {
      ast_free(node);
      return ast_number(0);
    }
    /* cos(u)^2 - sin(u)^2 -> cos(2*u) */
    if (is_call1_squared(L, "cos") && is_call1_squared(R, "sin") &&
        ast_equal(L->as.binop.left->as.call.args[0],
                  R->as.binop.left->as.call.args[0])) {
      AstNode *arg = ast_clone(L->as.binop.left->as.call.args[0]);
      ast_free(node);
      AstNode *two_u = ast_binop(OP_MUL, ast_number(2), arg);
      return ast_func_call("cos", 3, (AstNode *[]){two_u}, 1);
    }
    /* sin(u)^2 - cos(u)^2 -> -cos(2*u) */
    if (is_call1_squared(L, "sin") && is_call1_squared(R, "cos") &&
        ast_equal(L->as.binop.left->as.call.args[0],
                  R->as.binop.left->as.call.args[0])) {
      AstNode *arg = ast_clone(L->as.binop.left->as.call.args[0]);
      ast_free(node);
      AstNode *two_u = ast_binop(OP_MUL, ast_number(2), arg);
      return ast_unary_neg(ast_func_call("cos", 3, (AstNode *[]){two_u}, 1));
    }
    /* cosh(u)^2 - sinh(u)^2 -> 1 */
    if (is_call1_squared(L, "cosh") && is_call1_squared(R, "sinh") &&
        ast_equal(L->as.binop.left->as.call.args[0],
                  R->as.binop.left->as.call.args[0])) {
      ast_free(node);
      return ast_number(1);
    }
    /* sinh(u)^2 - cosh(u)^2 -> -1 */
    if (is_call1_squared(L, "sinh") && is_call1_squared(R, "cosh") &&
        ast_equal(L->as.binop.left->as.call.args[0],
                  R->as.binop.left->as.call.args[0])) {
      ast_free(node);
      return ast_number(-1);
    }
    break;

  case OP_MUL:
    if ((is_inf_node(L) && is_num(R)) || (is_inf_node(R) && is_num(L))) {
      AstNode *coeff = is_inf_node(L) ? R : L;
      int sign = infinity_sign(is_inf_node(L) ? L : R);
      if (c_is_zero(coeff->as.number)) {
        ast_free(node);
        return ast_undefined();
      }
      if (!c_is_real(coeff->as.number)) {
        ast_free(node);
        return ast_undefined();
      }
      if (coeff->as.number.re < 0)
        sign = -sign;
      ast_free(node);
      return ast_infinity(sign);
    }
    if (is_inf_node(L) && is_inf_node(R)) {
      int sign = infinity_sign(L) * infinity_sign(R);
      ast_free(node);
      return ast_infinity(sign);
    }
    if (is_number(L, 0) || is_number(R, 0)) {
      ast_free(node);
      return ast_number(0);
    }
    if (is_number(R, 1)) {
      node->as.binop.left = NULL;
      ast_free(node);
      return L;
    }
    if (is_number(L, 1)) {
      node->as.binop.right = NULL;
      ast_free(node);
      return R;
    }
    if (is_number(R, -1)) {
      node->as.binop.left = NULL;
      ast_free(node);
      return sym_simplify(ast_unary_neg(L));
    }
    if (is_number(L, -1)) {
      node->as.binop.right = NULL;
      ast_free(node);
      return sym_simplify(ast_unary_neg(R));
    }
    /* A * (-B) -> -(A * B) */
    if (R->type == AST_UNARY_NEG) {
      AstNode *a = L;
      AstNode *b = R->as.unary.operand;
      R->as.unary.operand = NULL;
      node->as.binop.left = NULL;
      ast_free(node);
      return sym_simplify(ast_unary_neg(ast_binop(OP_MUL, a, b)));
    }
    /* (-A) * B -> -(A * B) */
    if (L->type == AST_UNARY_NEG) {
      AstNode *a = L->as.unary.operand;
      AstNode *b = R;
      L->as.unary.operand = NULL;
      node->as.binop.right = NULL;
      ast_free(node);
      return sym_simplify(ast_unary_neg(ast_binop(OP_MUL, a, b)));
    }
    /* c1 * (c2 * E) -> (c1*c2) * E */
    if (is_num(L) && R->type == AST_BINOP && R->as.binop.op == OP_MUL &&
        is_num(R->as.binop.left)) {
      Complex c = c_mul(L->as.number, R->as.binop.left->as.number);
      AstNode *base = R->as.binop.right;
      R->as.binop.right = NULL;
      ast_free(node);
      return sym_simplify(ast_binop(OP_MUL, ast_number_complex(c), base));
    }
    /* x * x -> x^2 */
    if (ast_equal(L, R)) {
      AstNode *base = ast_clone(L);
      ast_free(node);
      return ast_binop(OP_POW, base, ast_number(2));
    }
    /* x^a * x^b -> x^(a+b) */
    if (L->type == AST_BINOP && L->as.binop.op == OP_POW &&
        R->type == AST_BINOP && R->as.binop.op == OP_POW &&
        ast_equal(L->as.binop.left, R->as.binop.left)) {
      AstNode *base = ast_clone(L->as.binop.left);
      AstNode *exp = ast_binop(OP_ADD, ast_clone(L->as.binop.right),
                               ast_clone(R->as.binop.right));
      ast_free(node);
      return sym_simplify(ast_binop(OP_POW, base, sym_simplify(exp)));
    }
    /* x * x^n -> x^(n+1) */
    if (R->type == AST_BINOP && R->as.binop.op == OP_POW &&
        ast_equal(L, R->as.binop.left)) {
      AstNode *base = ast_clone(L);
      AstNode *exp =
          ast_binop(OP_ADD, ast_clone(R->as.binop.right), ast_number(1));
      ast_free(node);
      return sym_simplify(ast_binop(OP_POW, base, sym_simplify(exp)));
    }
    /* x^n * x -> x^(n+1) */
    if (L->type == AST_BINOP && L->as.binop.op == OP_POW &&
        ast_equal(L->as.binop.left, R)) {
      AstNode *base = ast_clone(R);
      AstNode *exp =
          ast_binop(OP_ADD, ast_clone(L->as.binop.right), ast_number(1));
      ast_free(node);
      return sym_simplify(ast_binop(OP_POW, base, sym_simplify(exp)));
    }
    /* A * (1/A) -> 1 */
    if (R->type == AST_BINOP && R->as.binop.op == OP_DIV &&
        is_number(R->as.binop.left, 1) && ast_equal(L, R->as.binop.right)) {
      ast_free(node);
      return ast_number(1);
    }
    /* (1/A) * A -> 1 */
    if (L->type == AST_BINOP && L->as.binop.op == OP_DIV &&
        is_number(L->as.binop.left, 1) && ast_equal(L->as.binop.right, R)) {
      ast_free(node);
      return ast_number(1);
    }
    /* A * (B/A) -> B */
    if (R->type == AST_BINOP && R->as.binop.op == OP_DIV &&
        ast_equal(L, R->as.binop.right)) {
      AstNode *b = ast_clone(R->as.binop.left);
      ast_free(node);
      return sym_simplify(b);
    }
    /* (A/B) * B -> A */
    if (L->type == AST_BINOP && L->as.binop.op == OP_DIV &&
        ast_equal(L->as.binop.right, R)) {
      AstNode *a = ast_clone(L->as.binop.left);
      ast_free(node);
      return sym_simplify(a);
    }
    /* (A + B) * C -> A*C + B*C */
    if (L->type == AST_BINOP && L->as.binop.op == OP_ADD) {
      AstNode *a = ast_clone(L->as.binop.left);
      AstNode *b = ast_clone(L->as.binop.right);
      AstNode *c1 = ast_clone(R);
      AstNode *c2 = ast_clone(R);
      ast_free(node);
      return sym_simplify(ast_binop(OP_ADD, ast_binop(OP_MUL, a, c1),
                                    ast_binop(OP_MUL, b, c2)));
    }
    /* C * (A + B) -> C*A + C*B */
    if (R->type == AST_BINOP && R->as.binop.op == OP_ADD) {
      AstNode *a = ast_clone(R->as.binop.left);
      AstNode *b = ast_clone(R->as.binop.right);
      AstNode *c1 = ast_clone(L);
      AstNode *c2 = ast_clone(L);
      ast_free(node);
      return sym_simplify(ast_binop(OP_ADD, ast_binop(OP_MUL, c1, a),
                                    ast_binop(OP_MUL, c2, b)));
    }
    /* (A - B) * C -> A*C - B*C */
    if (L->type == AST_BINOP && L->as.binop.op == OP_SUB) {
      AstNode *a = ast_clone(L->as.binop.left);
      AstNode *b = ast_clone(L->as.binop.right);
      AstNode *c1 = ast_clone(R);
      AstNode *c2 = ast_clone(R);
      ast_free(node);
      return sym_simplify(ast_binop(OP_SUB, ast_binop(OP_MUL, a, c1),
                                    ast_binop(OP_MUL, b, c2)));
    }
    /* C * (A - B) -> C*A - C*B */
    if (R->type == AST_BINOP && R->as.binop.op == OP_SUB) {
      AstNode *a = ast_clone(R->as.binop.left);
      AstNode *b = ast_clone(R->as.binop.right);
      AstNode *c1 = ast_clone(L);
      AstNode *c2 = ast_clone(L);
      ast_free(node);
      return sym_simplify(ast_binop(OP_SUB, ast_binop(OP_MUL, c1, a),
                                    ast_binop(OP_MUL, c2, b)));
    }
    {
      AstNode *normalized = normalize_mul_factors(node);
      if (normalized && !ast_equal(normalized, node)) {
        ast_free(node);
        return sym_simplify(normalized);
      }
      ast_free(normalized);
    }
    break;

  case OP_DIV:
    if (is_inf_node(L) && is_inf_node(R)) {
      ast_free(node);
      return ast_undefined();
    }
    if (is_inf_node(R) && !is_inf_node(L)) {
      ast_free(node);
      return ast_number(0);
    }
    if (is_inf_node(L) && is_num(R) && !c_is_zero(R->as.number)) {
      int sign = infinity_sign(L);
      if (!c_is_real(R->as.number)) {
        ast_free(node);
        return ast_undefined();
      }
      if (R->as.number.re < 0)
        sign = -sign;
      ast_free(node);
      return ast_infinity(sign);
    }
    if (is_number(R, 1)) {
      node->as.binop.left = NULL;
      ast_free(node);
      return L;
    }
    if (is_number(L, 0)) {
      ast_free(node);
      return ast_number(0);
    }
    /* x / x -> 1 */
    if (ast_equal(L, R)) {
      ast_free(node);
      return ast_number(1);
    }
    /* (a*x) / x -> a */
    if (L->type == AST_BINOP && L->as.binop.op == OP_MUL &&
        ast_equal(L->as.binop.right, R)) {
      AstNode *a = ast_clone(L->as.binop.left);
      ast_free(node);
      return sym_simplify(a);
    }
    /* (c1 * E) / c2 -> (c1/c2) * E when both constants */
    if (is_num(R) && !c_is_zero(R->as.number) && L->type == AST_BINOP &&
        L->as.binop.op == OP_MUL && is_num(L->as.binop.left)) {
      Complex c = c_div(L->as.binop.left->as.number, R->as.number);
      AstNode *base = ast_clone(L->as.binop.right);
      ast_free(node);
      return sym_simplify(ast_binop(OP_MUL, ast_number_complex(c), base));
    }
    /* sin(u) / cos(u) -> tan(u) */
    if (is_call1(L, "sin") && is_call1(R, "cos") &&
        ast_equal(L->as.call.args[0], R->as.call.args[0])) {
      AstNode *arg = ast_clone(L->as.call.args[0]);
      ast_free(node);
      AstNode *args[] = {arg};
      return ast_func_call("tan", 3, args, 1);
    }
    /* cos(u) / sin(u) -> 1/tan(u) */
    if (is_call1(L, "cos") && is_call1(R, "sin") &&
        ast_equal(L->as.call.args[0], R->as.call.args[0])) {
      AstNode *arg = ast_clone(L->as.call.args[0]);
      ast_free(node);
      AstNode *args[] = {arg};
      AstNode *t = ast_func_call("tan", 3, args, 1);
      return ast_binop(OP_DIV, ast_number(1), t);
    }
    /* sinh(u) / cosh(u) -> tanh(u) */
    if (is_call1(L, "sinh") && is_call1(R, "cosh") &&
        ast_equal(L->as.call.args[0], R->as.call.args[0])) {
      AstNode *arg = ast_clone(L->as.call.args[0]);
      ast_free(node);
      AstNode *args[] = {arg};
      return ast_func_call("tanh", 4, args, 1);
    }
    break;

  case OP_POW:
    if (is_inf_node(L) && is_number(R, 0)) {
      ast_free(node);
      return ast_undefined();
    }
    if (is_number(R, 0)) {
      ast_free(node);
      return ast_number(1);
    }
    if (is_number(R, 1)) {
      node->as.binop.left = NULL;
      ast_free(node);
      return L;
    }
    if (is_number(L, 0)) {
      ast_free(node);
      return ast_number(0);
    }
    if (is_number(L, 1)) {
      ast_free(node);
      return ast_number(1);
    }
    if (is_inf_node(R)) {
      int exp_sign = infinity_sign(R);
      if (is_inf_node(L)) {
        ast_free(node);
        return exp_sign > 0 ? ast_infinity(1) : ast_number(0);
      }

      double base = 0.0;
      if (positive_real_number_value(L, &base)) {
        ast_free(node);
        if (base > 1.0)
          return exp_sign > 0 ? ast_infinity(1) : ast_number(0);
        if (base < 1.0)
          return exp_sign > 0 ? ast_number(0) : ast_infinity(1);
        return ast_number(1);
      }

      ast_free(node);
      return ast_undefined();
    }
    if (is_inf_node(L) && is_num(R) && c_is_real(R->as.number)) {
      double exp = R->as.number.re;
      int sign = infinity_sign(L);
      ast_free(node);
      if (exp < 0.0)
        return ast_number(0);
      if (sign < 0 && exp == (int)exp && ((int)exp % 2 != 0))
        return ast_infinity(-1);
      return ast_infinity(1);
    }
    /* sqrt(x)^2 -> x only when x is known nonnegative; otherwise the rewrite
     * hides the original sqrt domain restriction. */
    if (is_number(R, 2) && is_call1(L, "sqrt") &&
        sia_known_nonnegative(L->as.call.args[0])) {
      AstNode *inner = ast_clone(L->as.call.args[0]);
      ast_free(node);
      return inner;
    }
    /* sqrt(x)^(2n) -> x^n under the same domain guard. */
    if (is_call1(L, "sqrt") && is_num(R) && c_is_real(R->as.number) &&
        R->as.number.re > 2 && R->as.number.re == (int)R->as.number.re &&
        (int)R->as.number.re % 2 == 0 &&
        sia_known_nonnegative(L->as.call.args[0])) {
      AstNode *inner = ast_clone(L->as.call.args[0]);
      int n = (int)R->as.number.re / 2;
      ast_free(node);
      return sym_simplify(ast_binop(OP_POW, inner, ast_number(n)));
    }
    /* (-x)^n -> x^n for even integer n */
    if (L->type == AST_UNARY_NEG && is_num(R) && c_is_real(R->as.number) &&
        R->as.number.re == (int)R->as.number.re &&
        (int)R->as.number.re % 2 == 0) {
      AstNode *inner = ast_clone(L->as.unary.operand);
      AstNode *exp = ast_clone(R);
      ast_free(node);
      return sym_simplify(ast_binop(OP_POW, inner, exp));
    }
    /* (x^a)^b -> x^(a*b) */
    if (L->type == AST_BINOP && L->as.binop.op == OP_POW) {
      AstNode *base = ast_clone(L->as.binop.left);
      AstNode *exp =
          ast_binop(OP_MUL, ast_clone(L->as.binop.right), ast_clone(R));
      ast_free(node);
      return sym_simplify(ast_binop(OP_POW, base, sym_simplify(exp)));
    }
    break;
  }

  return node;
}

static void flatten_add(AstNode *node, AstNode ***terms, size_t *count,
                        size_t *cap) {
  if (!node)
    return;
  if (node->type == AST_BINOP && node->as.binop.op == OP_ADD) {
    flatten_add(node->as.binop.left, terms, count, cap);
    flatten_add(node->as.binop.right, terms, count, cap);
  } else if (node->type == AST_BINOP && node->as.binop.op == OP_SUB) {
    flatten_add(node->as.binop.left, terms, count, cap);
    AstNode *neg = sym_simplify(ast_unary_neg(ast_clone(node->as.binop.right)));
    flatten_add(neg, terms, count, cap);
    ast_free(neg);
  } else if (node->type == AST_UNARY_NEG &&
             node->as.unary.operand->type == AST_BINOP &&
             node->as.unary.operand->as.binop.op == OP_ADD) {
    AstNode *neg_left = sym_simplify(
        ast_unary_neg(ast_clone(node->as.unary.operand->as.binop.left)));
    AstNode *neg_right = sym_simplify(
        ast_unary_neg(ast_clone(node->as.unary.operand->as.binop.right)));
    flatten_add(neg_left, terms, count, cap);
    flatten_add(neg_right, terms, count, cap);
    ast_free(neg_left);
    ast_free(neg_right);
  } else if (node->type == AST_UNARY_NEG &&
             node->as.unary.operand->type == AST_BINOP &&
             node->as.unary.operand->as.binop.op == OP_SUB) {
    AstNode *neg_left = sym_simplify(
        ast_unary_neg(ast_clone(node->as.unary.operand->as.binop.left)));
    AstNode *right =
        sym_simplify(ast_clone(node->as.unary.operand->as.binop.right));
    flatten_add(neg_left, terms, count, cap);
    flatten_add(right, terms, count, cap);
    ast_free(neg_left);
    ast_free(right);
  } else {
    if (*count >= *cap) {
      *cap *= 2;
      *terms = realloc(*terms, *cap * sizeof(AstNode *));
    }
    (*terms)[(*count)++] = ast_clone(node);
  }
}

static AstNode *try_cancel_common_add_factor(AstNode **terms, size_t count) {
  size_t active_count = 0;
  size_t k = 0;
  FactorizedTerm *infos = NULL;
  AstNode *common_base = NULL;
  AstNode *common_exp = NULL;
  AstNode *common_factor = NULL;
  AstNode *inner = NULL;
  AstNode *product = NULL;
  AstNode *normalized = NULL;

  for (size_t i = 0; i < count; i++)
    if (terms[i])
      active_count++;
  if (active_count < 2)
    return NULL;

  infos = calloc(active_count, sizeof(*infos));
  if (!infos)
    return NULL;

  for (size_t i = 0; i < count; i++) {
    if (!terms[i])
      continue;
    infos[k].coeff = c_real(1.0);
    if (!flatten_mul_factors(terms[i], &infos[k].coeff, &infos[k].factors)) {
      factorized_terms_free(infos, active_count);
      return NULL;
    }
    merge_like_factors(&infos[k].factors);
    k++;
  }

  for (size_t candidate = 0; candidate < infos[0].factors.count; candidate++) {
    AstNode *base = infos[0].factors.items[candidate].base;
    AstNode *exp = infos[0].factors.items[candidate].exp;
    int found = 1;

    if (!base || !exp || is_number(exp, 0))
      continue;

    for (size_t i = 1; i < active_count; i++) {
      if (find_power_factor(&infos[i].factors, base, exp) < 0) {
        found = 0;
        break;
      }
    }

    if (found) {
      common_base = ast_clone(base);
      common_exp = ast_clone(exp);
      break;
    }
  }

  if (!common_base || !common_exp) {
    factorized_terms_free(infos, active_count);
    return NULL;
  }

  common_factor = clone_factor_node(common_base, common_exp);
  if (!common_factor) {
    ast_free(common_base);
    ast_free(common_exp);
    factorized_terms_free(infos, active_count);
    return NULL;
  }

  for (size_t i = 0; i < active_count; i++) {
    int idx = find_power_factor(&infos[i].factors, common_base, common_exp);
    AstNode *residual = NULL;

    if (idx < 0) {
      ast_free(common_base);
      ast_free(common_exp);
      ast_free(common_factor);
      ast_free(inner);
      factorized_terms_free(infos, active_count);
      return NULL;
    }

    ast_free(infos[i].factors.items[idx].base);
    ast_free(infos[i].factors.items[idx].exp);
    infos[i].factors.items[idx].base = NULL;
    infos[i].factors.items[idx].exp = NULL;

    residual = build_factorized_term(infos[i].coeff, &infos[i].factors);
    if (!residual) {
      ast_free(common_base);
      ast_free(common_exp);
      ast_free(common_factor);
      ast_free(inner);
      factorized_terms_free(infos, active_count);
      return NULL;
    }

    if (!inner)
      inner = residual;
    else
      inner = ast_binop(OP_ADD, inner, residual);
  }

  inner = ast_canonicalize(inner);
  {
    AstNode *collected = sym_collect_terms(inner);
    ast_free(inner);
    inner = collected;
  }
  {
    AstNode *poly = ast_polynomial_canonicalize(inner);
    if (poly) {
      ast_free(inner);
      inner = poly;
    }
  }

  product = ast_binop(OP_MUL, inner, common_factor);
  inner = NULL;
  common_factor = NULL;
  normalized = normalize_mul_factors(product);
  ast_free(product);

  ast_free(common_base);
  ast_free(common_exp);
  factorized_terms_free(infos, active_count);

  if (!normalized)
    return NULL;

  if (normalized->type == AST_BINOP && normalized->as.binop.op == OP_MUL) {
    ast_free(normalized);
    return NULL;
  }

  return normalized;
}

AstNode *sym_collect_terms(AstNode *expr) {
  if (!expr)
    return NULL;
  size_t cap = 16, count = 0;
  AstNode **terms = malloc(cap * sizeof(AstNode *));
  flatten_add(expr, &terms, &count, &cap);

  for (size_t i = 0; i < count; i++) {
    if (!terms[i])
      continue;

    Complex c_i = c_real(1.0);
    AstNode *b_i = NULL;
    if (!sym_extract_coeff_and_base(terms[i], &c_i, &b_i))
      continue;

    for (size_t j = i + 1; j < count; j++) {
      if (!terms[j])
        continue;

      Complex c_j = c_real(1.0);
      AstNode *b_j = NULL;
      if (!sym_extract_coeff_and_base(terms[j], &c_j, &b_j)) {
        ast_free(b_j);
        continue;
      }

      int eq = 0;
      if (b_i == NULL && b_j == NULL)
        eq = 1;
      else if (b_i != NULL && b_j != NULL && ast_equal(b_i, b_j))
        eq = 1;

      if (eq) {
        c_i = c_add(c_i, c_j);
        ast_free(terms[j]);
        terms[j] = NULL;
      }

      ast_free(b_j);
    }

    AstNode *cloned_b_i = b_i ? ast_clone(b_i) : NULL;
    ast_free(b_i);
    ast_free(terms[i]);
    if (cloned_b_i) {
      if (c_is_zero(c_i)) {
        ast_free(cloned_b_i);
        terms[i] = ast_number(0);
      } else if (c_is_one(c_i)) {
        terms[i] = cloned_b_i;
      } else if (c_is_minus_one(c_i)) {
        terms[i] = sym_simplify(ast_unary_neg(cloned_b_i));
      } else {
        terms[i] = ast_binop(OP_MUL, ast_number_complex(c_i), cloned_b_i);
      }
    } else {
      terms[i] = ast_number_complex(c_i);
    }
  }

  /* trig identity: c*sin(u)^2 + c*cos(u)^2 -> c
   *                c*cos(u)^2 - c*sin(u)^2 -> c*cos(2*u) */
  for (size_t i = 0; i < count; i++) {
    if (!terms[i])
      continue;
    Complex c_i = c_real(1.0);
    AstNode *base_i = terms[i];
    for (;;) {
      if (base_i->type == AST_BINOP && base_i->as.binop.op == OP_MUL &&
          base_i->as.binop.left->type == AST_NUMBER) {
        c_i = c_mul(c_i, base_i->as.binop.left->as.number);
        base_i = base_i->as.binop.right;
      } else if (base_i->type == AST_UNARY_NEG) {
        c_i = c_neg(c_i);
        base_i = base_i->as.unary.operand;
      } else {
        break;
      }
    }
    if (!is_call1_squared(base_i, "sin") && !is_call1_squared(base_i, "cos"))
      continue;
    int i_is_sin = is_call1_squared(base_i, "sin");
    AstNode *arg_i = base_i->as.binop.left->as.call.args[0];

    for (size_t j = i + 1; j < count; j++) {
      if (!terms[j])
        continue;
      Complex c_j = c_real(1.0);
      AstNode *base_j = terms[j];
      for (;;) {
        if (base_j->type == AST_BINOP && base_j->as.binop.op == OP_MUL &&
            base_j->as.binop.left->type == AST_NUMBER) {
          c_j = c_mul(c_j, base_j->as.binop.left->as.number);
          base_j = base_j->as.binop.right;
        } else if (base_j->type == AST_UNARY_NEG) {
          c_j = c_neg(c_j);
          base_j = base_j->as.unary.operand;
        } else {
          break;
        }
      }
      int j_is_sin = is_call1_squared(base_j, "sin");
      int j_is_cos = is_call1_squared(base_j, "cos");
      if (!j_is_sin && !j_is_cos)
        continue;
      if (i_is_sin == j_is_sin)
        continue;
      AstNode *arg_j = base_j->as.binop.left->as.call.args[0];
      if (!ast_equal(arg_i, arg_j))
        continue;

      if (c_eq(c_i, c_j)) {
        /* c*sin^2(u) + c*cos^2(u) -> c */
        ast_free(terms[i]);
        ast_free(terms[j]);
        terms[i] = ast_number_complex(c_i);
        terms[j] = NULL;
        break;
      }

      /* c*cos^2(u) + (-c)*sin^2(u) -> c*cos(2*u) */
      Complex ci_c = i_is_sin ? c_j : c_i;
      Complex ci_s = i_is_sin ? c_i : c_j;
      Complex neg_ci_s = c_neg(ci_s);
      if (c_eq(ci_c, neg_ci_s)) {
        AstNode *two_u = ast_binop(OP_MUL, ast_number(2), ast_clone(arg_i));
        AstNode *cos2u = ast_func_call("cos", 3, (AstNode *[]){two_u}, 1);
        ast_free(terms[i]);
        ast_free(terms[j]);
        if (c_is_one(ci_c)) {
          terms[i] = cos2u;
        } else if (c_is_minus_one(ci_c)) {
          terms[i] = sym_simplify(ast_unary_neg(cos2u));
        } else {
          terms[i] = ast_binop(OP_MUL, ast_number_complex(ci_c), cos2u);
        }
        terms[j] = NULL;
        break;
      }
    }
  }

  /* merge constants that trig reduction may have introduced */
  Complex const_sum = c_real(0);
  int has_const = 0;
  for (size_t i = 0; i < count; i++) {
    if (terms[i] && terms[i]->type == AST_NUMBER) {
      const_sum = c_add(const_sum, terms[i]->as.number);
      ast_free(terms[i]);
      terms[i] = NULL;
      has_const = 1;
    }
  }
  if (has_const && !c_is_zero(const_sum)) {
    for (size_t i = 0; i < count; i++) {
      if (!terms[i]) {
        terms[i] = ast_number_complex(const_sum);
        break;
      }
    }
  }

  {
    AstNode *factored = try_cancel_common_add_factor(terms, count);
    if (factored) {
      for (size_t i = 0; i < count; i++)
        ast_free(terms[i]);
      free(terms);
      return ast_canonicalize(factored);
    }
  }

  AstNode *res = NULL;
  for (size_t i = 0; i < count; i++) {
    if (!terms[i])
      continue;
    if (!res) {
      res = terms[i];
    } else {
      res = ast_binop(OP_ADD, res, terms[i]);
    }
  }
  free(terms);

  if (!res)
    return ast_number(0);
  return ast_canonicalize(res);
}

static AstNode *simplify_subexpressions(AstNode *node) {
  if (!node)
    return NULL;
  switch (node->type) {
  case AST_NUMBER:
  case AST_VARIABLE:
  case AST_INFINITY:
  case AST_UNDEFINED:
    return node;
  case AST_MATRIX: {
    size_t total = node->as.matrix.rows * node->as.matrix.cols;
    for (size_t i = 0; i < total; i++)
      node->as.matrix.elements[i] =
          simplify_subexpressions(node->as.matrix.elements[i]);
    return node;
  }
  case AST_UNARY_NEG:
    node->as.unary.operand = simplify_subexpressions(node->as.unary.operand);
    return node;
  case AST_FUNC_CALL:
    for (size_t i = 0; i < node->as.call.nargs; i++)
      node->as.call.args[i] = simplify_subexpressions(node->as.call.args[i]);
    return node;
  case AST_LIMIT:
    node->as.limit.expr = simplify_subexpressions(node->as.limit.expr);
    node->as.limit.target = simplify_subexpressions(node->as.limit.target);
    return node;
  case AST_EQ:
    node->as.eq.lhs = simplify_subexpressions(node->as.eq.lhs);
    node->as.eq.rhs = simplify_subexpressions(node->as.eq.rhs);
    return node;
  case AST_BINOP:
    break;
  }
  node->as.binop.left = simplify_subexpressions(node->as.binop.left);
  node->as.binop.right = simplify_subexpressions(node->as.binop.right);

  if (node->as.binop.op == OP_POW || node->as.binop.op == OP_MUL) {
    AstNode *poly = ast_polynomial_canonicalize(node->as.binop.left);
    if (poly) {
      ast_free(node->as.binop.left);
      node->as.binop.left = poly;
    } else {
      AstNode *coll = sym_collect_terms(node->as.binop.left);
      if (coll) {
        ast_free(node->as.binop.left);
        node->as.binop.left = coll;
      }
    }
    poly = ast_polynomial_canonicalize(node->as.binop.right);
    if (poly) {
      ast_free(node->as.binop.right);
      node->as.binop.right = poly;
    }
  }
  return node;
}

static AstNode *extract_numerator_excluding(const AstNode *term, Complex *coeff,
                                            const AstNode *excl_base,
                                            const AstNode *excl_exp) {
  PowerFactorList factors = {0};
  AstNode *result = NULL;
  int excl_idx;

  *coeff = c_real(1.0);
  if (!flatten_mul_factors(term, coeff, &factors)) {
    power_factor_list_free(&factors);
    return NULL;
  }
  merge_like_factors(&factors);

  excl_idx = find_power_factor(&factors, excl_base, excl_exp);

  {
    PowerFactorList numer = {0};
    for (size_t f = 0; f < factors.count; f++) {
      if ((int)f == excl_idx)
        continue;
      power_factor_list_push(&numer, ast_clone(factors.items[f].base),
                             ast_clone(factors.items[f].exp));
    }
    if (numer.count == 0) {
      result = ast_number(1);
    } else {
      NodeList nl = {0};
      append_factor_nodes(&numer, &nl);
      result = build_mul_chain(c_real(1.0), &nl);
      node_list_free(&nl);
    }
    power_factor_list_free(&numer);
  }

  power_factor_list_free(&factors);
  return result;
}

static AstNode *combine_common_denominators(AstNode *node) {
  size_t cap = 16, count = 0;
  int did_combine = 0;
  AstNode **terms = malloc(cap * sizeof(AstNode *));
  flatten_add(node, &terms, &count, &cap);

  for (size_t i = 0; i < count; i++) {
    if (!terms[i])
      continue;
    Complex coeff_i = c_real(1.0);
    PowerFactorList factors_i = {0};
    if (!flatten_mul_factors(terms[i], &coeff_i, &factors_i)) {
      power_factor_list_free(&factors_i);
      continue;
    }
    merge_like_factors(&factors_i);

    AstNode *denom_base = NULL;
    AstNode *denom_exp = NULL;
    int denom_idx = -1;
    for (size_t f = 0; f < factors_i.count; f++) {
      if (factors_i.items[f].exp &&
          factors_i.items[f].exp->type == AST_NUMBER &&
          c_is_real(factors_i.items[f].exp->as.number) &&
          factors_i.items[f].exp->as.number.re < 0) {
        denom_base = factors_i.items[f].base;
        denom_exp = factors_i.items[f].exp;
        denom_idx = (int)f;
        break;
      }
    }

    if (denom_idx < 0) {
      power_factor_list_free(&factors_i);
      continue;
    }

    AstNode *numer_sum =
        extract_numerator_excluding(terms[i], &coeff_i, denom_base, denom_exp);
    if (!numer_sum) {
      power_factor_list_free(&factors_i);
      continue;
    }
    if (!c_is_one(coeff_i))
      numer_sum = ast_binop(OP_MUL, ast_number_complex(coeff_i), numer_sum);

    int any_combined = 0;
    for (size_t j = i + 1; j < count; j++) {
      if (!terms[j])
        continue;
      Complex coeff_j = c_real(1.0);
      PowerFactorList factors_j = {0};
      if (!flatten_mul_factors(terms[j], &coeff_j, &factors_j)) {
        power_factor_list_free(&factors_j);
        continue;
      }
      merge_like_factors(&factors_j);

      int match = find_power_factor(&factors_j, denom_base, denom_exp);
      if (match < 0) {
        power_factor_list_free(&factors_j);
        continue;
      }

      AstNode *numer_j = extract_numerator_excluding(terms[j], &coeff_j,
                                                     denom_base, denom_exp);
      if (!numer_j) {
        power_factor_list_free(&factors_j);
        continue;
      }
      if (!c_is_one(coeff_j))
        numer_j = ast_binop(OP_MUL, ast_number_complex(coeff_j), numer_j);

      numer_sum = ast_binop(OP_ADD, numer_sum, numer_j);
      ast_free(terms[j]);
      terms[j] = NULL;
      any_combined = 1;
      did_combine = 1;

      power_factor_list_free(&factors_j);
    }

    if (any_combined) {
      AstNode *denom_factor = clone_factor_node(denom_base, denom_exp);
      ast_free(terms[i]);
      numer_sum = sym_simplify(numer_sum);
      terms[i] = ast_binop(OP_MUL, numer_sum, denom_factor);
    } else {
      ast_free(numer_sum);
    }

    power_factor_list_free(&factors_i);
  }

  if (!did_combine) {
    for (size_t i = 0; i < count; i++)
      ast_free(terms[i]);
    free(terms);
    return NULL;
  }

  AstNode *res = NULL;
  for (size_t i = 0; i < count; i++) {
    if (!terms[i])
      continue;
    res = res ? ast_binop(OP_ADD, res, terms[i]) : terms[i];
  }
  free(terms);
  return res ? res : ast_number(0);
}

static AstNode *try_rational_reduce(AstNode *node) {
  if (!node)
    return NULL;

  Complex coeff = c_real(1.0);
  PowerFactorList factors = {0};
  if (!flatten_mul_factors(node, &coeff, &factors)) {
    power_factor_list_free(&factors);
    return NULL;
  }
  merge_like_factors(&factors);

  AstNode *numer_ast = NULL;
  AstNode *denom_ast = NULL;

  PowerFactorList numer_factors = {0};
  PowerFactorList denom_factors = {0};

  for (size_t i = 0; i < factors.count; i++) {
    if (factors.items[i].exp && factors.items[i].exp->type == AST_NUMBER &&
        c_is_real(factors.items[i].exp->as.number) &&
        factors.items[i].exp->as.number.re < 0) {
      double neg_exp = -factors.items[i].exp->as.number.re;
      if (neg_exp == (int)neg_exp && neg_exp >= 1) {
        AstNode *pos_exp = ast_number(neg_exp);
        AstNode *base = ast_clone(factors.items[i].base);
        if (neg_exp == 1.0) {
          denom_ast = denom_ast ? ast_binop(OP_MUL, denom_ast, base) : base;
        } else {
          AstNode *raised = ast_binop(OP_POW, base, pos_exp);
          denom_ast = denom_ast ? ast_binop(OP_MUL, denom_ast, raised) : raised;
        }
      } else {
        power_factor_list_push(&numer_factors, ast_clone(factors.items[i].base),
                               ast_clone(factors.items[i].exp));
      }
    } else {
      power_factor_list_push(&numer_factors, ast_clone(factors.items[i].base),
                             ast_clone(factors.items[i].exp));
    }
  }

  if (!denom_ast) {
    power_factor_list_free(&factors);
    power_factor_list_free(&numer_factors);
    power_factor_list_free(&denom_factors);
    return NULL;
  }

  {
    NodeList nl = {0};
    append_factor_nodes(&numer_factors, &nl);
    numer_ast = build_mul_chain(coeff, &nl);
    node_list_free(&nl);
  }

  if (!numer_ast)
    numer_ast = ast_number_complex(coeff);

  AstNode *reduced = ast_poly_gcd_reduce(numer_ast, denom_ast);

  if (!reduced && numer_factors.count > 1) {
    PowerFactorList poly_factors = {0};
    PowerFactorList nonpoly_factors = {0};

    for (size_t i = 0; i < numer_factors.count; i++) {
      AstNode *fnode = build_factor_node_clone(&numer_factors.items[i]);
      AstNode *test = ast_polynomial_canonicalize(fnode);
      if (test) {
        ast_free(test);
        power_factor_list_push(&poly_factors,
                               ast_clone(numer_factors.items[i].base),
                               ast_clone(numer_factors.items[i].exp));
      } else {
        power_factor_list_push(&nonpoly_factors,
                               ast_clone(numer_factors.items[i].base),
                               ast_clone(numer_factors.items[i].exp));
      }
      ast_free(fnode);
    }

    if (poly_factors.count > 0 && nonpoly_factors.count > 0) {
      AstNode *poly_numer = NULL;
      {
        NodeList nl = {0};
        append_factor_nodes(&poly_factors, &nl);
        poly_numer = build_mul_chain(coeff, &nl);
        node_list_free(&nl);
      }
      if (!poly_numer)
        poly_numer = ast_number_complex(coeff);

      AstNode *poly_reduced = ast_poly_gcd_reduce(poly_numer, denom_ast);
      if (poly_reduced) {
        NodeList nl = {0};
        append_factor_nodes(&nonpoly_factors, &nl);
        AstNode *nonpoly_part = build_mul_chain(c_real(1.0), &nl);
        node_list_free(&nl);

        if (nonpoly_part)
          reduced = ast_binop(OP_MUL, poly_reduced, nonpoly_part);
        else
          reduced = poly_reduced;
      }
      ast_free(poly_numer);
    }

    power_factor_list_free(&poly_factors);
    power_factor_list_free(&nonpoly_factors);
  }

  if (!reduced && numer_ast &&
      (numer_ast->type == AST_BINOP && (numer_ast->as.binop.op == OP_ADD ||
                                        numer_ast->as.binop.op == OP_SUB))) {
    AstNode **terms = NULL;
    size_t tcount = 0, tcap = 4;
    terms = malloc(tcap * sizeof(AstNode *));
    flatten_add(numer_ast, &terms, &tcount, &tcap);

    if (tcount >= 2) {
      Complex tcoeff0 = c_real(1.0);
      PowerFactorList tfactors0 = {0};
      flatten_mul_factors(terms[0], &tcoeff0, &tfactors0);
      merge_like_factors(&tfactors0);

      PowerFactorList common = {0};
      for (size_t f = 0; f < tfactors0.count; f++) {
        if (!tfactors0.items[f].base)
          continue;
        AstNode *test = ast_polynomial_canonicalize(
            build_factor_node_clone(&tfactors0.items[f]));
        if (test) {
          ast_free(test);
          continue;
        }

        int present_in_all = 1;
        for (size_t t = 1; t < tcount && present_in_all; t++) {
          Complex tc = c_real(1.0);
          PowerFactorList tf = {0};
          flatten_mul_factors(terms[t], &tc, &tf);
          merge_like_factors(&tf);
          if (find_power_factor(&tf, tfactors0.items[f].base,
                                tfactors0.items[f].exp) < 0)
            present_in_all = 0;
          power_factor_list_free(&tf);
        }

        if (present_in_all) {
          power_factor_list_push(&common, ast_clone(tfactors0.items[f].base),
                                 ast_clone(tfactors0.items[f].exp));
        }
      }
      power_factor_list_free(&tfactors0);

      if (common.count > 0) {
        AstNode *remainder_sum = NULL;
        for (size_t t = 0; t < tcount; t++) {
          Complex tc = c_real(1.0);
          PowerFactorList tf = {0};
          flatten_mul_factors(terms[t], &tc, &tf);
          merge_like_factors(&tf);

          for (size_t c = 0; c < common.count; c++) {
            int idx = find_power_factor(&tf, common.items[c].base,
                                        common.items[c].exp);
            if (idx >= 0) {
              ast_free(tf.items[idx].base);
              ast_free(tf.items[idx].exp);
              tf.items[idx].base = NULL;
              tf.items[idx].exp = NULL;
            }
          }

          NodeList nl = {0};
          append_factor_nodes(&tf, &nl);
          AstNode *term_remainder = build_mul_chain(tc, &nl);
          node_list_free(&nl);
          power_factor_list_free(&tf);

          if (!term_remainder)
            term_remainder = ast_number_complex(tc);
          remainder_sum = remainder_sum
                              ? ast_binop(OP_ADD, remainder_sum, term_remainder)
                              : term_remainder;
        }

        AstNode *poly_part = sym_simplify(remainder_sum);
        AstNode *poly_reduced = ast_poly_gcd_reduce(poly_part, denom_ast);
        if (poly_reduced) {
          NodeList nl = {0};
          append_factor_nodes(&common, &nl);
          AstNode *common_part = build_mul_chain(c_real(1.0), &nl);
          node_list_free(&nl);

          if (common_part)
            reduced = ast_binop(OP_MUL, poly_reduced, common_part);
          else
            reduced = poly_reduced;
        }
        ast_free(poly_part);
      }
      power_factor_list_free(&common);
    }

    for (size_t i = 0; i < tcount; i++)
      ast_free(terms[i]);
    free(terms);
  }

  ast_free(numer_ast);
  ast_free(denom_ast);
  power_factor_list_free(&factors);
  power_factor_list_free(&numer_factors);
  power_factor_list_free(&denom_factors);

  return reduced;
}

static AstNode *sym_full_simplify_once(AstNode *node) {
  AstNode *expanded = NULL;
  AstNode *collected = NULL;
  AstNode *poly = NULL;

  if (!node)
    return NULL;

  node = sym_simplify(node);

  expanded = sym_expand(node);
  ast_free(node);
  node = expanded;

  node = simplify_subexpressions(node);

  node = ast_canonicalize(node);

  collected = sym_collect_terms(node);
  ast_free(node);
  node = collected;

  poly = ast_polynomial_canonicalize(node);
  if (poly) {
    ast_free(node);
    node = poly;
  }

  node = sym_simplify(node);

  {
    AstNode *combined = combine_common_denominators(node);
    if (combined) {
      ast_free(node);
      node = combined;

      AstNode *reduced = try_rational_reduce(node);
      if (reduced) {
        ast_free(node);
        node = reduced;
      }
    }
  }

  return node;
}

AstNode *sym_full_simplify(AstNode *node) {
  AstNode *current = NULL;

  if (!node)
    return NULL;

  if (node->type == AST_MATRIX) {
    size_t total = node->as.matrix.rows * node->as.matrix.cols;
    AstNode **elems = malloc(total * sizeof(AstNode *));
    for (size_t i = 0; i < total; i++)
      elems[i] = sym_full_simplify(ast_clone(node->as.matrix.elements[i]));
    AstNode *result =
        ast_matrix(elems, node->as.matrix.rows, node->as.matrix.cols);
    free(elems);
    ast_free(node);
    return result;
  }

  current = sym_full_simplify_once(node);
  for (int pass = 0; pass < 3; pass++) {
    AstNode *next = sym_full_simplify_once(ast_clone(current));
    if (ast_equal(current, next)) {
      ast_free(next);
      return current;
    }
    ast_free(current);
    current = next;
  }

  return current;
}

static AstNode *expand_pow(AstNode *base, int n) {
  if (n == 1)
    return sym_expand(base);
  AstNode *rest = expand_pow(base, n - 1);
  AstNode *A = ast_clone(base->as.binop.left);
  AstNode *B = ast_clone(base->as.binop.right);
  AstNode *t1 = ast_binop(OP_MUL, A, ast_clone(rest));
  AstNode *t2 = ast_binop(OP_MUL, B, rest);
  return sym_simplify(
      ast_binop(base->as.binop.op, sym_simplify(t1), sym_simplify(t2)));
}

static AstNode *sym_matrix_mul(const AstNode *m1, const AstNode *m2) {
  if (m1->type != AST_MATRIX || m2->type != AST_MATRIX)
    return NULL;
  if (m1->as.matrix.cols != m2->as.matrix.rows)
    return NULL;

  size_t rows = m1->as.matrix.rows;
  size_t cols = m2->as.matrix.cols;
  size_t inner = m1->as.matrix.cols;

  AstNode **elems = malloc(rows * cols * sizeof(AstNode *));
  for (size_t r = 0; r < rows; r++) {
    for (size_t c = 0; c < cols; c++) {
      AstNode *sum = NULL;
      for (size_t k = 0; k < inner; k++) {
        AstNode *left = ast_clone(m1->as.matrix.elements[r * inner + k]);
        AstNode *right = ast_clone(m2->as.matrix.elements[k * cols + c]);
        AstNode *prod = ast_binop(OP_MUL, left, right);
        if (!sum) {
          sum = prod;
        } else {
          sum = ast_binop(OP_ADD, sum, prod);
        }
      }
      AstNode *expanded = sym_expand(sum);
      expanded = ast_canonicalize(expanded);
      expanded = sym_collect_terms(expanded);
      expanded = sym_simplify(expanded);

      /* re-expand to catch OP_POW created by sym_simplify (A*A -> A^2) */
      expanded = sym_expand(expanded);
      expanded = ast_canonicalize(expanded);
      expanded = sym_collect_terms(expanded);
      elems[r * cols + c] = sym_simplify(expanded);
    }
  }
  AstNode *res = ast_matrix(elems, rows, cols);
  free(elems);
  return res;
}

AstNode *sym_expand(AstNode *node) {
  if (!node)
    return NULL;

  switch (node->type) {
  case AST_NUMBER:
  case AST_VARIABLE:
  case AST_INFINITY:
  case AST_UNDEFINED:
    return ast_clone(node);
  case AST_MATRIX: {
    size_t total = node->as.matrix.rows * node->as.matrix.cols;
    AstNode **elems = malloc(total * sizeof(AstNode *));
    for (size_t i = 0; i < total; i++)
      elems[i] = sym_expand(node->as.matrix.elements[i]);
    AstNode *m = ast_matrix(elems, node->as.matrix.rows, node->as.matrix.cols);
    free(elems);
    return m;
  }
  case AST_UNARY_NEG:
    return ast_unary_neg(sym_expand(node->as.unary.operand));
  case AST_FUNC_CALL: {
    AstNode **args = malloc(node->as.call.nargs * sizeof(AstNode *));
    for (size_t i = 0; i < node->as.call.nargs; i++)
      args[i] = sym_expand(node->as.call.args[i]);
    AstNode *c = ast_func_call(node->as.call.name, strlen(node->as.call.name),
                               args, node->as.call.nargs);
    free(args);
    return c;
  }
  case AST_LIMIT:
    return ast_limit_directed(sym_expand(node->as.limit.expr),
                              node->as.limit.var, strlen(node->as.limit.var),
                              sym_expand(node->as.limit.target),
                              node->as.limit.direction);
  case AST_EQ:
    return ast_eq(sym_expand(node->as.eq.lhs), sym_expand(node->as.eq.rhs));
  case AST_BINOP: {
    AstNode *L = sym_expand(node->as.binop.left);
    AstNode *R = sym_expand(node->as.binop.right);

    /* expand (A + B)^n and (A - B)^n for small integers n */
    if (node->as.binop.op == OP_POW && R->type == AST_NUMBER &&
        c_is_real(R->as.number) && L->type == AST_BINOP &&
        (L->as.binop.op == OP_ADD || L->as.binop.op == OP_SUB)) {
      int n = (int)R->as.number.re;
      if (n == R->as.number.re && n >= 2 && n <= 10) {
        AstNode *res = expand_pow(L, n);
        ast_free(L);
        ast_free(R);
        return res;
      }
    }

    /* expand (A * B)^n -> A^n * B^n for small positive integer n */
    if (node->as.binop.op == OP_POW && R->type == AST_NUMBER &&
        c_is_real(R->as.number) && L->type == AST_BINOP &&
        L->as.binop.op == OP_MUL) {
      int n = (int)R->as.number.re;
      if (n == R->as.number.re && n >= 2 && n <= 10) {
        AstNode *lhs = sym_expand(
            ast_binop(OP_POW, ast_clone(L->as.binop.left), ast_number(n)));
        AstNode *rhs = sym_expand(
            ast_binop(OP_POW, ast_clone(L->as.binop.right), ast_number(n)));
        ast_free(L);
        ast_free(R);
        return sym_simplify(ast_binop(OP_MUL, lhs, rhs));
      }
    }

    /* expand M^n for matrices */
    if (node->as.binop.op == OP_POW && R->type == AST_NUMBER &&
        c_is_real(R->as.number) && L->type == AST_MATRIX) {
      int n = (int)R->as.number.re;
      if (n == R->as.number.re && n >= 2 && n <= 10) {
        AstNode *res = ast_clone(L);
        for (int i = 1; i < n; i++) {
          AstNode *next = sym_matrix_mul(res, L);
          ast_free(res);
          res = next;
        }
        ast_free(L);
        ast_free(R);
        return res;
      }
    }

    return sym_simplify(ast_binop(node->as.binop.op, L, R));
  }
  }
  return NULL;
}

AstNode *sym_reduce_rational_subexprs(const AstNode *node) {
  if (!node)
    return NULL;

  if (node->type == AST_BINOP && node->as.binop.op == OP_DIV) {
    AstNode *ln = sym_reduce_rational_subexprs(node->as.binop.left);
    AstNode *rn = sym_reduce_rational_subexprs(node->as.binop.right);
    AstNode *reduced = ast_poly_gcd_reduce(ln, rn);
    if (reduced) {
      ast_free(ln);
      ast_free(rn);
      return reduced;
    }
    return ast_binop(OP_DIV, ln, rn);
  }

  if (node->type == AST_BINOP) {
    AstNode *ln = sym_reduce_rational_subexprs(node->as.binop.left);
    AstNode *rn = sym_reduce_rational_subexprs(node->as.binop.right);
    return ast_binop(node->as.binop.op, ln, rn);
  }

  if (node->type == AST_UNARY_NEG) {
    return ast_unary_neg(sym_reduce_rational_subexprs(node->as.unary.operand));
  }

  if (node->type == AST_FUNC_CALL) {
    AstNode **args = malloc(node->as.call.nargs * sizeof(AstNode *));
    for (size_t i = 0; i < node->as.call.nargs; i++)
      args[i] = sym_reduce_rational_subexprs(node->as.call.args[i]);
    AstNode *r = ast_func_call(node->as.call.name, strlen(node->as.call.name),
                               args, node->as.call.nargs);
    free(args);
    return r;
  }

  if (node->type == AST_LIMIT) {
    return ast_limit_directed(
        sym_reduce_rational_subexprs(node->as.limit.expr), node->as.limit.var,
        strlen(node->as.limit.var),
        sym_reduce_rational_subexprs(node->as.limit.target),
        node->as.limit.direction);
  }

  return ast_clone(node);
}
