#ifndef SIA_ASSUMPTIONS_H
#define SIA_ASSUMPTIONS_H

#include "ast.h"

typedef enum {
  SIA_SIGN_UNKNOWN,
  SIA_SIGN_NEGATIVE,
  SIA_SIGN_ZERO,
  SIA_SIGN_POSITIVE,
  SIA_SIGN_NONNEGATIVE,
  SIA_SIGN_NONPOSITIVE
} SiaSign;

SiaSign sia_known_sign(const AstNode *expr);
int sia_known_nonnegative(const AstNode *expr);
int sia_known_positive(const AstNode *expr);

#endif
