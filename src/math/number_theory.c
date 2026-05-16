#include "sia/number_theory.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static unsigned long long uabs_ll(long long value) {
  return value < 0 ? 0ull - (unsigned long long)value
                   : (unsigned long long)value;
}

static unsigned long long gcd_u64(unsigned long long left,
                                  unsigned long long right) {
  while (right != 0) {
    unsigned long long rem = left % right;
    left = right;
    right = rem;
  }
  return left;
}

static int exact_integer_value(Complex value, unsigned long long *out,
                               char **error) {
  if (!value.exact || !fraction_is_zero(value.im_q) || value.re_q.den != 1) {
    if (error)
      *error = strdup("gcd/lcm require integer arguments");
    return 0;
  }

  *out = uabs_ll(value.re_q.num);
  return 1;
}

static Complex exact_integer_result(unsigned long long value) {
  return c_from_fractions(fraction_make((long long)value, 1),
                          fraction_make(0, 1));
}

NumberTheoryKind number_theory_kind(const AstNode *node) {
  if (!node || node->type != AST_FUNC_CALL || node->as.call.nargs != 2)
    return NT_KIND_NONE;
  if (strcmp(node->as.call.name, "gcd") == 0)
    return NT_KIND_GCD;
  if (strcmp(node->as.call.name, "lcm") == 0)
    return NT_KIND_LCM;
  return NT_KIND_NONE;
}

Complex number_theory_eval_value(NumberTheoryKind kind, Complex left,
                                 Complex right, int *ok, char **error) {
  unsigned long long left_value = 0;
  unsigned long long right_value = 0;
  if (!exact_integer_value(left, &left_value, error) ||
      !exact_integer_value(right, &right_value, error)) {
    if (ok)
      *ok = 0;
    return c_real(0.0);
  }

  unsigned long long result = 0;
  if (kind == NT_KIND_GCD) {
    result = gcd_u64(left_value, right_value);
  } else if (kind == NT_KIND_LCM) {
    if (left_value == 0 || right_value == 0) {
      result = 0;
    } else {
      unsigned long long gcd = gcd_u64(left_value, right_value);
      unsigned long long quotient = left_value / gcd;
      if (quotient > (unsigned long long)LLONG_MAX / right_value) {
        if (ok)
          *ok = 0;
        if (error)
          *error = strdup("lcm overflow");
        return c_real(0.0);
      }
      result = quotient * right_value;
    }
  }

  if (result > (unsigned long long)LLONG_MAX) {
    if (ok)
      *ok = 0;
    if (error)
      *error = strdup(kind == NT_KIND_GCD ? "gcd overflow" : "lcm overflow");
    return c_real(0.0);
  }

  if (ok)
    *ok = 1;
  if (error)
    *error = NULL;
  return exact_integer_result(result);
}

AstNode *number_theory_simplify_call(AstNode *node) {
  NumberTheoryKind kind = number_theory_kind(node);
  if (kind == NT_KIND_NONE)
    return node;

  if (node->as.call.args[0]->type != AST_NUMBER ||
      node->as.call.args[1]->type != AST_NUMBER)
    return node;

  int ok = 0;
  char *error = NULL;
  Complex result =
      number_theory_eval_value(kind, node->as.call.args[0]->as.number,
                               node->as.call.args[1]->as.number, &ok, &error);
  free(error);
  if (!ok)
    return node;

  ast_free(node);
  return ast_number_complex(result);
}
