#ifndef SIA_LATEX_H
#define SIA_LATEX_H

#include "ast.h"

char *ast_to_latex(const AstNode *node);

void latex_print_document(const char *latex_body);

#endif
