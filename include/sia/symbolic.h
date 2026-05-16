#ifndef SIA_SYMBOLIC_H
#define SIA_SYMBOLIC_H

#include "ast.h"

AstNode *sym_simplify(AstNode *node);
AstNode *sym_full_simplify(AstNode *node);
AstNode *sym_diff(const AstNode *expr, const char *var);
AstNode *sym_diff_n(const AstNode *expr, const char *var, int order);
AstNode *sym_grad(const AstNode *expr, const AstNode *vars_matrix);
AstNode *sym_subs(const AstNode *expr, const char *var, const AstNode *val);
AstNode *sym_taylor(const AstNode *expr, const char *var, const AstNode *a,
                    int order);
AstNode *sym_integrate(const AstNode *expr, const char *var);
AstNode *sym_expand(AstNode *node);
AstNode *sym_collect_terms(AstNode *expr);
AstNode *sym_factor(AstNode *node);
AstNode *sym_det(const AstNode *matrix);
AstNode *sym_laplace(const AstNode *expr, const char *t, const char *s);
int sym_contains_var(const AstNode *n, const char *var);

#endif
