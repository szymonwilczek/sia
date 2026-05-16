#ifndef SIA_LOGARITHM_H
#define SIA_LOGARITHM_H

#include "ast.h"

typedef enum {
  LOG_KIND_NONE = 0,
  LOG_KIND_LN,
  LOG_KIND_BASE10,
  LOG_KIND_BASE2,
  LOG_KIND_GENERIC
} LogKind;

LogKind log_kind(const AstNode *node);
Complex log_eval_value_base(Complex value, Complex base, int *ok, char **error);
AstNode *log_simplify_call(AstNode *node);
AstNode *log_diff_call(const AstNode *node, const char *var);
AstNode *log_solve_call(const AstNode *node, const AstNode *rhs,
                        const char *var);

#endif
