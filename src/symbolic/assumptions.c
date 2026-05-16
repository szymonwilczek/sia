#include "sia/assumptions.h"
#include <math.h>
#include <string.h>

static int is_nonnegative_integer(const AstNode *expr, long long *out) {
  double rounded = 0.0;

  if (!expr || expr->type != AST_NUMBER || !c_is_real(expr->as.number))
    return 0;

  rounded = round(expr->as.number.re);
  if (fabs(expr->as.number.re - rounded) > 1e-9 || rounded < 0.0)
    return 0;

  if (out)
    *out = (long long)rounded;
  return 1;
}

static int sign_is_nonnegative(SiaSign sign) {
  return sign == SIA_SIGN_ZERO || sign == SIA_SIGN_POSITIVE ||
         sign == SIA_SIGN_NONNEGATIVE;
}

static int sign_is_positive(SiaSign sign) { return sign == SIA_SIGN_POSITIVE; }

static int sign_is_nonpositive(SiaSign sign) {
  return sign == SIA_SIGN_ZERO || sign == SIA_SIGN_NEGATIVE ||
         sign == SIA_SIGN_NONPOSITIVE;
}

static SiaSign negate_sign(SiaSign sign) {
  switch (sign) {
  case SIA_SIGN_NEGATIVE:
    return SIA_SIGN_POSITIVE;
  case SIA_SIGN_ZERO:
    return SIA_SIGN_ZERO;
  case SIA_SIGN_POSITIVE:
    return SIA_SIGN_NEGATIVE;
  case SIA_SIGN_NONNEGATIVE:
    return SIA_SIGN_NONPOSITIVE;
  case SIA_SIGN_NONPOSITIVE:
    return SIA_SIGN_NONNEGATIVE;
  case SIA_SIGN_UNKNOWN:
    break;
  }
  return SIA_SIGN_UNKNOWN;
}

static SiaSign multiply_signs(SiaSign left, SiaSign right) {
  if (left == SIA_SIGN_ZERO || right == SIA_SIGN_ZERO)
    return SIA_SIGN_ZERO;
  if (left == SIA_SIGN_POSITIVE)
    return right;
  if (right == SIA_SIGN_POSITIVE)
    return left;
  if (left == SIA_SIGN_NEGATIVE)
    return negate_sign(right);
  if (right == SIA_SIGN_NEGATIVE)
    return negate_sign(left);
  if (sign_is_nonnegative(left) && sign_is_nonnegative(right))
    return SIA_SIGN_NONNEGATIVE;
  if (sign_is_nonpositive(left) && sign_is_nonpositive(right))
    return SIA_SIGN_NONNEGATIVE;
  if ((sign_is_nonnegative(left) && sign_is_nonpositive(right)) ||
      (sign_is_nonpositive(left) && sign_is_nonnegative(right)))
    return SIA_SIGN_NONPOSITIVE;
  return SIA_SIGN_UNKNOWN;
}

