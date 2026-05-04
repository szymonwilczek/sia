#ifndef SIA_EVAL_H
#define SIA_EVAL_H

#include "ast.h"

typedef struct {
  double value;
  int ok;
  char *error;
} EvalResult;

EvalResult eval(const AstNode *node);
void eval_result_free(EvalResult *r);

#endif
