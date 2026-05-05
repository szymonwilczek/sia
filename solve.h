#ifndef SIA_SOLVE_H
#define SIA_SOLVE_H

#include "ast.h"
#include "symtab.h"

typedef struct {
  double *roots;
  size_t count;
  int ok;
  char *error;
} SolveResult;

SolveResult sym_solve(const AstNode *expr, const char *var, double x0,
                      const SymTab *st);
void solve_result_free(SolveResult *r);

#endif
