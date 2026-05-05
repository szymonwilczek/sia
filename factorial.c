#include "factorial.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int factorial_ll(long long n, long long *out) {
  long long result = 1;
  for (long long i = 2; i <= n; i++) {
    if (__builtin_mul_overflow(result, i, &result))
      return 0;
  }
  *out = result;
  return 1;
}

int factorial_is_call(const AstNode *node) {
  return node && node->type == AST_FUNC_CALL && node->as.call.nargs == 1 &&
         strcmp(node->as.call.name, "factorial") == 0;
}

Complex factorial_eval_value(Complex value, int *ok, char **error) {
  if (!value.exact || !fraction_is_zero(value.im_q) || value.re_q.den != 1) {
    if (ok)
      *ok = 0;
    if (error)
      *error = strdup("factorial requires a non-negative integer");
    return c_real(0.0);
  }

  if (value.re_q.num < 0) {
    if (ok)
      *ok = 0;
    if (error)
      *error = strdup("factorial requires a non-negative integer");
    return c_real(0.0);
  }

  long long result = 0;
  if (!factorial_ll(value.re_q.num, &result)) {
    if (ok)
      *ok = 0;
    if (error)
      *error = strdup("factorial overflow");
    return c_real(0.0);
  }

  if (ok)
    *ok = 1;
  if (error)
    *error = NULL;
  return c_from_fractions(fraction_make(result, 1), fraction_make(0, 1));
}

AstNode *factorial_simplify_call(AstNode *node) {
  if (!factorial_is_call(node))
    return node;
  if (node->as.call.args[0]->type != AST_NUMBER)
    return node;

  int ok = 0;
  char *error = NULL;
  Complex result =
      factorial_eval_value(node->as.call.args[0]->as.number, &ok, &error);
  free(error);
  if (!ok)
    return node;

  ast_free(node);
  return ast_number_complex(result);
}
