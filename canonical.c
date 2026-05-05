#include "canonical.h"
#include <stdlib.h>
#include <string.h>

static int node_sort_key(const AstNode *n) {
  switch (n->type) {
  case AST_NUMBER:
    return 0;
  case AST_VARIABLE:
    return 1;
  case AST_FUNC_CALL:
    return 2;
  case AST_UNARY_NEG:
    return 3;
  case AST_BINOP:
    return 4;
  case AST_MATRIX:
    return 5;
  }
  return 5;
}

static int node_compare(const AstNode *a, const AstNode *b) {
  int ka = node_sort_key(a);
  int kb = node_sort_key(b);
  if (ka != kb)
    return ka - kb;

  switch (a->type) {
  case AST_NUMBER:
    if (a->as.number < b->as.number)
      return -1;
    if (a->as.number > b->as.number)
      return 1;
    return 0;
  case AST_VARIABLE:
    return strcmp(a->as.variable, b->as.variable);
  case AST_FUNC_CALL: {
    int c = strcmp(a->as.call.name, b->as.call.name);
    if (c != 0)
      return c;
    if (a->as.call.nargs != b->as.call.nargs)
      return (int)a->as.call.nargs - (int)b->as.call.nargs;
    for (size_t i = 0; i < a->as.call.nargs; i++) {
      c = node_compare(a->as.call.args[i], b->as.call.args[i]);
      if (c != 0)
        return c;
    }
    return 0;
  }
  case AST_UNARY_NEG:
    return node_compare(a->as.unary.operand, b->as.unary.operand);
  case AST_BINOP: {
    if (a->as.binop.op != b->as.binop.op)
      return (int)a->as.binop.op - (int)b->as.binop.op;
    int c = node_compare(a->as.binop.left, b->as.binop.left);
    if (c != 0)
      return c;
    return node_compare(a->as.binop.right, b->as.binop.right);
  }
  case AST_MATRIX: {
    size_t n = (a->as.matrix.rows < b->as.matrix.rows)   ? -1
               : (a->as.matrix.rows > b->as.matrix.rows) ? 1
                                                         : 0;
    if (n)
      return (int)n;
    n = (a->as.matrix.cols < b->as.matrix.cols)   ? -1
        : (a->as.matrix.cols > b->as.matrix.cols) ? 1
                                                  : 0;
    if (n)
      return (int)n;
    for (size_t i = 0; i < a->as.matrix.rows * a->as.matrix.cols; i++) {
      int c = node_compare(a->as.matrix.elements[i], b->as.matrix.elements[i]);
      if (c != 0)
        return c;
    }
    return 0;
  }
  }
  return 0;
}

static void sort_commutative(AstNode *node) {
  if (!node || node->type != AST_BINOP)
    return;
  if (node->as.binop.op != OP_ADD && node->as.binop.op != OP_MUL)
    return;

  if (node_compare(node->as.binop.left, node->as.binop.right) > 0) {
    AstNode *tmp = node->as.binop.left;
    node->as.binop.left = node->as.binop.right;
    node->as.binop.right = tmp;
  }
}

AstNode *ast_canonicalize(AstNode *node) {
  if (!node)
    return NULL;

  switch (node->type) {
  case AST_NUMBER:
  case AST_VARIABLE:
    return node;

  case AST_MATRIX: {
    size_t total = node->as.matrix.rows * node->as.matrix.cols;
    for (size_t i = 0; i < total; i++)
      node->as.matrix.elements[i] =
          ast_canonicalize(node->as.matrix.elements[i]);
    return node;
  }

  case AST_UNARY_NEG:
    node->as.unary.operand = ast_canonicalize(node->as.unary.operand);
    return node;

  case AST_FUNC_CALL:
    for (size_t i = 0; i < node->as.call.nargs; i++)
      node->as.call.args[i] = ast_canonicalize(node->as.call.args[i]);
    return node;

  case AST_BINOP:
    break;
  }

  /* A - B  =>  A + (-1 * B) */
  if (node->as.binop.op == OP_SUB) {
    node->as.binop.op = OP_ADD;
    node->as.binop.right =
        ast_binop(OP_MUL, ast_number(-1), node->as.binop.right);
  }

  /* A / B  =>  A * B^(-1) */
  if (node->as.binop.op == OP_DIV) {
    node->as.binop.op = OP_MUL;
    node->as.binop.right =
        ast_binop(OP_POW, node->as.binop.right, ast_number(-1));
  }

  node->as.binop.left = ast_canonicalize(node->as.binop.left);
  node->as.binop.right = ast_canonicalize(node->as.binop.right);

  sort_commutative(node);

  return node;
}
