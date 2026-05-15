#include "symbolic.h"
#include "canonical.h"
#include "factorial.h"
#include "logarithm.h"
#include "number_theory.h"
#include "trigonometry/trigonometry.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int is_number(const AstNode *n, double v) {
  return n && n->type == AST_NUMBER && c_is_real(n->as.number) &&
         n->as.number.re == v;
}

static int is_num(const AstNode *n) { return n && n->type == AST_NUMBER; }

static int is_var(const AstNode *n, const char *var) {
  return n && n->type == AST_VARIABLE && strcmp(n->as.variable, var) == 0;
}

/* check if n is func(arg) with given name and 1 argument */
static int is_call1(const AstNode *n, const char *name) {
  return n && n->type == AST_FUNC_CALL && n->as.call.nargs == 1 &&
         strcmp(n->as.call.name, name) == 0;
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

static int is_zero_node(const AstNode *n) {
  return n && n->type == AST_NUMBER && c_is_zero(n->as.number);
}

int sym_contains_var(const AstNode *n, const char *var) {
  if (!n)
    return 0;
  switch (n->type) {
  case AST_NUMBER:
    return 0;
  case AST_VARIABLE:
    return strcmp(n->as.variable, var) == 0;
  case AST_BINOP:
    return sym_contains_var(n->as.binop.left, var) ||
           sym_contains_var(n->as.binop.right, var);
  case AST_UNARY_NEG:
    return sym_contains_var(n->as.unary.operand, var);
  case AST_FUNC_CALL:
    for (size_t i = 0; i < n->as.call.nargs; i++)
      if (sym_contains_var(n->as.call.args[i], var))
        return 1;
    return 0;
  case AST_MATRIX: {
    size_t total = n->as.matrix.rows * n->as.matrix.cols;
    for (size_t i = 0; i < total; i++)
      if (sym_contains_var(n->as.matrix.elements[i], var))
        return 1;
    return 0;
  }
  }
  return 0;
}

static int ast_equal(const AstNode *a, const AstNode *b) {
  if (!a && !b)
    return 1;
  if (!a || !b)
    return 0;
  if (a->type != b->type)
    return 0;

  switch (a->type) {
  case AST_NUMBER:
    return c_eq(a->as.number, b->as.number);
  case AST_VARIABLE:
    return strcmp(a->as.variable, b->as.variable) == 0;
  case AST_BINOP:
    return a->as.binop.op == b->as.binop.op &&
           ast_equal(a->as.binop.left, b->as.binop.left) &&
           ast_equal(a->as.binop.right, b->as.binop.right);
  case AST_UNARY_NEG:
    return ast_equal(a->as.unary.operand, b->as.unary.operand);
  case AST_FUNC_CALL:
    if (strcmp(a->as.call.name, b->as.call.name) != 0)
      return 0;
    if (a->as.call.nargs != b->as.call.nargs)
      return 0;
    for (size_t i = 0; i < a->as.call.nargs; i++)
      if (!ast_equal(a->as.call.args[i], b->as.call.args[i]))
        return 0;
    return 1;
  case AST_MATRIX:
    if (a->as.matrix.rows != b->as.matrix.rows ||
        a->as.matrix.cols != b->as.matrix.cols)
      return 0;
    for (size_t i = 0; i < a->as.matrix.rows * a->as.matrix.cols; i++)
      if (!ast_equal(a->as.matrix.elements[i], b->as.matrix.elements[i]))
        return 0;
    return 1;
  }
  return 0;
}

typedef struct {
  AstNode **items;
  size_t count;
  size_t cap;
} NodeList;

typedef struct {
  AstNode *base;
  AstNode *exp;
} PowerFactor;

typedef struct {
  PowerFactor *items;
  size_t count;
  size_t cap;
} PowerFactorList;

static int node_list_push(NodeList *list, AstNode *node) {
  if (list->count == list->cap) {
    size_t new_cap = list->cap ? list->cap * 2 : 4;
    AstNode **items = realloc(list->items, new_cap * sizeof(AstNode *));
    if (!items)
      return 0;
    list->items = items;
    list->cap = new_cap;
  }

  list->items[list->count++] = node;
  return 1;
}

static void node_list_free(NodeList *list) {
  if (!list)
    return;
  for (size_t i = 0; i < list->count; i++)
    ast_free(list->items[i]);
  free(list->items);
  list->items = NULL;
  list->count = 0;
  list->cap = 0;
}

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

static AstNode *normalize_mul_factors(const AstNode *node) {
  Complex coeff = c_real(1.0);
  PowerFactorList factors = {0};
  NodeList nodes = {0};
  AstNode *result = NULL;

  if (!flatten_mul_factors(node, &coeff, &factors)) {
    power_factor_list_free(&factors);
    return NULL;
  }

  for (size_t i = 0; i < factors.count; i++) {
    if (!factors.items[i].base)
      continue;
    for (size_t j = i + 1; j < factors.count; j++) {
      AstNode *exp = NULL;
      if (!factors.items[j].base)
        continue;
      if (!ast_equal(factors.items[i].base, factors.items[j].base))
        continue;

      exp = sym_simplify(
          ast_binop(OP_ADD, factors.items[i].exp, factors.items[j].exp));
      factors.items[i].exp = exp;
      factors.items[j].exp = NULL;
      ast_free(factors.items[j].base);
      factors.items[j].base = NULL;
    }
    merge_like_factors(&factors);
    fold_abs_power_identities(&factors);
    if (!append_factor_nodes(&factors, &nodes)) {
      node_list_free(&nodes);
      power_factor_list_free(&factors);
      return NULL;
    }

    for (size_t i = 0; i < factors.count; i++) {
      AstNode *factor_node = NULL;
      if (!factors.items[i].base)
        continue;
      factor_node = build_factor_node(&factors.items[i]);
      if (!is_number(factor_node, 1) || nodes.count == 0) {
        if (!node_list_push(&nodes, factor_node)) {
          ast_free(factor_node);
          node_list_free(&nodes);
          power_factor_list_free(&factors);
          return NULL;
        }
      } else {
        ast_free(factor_node);
      }
      merge_like_factors(&factors);
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
    return NULL;
  }
}

static int extract_coeff_and_base(const AstNode *node, Complex *coeff,
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
    return node;

  case AST_MATRIX: {
    size_t total = node->as.matrix.rows * node->as.matrix.cols;
    for (size_t i = 0; i < total; i++)
      node->as.matrix.elements[i] = sym_simplify(node->as.matrix.elements[i]);
    return node;
  }

  case AST_UNARY_NEG:
    node->as.unary.operand = sym_simplify(node->as.unary.operand);
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
    return node;

  case AST_FUNC_CALL:
    for (size_t i = 0; i < node->as.call.nargs; i++)
      node->as.call.args[i] = sym_simplify(node->as.call.args[i]);
    if (factorial_is_call(node))
      return factorial_simplify_call(node);
    if (trig_kind(node) != TRIG_KIND_NONE)
      return trig_simplify_call(node);
    if (number_theory_kind(node) != NT_KIND_NONE)
      return number_theory_simplify_call(node);
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
      if (fold_unary_numeric_call(node->as.call.name,
                                  node->as.call.args[0]->as.number, &folded)) {
        ast_free(node);
        return ast_number_complex(folded);
      }
    }
    /* exp(ln(x)) -> x */
    if (is_call1(node, "exp") && is_call1(node->as.call.args[0], "ln")) {
      AstNode *inner = ast_clone(node->as.call.args[0]->as.call.args[0]);
      ast_free(node);
      return inner;
    }
    /* ln(exp(x)) -> x */
    if (is_call1(node, "ln") && is_call1(node->as.call.args[0], "exp")) {
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
  } else {
    if (*count >= *cap) {
      *cap *= 2;
      *terms = realloc(*terms, *cap * sizeof(AstNode *));
    }
    (*terms)[(*count)++] = ast_clone(node);
  }
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
    if (!extract_coeff_and_base(terms[i], &c_i, &b_i))
      continue;

    for (size_t j = i + 1; j < count; j++) {
      if (!terms[j])
        continue;

      Complex c_j = c_real(1.0);
      AstNode *b_j = NULL;
      if (!extract_coeff_and_base(terms[j], &c_j, &b_j)) {
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

static AstNode *sym_full_simplify_once(AstNode *node) {
  AstNode *expanded = NULL;
  AstNode *collected = NULL;

  if (!node)
    return NULL;

  node = sym_simplify(node);

  expanded = sym_expand(node);
  ast_free(node);
  node = expanded;

  node = ast_canonicalize(node);

  collected = sym_collect_terms(node);
  ast_free(node);
  node = collected;

  return sym_simplify(node);
}

AstNode *sym_full_simplify(AstNode *node) {
  AstNode *current = NULL;

  if (!node)
    return NULL;

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

AstNode *sym_diff(const AstNode *expr, const char *var) {
  if (!expr)
    return ast_number(0);

  switch (expr->type) {
  case AST_NUMBER:
    return ast_number(0);

  case AST_MATRIX: {
    size_t total = expr->as.matrix.rows * expr->as.matrix.cols;
    AstNode **elems = malloc(total * sizeof(AstNode *));
    for (size_t i = 0; i < total; i++)
      elems[i] = sym_diff(expr->as.matrix.elements[i], var);
    AstNode *r = ast_matrix(elems, expr->as.matrix.rows, expr->as.matrix.cols);
    free(elems);
    return sym_simplify(r);
  }

  case AST_VARIABLE:
    return ast_number(is_var(expr, var) ? 1.0 : 0.0);

  case AST_UNARY_NEG:
    return sym_simplify(ast_unary_neg(sym_diff(expr->as.unary.operand, var)));

  case AST_BINOP: {
    const AstNode *L = expr->as.binop.left;
    const AstNode *R = expr->as.binop.right;

    switch (expr->as.binop.op) {
    case OP_ADD:
      return sym_simplify(
          ast_binop(OP_ADD, sym_diff(L, var), sym_diff(R, var)));

    case OP_SUB:
      return sym_simplify(
          ast_binop(OP_SUB, sym_diff(L, var), sym_diff(R, var)));

    case OP_MUL:
      return sym_simplify(
          ast_binop(OP_ADD, ast_binop(OP_MUL, sym_diff(L, var), ast_clone(R)),
                    ast_binop(OP_MUL, ast_clone(L), sym_diff(R, var))));

    case OP_DIV:
      return sym_simplify(ast_binop(
          OP_DIV,
          ast_binop(OP_SUB, ast_binop(OP_MUL, sym_diff(L, var), ast_clone(R)),
                    ast_binop(OP_MUL, ast_clone(L), sym_diff(R, var))),
          ast_binop(OP_POW, ast_clone(R), ast_number(2))));

    case OP_POW:
      if (!sym_contains_var(R, var) && !sym_contains_var(L, var)) {
        return ast_number(0);
      }
      /* x^n where n constant: n*x^(n-1)*x' */
      if (!sym_contains_var(R, var)) {
        return sym_simplify(ast_binop(
            OP_MUL,
            ast_binop(
                OP_MUL, ast_clone(R),
                ast_binop(OP_POW, ast_clone(L),
                          ast_binop(OP_SUB, ast_clone(R), ast_number(1)))),
            sym_diff(L, var)));
      }
      /* a^x where a constant: a^x * ln(a) * x' */
      if (!sym_contains_var(L, var)) {
        AstNode *lna_args[1] = {ast_clone(L)};
        return sym_simplify(
            ast_binop(OP_MUL,
                      ast_binop(OP_MUL, ast_clone(expr),
                                ast_func_call("ln", 2, lna_args, 1)),
                      sym_diff(R, var)));
      }
      /* f(x)^g(x): rewrite as e^(g*ln(f)), differentiate that
       * d/dx[f^g] = f^g * (g'*ln(f) + g*f'/f) */
      {
        AstNode *ln_f_args[1] = {ast_clone(L)};
        AstNode *ln_f = ast_func_call("ln", 2, ln_f_args, 1);

        AstNode *term1 = ast_binop(OP_MUL, sym_diff(R, var), ast_clone(ln_f));

        AstNode *term2 =
            ast_binop(OP_MUL, ast_clone(R),
                      ast_binop(OP_DIV, sym_diff(L, var), ast_clone(L)));

        ast_free(ln_f);

        return sym_simplify(ast_binop(OP_MUL, ast_clone(expr),
                                      ast_binop(OP_ADD, term1, term2)));
      }
    }
    break;
  }

  case AST_FUNC_CALL: {
    if (!sym_contains_var(expr, var))
      return ast_number(0);

    if (log_kind(expr) != LOG_KIND_NONE)
      return log_diff_call(expr, var);

    if (trig_kind(expr) != TRIG_KIND_NONE)
      return trig_diff_call(expr, var);

    const char *name = expr->as.call.name;
    if (expr->as.call.nargs != 1)
      return NULL;
    const AstNode *inner = expr->as.call.args[0];
    AstNode *inner_d = sym_diff(inner, var);

    AstNode *outer_d = NULL;
    if (strcmp(name, "sin") == 0) {
      outer_d = ast_func_call("cos", 3, (AstNode *[]){ast_clone(inner)}, 1);
    } else if (strcmp(name, "cos") == 0) {
      outer_d = ast_unary_neg(
          ast_func_call("sin", 3, (AstNode *[]){ast_clone(inner)}, 1));
    } else if (strcmp(name, "tan") == 0) {
      outer_d = ast_binop(
          OP_DIV, ast_number(1),
          ast_binop(OP_POW,
                    ast_func_call("cos", 3, (AstNode *[]){ast_clone(inner)}, 1),
                    ast_number(2)));
    } else if (strcmp(name, "sinh") == 0) {
      outer_d = ast_func_call("cosh", 4, (AstNode *[]){ast_clone(inner)}, 1);
    } else if (strcmp(name, "cosh") == 0) {
      outer_d = ast_func_call("sinh", 4, (AstNode *[]){ast_clone(inner)}, 1);
    } else if (strcmp(name, "tanh") == 0) {
      outer_d =
          ast_binop(OP_DIV, ast_number(1),
                    ast_binop(OP_POW,
                              ast_func_call("cosh", 4,
                                            (AstNode *[]){ast_clone(inner)}, 1),
                              ast_number(2)));
    } else if (strcmp(name, "ln") == 0) {
      outer_d = ast_binop(OP_DIV, ast_number(1), ast_clone(inner));
    } else if (strcmp(name, "exp") == 0) {
      outer_d = ast_clone(expr);
    } else if (strcmp(name, "sqrt") == 0) {
      outer_d = ast_binop(
          OP_DIV, ast_number(1),
          ast_binop(
              OP_MUL, ast_number(2),
              ast_func_call("sqrt", 4, (AstNode *[]){ast_clone(inner)}, 1)));
    } else if (strcmp(name, "abs") == 0) {
      outer_d = ast_binop(
          OP_DIV, ast_clone(inner),
          ast_func_call("abs", 3, (AstNode *[]){ast_clone(inner)}, 1));
    } else if (strcmp(name, "asin") == 0) {
      /* 1/sqrt(1-u^2) */
      outer_d = ast_binop(OP_DIV, ast_number(1),
                          ast_func_call("sqrt", 4,
                                        (AstNode *[]){ast_binop(
                                            OP_SUB, ast_number(1),
                                            ast_binop(OP_POW, ast_clone(inner),
                                                      ast_number(2)))},
                                        1));
    } else if (strcmp(name, "acos") == 0) {
      /* -1/sqrt(1-u^2) */
      outer_d = ast_unary_neg(ast_binop(
          OP_DIV, ast_number(1),
          ast_func_call(
              "sqrt", 4,
              (AstNode *[]){ast_binop(
                  OP_SUB, ast_number(1),
                  ast_binop(OP_POW, ast_clone(inner), ast_number(2)))},
              1)));
    } else if (strcmp(name, "atan") == 0) {
      /* 1/(1+u^2) */
      outer_d = ast_binop(
          OP_DIV, ast_number(1),
          ast_binop(OP_ADD, ast_number(1),
                    ast_binop(OP_POW, ast_clone(inner), ast_number(2))));
    } else {
      ast_free(inner_d);
      return NULL;
    }

    return sym_simplify(ast_binop(OP_MUL, outer_d, inner_d));
  }
  }

  return NULL;
}

AstNode *sym_diff_n(const AstNode *expr, const char *var, int order) {
  AstNode *current = NULL;

  if (!expr || !var || order < 0)
    return NULL;

  current = ast_clone(expr);
  if (!current)
    return NULL;

  if (order == 0)
    return sym_full_simplify(current);

  for (int i = 0; i < order; i++) {
    AstNode *next = sym_diff(current, var);
    ast_free(current);
    if (!next)
      return NULL;
    current = sym_full_simplify(next);
  }

  return current;
}

AstNode *sym_grad(const AstNode *expr, const AstNode *vars_matrix) {
  size_t total = 0;
  AstNode **elems = NULL;
  AstNode *result = NULL;

  if (!expr || !vars_matrix || vars_matrix->type != AST_MATRIX)
    return NULL;

  total = vars_matrix->as.matrix.rows * vars_matrix->as.matrix.cols;
  elems = calloc(total, sizeof(AstNode *));
  if (!elems)
    return NULL;

  for (size_t i = 0; i < total; i++) {
    const AstNode *var_node = vars_matrix->as.matrix.elements[i];
    if (!var_node || var_node->type != AST_VARIABLE) {
      for (size_t j = 0; j < i; j++)
        ast_free(elems[j]);
      free(elems);
      return NULL;
    }

    elems[i] = sym_diff(expr, var_node->as.variable);
    if (!elems[i]) {
      for (size_t j = 0; j <= i; j++)
        ast_free(elems[j]);
      free(elems);
      return NULL;
    }
    elems[i] = sym_simplify(elems[i]);
  }

  result = ast_matrix(elems, vars_matrix->as.matrix.rows,
                      vars_matrix->as.matrix.cols);
  free(elems);
  return result;
}

AstNode *sym_subs(const AstNode *expr, const char *var, const AstNode *val) {
  if (!expr || !var || !val)
    return NULL;

  switch (expr->type) {
  case AST_NUMBER:
    return ast_clone(expr);
  case AST_VARIABLE:
    if (strcmp(expr->as.variable, var) == 0)
      return ast_clone(val);
    return ast_clone(expr);
  case AST_BINOP:
    return ast_binop(expr->as.binop.op, sym_subs(expr->as.binop.left, var, val),
                     sym_subs(expr->as.binop.right, var, val));
  case AST_UNARY_NEG:
    return ast_unary_neg(sym_subs(expr->as.unary.operand, var, val));
  case AST_FUNC_CALL: {
    AstNode **args = NULL;
    if (expr->as.call.nargs > 0) {
      args = calloc(expr->as.call.nargs, sizeof(AstNode *));
      if (!args)
        return NULL;
      for (size_t i = 0; i < expr->as.call.nargs; i++) {
        args[i] = sym_subs(expr->as.call.args[i], var, val);
        if (!args[i]) {
          for (size_t j = 0; j < i; j++)
            ast_free(args[j]);
          free(args);
          return NULL;
        }
      }
    }
    AstNode *result =
        ast_func_call(expr->as.call.name, strlen(expr->as.call.name), args,
                      expr->as.call.nargs);
    free(args);
    return result;
  }
  case AST_MATRIX: {
    size_t total = expr->as.matrix.rows * expr->as.matrix.cols;
    AstNode **elems = calloc(total, sizeof(AstNode *));
    if (!elems)
      return NULL;
    for (size_t i = 0; i < total; i++) {
      elems[i] = sym_subs(expr->as.matrix.elements[i], var, val);
      if (!elems[i]) {
        for (size_t j = 0; j < i; j++)
          ast_free(elems[j]);
        free(elems);
        return NULL;
      }
    }
    AstNode *result =
        ast_matrix(elems, expr->as.matrix.rows, expr->as.matrix.cols);
    free(elems);
    return result;
  }
  }

  return NULL;
}

AstNode *sym_taylor(const AstNode *expr, const char *var, const AstNode *a,
                    int order) {
  AstNode *sum = NULL;
  AstNode *current = NULL;
  AstNode *base = NULL;

  if (!expr || !var || !a || order < 0)
    return NULL;

  base = sym_simplify(
      ast_binop(OP_SUB, ast_variable(var, strlen(var)), ast_clone(a)));
  current = ast_clone(expr);
  if (!base || !current) {
    ast_free(base);
    ast_free(current);
    return NULL;
  }

  for (int k = 0; k <= order; k++) {
    AstNode *coeff = sym_subs(current, var, a);
    if (!coeff) {
      ast_free(base);
      ast_free(current);
      ast_free(sum);
      return NULL;
    }
    coeff = sym_simplify(coeff);

    if (!is_zero_node(coeff)) {
      AstNode *term = coeff;

      if (k > 0) {
        AstNode *power = NULL;
        Complex factorial_value;
        int ok = 0;
        char *error = NULL;

        if (k == 1) {
          power = ast_clone(base);
        } else {
          power = ast_binop(OP_POW, ast_clone(base), ast_number(k));
        }
        term = sym_simplify(ast_binop(OP_MUL, term, power));

        factorial_value = factorial_eval_value(
            c_from_fractions(fraction_make(k, 1), fraction_make(0, 1)), &ok,
            &error);
        free(error);
        if (!ok) {
          ast_free(term);
          ast_free(base);
          ast_free(current);
          ast_free(sum);
          return NULL;
        }

        term = sym_simplify(
            ast_binop(OP_DIV, term, ast_number_complex(factorial_value)));
      }

      if (!sum)
        sum = term;
      else
        sum = sym_simplify(ast_binop(OP_ADD, sum, term));
    } else {
      ast_free(coeff);
    }

    if (k < order) {
      AstNode *next = sym_diff(current, var);
      ast_free(current);
      if (!next) {
        ast_free(base);
        ast_free(sum);
        return NULL;
      }
      current = sym_simplify(next);
    }
  }

  ast_free(base);
  ast_free(current);

  if (!sum)
    return ast_number(0);

  sum = ast_canonicalize(sum);
  sum = sym_collect_terms(sum);
  sum = sym_simplify(sum);
  return sum;
}

static AstNode *integrate_trig(const AstNode *expr, const char *var) {
  if (expr->type != AST_FUNC_CALL || expr->as.call.nargs != 1)
    return NULL;

  const char *name = expr->as.call.name;
  const AstNode *inner = expr->as.call.args[0];

  /* TODO: only handle direct variable argument for now */
  if (!is_var(inner, var))
    return NULL;

  if (strcmp(name, "sin") == 0) {
    /* int sin(x) = -cos(x) */
    return ast_unary_neg(ast_func_call(
        "cos", 3, (AstNode *[]){ast_variable(var, strlen(var))}, 1));
  }
  if (strcmp(name, "cos") == 0) {
    /* int cos(x) = sin(x) */
    return ast_func_call("sin", 3,
                         (AstNode *[]){ast_variable(var, strlen(var))}, 1);
  }
  if (strcmp(name, "exp") == 0) {
    /* int exp(x) = exp(x) */
    return ast_func_call("exp", 3,
                         (AstNode *[]){ast_variable(var, strlen(var))}, 1);
  }
  if (strcmp(name, "tan") == 0) {
    /* int tan(x) = -ln(cos(x)) */
    return ast_unary_neg(ast_func_call(
        "ln", 2,
        (AstNode *[]){ast_func_call(
            "cos", 3, (AstNode *[]){ast_variable(var, strlen(var))}, 1)},
        1));
  }

  return NULL;
}

static AstNode *integrate_monomial(const AstNode *expr, const char *var) {
  /* constant -> c*x */
  if (!sym_contains_var(expr, var)) {
    return ast_binop(OP_MUL, ast_clone(expr), ast_variable(var, strlen(var)));
  }

  /* x -> x^2/2 */
  if (is_var(expr, var)) {
    return ast_binop(
        OP_DIV,
        ast_binop(OP_POW, ast_variable(var, strlen(var)), ast_number(2)),
        ast_number(2));
  }

  /* x^n -> x^(n+1)/(n+1) */
  if (expr->type == AST_BINOP && expr->as.binop.op == OP_POW &&
      is_var(expr->as.binop.left, var) && is_num(expr->as.binop.right) &&
      c_is_real(expr->as.binop.right->as.number)) {
    double n = expr->as.binop.right->as.number.re;
    if (n == -1.0) {
      return ast_func_call("ln", 2,
                           (AstNode *[]){ast_variable(var, strlen(var))}, 1);
    }
    double n1 = n + 1.0;
    return ast_binop(
        OP_DIV,
        ast_binop(OP_POW, ast_variable(var, strlen(var)), ast_number(n1)),
        ast_number(n1));
  }

  /* c*f(x) -> c * int(f(x)) */
  if (expr->type == AST_BINOP && expr->as.binop.op == OP_MUL) {
    if (!sym_contains_var(expr->as.binop.left, var)) {
      AstNode *inner = integrate_monomial(expr->as.binop.right, var);
      if (!inner)
        inner = integrate_trig(expr->as.binop.right, var);
      if (inner)
        return ast_binop(OP_MUL, ast_clone(expr->as.binop.left), inner);
    }
    if (!sym_contains_var(expr->as.binop.right, var)) {
      AstNode *inner = integrate_monomial(expr->as.binop.left, var);
      if (!inner)
        inner = integrate_trig(expr->as.binop.left, var);
      if (inner)
        return ast_binop(OP_MUL, ast_clone(expr->as.binop.right), inner);
    }
  }

  return NULL;
}

AstNode *sym_integrate(const AstNode *expr, const char *var) {
  if (!expr)
    return NULL;

  /* linearity: int(a +/- b) = int(a) +/- int(b) */
  if (expr->type == AST_BINOP &&
      (expr->as.binop.op == OP_ADD || expr->as.binop.op == OP_SUB)) {
    AstNode *li = sym_integrate(expr->as.binop.left, var);
    AstNode *ri = sym_integrate(expr->as.binop.right, var);
    if (li && ri)
      return sym_simplify(ast_binop(expr->as.binop.op, li, ri));
    ast_free(li);
    ast_free(ri);
    return NULL;
  }

  /* negation */
  if (expr->type == AST_UNARY_NEG) {
    AstNode *inner = sym_integrate(expr->as.unary.operand, var);
    if (inner)
      return sym_simplify(ast_unary_neg(inner));
    return NULL;
  }

  /* try trig/exp integrals first */
  AstNode *result = integrate_trig(expr, var);
  if (result)
    return sym_simplify(result);

  /* then monomial rule */
  result = integrate_monomial(expr, var);
  if (result)
    return sym_simplify(result);

  return NULL;
}

static AstNode *sym_det_recursive(AstNode **elems, size_t n, size_t stride) {
  if (n == 1)
    return ast_clone(elems[0]);

  if (n == 2) {
    AstNode *ad =
        ast_binop(OP_MUL, ast_clone(elems[0]), ast_clone(elems[stride + 1]));
    AstNode *bc =
        ast_binop(OP_MUL, ast_clone(elems[1]), ast_clone(elems[stride]));
    AstNode *res = ast_binop(OP_SUB, ad, bc);
    res = sym_simplify(res);
    res = sym_expand(res);
    res = ast_canonicalize(res);
    res = sym_collect_terms(res);
    return sym_simplify(res);
  }

  AstNode **sub = malloc((n - 1) * (n - 1) * sizeof(AstNode *));
  AstNode *det = NULL;

  for (size_t col = 0; col < n; col++) {
    size_t si = 0;
    for (size_t r = 1; r < n; r++)
      for (size_t c = 0; c < n; c++) {
        if (c == col)
          continue;
        sub[si++] = elems[r * stride + c];
      }

    AstNode *cofactor = sym_det_recursive(sub, n - 1, n - 1);
    AstNode *term = ast_binop(OP_MUL, ast_clone(elems[col]), cofactor);
    term = sym_simplify(term);

    if (!det) {
      det = col % 2 == 0 ? term : sym_simplify(ast_unary_neg(term));
    } else {
      if (col % 2 == 0)
        det = ast_binop(OP_ADD, det, term);
      else
        det = ast_binop(OP_SUB, det, term);
    }
  }

  free(sub);

  det = sym_simplify(det);
  det = sym_expand(det);
  det = ast_canonicalize(det);
  det = sym_collect_terms(det);
  return sym_simplify(det);
}

AstNode *sym_det(const AstNode *matrix) {
  if (!matrix || matrix->type != AST_MATRIX)
    return NULL;
  if (matrix->as.matrix.rows != matrix->as.matrix.cols)
    return NULL;

  return sym_det_recursive(matrix->as.matrix.elements, matrix->as.matrix.rows,
                           matrix->as.matrix.cols);
}
