#ifndef SIA_EVAL_H
#define SIA_EVAL_H

#include "ast.h"
#include "symtab.h"

typedef struct {
  double value;
  int ok;
  char *error;
} EvalResult;

EvalResult eval(const AstNode *node, const SymTab *st);
void eval_result_free(EvalResult *r);

#endif
