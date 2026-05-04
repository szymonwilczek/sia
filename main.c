#include "ast.h"
#include "eval.h"
#include "parser.h"
#include "symbolic.h"
#include "symtab.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int is_tty;
static SymTab global_symtab;

static void print_bold(const char *s) {
  if (is_tty)
    fprintf(stdout, "\033[1m%s\033[0m", s);
  else
    fputs(s, stdout);
}

static void print_error(const char *msg) {
  if (is_tty)
    fprintf(stderr, "\033[31m\033[1merror:\033[0m\033[31m %s\033[0m\n", msg);
  else
    fprintf(stderr, "error: %s\n", msg);
}

static void format_number(char *buf, size_t size, double v) {
  if (v == (long long)v && fabs(v) < 1e15)
    snprintf(buf, size, "%lld", (long long)v);
  else
    snprintf(buf, size, "%g", v);
}

static void output_str(const char *s, int batch_mode) {
  if (batch_mode)
    printf("%s", s);
  else {
    print_bold(s);
    putchar('\n');
  }
}

static int handle_let(const char *input) {
  /* "let NAME = EXPR" */
  const char *p = input;
  while (*p == ' ')
    p++;
  if (strncmp(p, "let ", 4) != 0)
    return 0;
  p += 4;
  while (*p == ' ')
    p++;

  const char *name_start = p;
  while (*p && *p != ' ' && *p != '=')
    p++;
  size_t name_len = (size_t)(p - name_start);
  if (name_len == 0)
    return 0;

  char *name = malloc(name_len + 1);
  memcpy(name, name_start, name_len);
  name[name_len] = '\0';

  while (*p == ' ')
    p++;
  if (*p != '=') {
    free(name);
    return 0;
  }
  p++;
  while (*p == ' ')
    p++;

  ParseResult pr = parse(p);
  if (pr.error) {
    print_error(pr.error);
    parse_result_free(&pr);
    free(name);
    return 1;
  }

  EvalResult er = eval(pr.root, &global_symtab);
  if (er.ok) {
    symtab_set(&global_symtab, name, er.value);
    char buf[64];
    format_number(buf, sizeof buf, er.value);
    char out[256];
    snprintf(out, sizeof out, "%s = %s", name, buf);
    print_bold(out);
    putchar('\n');
  } else {
    /* store as symbolic expression */
    symtab_set_expr(&global_symtab, name, ast_clone(pr.root));
    char *s = ast_to_string(pr.root);
    char out[512];
    snprintf(out, sizeof out, "%s = %s", name, s);
    print_bold(out);
    putchar('\n');
    free(s);
  }

  eval_result_free(&er);
  parse_result_free(&pr);
  free(name);
  return 0;
}

static int process_input(const char *input, int batch_mode) {
  if (!batch_mode && handle_let(input) != 0)
    return 0;
  if (!batch_mode) {
    const char *p = input;
    while (*p == ' ')
      p++;
    if (strncmp(p, "let ", 4) == 0)
      return handle_let(input);
  }

  ParseResult pr = parse(input);
  if (pr.error) {
    print_error(pr.error);
    parse_result_free(&pr);
    return 1;
  }

  /* symbolic function calls: diff, int, simplify */
  if (pr.root->type == AST_FUNC_CALL) {
    if (strcmp(pr.root->as.call.name, "diff") == 0 &&
        pr.root->as.call.nargs == 2 &&
        pr.root->as.call.args[1]->type == AST_VARIABLE) {

      AstNode *result = sym_diff(pr.root->as.call.args[0],
                                 pr.root->as.call.args[1]->as.variable);
      if (result) {
        result = sym_simplify(result);
        char *s = ast_to_string(result);
        output_str(s, batch_mode);
        free(s);
        ast_free(result);
      } else {
        print_error("cannot differentiate expression");
      }
      parse_result_free(&pr);
      return result ? 0 : 1;
    }

    if ((strcmp(pr.root->as.call.name, "int") == 0 ||
         strcmp(pr.root->as.call.name, "integrate") == 0) &&
        pr.root->as.call.nargs == 2 &&
        pr.root->as.call.args[1]->type == AST_VARIABLE) {

      AstNode *result = sym_integrate(pr.root->as.call.args[0],
                                      pr.root->as.call.args[1]->as.variable);
      if (result) {
        result = sym_simplify(result);
        char *s = ast_to_string(result);
        size_t len = strlen(s);
        char *out = malloc(len + 5);
        memcpy(out, s, len);
        memcpy(out + len, " + C", 5);
        output_str(out, batch_mode);
        free(s);
        free(out);
        ast_free(result);
      } else {
        print_error("cannot integrate expression");
      }
      parse_result_free(&pr);
      return result ? 0 : 1;
    }

    if (strcmp(pr.root->as.call.name, "simplify") == 0 &&
        pr.root->as.call.nargs == 1) {

      AstNode *simplified = sym_simplify(ast_clone(pr.root->as.call.args[0]));
      char *s = ast_to_string(simplified);
      output_str(s, batch_mode);
      free(s);
      ast_free(simplified);
      parse_result_free(&pr);
      return 0;
    }
  }

  /* numeric evaluation */
  EvalResult er = eval(pr.root, &global_symtab);
  if (!er.ok) {
    print_error(er.error);
    eval_result_free(&er);
    parse_result_free(&pr);
    return 1;
  }

  char buf[64];
  format_number(buf, sizeof buf, er.value);
  output_str(buf, batch_mode);

  eval_result_free(&er);
  parse_result_free(&pr);
  return 0;
}

static void repl(void) {
  char line[4096];
  fprintf(stderr, "sia v0.1.0 — type an expression, or 'quit' to exit\n");

  for (;;) {
    fprintf(stdout, "sia> ");
    fflush(stdout);

    if (!fgets(line, sizeof line, stdin))
      break;

    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      line[--len] = '\0';

    if (len == 0)
      continue;
    if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0)
      break;

    process_input(line, 0);
  }
}

int main(int argc, char **argv) {
  is_tty = isatty(fileno(stdout));
  symtab_init(&global_symtab);

  if (argc > 1) {
    size_t total = 0;
    for (int i = 1; i < argc; i++)
      total += strlen(argv[i]) + 1;

    char *expr = malloc(total + 1);
    expr[0] = '\0';
    for (int i = 1; i < argc; i++) {
      if (i > 1)
        strcat(expr, " ");
      strcat(expr, argv[i]);
    }

    int rc = process_input(expr, 1);
    free(expr);
    symtab_free(&global_symtab);
    return rc;
  }

  repl();
  symtab_free(&global_symtab);
  return 0;
}