SiaSign sia_known_sign(const AstNode *expr) {
  if (!expr)
    return SIA_SIGN_UNKNOWN;

  switch (expr->type) {
  case AST_NUMBER:
    if (!c_is_real(expr->as.number))
      return SIA_SIGN_UNKNOWN;
    if (fabs(expr->as.number.re) < 1e-12)
      return SIA_SIGN_ZERO;
    return expr->as.number.re > 0.0 ? SIA_SIGN_POSITIVE : SIA_SIGN_NEGATIVE;

  case AST_INFINITY:
    return expr->as.infinity.sign > 0 ? SIA_SIGN_POSITIVE : SIA_SIGN_NEGATIVE;

  case AST_VARIABLE:
    if (strcmp(expr->as.variable, "e") == 0 ||
        strcmp(expr->as.variable, "pi") == 0)
      return SIA_SIGN_POSITIVE;
    return SIA_SIGN_UNKNOWN;

  case AST_UNARY_NEG:
    return negate_sign(sia_known_sign(expr->as.unary.operand));

  case AST_FUNC_CALL:
    if (expr->as.call.nargs == 1) {
      if (strcmp(expr->as.call.name, "abs") == 0 ||
          strcmp(expr->as.call.name, "sqrt") == 0)
        return SIA_SIGN_NONNEGATIVE;
      if (strcmp(expr->as.call.name, "exp") == 0)
        return SIA_SIGN_POSITIVE;
    }
    return SIA_SIGN_UNKNOWN;

  case AST_BINOP:
    switch (expr->as.binop.op) {
    case OP_ADD: {
      SiaSign left = sia_known_sign(expr->as.binop.left);
      SiaSign right = sia_known_sign(expr->as.binop.right);
      if (sign_is_positive(left) && sign_is_positive(right))
        return SIA_SIGN_POSITIVE;
      if (sign_is_nonnegative(left) && sign_is_nonnegative(right))
        return SIA_SIGN_NONNEGATIVE;
      if (left == SIA_SIGN_NEGATIVE && right == SIA_SIGN_NEGATIVE)
        return SIA_SIGN_NEGATIVE;
      if (sign_is_nonpositive(left) && sign_is_nonpositive(right))
        return SIA_SIGN_NONPOSITIVE;
      break;
    }
    case OP_SUB: {
      SiaSign left = sia_known_sign(expr->as.binop.left);
      SiaSign right = negate_sign(sia_known_sign(expr->as.binop.right));
      if (sign_is_positive(left) && sign_is_positive(right))
        return SIA_SIGN_POSITIVE;
      if (sign_is_nonnegative(left) && sign_is_nonnegative(right))
        return SIA_SIGN_NONNEGATIVE;
      if (left == SIA_SIGN_NEGATIVE && right == SIA_SIGN_NEGATIVE)
        return SIA_SIGN_NEGATIVE;
      if (sign_is_nonpositive(left) && sign_is_nonpositive(right))
        return SIA_SIGN_NONPOSITIVE;
      break;
    }
    case OP_MUL:
      return multiply_signs(sia_known_sign(expr->as.binop.left),
                            sia_known_sign(expr->as.binop.right));
    case OP_DIV: {
      SiaSign denominator = sia_known_sign(expr->as.binop.right);
      if (denominator == SIA_SIGN_ZERO || denominator == SIA_SIGN_NONNEGATIVE ||
          denominator == SIA_SIGN_NONPOSITIVE)
        return SIA_SIGN_UNKNOWN;
      return multiply_signs(sia_known_sign(expr->as.binop.left), denominator);
    }
    case OP_POW: {
      long long exponent = 0;
      SiaSign base = sia_known_sign(expr->as.binop.left);
      if (!is_nonnegative_integer(expr->as.binop.right, &exponent))
        return SIA_SIGN_UNKNOWN;
      if (exponent == 0)
        return SIA_SIGN_POSITIVE;
      if (exponent % 2 == 0)
        return base == SIA_SIGN_ZERO ? SIA_SIGN_ZERO : SIA_SIGN_NONNEGATIVE;
      if (base == SIA_SIGN_POSITIVE || base == SIA_SIGN_NEGATIVE ||
          base == SIA_SIGN_ZERO)
        return base;
      if (base == SIA_SIGN_NONNEGATIVE)
        return SIA_SIGN_NONNEGATIVE;
      if (base == SIA_SIGN_NONPOSITIVE)
        return SIA_SIGN_NONPOSITIVE;
      break;
    }
    }
    return SIA_SIGN_UNKNOWN;

  case AST_LIMIT:
  case AST_MATRIX:
  case AST_EQ:
  case AST_UNDEFINED:
    return SIA_SIGN_UNKNOWN;
  }

  return SIA_SIGN_UNKNOWN;
}

int sia_known_nonnegative(const AstNode *expr) {
  return sign_is_nonnegative(sia_known_sign(expr));
}

int sia_known_positive(const AstNode *expr) {
  return sign_is_positive(sia_known_sign(expr));
}
