#ifndef SIA_LIMITS_H
#define SIA_LIMITS_H

#include "ast.h"

AstNode *sym_limit(const AstNode *expr, const char *var, const AstNode *target);
AstNode *sym_limit_directed(const AstNode *expr, const char *var,
                            const AstNode *target, int direction);

#endif
