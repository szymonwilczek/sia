#ifndef SIA_TRIGONOMETRY_H
#define SIA_TRIGONOMETRY_H

#include "../ast.h"

typedef enum {
  TRIG_KIND_NONE = 0,
  TRIG_KIND_SIN,
  TRIG_KIND_COS,
  TRIG_KIND_TAN,
  TRIG_KIND_ASIN,
  TRIG_KIND_ACOS,
  TRIG_KIND_ATAN
} TrigKind;

TrigKind trig_kind(const AstNode *node);
Complex trig_eval_value(TrigKind kind, Complex value, int *ok, char **error);
AstNode *trig_simplify_call(AstNode *node);
AstNode *trig_diff_call(const AstNode *node, const char *var);

#endif
