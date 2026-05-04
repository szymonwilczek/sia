#include "ast.h"
#include "eval.h"
#include "parser.h"
#include "symbolic.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int is_tty;

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

static int process_input(const char *input, int batch_mode) {
  ParseResult pr = parse(input);
  if (pr.error) {
    print_error(pr.error);
    parse_result_free(&pr);
    return 1;
  }

  /* check whether its a symbolic function call (diff, int) */
  if (pr.root->type == AST_FUNC_CALL) {
    if (strcmp(pr.root->as.call.name, "diff") == 0 &&
        pr.root->as.call.nargs == 2 &&
        pr.root->as.call.args[1]->type == AST_VARIABLE) {

      AstNode *result = sym_diff(pr.root->as.call.args[0],
                                 pr.root->as.call.args[1]->as.variable);
      if (result) {
        result = sym_simplify(result);
        char *s = ast_to_string(result);
        if (batch_mode)
          printf("%s", s);
        else
          print_bold(s);
        if (!batch_mode)
          putchar('\n');
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
        /* + C append */
        size_t len = strlen(s);
        char *out = malloc(len + 5);
        memcpy(out, s, len);
        memcpy(out + len, " + C", 5);
        if (batch_mode)
          printf("%s", out);
        else
          print_bold(out);
        if (!batch_mode)
          putchar('\n');
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
      if (batch_mode)
        printf("%s", s);
      else
        print_bold(s);
      if (!batch_mode)
        putchar('\n');
      free(s);
      ast_free(simplified);
      parse_result_free(&pr);
      return 0;
    }
  }

  /* numeric evaluation */
  EvalResult er = eval(pr.root);
  if (!er.ok) {
    print_error(er.error);
    eval_result_free(&er);
    parse_result_free(&pr);
    return 1;
  }

  if (batch_mode) {
    if (er.value == (long long)er.value && fabs(er.value) < 1e15)
      printf("%lld", (long long)er.value);
    else
      printf("%g", er.value);
  } else {
    char buf[64];
    if (er.value == (long long)er.value && fabs(er.value) < 1e15)
      snprintf(buf, sizeof buf, "%lld", (long long)er.value);
    else
      snprintf(buf, sizeof buf, "%g", er.value);
    print_bold(buf);
    putchar('\n');
  }

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

  if (argc > 1) {
    /* CLI: concatenate all args as expression */
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
    return rc;
  }

  repl();
  return 0;
}
