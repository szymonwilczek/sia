#ifndef SIA_SYMTAB_H
#define SIA_SYMTAB_H

#include "ast.h"

#define SYMTAB_BUCKETS 64

typedef struct SymEntry SymEntry;
struct SymEntry {
  char *name;
  double value;
  AstNode *expr;
  SymEntry *next;
};

typedef struct {
  SymEntry *buckets[SYMTAB_BUCKETS];
} SymTab;

void symtab_init(SymTab *st);
void symtab_free(SymTab *st);
void symtab_set(SymTab *st, const char *name, double value);
void symtab_set_expr(SymTab *st, const char *name, AstNode *expr);
int symtab_get(const SymTab *st, const char *name, double *out);
AstNode *symtab_get_expr(const SymTab *st, const char *name);
void symtab_remove(SymTab *st, const char *name);

#endif
