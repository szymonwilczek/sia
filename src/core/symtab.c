#include "sia/symtab.h"
#include <stdlib.h>
#include <string.h>

static unsigned hash(const char *s) {
  unsigned h = 5381;
  while (*s)
    h = h * 33 + (unsigned char)*s++;
  return h % SYMTAB_BUCKETS;
}

void symtab_init(SymTab *st) {
  for (int i = 0; i < SYMTAB_BUCKETS; i++)
    st->buckets[i] = NULL;
}

void symtab_free(SymTab *st) {
  for (int i = 0; i < SYMTAB_BUCKETS; i++) {
    SymEntry *e = st->buckets[i];
    while (e) {
      SymEntry *next = e->next;
      free(e->name);
      ast_free(e->expr);
      if (e->params) {
        for (size_t j = 0; j < e->num_params; j++)
          free(e->params[j]);
        free(e->params);
      }
      free(e);
      e = next;
    }
    st->buckets[i] = NULL;
  }
}

static SymEntry *find(const SymTab *st, const char *name) {
  unsigned idx = hash(name);
  SymEntry *e = st->buckets[idx];
  while (e) {
    if (strcmp(e->name, name) == 0)
      return e;
    e = e->next;
  }
  return NULL;
}

static SymEntry *create_entry(SymTab *st, const char *name) {
  SymEntry *e = find(st, name);
  if (e) {
    ast_free(e->expr);
    e->expr = NULL;
    if (e->params) {
      for (size_t j = 0; j < e->num_params; j++)
        free(e->params[j]);
      free(e->params);
      e->params = NULL;
    }
    e->num_params = 0;
    return e;
  }
  e = malloc(sizeof *e);
  if (!e)
    abort();
  e->name = strdup(name);
  e->value = c_real(0.0);
  e->expr = NULL;
  e->params = NULL;
  e->num_params = 0;
  unsigned idx = hash(name);
  e->next = st->buckets[idx];
  st->buckets[idx] = e;
  return e;
}

void symtab_set(SymTab *st, const char *name, Complex value) {
  SymEntry *e = create_entry(st, name);
  e->value = value;
}

void symtab_set_expr(SymTab *st, const char *name, AstNode *expr) {
  SymEntry *e = create_entry(st, name);
  e->expr = expr;
  e->value = c_real(0.0);
}

void symtab_set_func(SymTab *st, const char *name, char **params,
                     size_t num_params, AstNode *expr) {
  SymEntry *e = create_entry(st, name);
  e->expr = expr;
  e->params = params;
  e->num_params = num_params;
  e->value = c_real(0.0);
}

int symtab_get(const SymTab *st, const char *name, Complex *out) {
  SymEntry *e = find(st, name);
  if (!e || e->expr)
    return 0;
  *out = e->value;
  return 1;
}

AstNode *symtab_get_expr(const SymTab *st, const char *name) {
  SymEntry *e = find(st, name);
  if (!e || e->params)
    return NULL;
  return e->expr;
}

int symtab_get_func(const SymTab *st, const char *name, char ***params,
                    size_t *num_params, AstNode **expr) {
  SymEntry *e = find(st, name);
  if (!e || !e->params || !e->expr)
    return 0;
  if (params)
    *params = e->params;
  if (num_params)
    *num_params = e->num_params;
  if (expr)
    *expr = e->expr;
  return 1;
}

void symtab_remove(SymTab *st, const char *name) {
  unsigned idx = hash(name);
  SymEntry **pp = &st->buckets[idx];
  while (*pp) {
    if (strcmp((*pp)->name, name) == 0) {
      SymEntry *e = *pp;
      *pp = e->next;
      free(e->name);
      ast_free(e->expr);
      if (e->params) {
        for (size_t j = 0; j < e->num_params; j++)
          free(e->params[j]);
        free(e->params);
      }
      free(e);
      return;
    }
    pp = &(*pp)->next;
  }
}
