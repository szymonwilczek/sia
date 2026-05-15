#ifndef SIA_CANONICAL_H
#define SIA_CANONICAL_H

#include "ast.h"

AstNode *ast_canonicalize(AstNode *node);
AstNode *ast_polynomial_canonicalize(const AstNode *node);

#endif
