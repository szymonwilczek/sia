#ifndef SIA_NUMBER_THEORY_H
#define SIA_NUMBER_THEORY_H

#include "ast.h"

typedef enum { NT_KIND_NONE = 0, NT_KIND_GCD, NT_KIND_LCM } NumberTheoryKind;

NumberTheoryKind number_theory_kind(const AstNode *node);
Complex number_theory_eval_value(NumberTheoryKind kind, Complex left,
                                 Complex right, int *ok, char **error);
AstNode *number_theory_simplify_call(AstNode *node);

#endif
