#include "eval.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EvalResult ok(double v) {
  return (EvalResult){.value = v, .ok = 1, .error = NULL};
}

static EvalResult fail(const char *msg) {
  return (EvalResult){.value = 0.0, .ok = 0, .error = strdup(msg)};
}

static EvalResult eval_call(const char *name, double *args, size_t nargs) {
  if (nargs == 1) {
    double a = args[0];
    if (strcmp(name, "sin") == 0)
      return ok(sin(a));
    if (strcmp(name, "cos") == 0)
      return ok(cos(a));
    if (strcmp(name, "tan") == 0)
      return ok(tan(a));
    if (strcmp(name, "asin") == 0)
      return ok(asin(a));
    if (strcmp(name, "acos") == 0)
      return ok(acos(a));
    if (strcmp(name, "atan") == 0)
      return ok(atan(a));
    if (strcmp(name, "sqrt") == 0)
      return ok(sqrt(a));
    if (strcmp(name, "abs") == 0)
      return ok(fabs(a));
    if (strcmp(name, "exp") == 0)
      return ok(exp(a));
    if (strcmp(name, "log") == 0)
      return ok(log10(a));
    if (strcmp(name, "ln") == 0)
      return ok(log(a));
    if (strcmp(name, "ceil") == 0)
      return ok(ceil(a));
    if (strcmp(name, "floor") == 0)
      return ok(floor(a));
    if (strcmp(name, "round") == 0)
      return ok(round(a));
  }
  if (nargs == 2) {
    if (strcmp(name, "atan2") == 0)
      return ok(atan2(args[0], args[1]));
    if (strcmp(name, "pow") == 0)
      return ok(pow(args[0], args[1]));
  }

  char buf[128];
  snprintf(buf, sizeof buf, "unknown function: %s/%zu", name, nargs);
  return fail(buf);
}

EvalResult eval(const AstNode *node) {
  if (!node)
    return fail("null expression");

  switch (node->type) {
  case AST_NUMBER:
    return ok(node->as.number);

  case AST_VARIABLE: {
    if (strcmp(node->as.variable, "pi") == 0)
      return ok(M_PI);
    if (strcmp(node->as.variable, "e") == 0)
      return ok(M_E);
    char buf[128];
    snprintf(buf, sizeof buf, "undefined variable: %s", node->as.variable);
    return fail(buf);
  }

  case AST_BINOP: {
    EvalResult l = eval(node->as.binop.left);
    if (!l.ok)
      return l;
    EvalResult r = eval(node->as.binop.right);
    if (!r.ok) {
      eval_result_free(&l);
      return r;
    }

    switch (node->as.binop.op) {
    case OP_ADD:
      return ok(l.value + r.value);
    case OP_SUB:
      return ok(l.value - r.value);
    case OP_MUL:
      return ok(l.value * r.value);
    case OP_DIV:
      if (r.value == 0.0)
        return fail("division by zero");
      return ok(l.value / r.value);
    case OP_POW:
      return ok(pow(l.value, r.value));
    }
    return fail("unknown operator");
  }

  case AST_UNARY_NEG: {
    EvalResult r = eval(node->as.unary.operand);
    if (!r.ok)
      return r;
    return ok(-r.value);
  }

  case AST_FUNC_CALL: {
    double args[16];
    for (size_t i = 0; i < node->as.call.nargs; i++) {
      EvalResult a = eval(node->as.call.args[i]);
      if (!a.ok)
        return a;
      args[i] = a.value;
    }
    return eval_call(node->as.call.name, args, node->as.call.nargs);
  }
  }

  return fail("unknown node type");
}

void eval_result_free(EvalResult *r) {
  free(r->error);
  r->error = NULL;
  r->ok = 0;
}
