#ifndef SIA_FACTORIAL_H
#define SIA_FACTORIAL_H

#include "ast.h"

int factorial_is_call(const AstNode *node);
Complex factorial_eval_value(Complex value, int *ok, char **error);
AstNode *factorial_simplify_call(AstNode *node);

#endif
