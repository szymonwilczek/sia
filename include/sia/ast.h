#ifndef SIA_AST_H
#define SIA_AST_H

#include "complex.h"
#include "matrix.h"
#include <stddef.h>

typedef enum {
  AST_NUMBER,
  AST_VARIABLE,
  AST_BINOP,
  AST_UNARY_NEG,
  AST_FUNC_CALL,
  AST_LIMIT,
  AST_MATRIX,
  AST_EQ,
  AST_INFINITY,
  AST_UNDEFINED
} AstType;

typedef enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW } BinOpKind;

typedef struct AstNode AstNode;

struct AstNode {
  AstType type;
  union {
    Complex number;

    char *variable;

    struct {
      BinOpKind op;
      AstNode *left;
      AstNode *right;
    } binop;

    struct {
      AstNode *operand;
    } unary;

    struct {
      char *name;
      AstNode **args;
      size_t nargs;
    } call;

    struct {
      AstNode *expr;
      char *var;
      AstNode *target;
    } limit;

    struct {
      AstNode **elements; /* row-major, size = rows * cols */
      size_t rows;
      size_t cols;
    } matrix;

    struct {
      AstNode *lhs;
      AstNode *rhs;
    } eq;

    struct {
      int sign; /* +1 or -1 */
    } infinity;
  } as;
};

AstNode *ast_number(double val);
AstNode *ast_number_complex(Complex value);
AstNode *ast_complex(double re, double im);
AstNode *ast_variable(const char *name, size_t len);
AstNode *ast_binop(BinOpKind op, AstNode *left, AstNode *right);
AstNode *ast_unary_neg(AstNode *operand);
AstNode *ast_func_call(const char *name, size_t namelen, AstNode **args,
                       size_t nargs);
AstNode *ast_limit(AstNode *expr, const char *var, size_t varlen,
                   AstNode *target);
AstNode *ast_matrix(AstNode **elements, size_t rows, size_t cols);
AstNode *ast_eq(AstNode *lhs, AstNode *rhs);
AstNode *ast_infinity(int sign);
AstNode *ast_undefined(void);

AstNode *ast_clone(const AstNode *node);
void ast_free(AstNode *node);

char *ast_to_string(const AstNode *node);

#endif
