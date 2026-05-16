#include "sia/limits.h"
#include "sia/eval.h"
#include "sia/symbolic.h"
#include "sia/symtab.h"
#include "symbolic_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  LIMIT_DIRECT_OK,
  LIMIT_INDETERMINATE,
  LIMIT_UNSUPPORTED
} LimitStatus;

typedef enum {
  LIMIT_CLASS_ZERO,
  LIMIT_CLASS_FINITE,
  LIMIT_CLASS_INF,
  LIMIT_CLASS_OTHER
} LimitClass;

static int is_zero_number(const AstNode *node) {
  return node && node->type == AST_NUMBER && c_is_zero(node->as.number);
}

static int is_finite_number(const AstNode *node) {
  return node && node->type == AST_NUMBER && isfinite(node->as.number.re) &&
         isfinite(node->as.number.im);
}

static int is_nan_number(const AstNode *node) {
  return node && node->type == AST_NUMBER &&
         (isnan(node->as.number.re) || isnan(node->as.number.im));
}

static int is_positive_inf_node(const AstNode *node) {
  return node && ((node->type == AST_INFINITY && node->as.infinity.sign > 0) ||
                  (node->type == AST_VARIABLE &&
                   strcmp(node->as.variable, "inf") == 0));
}

static int is_negative_inf_node(const AstNode *node) {
  return node && ((node->type == AST_INFINITY && node->as.infinity.sign < 0) ||
                  (node->type == AST_UNARY_NEG &&
                   is_positive_inf_node(node->as.unary.operand)));
}

static int is_inf_node(const AstNode *node) {
  return is_positive_inf_node(node) || is_negative_inf_node(node);
}

static int inf_sign(const AstNode *node) {
  if (is_negative_inf_node(node))
    return -1;
  if (is_positive_inf_node(node))
    return 1;
  return 0;
}

static int finite_real_number(const AstNode *node, double *out) {
  if (!node || node->type != AST_NUMBER || !c_is_real(node->as.number) ||
      !isfinite(node->as.number.re))
    return 0;
  if (out)
    *out = node->as.number.re;
  return 1;
}

static AstNode *limit_number(double value) {
  double rounded = round(value);
  if (isfinite(value) && fabs(value - rounded) < 1e-9)
    return ast_number(rounded);
  return ast_number(value);
}

static int contains_inf(const AstNode *node) {
  if (!node)
    return 0;

  switch (node->type) {
  case AST_NUMBER:
    return isinf(node->as.number.re) || isinf(node->as.number.im);
  case AST_VARIABLE:
    return strcmp(node->as.variable, "inf") == 0;
  case AST_BINOP:
    return contains_inf(node->as.binop.left) ||
           contains_inf(node->as.binop.right);
  case AST_UNARY_NEG:
    return contains_inf(node->as.unary.operand);
  case AST_FUNC_CALL:
    for (size_t i = 0; i < node->as.call.nargs; i++)
      if (contains_inf(node->as.call.args[i]))
        return 1;
    return 0;
  case AST_LIMIT:
    return contains_inf(node->as.limit.expr) ||
           contains_inf(node->as.limit.target);
  case AST_MATRIX: {
    size_t total = node->as.matrix.rows * node->as.matrix.cols;
    for (size_t i = 0; i < total; i++)
      if (contains_inf(node->as.matrix.elements[i]))
        return 1;
    return 0;
  }
  case AST_EQ:
    return contains_inf(node->as.eq.lhs) || contains_inf(node->as.eq.rhs);
  case AST_INFINITY:
    return 1;
  case AST_UNDEFINED:
    return 0;
  }
  return 0;
}

static LimitClass classify_node(const AstNode *node) {
  if (is_zero_number(node))
    return LIMIT_CLASS_ZERO;
  if (is_finite_number(node))
    return LIMIT_CLASS_FINITE;
  if (is_inf_node(node) || contains_inf(node))
    return LIMIT_CLASS_INF;
  return LIMIT_CLASS_OTHER;
}

static int has_zero_denominator(const AstNode *node);

