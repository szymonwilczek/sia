#include "limits.h"
#include "eval.h"
#include "symbolic.h"
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
  return node && node->type == AST_VARIABLE &&
         strcmp(node->as.variable, "inf") == 0;
}

static int is_negative_inf_node(const AstNode *node) {
  return node && node->type == AST_UNARY_NEG &&
         is_positive_inf_node(node->as.unary.operand);
}

static int is_inf_node(const AstNode *node) {
  return is_positive_inf_node(node) || is_negative_inf_node(node);
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
                                                AstNode **out) {
  AstNode *num = NULL;
  AstNode *den = NULL;
  if (!split_fraction(expr, &num, &den))
    return LIMIT_UNSUPPORTED;

  LimitClass num_class = classify_at(num, var, target);
  LimitClass den_class = classify_at(den, var, target);
  ast_free(num);
  ast_free(den);

  if ((num_class == LIMIT_CLASS_ZERO && den_class == LIMIT_CLASS_ZERO) ||
      (num_class == LIMIT_CLASS_INF && den_class == LIMIT_CLASS_INF))
    return LIMIT_INDETERMINATE;

  if (num_class == LIMIT_CLASS_FINITE && den_class == LIMIT_CLASS_ZERO) {
    *out = ast_variable("inf", 3);
    return LIMIT_DIRECT_OK;
  }

  return LIMIT_UNSUPPORTED;
}

static LimitStatus direct_substitution(const AstNode *expr, const char *var,
                                       const AstNode *target, AstNode **out) {
  LimitStatus fraction_status =
      direct_fraction_substitution(expr, var, target, out);
  if (fraction_status != LIMIT_UNSUPPORTED)
    return fraction_status;

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
    *out = simplified;
    return LIMIT_DIRECT_OK;
  }

  if (is_inf_reciprocal_form(simplified)) {
    ast_free(simplified);
    *out = ast_number(0);
    return LIMIT_DIRECT_OK;
  }

  if (is_inf_node(simplified) || contains_inf(simplified)) {
    ast_free(simplified);
    *out = ast_variable("inf", 3);
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
                               const AstNode *target, int depth);

static AstNode *limit_inner(const AstNode *expr, const char *var,
                            const AstNode *target, int depth) {
  AstNode *normalized = sym_full_simplify(ast_clone(expr));
  AstNode *direct = NULL;
  LimitStatus status;

  if (!normalized || depth > 8) {
    ast_free(normalized);
    return NULL;
  }

  status = direct_substitution(normalized, var, target, &direct);
  if (status == LIMIT_DIRECT_OK) {
    ast_free(normalized);
    return direct;
  }

  if (status == LIMIT_INDETERMINATE) {
    AstNode *result = limit_lhopital(normalized, var, target, depth + 1);
    ast_free(normalized);
    return result;
  }

  ast_free(normalized);
  return NULL;
}

static AstNode *limit_lhopital(const AstNode *expr, const char *var,
                               const AstNode *target, int depth) {
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
  AstNode *result = limit_inner(quotient, var, target, depth);
  ast_free(quotient);
  return result;
}

AstNode *sym_limit(const AstNode *expr, const char *var,
                   const AstNode *target) {
  if (!expr || !var || !target)
    return NULL;

  return limit_inner(expr, var, target, 0);
}
