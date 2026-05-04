#ifndef SIA_PARSER_H
#define SIA_PARSER_H

#include "ast.h"

typedef struct {
  AstNode *root;
  char *error;
} ParseResult;

ParseResult parse(const char *input);
void parse_result_free(ParseResult *r);

#endif
