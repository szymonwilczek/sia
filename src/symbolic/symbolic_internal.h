#ifndef SIA_SYMBOLIC_INTERNAL_H
#define SIA_SYMBOLIC_INTERNAL_H

#include "sia/ast.h"
#include "sia/symbolic.h"
#include <stdlib.h>
#include <string.h>

static inline int is_number(const AstNode *n, double v) {
  return n && n->type == AST_NUMBER && c_is_real(n->as.number) &&
         n->as.number.re == v;
}

static inline int is_num(const AstNode *n) {
  return n && n->type == AST_NUMBER;
}

static inline int is_var(const AstNode *n, const char *var) {
  return n && n->type == AST_VARIABLE && strcmp(n->as.variable, var) == 0;
}

static inline int is_call1(const AstNode *n, const char *name) {
  return n && n->type == AST_FUNC_CALL && n->as.call.nargs == 1 &&
         strcmp(n->as.call.name, name) == 0;
}

static inline int is_zero_node(const AstNode *n) {
  return n && n->type == AST_NUMBER && c_is_zero(n->as.number);
}

static inline AstNode *ast_rational(long long num, long long den) {
  return ast_number_complex(
      c_from_fractions(fraction_make(num, den), fraction_make(0, 1)));
}

static inline int ast_equal(const AstNode *a, const AstNode *b) {
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
  case AST_LIMIT:
    return strcmp(a->as.limit.var, b->as.limit.var) == 0 &&
           ast_equal(a->as.limit.target, b->as.limit.target) &&
           ast_equal(a->as.limit.expr, b->as.limit.expr);
  case AST_MATRIX:
    if (a->as.matrix.rows != b->as.matrix.rows ||
        a->as.matrix.cols != b->as.matrix.cols)
      return 0;
    for (size_t i = 0; i < a->as.matrix.rows * a->as.matrix.cols; i++)
      if (!ast_equal(a->as.matrix.elements[i], b->as.matrix.elements[i]))
        return 0;
    return 1;
  case AST_EQ:
    return ast_equal(a->as.eq.lhs, b->as.eq.lhs) &&
           ast_equal(a->as.eq.rhs, b->as.eq.rhs);
  }
  return 0;
}

typedef struct {
  AstNode **items;
  size_t count;
  size_t cap;
} NodeList;

static inline int node_list_push(NodeList *list, AstNode *node) {
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

static inline void node_list_free(NodeList *list) {
  if (!list)
    return;
  for (size_t i = 0; i < list->count; i++)
    ast_free(list->items[i]);
  free(list->items);
  list->items = NULL;
  list->count = 0;
  list->cap = 0;
}

AstNode *sym_reduce_rational_subexprs(const AstNode *node);
int sym_extract_coeff_and_base(const AstNode *node, Complex *coeff,
                               AstNode **base);

#endif
