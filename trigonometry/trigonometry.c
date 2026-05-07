#include "trigonometry.h"
#include "../symbolic.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

Complex c_sin(Complex z) {
  return c_make(sin(z.re) * cosh(z.im), cos(z.re) * sinh(z.im));
}

Complex c_cos(Complex z) {
  return c_make(cos(z.re) * cosh(z.im), -sin(z.re) * sinh(z.im));
}

Complex c_tan(Complex z) { return c_div(c_sin(z), c_cos(z)); }

Complex c_asin(Complex z) {
  /* asin(z) = -i * ln(iz + sqrt(1 - z^2)) */
  Complex i_ = c_make(0, 1);
  Complex mi = c_make(0, -1);
  Complex one = c_real(1);
  Complex arg = c_add(c_mul(i_, z), c_sqrt(c_sub(one, c_mul(z, z))));
  return c_mul(mi, c_log(arg));
}

Complex c_acos(Complex z) {
  /* acos(z) = pi/2 - asin(z) */
  Complex half_pi = c_real(M_PI / 2.0);
  return c_sub(half_pi, c_asin(z));
}

Complex c_atan(Complex z) {
  /* atan(z) = (i/2) * ln((1-iz)/(1+iz)) */
  Complex i_ = c_make(0, 1);
  Complex half_i = c_make(0, 0.5);
  Complex iz = c_mul(i_, z);
  Complex one = c_real(1);
  return c_mul(half_i, c_log(c_div(c_sub(one, iz), c_add(one, iz))));
}

TrigKind trig_kind(const AstNode *node) {
  if (!node || node->type != AST_FUNC_CALL || node->as.call.nargs != 1)
    return TRIG_KIND_NONE;

  const char *name = node->as.call.name;
  if (strcmp(name, "sin") == 0)
    return TRIG_KIND_SIN;
  if (strcmp(name, "cos") == 0)
    return TRIG_KIND_COS;
  if (strcmp(name, "tan") == 0)
    return TRIG_KIND_TAN;
  if (strcmp(name, "asin") == 0)
    return TRIG_KIND_ASIN;
  if (strcmp(name, "acos") == 0)
    return TRIG_KIND_ACOS;
  if (strcmp(name, "atan") == 0)
    return TRIG_KIND_ATAN;
  return TRIG_KIND_NONE;
}

Complex trig_eval_value(TrigKind kind, Complex value, int *ok, char **error) {
  if (ok)
    *ok = 1;
  if (error)
    *error = NULL;

  switch (kind) {
  case TRIG_KIND_SIN:
    return c_sin(value);
  case TRIG_KIND_COS:
    return c_cos(value);
  case TRIG_KIND_TAN:
    return c_tan(value);
  case TRIG_KIND_ASIN:
    return c_asin(value);
  case TRIG_KIND_ACOS:
    return c_acos(value);
  case TRIG_KIND_ATAN:
    return c_atan(value);
  default:
    if (ok)
      *ok = 0;
    if (error)
      *error = strdup("unknown trigonometric function");
    return c_real(0.0);
  }
}

static AstNode *pi_fraction(long long num, long long den) {
  AstNode *pi = ast_variable("pi", 2);
  if (den == 1)
    return num == 1 ? pi : ast_binop(OP_MUL, ast_number((double)num), pi);
  return ast_binop(
      OP_DIV, num == 1 ? pi : ast_binop(OP_MUL, ast_number((double)num), pi),
      ast_number((double)den));
}

static AstNode *maybe_special_inverse_trig(TrigKind kind, Complex value) {
  if (!c_is_real(value))
    return NULL;

  switch (kind) {
  case TRIG_KIND_ASIN:
    if (c_is_zero(value))
      return ast_number(0);
    if (c_is_one(value))
      return pi_fraction(1, 2);
    if (c_is_minus_one(value))
      return ast_unary_neg(pi_fraction(1, 2));
    break;
  case TRIG_KIND_ACOS:
    if (c_is_one(value))
      return ast_number(0);
    if (c_is_zero(value))
      return pi_fraction(1, 2);
    if (c_is_minus_one(value))
      return ast_variable("pi", 2);
    break;
  case TRIG_KIND_ATAN:
    if (c_is_zero(value))
      return ast_number(0);
    if (c_is_one(value))
      return pi_fraction(1, 4);
    if (c_is_minus_one(value))
      return ast_unary_neg(pi_fraction(1, 4));
    break;
  default:
    break;
  }

  return NULL;
}