static int real_sign(double value) {
  if (value > 1e-12)
    return 1;
  if (value < -1e-12)
    return -1;
  return 0;
}

static int eval_real_at(const AstNode *expr, const char *var, double x,
                        double *out) {
  SymTab st;
  EvalResult result;
  if (!expr || !var || !out)
    return 0;

  symtab_init(&st);
  symtab_set(&st, var, c_real(x));
  result = eval(expr, &st);
  symtab_free(&st);
  if (!result.ok || !c_is_real(result.value) || !isfinite(result.value.re)) {
    eval_result_free(&result);
    return 0;
  }
  *out = result.value.re;
  eval_result_free(&result);
  return 1;
}

static int finite_target_value(const AstNode *target, double *out) {
  if (!target || target->type != AST_NUMBER || !c_is_real(target->as.number) ||
      !isfinite(target->as.number.re))
    return 0;
  *out = target->as.number.re;
  return 1;
}

static int sign_at(const AstNode *expr, const char *var,
                   const AstNode *target) {
  AstNode *sub = sym_subs(expr, var, target);
  AstNode *simplified = sub ? sym_full_simplify(sub) : NULL;
  int sign = 0;

  if (simplified && simplified->type == AST_NUMBER &&
      c_is_real(simplified->as.number))
    sign = real_sign(simplified->as.number.re);
  else if (simplified)
    sign = inf_sign(simplified);

  ast_free(simplified);
  return sign;
}

static int sign_near(const AstNode *expr, const char *var,
                     const AstNode *target, int direction) {
  double center = 0.0;
  double sample = 0.0;
  double value = 0.0;
  double step = 0.0;

  if (!finite_target_value(target, &center) || direction == 0)
    return 0;

  step = fmax(1e-6, fabs(center) * 1e-6);
  sample = center + (direction > 0 ? step : -step);
  if (!eval_real_at(expr, var, sample, &value))
    return 0;
  return real_sign(value);
}

static AstNode *directed_quotient_probe(const AstNode *expr, const char *var,
                                        const AstNode *target, int direction) {
  double center = 0.0;
  double step = 0.0;
  double v1 = 0.0;
  double v2 = 0.0;

  if (direction == 0 || !finite_target_value(target, &center))
    return NULL;

  step = fmax(1e-6, fabs(center) * 1e-6);
  if (!eval_real_at(expr, var, center + (direction > 0 ? step : -step), &v1) ||
      !eval_real_at(expr, var,
                    center + (direction > 0 ? step * 0.5 : -step * 0.5), &v2))
    return NULL;

  if (isfinite(v1) && isfinite(v2) && fabs(v1 - v2) < 1e-7)
    return limit_number(v2);

  return NULL;
}

static int real_pow_int(double base, int exp, double *out) {
  double result = 1.0;
  if (exp < 0)
    return 0;
  for (int i = 0; i < exp; i++)
    result *= base;
  if (!isfinite(result))
    return 0;
  *out = result;
  return 1;
}

