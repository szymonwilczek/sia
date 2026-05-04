#ifndef SIA_SYMBOLIC_H
#define SIA_SYMBOLIC_H

#include "ast.h"

AstNode *sym_simplify(AstNode *node);
AstNode *sym_diff(const AstNode *expr, const char *var);
AstNode *sym_integrate(const AstNode *expr, const char *var);

#endif
