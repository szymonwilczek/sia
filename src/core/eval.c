#include "sia/eval.h"
#include "sia/factorial.h"
#include "sia/logarithm.h"
#include "sia/number_theory.h"
#include "sia/trigonometry.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EvalResult ok(Complex v) {
  return (EvalResult){.value = v, .ok = 1, .error = NULL};
}

static EvalResult ok_real(double v) {
  return (EvalResult){.value = c_real(v), .ok = 1, .error = NULL};
}

static EvalResult fail(const char *msg) {
  return (EvalResult){.value = c_real(0.0), .ok = 0, .error = strdup(msg)};
}

static EvalResult eval_call(const char *name, Complex *args, size_t nargs) {
  AstNode node;
  node.type = AST_FUNC_CALL;
  node.as.call.name = (char *)name;
  node.as.call.args = NULL;
  node.as.call.nargs = nargs;

  LogKind kind = log_kind(&node);
  if (kind != LOG_KIND_NONE) {
    if (kind == LOG_KIND_LN) {
      Complex a = args[0];
      if (c_is_zero(a))
        return fail("domain error: ln of zero");
      return ok(c_log(a));
    }

    Complex base = (kind == LOG_KIND_BASE10)  ? c_real(10.0)
                   : (kind == LOG_KIND_BASE2) ? c_real(2.0)
                                              : args[1];
    char *error = NULL;
    int ok_flag = 0;
    Complex result = log_eval_value_base(args[0], base, &ok_flag, &error);
    if (!ok_flag) {
      EvalResult r = fail(error ? error : "domain error: log");
      free(error);
      return r;
    }
    free(error);
    return ok(result);
  }

  if (factorial_is_call(&node)) {
    char *error = NULL;
    int ok_flag = 0;
    Complex result = factorial_eval_value(args[0], &ok_flag, &error);
    if (!ok_flag) {
      EvalResult r = fail(error ? error : "domain error: factorial");
      free(error);
      return r;
    }
    free(error);
    return ok(result);
  }

  TrigKind trig = trig_kind(&node);
  if (trig != TRIG_KIND_NONE) {
    char *error = NULL;
    int ok_flag = 0;
    Complex result = trig_eval_value(trig, args[0], &ok_flag, &error);
    if (!ok_flag) {
      EvalResult r = fail(error ? error : "domain error: trig");
      free(error);
      return r;
    }
    free(error);
    return ok(result);
  }

  NumberTheoryKind nt_kind = number_theory_kind(&node);
  if (nt_kind != NT_KIND_NONE) {
    char *error = NULL;
    int ok_flag = 0;
    Complex result =
        number_theory_eval_value(nt_kind, args[0], args[1], &ok_flag, &error);
    if (!ok_flag) {
      EvalResult r = fail(error ? error : "domain error: gcd/lcm");
      free(error);
      return r;
    }
    free(error);
    return ok(result);
  }

  if (nargs == 1) {
    Complex a = args[0];
    if (strcmp(name, "sin") == 0)
      return ok(c_sin(a));
    if (strcmp(name, "cos") == 0)
      return ok(c_cos(a));
    if (strcmp(name, "tan") == 0)
      return ok(c_tan(a));
    if (strcmp(name, "asin") == 0)
      return ok(c_asin(a));
    if (strcmp(name, "acos") == 0)
      return ok(c_acos(a));
    if (strcmp(name, "atan") == 0)
      return ok(c_atan(a));
    if (strcmp(name, "sqrt") == 0)
      return ok(c_sqrt(a));
    if (strcmp(name, "abs") == 0)
      return ok_real(c_abs(a));
    if (strcmp(name, "exp") == 0)
      return ok(c_exp(a));
    if (strcmp(name, "ln") == 0) {
      if (c_is_zero(a))
        return fail("domain error: ln of zero");
      return ok(c_log(a));
    }
    if (strcmp(name, "ceil") == 0) {
      if (!c_is_real(a))
        return fail("ceil requires real argument");
      return ok_real(ceil(a.re));
    }
    if (strcmp(name, "floor") == 0) {
      if (!c_is_real(a))
        return fail("floor requires real argument");
      return ok_real(floor(a.re));
    }
    if (strcmp(name, "round") == 0) {
      if (!c_is_real(a))
        return fail("round requires real argument");
      return ok_real(round(a.re));
    }
    if (strcmp(name, "sinh") == 0)
      return ok(c_sinh(a));
    if (strcmp(name, "cosh") == 0)
      return ok(c_cosh(a));
    if (strcmp(name, "tanh") == 0)
      return ok(c_tanh(a));
    if (strcmp(name, "Re") == 0 || strcmp(name, "re") == 0)
      return ok_real(a.re);
    if (strcmp(name, "Im") == 0 || strcmp(name, "im") == 0)
      return ok_real(a.im);
    if (strcmp(name, "conj") == 0)
      return ok(c_conj(a));
    if (strcmp(name, "arg") == 0)
      return ok_real(atan2(a.im, a.re));
  }
  if (nargs == 2) {
    if (strcmp(name, "atan2") == 0) {
      if (!c_is_real(args[0]) || !c_is_real(args[1]))
        return fail("atan2 requires real arguments");
      return ok_real(atan2(args[0].re, args[1].re));
    }
    if (strcmp(name, "pow") == 0)
      return ok(c_pow(args[0], args[1]));
    if (strcmp(name, "min") == 0) {
      if (!c_is_real(args[0]) || !c_is_real(args[1]))
        return fail("min requires real arguments");
      return ok_real(fmin(args[0].re, args[1].re));
    }
    if (strcmp(name, "max") == 0) {
      if (!c_is_real(args[0]) || !c_is_real(args[1]))
        return fail("max requires real arguments");
      return ok_real(fmax(args[0].re, args[1].re));
    }
  }

  char buf[128];
  snprintf(buf, sizeof buf, "unknown function: %s/%zu", name, nargs);
  return fail(buf);
}