static int polynomial_leading_term(const AstNode *expr, const char *var,
                                   int *degree, double *coeff) {
  if (!expr || !var || !degree || !coeff)
    return 0;

  switch (expr->type) {
  case AST_NUMBER:
    if (!finite_real_number(expr, coeff))
      return 0;
    *degree = 0;
    return 1;

  case AST_VARIABLE:
    if (strcmp(expr->as.variable, var) != 0)
      return 0;
    *degree = 1;
    *coeff = 1.0;
    return 1;

  case AST_UNARY_NEG:
    if (!polynomial_leading_term(expr->as.unary.operand, var, degree, coeff))
      return 0;
    *coeff = -*coeff;
    return 1;

  case AST_BINOP: {
    int ld = 0, rd = 0;
    double lc = 0.0, rc = 0.0;
    switch (expr->as.binop.op) {
    case OP_ADD:
    case OP_SUB:
      if (!polynomial_leading_term(expr->as.binop.left, var, &ld, &lc) ||
          !polynomial_leading_term(expr->as.binop.right, var, &rd, &rc))
        return 0;
      if (expr->as.binop.op == OP_SUB)
        rc = -rc;
      if (ld > rd) {
        *degree = ld;
        *coeff = lc;
        return 1;
      }
      if (rd > ld) {
        *degree = rd;
        *coeff = rc;
        return 1;
      }
      if (lc + rc == 0.0)
        return 0;
      *degree = ld;
      *coeff = lc + rc;
      return 1;

    case OP_MUL:
      if (!polynomial_leading_term(expr->as.binop.left, var, &ld, &lc) ||
          !polynomial_leading_term(expr->as.binop.right, var, &rd, &rc))
        return 0;
      *degree = ld + rd;
      *coeff = lc * rc;
      return isfinite(*coeff);

    case OP_POW: {
      double exp_value = 0.0;
      int exp = 0;
      if (!finite_real_number(expr->as.binop.right, &exp_value) ||
          exp_value < 0.0 || exp_value != (int)exp_value)
        return 0;
      exp = (int)exp_value;
      if (!polynomial_leading_term(expr->as.binop.left, var, &ld, &lc) ||
          !real_pow_int(lc, exp, coeff))
        return 0;
      *degree = ld * exp;
      return 1;
    }

    case OP_DIV:
      return 0;
    }
    return 0;
  }

  case AST_FUNC_CALL:
  case AST_LIMIT:
  case AST_MATRIX:
  case AST_EQ:
  case AST_INFINITY:
  case AST_UNDEFINED:
    return 0;
  }
  return 0;
}

static AstNode *limit_rational_polynomial_at_infinity(const AstNode *num,
                                                      const AstNode *den,
                                                      const char *var,
                                                      const AstNode *target) {
  int target_sign = inf_sign(target);
  int nd = 0, dd = 0;
  double nc = 0.0, dc = 0.0;

  if (target_sign == 0 || !polynomial_leading_term(num, var, &nd, &nc) ||
      !polynomial_leading_term(den, var, &dd, &dc) || dc == 0.0)
    return NULL;

  if (nd < dd)
    return ast_number(0);

  double ratio = nc / dc;
  if (!isfinite(ratio) || ratio == 0.0)
    return NULL;

  if (nd == dd)
    return limit_number(ratio);

  int sign = ratio < 0.0 ? -1 : 1;
  if (target_sign < 0 && ((nd - dd) % 2 != 0))
    sign = -sign;
  return ast_infinity(sign);
}

static AstNode *limit_polynomial_at_infinity(const AstNode *expr,
                                             const char *var,
                                             const AstNode *target) {
  int target_sign = inf_sign(target);
  int degree = 0;
  double coeff = 0.0;

  if (target_sign == 0 || !polynomial_leading_term(expr, var, &degree, &coeff))
    return NULL;

  if (degree == 0)
    return limit_number(coeff);
  if (coeff == 0.0)
    return NULL;

  int sign = coeff < 0.0 ? -1 : 1;
  if (target_sign < 0 && (degree % 2 != 0))
    sign = -sign;
  return ast_infinity(sign);
}

static AstNode *directed_abs_denominator_limit(const AstNode *num,
                                               const AstNode *den,
                                               const char *var,
                                               const AstNode *target,
                                               int direction) {
  if (direction == 0 || !den || den->type != AST_FUNC_CALL ||
      strcmp(den->as.call.name, "abs") != 0 || den->as.call.nargs != 1)
    return NULL;

  AstNode *arg = den->as.call.args[0];
  int sign = sign_near(arg, var, target, direction);
  if (sign == 0)
    return NULL;

  AstNode *signed_den =
      sign > 0 ? ast_clone(arg) : ast_unary_neg(ast_clone(arg));
  AstNode *quotient = ast_binop(OP_DIV, ast_clone(num), signed_den);
  AstNode *simplified = sym_full_simplify(quotient);
  AstNode *sub = simplified ? sym_subs(simplified, var, target) : NULL;
  AstNode *result = sub ? sym_full_simplify(sub) : NULL;

  ast_free(simplified);
  if (!result)
    return NULL;

  if (classify_node(result) == LIMIT_CLASS_OTHER ||
      has_zero_denominator(result)) {
    ast_free(result);
    return NULL;
  }

  return result;
}

