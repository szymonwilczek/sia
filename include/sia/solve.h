#ifndef SIA_SOLVE_H
#define SIA_SOLVE_H

#include "ast.h"
#include "symtab.h"

typedef struct {
  Complex *roots;
  size_t count;
  AstNode **symbolic_roots;
  size_t symbolic_count;
  int ok;
  char *error;
} SolveResult;

SolveResult sym_solve(const AstNode *expr, const char *var, Complex x0,
                      const SymTab *st);
void solve_result_free(SolveResult *r);

#endif
