#ifndef SIA_SYMBOLIC_H
#define SIA_SYMBOLIC_H

#include "ast.h"

AstNode *sym_simplify(AstNode *node);
AstNode *sym_diff(const AstNode *expr, const char *var);
AstNode *sym_integrate(const AstNode *expr, const char *var);
AstNode *sym_expand(AstNode *node);
AstNode *sym_collect_terms(AstNode *expr);
AstNode *sym_det(const AstNode *matrix);

#endif