static AstNode *directed_abs_numerator_limit(const AstNode *num,
                                             const AstNode *den,
                                             const char *var,
                                             const AstNode *target,
                                             int direction) {
  if (direction == 0 || !num || num->type != AST_FUNC_CALL ||
      strcmp(num->as.call.name, "abs") != 0 || num->as.call.nargs != 1)
    return NULL;

  AstNode *arg = num->as.call.args[0];
  int sign = sign_near(arg, var, target, direction);
  if (sign == 0)
    return NULL;

  AstNode *signed_num =
      sign > 0 ? ast_clone(arg) : ast_unary_neg(ast_clone(arg));
  AstNode *quotient = ast_binop(OP_DIV, signed_num, ast_clone(den));
  AstNode *simplified = sym_full_simplify(quotient);
  AstNode *sub = simplified ? sym_subs(simplified, var, target) : NULL;
  AstNode *result = sub ? sym_full_simplify(sub) : NULL;

  ast_free(simplified);
  if (!result)
    return NULL;

  if (classify_node(result) == LIMIT_CLASS_OTHER ||
      has_zero_denominator(result)) {
    ast_free(result);
    return NULL;
  }

  return result;
}

static int is_inf_reciprocal_form(const AstNode *node) {
  return node && node->type == AST_BINOP && node->as.binop.op == OP_POW &&
         is_inf_node(node->as.binop.left) &&
         node->as.binop.right->type == AST_NUMBER &&
         c_is_real(node->as.binop.right->as.number) &&
         node->as.binop.right->as.number.re < 0.0;
}

static LimitClass classify_for_indeterminate(const AstNode *node) {
  if (!node)
    return LIMIT_CLASS_OTHER;

  if (is_nan_number(node))
    return LIMIT_CLASS_INF;

  if (node->type == AST_BINOP && node->as.binop.op == OP_POW &&
      node->as.binop.left->type == AST_NUMBER &&
      c_is_zero(node->as.binop.left->as.number) &&
      node->as.binop.right->type == AST_NUMBER &&
      c_is_real(node->as.binop.right->as.number) &&
      node->as.binop.right->as.number.re < 0.0)
    return LIMIT_CLASS_INF;

  return classify_node(node);
}

static LimitClass classify_child_for_indeterminate(const AstNode *node) {
  LimitClass cls = classify_for_indeterminate(node);
  if (cls != LIMIT_CLASS_OTHER)
    return cls;

  AstNode *simplified = sym_full_simplify(ast_clone(node));
  cls = classify_for_indeterminate(simplified);
  ast_free(simplified);
  return cls;
}

static LimitClass classify_at(const AstNode *expr, const char *var,
                              const AstNode *target) {
  AstNode *sub = sym_subs(expr, var, target);
  if (!sub)
    return LIMIT_CLASS_OTHER;

  AstNode *simplified = sym_full_simplify(sub);
  LimitClass cls = classify_node(simplified);
  ast_free(simplified);
  return cls;
}

static int is_indeterminate_expr(const AstNode *node) {
  if (!node)
    return 0;

  if (node->type == AST_BINOP && node->as.binop.op == OP_DIV) {
    LimitClass lhs = classify_node(node->as.binop.left);
    LimitClass rhs = classify_node(node->as.binop.right);
    return (lhs == LIMIT_CLASS_ZERO && rhs == LIMIT_CLASS_ZERO) ||
           (lhs == LIMIT_CLASS_INF && rhs == LIMIT_CLASS_INF);
  }

  if (node->type == AST_BINOP && node->as.binop.op == OP_MUL) {
    LimitClass lhs = classify_node(node->as.binop.left);
    LimitClass rhs = classify_node(node->as.binop.right);
    return (lhs == LIMIT_CLASS_ZERO && rhs == LIMIT_CLASS_INF) ||
           (lhs == LIMIT_CLASS_INF && rhs == LIMIT_CLASS_ZERO);
  }

  return 0;
}

