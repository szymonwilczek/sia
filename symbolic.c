#include "symbolic.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int is_number(const AstNode *n, double v) {
  return n && n->type == AST_NUMBER && n->as.number == v;
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

/* check if n is f(u)^2 for a given function name f */
static int is_call1_squared(const AstNode *n, const char *fname) {
  return n && n->type == AST_BINOP && n->as.binop.op == OP_POW &&
         is_number(n->as.binop.right, 2) && is_call1(n->as.binop.left, fname);
}

static int contains_var(const AstNode *n, const char *var) {
  if (!n)
    return 0;
  switch (n->type) {
  case AST_NUMBER:
    return 0;
  case AST_VARIABLE:
    return strcmp(n->as.variable, var) == 0;
  case AST_BINOP:
    return contains_var(n->as.binop.left, var) ||
           contains_var(n->as.binop.right, var);
  case AST_UNARY_NEG:
    return contains_var(n->as.unary.operand, var);
  case AST_FUNC_CALL:
    for (size_t i = 0; i < n->as.call.nargs; i++)
      if (contains_var(n->as.call.args[i], var))
        return 1;
    return 0;
  case AST_MATRIX: {
    size_t total = n->as.matrix.rows * n->as.matrix.cols;
    for (size_t i = 0; i < total; i++)
      if (contains_var(n->as.matrix.elements[i], var))
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
    return a->as.number == b->as.number;
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
      double v = -node->as.unary.operand->as.number;
      ast_free(node);
      return ast_number(v);
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
    double result;
    switch (op) {
    case OP_ADD:
      result = L->as.number + R->as.number;
      break;
    case OP_SUB:
      result = L->as.number - R->as.number;
      break;
    case OP_MUL:
      result = L->as.number * R->as.number;
      break;
    case OP_DIV:
      if (R->as.number == 0.0)
        return node;
      result = L->as.number / R->as.number;
      break;
    case OP_POW:
      result = pow(L->as.number, R->as.number);
      break;
    default:
      return node;
    }
    ast_free(node);
    return ast_number(result);
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
      double c = L->as.binop.left->as.number + R->as.binop.left->as.number;
      AstNode *base = ast_clone(L->as.binop.right);
      ast_free(node);
      return sym_simplify(ast_binop(OP_MUL, ast_number(c), base));
    }
    /* c1*E + E -> (c1+1)*E */
    if (L->type == AST_BINOP && L->as.binop.op == OP_MUL &&
        is_num(L->as.binop.left) && ast_equal(L->as.binop.right, R)) {
      double c = L->as.binop.left->as.number + 1.0;
      node->as.binop.right = NULL;
      AstNode *base = L->as.binop.right;
      L->as.binop.right = NULL;
      ast_free(node);
      return sym_simplify(ast_binop(OP_MUL, ast_number(c), base));
    }
    /* E + c*E -> (c+1)*E */
    if (R->type == AST_BINOP && R->as.binop.op == OP_MUL &&
        is_num(R->as.binop.left) && ast_equal(L, R->as.binop.right)) {
      double c = R->as.binop.left->as.number + 1.0;
      node->as.binop.left = NULL;
      AstNode *base = ast_clone(L);
      ast_free(node);
      return sym_simplify(ast_binop(OP_MUL, ast_number(c), base));
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
    /* c1 * (c2 * E) -> (c1*c2) * E */
    if (is_num(L) && R->type == AST_BINOP && R->as.binop.op == OP_MUL &&
        is_num(R->as.binop.left)) {
      double c = L->as.number * R->as.binop.left->as.number;
      AstNode *base = R->as.binop.right;
      R->as.binop.right = NULL;
      ast_free(node);
      return sym_simplify(ast_binop(OP_MUL, ast_number(c), base));
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
    if (is_num(R) && R->as.number != 0.0 && L->type == AST_BINOP &&
        L->as.binop.op == OP_MUL && is_num(L->as.binop.left)) {
      double c = L->as.binop.left->as.number / R->as.number;
      AstNode *base = ast_clone(L->as.binop.right);
      ast_free(node);
      return sym_simplify(ast_binop(OP_MUL, ast_number(c), base));
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
      if (!contains_var(R, var) && !contains_var(L, var)) {
        return ast_number(0);
      }
      /* x^n where n constant: n*x^(n-1)*x' */
      if (!contains_var(R, var)) {
        return sym_simplify(ast_binop(
            OP_MUL,
            ast_binop(
                OP_MUL, ast_clone(R),
                ast_binop(OP_POW, ast_clone(L),
                          ast_binop(OP_SUB, ast_clone(R), ast_number(1)))),
            sym_diff(L, var)));
      }
      /* a^x where a constant: a^x * ln(a) * x' */
      if (!contains_var(L, var)) {
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
    } else if (strcmp(name, "ln") == 0) {
      outer_d = ast_binop(OP_DIV, ast_number(1), ast_clone(inner));
    } else if (strcmp(name, "log") == 0) {
      /* d/dx log10(u) = 1/(u * ln(10)) */
      outer_d =
          ast_binop(OP_DIV, ast_number(1),
                    ast_binop(OP_MUL, ast_clone(inner), ast_number(log(10.0))));
    } else if (strcmp(name, "exp") == 0) {
      outer_d = ast_clone(expr);
    } else if (strcmp(name, "sqrt") == 0) {
      outer_d = ast_binop(
          OP_DIV, ast_number(1),
          ast_binop(
              OP_MUL, ast_number(2),
              ast_func_call("sqrt", 4, (AstNode *[]){ast_clone(inner)}, 1)));
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
  if (!contains_var(expr, var)) {
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
      is_var(expr->as.binop.left, var) && is_num(expr->as.binop.right)) {
    double n = expr->as.binop.right->as.number;
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
    if (!contains_var(expr->as.binop.left, var)) {
      AstNode *inner = integrate_monomial(expr->as.binop.right, var);
      if (!inner)
        inner = integrate_trig(expr->as.binop.right, var);
      if (inner)
        return ast_binop(OP_MUL, ast_clone(expr->as.binop.left), inner);
    }
    if (!contains_var(expr->as.binop.right, var)) {
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