EvalResult eval(const AstNode *node, const SymTab *st) {
  if (!node)
    return fail("null expression");

  switch (node->type) {
  case AST_NUMBER:
    return ok(node->as.number);

  case AST_VARIABLE: {
    if (strcmp(node->as.variable, "pi") == 0)
      return ok_real(M_PI);
    if (strcmp(node->as.variable, "e") == 0)
      return ok_real(M_E);
    if (strcmp(node->as.variable, "i") == 0)
      return ok(c_make(0, 1));

    if (st) {
      Complex val;
      if (symtab_get(st, node->as.variable, &val))
        return ok(val);
    }

    char buf[128];
    snprintf(buf, sizeof buf, "undefined variable: %s", node->as.variable);
    return fail(buf);
  }

  case AST_BINOP: {
    EvalResult l = eval(node->as.binop.left, st);
    if (!l.ok)
      return l;
    EvalResult r = eval(node->as.binop.right, st);
    if (!r.ok) {
      eval_result_free(&l);
      return r;
    }

    switch (node->as.binop.op) {
    case OP_ADD:
      return ok(c_add(l.value, r.value));
    case OP_SUB:
      return ok(c_sub(l.value, r.value));
    case OP_MUL:
      return ok(c_mul(l.value, r.value));
    case OP_DIV:
      if (c_is_zero(r.value))
        return fail("division by zero");
      return ok(c_div(l.value, r.value));
    case OP_POW:
      return ok(c_pow(l.value, r.value));
    }
    return fail("unknown operator");
  }

  case AST_UNARY_NEG: {
    EvalResult r = eval(node->as.unary.operand, st);
    if (!r.ok)
      return r;
    return ok(c_neg(r.value));
  }

  case AST_FUNC_CALL: {
    Complex args[16];
    for (size_t i = 0; i < node->as.call.nargs; i++) {
      EvalResult a = eval(node->as.call.args[i], st);
      if (!a.ok)
        return a;
      args[i] = a.value;
    }
    return eval_call(node->as.call.name, args, node->as.call.nargs);
  }
  case AST_LIMIT:
    return fail("cannot evaluate limit as scalar");
  case AST_MATRIX:
    return fail("cannot evaluate matrix as scalar");
  case AST_EQ:
    return fail("cannot evaluate equation as scalar");
  case AST_INFINITY:
    return ok(c_real(node->as.infinity.sign < 0 ? -INFINITY : INFINITY));
  case AST_UNDEFINED:
    return fail("undefined value");
  }

  return fail("unknown node type");
}

void eval_result_free(EvalResult *r) {
  free(r->error);
  r->error = NULL;
  r->ok = 0;
}