static int is_indeterminate_after_substitution(const AstNode *node) {
  if (!node)
    return 0;

  if (node->type == AST_BINOP && node->as.binop.op == OP_DIV) {
    LimitClass lhs = classify_child_for_indeterminate(node->as.binop.left);
    LimitClass rhs = classify_child_for_indeterminate(node->as.binop.right);
    return (lhs == LIMIT_CLASS_ZERO && rhs == LIMIT_CLASS_ZERO) ||
           (lhs == LIMIT_CLASS_INF && rhs == LIMIT_CLASS_INF);
  }

  if (node->type == AST_BINOP && node->as.binop.op == OP_MUL) {
    LimitClass lhs = classify_child_for_indeterminate(node->as.binop.left);
    LimitClass rhs = classify_child_for_indeterminate(node->as.binop.right);
    if ((lhs == LIMIT_CLASS_ZERO && rhs == LIMIT_CLASS_INF) ||
        (lhs == LIMIT_CLASS_INF && rhs == LIMIT_CLASS_ZERO))
      return 1;
  }

  if (node->type == AST_BINOP)
    return is_indeterminate_after_substitution(node->as.binop.left) ||
           is_indeterminate_after_substitution(node->as.binop.right);
  if (node->type == AST_UNARY_NEG)
    return is_indeterminate_after_substitution(node->as.unary.operand);
  return 0;
}

static int has_zero_denominator(const AstNode *node) {
  if (!node)
    return 0;

  if (node->type == AST_BINOP && node->as.binop.op == OP_DIV &&
      is_zero_number(node->as.binop.right))
    return 1;

  if (node->type == AST_BINOP)
    return has_zero_denominator(node->as.binop.left) ||
           has_zero_denominator(node->as.binop.right);
  if (node->type == AST_UNARY_NEG)
    return has_zero_denominator(node->as.unary.operand);
  if (node->type == AST_FUNC_CALL) {
    for (size_t i = 0; i < node->as.call.nargs; i++)
      if (has_zero_denominator(node->as.call.args[i]))
        return 1;
  }
  return 0;
}

static int split_fraction(const AstNode *expr, AstNode **num, AstNode **den);

static LimitStatus direct_fraction_substitution(const AstNode *expr,
                                                const char *var,
                                                const AstNode *target,
                                                int direction, AstNode **out) {
  AstNode *num = NULL;
  AstNode *den = NULL;
  if (!split_fraction(expr, &num, &den))
    return LIMIT_UNSUPPORTED;

  AstNode *poly_inf =
      limit_rational_polynomial_at_infinity(num, den, var, target);
  if (poly_inf) {
    ast_free(num);
    ast_free(den);
    *out = poly_inf;
    return LIMIT_DIRECT_OK;
  }

  LimitClass num_class = classify_at(num, var, target);
  LimitClass den_class = classify_at(den, var, target);
  if ((num_class == LIMIT_CLASS_ZERO && den_class == LIMIT_CLASS_ZERO) ||
      (num_class == LIMIT_CLASS_INF && den_class == LIMIT_CLASS_INF)) {
    AstNode *abs_result =
        directed_abs_denominator_limit(num, den, var, target, direction);
    if (abs_result) {
      ast_free(num);
      ast_free(den);
      *out = abs_result;
      return LIMIT_DIRECT_OK;
    }

    abs_result = directed_abs_numerator_limit(num, den, var, target, direction);
    if (abs_result) {
      ast_free(num);
      ast_free(den);
      *out = abs_result;
      return LIMIT_DIRECT_OK;
    }

    if (direction != 0 && num_class == LIMIT_CLASS_ZERO &&
        den_class == LIMIT_CLASS_ZERO && num->type == AST_FUNC_CALL &&
        strcmp(num->as.call.name, "abs") == 0 && num->as.call.nargs == 1 &&
        ast_equal(num->as.call.args[0], den)) {
      int sign = sign_near(den, var, target, direction);
      if (sign != 0) {
        ast_free(num);
        ast_free(den);
        *out = ast_number(sign);
        return LIMIT_DIRECT_OK;
      }
    }
    AstNode *probed = directed_quotient_probe(expr, var, target, direction);
    ast_free(num);
    ast_free(den);
    if (probed) {
      *out = probed;
      return LIMIT_DIRECT_OK;
    }
    return LIMIT_INDETERMINATE;
  }

  if (num_class == LIMIT_CLASS_FINITE && den_class == LIMIT_CLASS_ZERO) {
    int num_sign = sign_at(num, var, target);
    int den_sign = 0;
    if (direction != 0) {
      den_sign = sign_near(den, var, target, direction);
    } else {
      int left = sign_near(den, var, target, -1);
      int right = sign_near(den, var, target, 1);
      if (left != 0 && left == right)
        den_sign = left;
      else if (left != 0 && right != 0) {
        ast_free(num);
        ast_free(den);
        *out = ast_undefined();
        return LIMIT_DIRECT_OK;
      }
    }
    ast_free(num);
    ast_free(den);
    *out = ast_infinity(num_sign && den_sign ? num_sign * den_sign : 1);
    return LIMIT_DIRECT_OK;
  }

  if (num_class == LIMIT_CLASS_FINITE && den_class == LIMIT_CLASS_INF) {
    ast_free(num);
    ast_free(den);
    *out = ast_number(0);
    return LIMIT_DIRECT_OK;
  }

  ast_free(num);
  ast_free(den);
  return LIMIT_UNSUPPORTED;
}