static AstNode *unwrap_inverse_arg(AstNode *node) {
  AstNode *inverse = node->as.call.args[0];
  AstNode *result = inverse->as.call.args[0];
  inverse->as.call.args[0] = NULL;
  ast_free(node);
  return result;
}

static int trig_are_inverse_pair(TrigKind outer, TrigKind inner) {
  return (outer == TRIG_KIND_SIN && inner == TRIG_KIND_ASIN) ||
         (outer == TRIG_KIND_ASIN && inner == TRIG_KIND_SIN) ||
         (outer == TRIG_KIND_COS && inner == TRIG_KIND_ACOS) ||
         (outer == TRIG_KIND_ACOS && inner == TRIG_KIND_COS) ||
         (outer == TRIG_KIND_TAN && inner == TRIG_KIND_ATAN) ||
         (outer == TRIG_KIND_ATAN && inner == TRIG_KIND_TAN);
}

AstNode *trig_simplify_call(AstNode *node) {
  TrigKind kind = trig_kind(node);
  if (kind == TRIG_KIND_NONE)
    return node;

  AstNode *arg = node->as.call.args[0];
  TrigKind arg_kind = trig_kind(arg);
  if (trig_are_inverse_pair(kind, arg_kind))
    return unwrap_inverse_arg(node);

  if ((kind == TRIG_KIND_ASIN || kind == TRIG_KIND_ATAN) &&
      arg->type == AST_UNARY_NEG) {
    AstNode *operand = arg->as.unary.operand;
    arg->as.unary.operand = NULL;
    ast_free(arg);
    node->as.call.args[0] = operand;
    return sym_simplify(ast_unary_neg(node));
  }

  if (arg->type != AST_NUMBER)
    return node;

  AstNode *special = maybe_special_inverse_trig(kind, arg->as.number);
  if (special) {
    ast_free(node);
    return special;
  }

  int ok = 0;
  char *error = NULL;
  Complex result = trig_eval_value(kind, arg->as.number, &ok, &error);
  free(error);
  if (!ok)
    return node;

  ast_free(node);
  return ast_number_complex(result);
}

AstNode *trig_diff_call(const AstNode *node, const char *var) {
  TrigKind kind = trig_kind(node);
  if (kind == TRIG_KIND_NONE)
    return NULL;

  const AstNode *inner = node->as.call.args[0];
  AstNode *inner_d = sym_diff(inner, var);

  AstNode *outer_d = NULL;
  switch (kind) {
  case TRIG_KIND_SIN:
    outer_d = ast_func_call("cos", 3, (AstNode *[]){ast_clone(inner)}, 1);
    break;
  case TRIG_KIND_COS:
    outer_d = ast_unary_neg(
        ast_func_call("sin", 3, (AstNode *[]){ast_clone(inner)}, 1));
    break;
  case TRIG_KIND_TAN:
    outer_d = ast_binop(
        OP_DIV, ast_number(1),
        ast_binop(OP_POW,
                  ast_func_call("cos", 3, (AstNode *[]){ast_clone(inner)}, 1),
                  ast_number(2)));
    break;
  case TRIG_KIND_ASIN:
    outer_d = ast_binop(
        OP_DIV, ast_number(1),
        ast_func_call("sqrt", 4,
                      (AstNode *[]){ast_binop(
                          OP_SUB, ast_number(1),
                          ast_binop(OP_POW, ast_clone(inner), ast_number(2)))},
                      1));
    break;
  case TRIG_KIND_ACOS:
    outer_d = ast_unary_neg(ast_binop(
        OP_DIV, ast_number(1),
        ast_func_call("sqrt", 4,
                      (AstNode *[]){ast_binop(
                          OP_SUB, ast_number(1),
                          ast_binop(OP_POW, ast_clone(inner), ast_number(2)))},
                      1)));
    break;
  case TRIG_KIND_ATAN:
    outer_d = ast_binop(
        OP_DIV, ast_number(1),
        ast_binop(OP_ADD, ast_number(1),
                  ast_binop(OP_POW, ast_clone(inner), ast_number(2))));
    break;
  default:
    break;
  }

  if (!outer_d) {
    ast_free(inner_d);
    return NULL;
  }

  return sym_simplify(ast_binop(OP_MUL, outer_d, inner_d));
}