static LimitStatus direct_substitution(const AstNode *expr, const char *var,
                                       const AstNode *target, int direction,
                                       AstNode **out) {
  LimitStatus fraction_status =
      direct_fraction_substitution(expr, var, target, direction, out);
  if (fraction_status != LIMIT_UNSUPPORTED)
    return fraction_status;

  AstNode *poly_inf = limit_polynomial_at_infinity(expr, var, target);
  if (poly_inf) {
    *out = poly_inf;
    return LIMIT_DIRECT_OK;
  }

  AstNode *sub = sym_subs(expr, var, target);
  if (!sub)
    return LIMIT_UNSUPPORTED;

  if (is_indeterminate_after_substitution(sub)) {
    ast_free(sub);
    return LIMIT_INDETERMINATE;
  }

  AstNode *simplified = sym_full_simplify(sub);
  if (!simplified)
    return LIMIT_UNSUPPORTED;

  if (is_indeterminate_expr(simplified)) {
    ast_free(simplified);
    return LIMIT_INDETERMINATE;
  }

  if (is_nan_number(simplified)) {
    ast_free(simplified);
    return LIMIT_INDETERMINATE;
  }

  if (is_finite_number(simplified)) {
    double value = 0.0;
    if (finite_real_number(simplified, &value)) {
      ast_free(simplified);
      *out = limit_number(value);
      return LIMIT_DIRECT_OK;
    }
    *out = simplified;
    return LIMIT_DIRECT_OK;
  }

  if (is_inf_reciprocal_form(simplified)) {
    ast_free(simplified);
    *out = ast_number(0);
    return LIMIT_DIRECT_OK;
  }

  if (is_inf_node(simplified)) {
    int sign = inf_sign(simplified);
    ast_free(simplified);
    *out = ast_infinity(sign);
    return LIMIT_DIRECT_OK;
  }

  if (contains_inf(simplified)) {
    ast_free(simplified);
    *out = ast_infinity(1);
    return LIMIT_DIRECT_OK;
  }

  if (!sym_contains_var(simplified, var) && !has_zero_denominator(simplified)) {
    *out = simplified;
    return LIMIT_DIRECT_OK;
  }

  ast_free(simplified);
  return LIMIT_UNSUPPORTED;
}

static AstNode *denominator_from_negative_power(const AstNode *node) {
  if (!node || node->type != AST_BINOP || node->as.binop.op != OP_POW ||
      node->as.binop.right->type != AST_NUMBER ||
      !c_is_real(node->as.binop.right->as.number))
    return NULL;

  double exp = node->as.binop.right->as.number.re;
  double pos = -exp;
  if (exp >= 0.0 || pos != (int)pos)
    return NULL;

  if (pos == 1.0)
    return ast_clone(node->as.binop.left);

  return ast_binop(OP_POW, ast_clone(node->as.binop.left), ast_number(pos));
}

static int split_inverse_product(const AstNode *expr, AstNode **num,
                                 AstNode **den) {
  AstNode *pow_den = denominator_from_negative_power(expr);
  if (pow_den) {
    *num = ast_number(1);
    *den = pow_den;
    return 1;
  }

  if (!expr || expr->type != AST_BINOP || expr->as.binop.op != OP_MUL)
    return 0;

  if (split_inverse_product(expr->as.binop.left, num, den)) {
    *num =
        sym_simplify(ast_binop(OP_MUL, *num, ast_clone(expr->as.binop.right)));
    return 1;
  }

  if (split_inverse_product(expr->as.binop.right, num, den)) {
    *num =
        sym_simplify(ast_binop(OP_MUL, ast_clone(expr->as.binop.left), *num));
    return 1;
  }

  return 0;
}

static int split_fraction(const AstNode *expr, AstNode **num, AstNode **den) {
  if (!expr || !num || !den)
    return 0;

  if (expr->type == AST_BINOP && expr->as.binop.op == OP_DIV) {
    *num = ast_clone(expr->as.binop.left);
    *den = ast_clone(expr->as.binop.right);
    return 1;
  }

  if (split_inverse_product(expr, num, den))
    return 1;

  return 0;
}

static AstNode *limit_lhopital(const AstNode *expr, const char *var,
                               const AstNode *target, int direction, int depth);

static AstNode *limit_inner(const AstNode *expr, const char *var,
                            const AstNode *target, int direction, int depth) {
  AstNode *direct = NULL;
  LimitStatus status;

  if (!expr || depth > 8)
    return NULL;

  status = direct_substitution(expr, var, target, direction, &direct);
  if (status == LIMIT_DIRECT_OK)
    return direct;

  if (status == LIMIT_INDETERMINATE) {
    AstNode *result = limit_lhopital(expr, var, target, direction, depth + 1);
    if (result)
      return result;
  }

  AstNode *normalized = sym_full_simplify(ast_clone(expr));
  if (!normalized)
    return NULL;

  status = direct_substitution(normalized, var, target, direction, &direct);
  if (status == LIMIT_DIRECT_OK) {
    ast_free(normalized);
    return direct;
  }

  if (status == LIMIT_INDETERMINATE) {
    AstNode *result =
        limit_lhopital(normalized, var, target, direction, depth + 1);
    ast_free(normalized);
    return result;
  }

  ast_free(normalized);
  return NULL;
}

static AstNode *limit_lhopital(const AstNode *expr, const char *var,
                               const AstNode *target, int direction,
                               int depth) {
  AstNode *num = NULL;
  AstNode *den = NULL;
  if (!split_fraction(expr, &num, &den))
    return NULL;

  LimitClass num_class = classify_at(num, var, target);
  LimitClass den_class = classify_at(den, var, target);
  if (!((num_class == LIMIT_CLASS_ZERO && den_class == LIMIT_CLASS_ZERO) ||
        (num_class == LIMIT_CLASS_INF && den_class == LIMIT_CLASS_INF))) {
    ast_free(num);
    ast_free(den);
    return NULL;
  }

  AstNode *dnum = sym_diff(num, var);
  AstNode *dden = sym_diff(den, var);
  ast_free(num);
  ast_free(den);
  if (!dnum || !dden) {
    ast_free(dnum);
    ast_free(dden);
    return NULL;
  }

  AstNode *quotient = ast_binop(OP_DIV, dnum, dden);
  AstNode *result = limit_inner(quotient, var, target, direction, depth);
  ast_free(quotient);
  return result;
}

AstNode *sym_limit(const AstNode *expr, const char *var,
                   const AstNode *target) {
  return sym_limit_directed(expr, var, target, 0);
}

AstNode *sym_limit_directed(const AstNode *expr, const char *var,
                            const AstNode *target, int direction) {
  if (!expr || !var || !target)
    return NULL;

  direction = direction < 0 ? -1 : (direction > 0 ? 1 : 0);
  return limit_inner(expr, var, target, direction, 0);
}
