#include "ast.h"
#include "canonical.h"
#include "eval.h"
#include "matrix.h"
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

/* replace variables in an AST with their stored values/expressions */
static AstNode *substitute_params(const AstNode *node, char **params,
                                  AstNode **args, size_t n) {
  if (!node)
    return NULL;
  switch (node->type) {
  case AST_NUMBER:
    return ast_clone(node);
  case AST_VARIABLE: {
    for (size_t i = 0; i < n; i++) {
      if (strcmp(node->as.variable, params[i]) == 0) {
        return ast_clone(args[i]);
      }
    }
    return ast_clone(node);
  }
  case AST_UNARY_NEG:
    return ast_unary_neg(
        substitute_params(node->as.unary.operand, params, args, n));
  case AST_BINOP:
    return ast_binop(node->as.binop.op,
                     substitute_params(node->as.binop.left, params, args, n),
                     substitute_params(node->as.binop.right, params, args, n));
  case AST_FUNC_CALL: {
    AstNode **new_args = malloc(node->as.call.nargs * sizeof(AstNode *));
    for (size_t i = 0; i < node->as.call.nargs; i++) {
      new_args[i] = substitute_params(node->as.call.args[i], params, args, n);
    }
    AstNode *c = ast_func_call(node->as.call.name, strlen(node->as.call.name),
                               new_args, node->as.call.nargs);
    free(new_args);
    return c;
  }
  case AST_MATRIX: {
    size_t total = node->as.matrix.rows * node->as.matrix.cols;
    AstNode **elems = malloc(total * sizeof(AstNode *));
    for (size_t i = 0; i < total; i++) {
      elems[i] =
          substitute_params(node->as.matrix.elements[i], params, args, n);
    }
    AstNode *m = ast_matrix(elems, node->as.matrix.rows, node->as.matrix.cols);
    free(elems);
    return m;
  }
  }
  return NULL;
}

static AstNode *substitute_vars(AstNode *node, const SymTab *st) {
  if (!node)
    return NULL;

  switch (node->type) {
  case AST_NUMBER:
    return node;

  case AST_MATRIX: {
    size_t total = node->as.matrix.rows * node->as.matrix.cols;
    for (size_t i = 0; i < total; i++)
      node->as.matrix.elements[i] =
          substitute_vars(node->as.matrix.elements[i], st);
    return node;
  }

  case AST_VARIABLE: {
    /* scalar value first */
    double val;
    if (symtab_get(st, node->as.variable, &val)) {
      ast_free(node);
      return ast_number(val);
    }
    /* stored expression (eg: matrix) */
    AstNode *expr = symtab_get_expr(st, node->as.variable);
    if (expr) {
      AstNode *clone = ast_clone(expr);
      ast_free(node);
      return clone;
    }
    return node;
  }

  case AST_BINOP:
    node->as.binop.left = substitute_vars(node->as.binop.left, st);
    node->as.binop.right = substitute_vars(node->as.binop.right, st);
    return node;

  case AST_UNARY_NEG:
    node->as.unary.operand = substitute_vars(node->as.unary.operand, st);
    return node;

  case AST_FUNC_CALL: {
    for (size_t i = 0; i < node->as.call.nargs; i++)
      node->as.call.args[i] = substitute_vars(node->as.call.args[i], st);

    char **params;
    size_t num_params;
    AstNode *body;
    if (symtab_get_func(st, node->as.call.name, &params, &num_params, &body)) {
      if (node->as.call.nargs == num_params) {
        AstNode *res =
            substitute_params(body, params, node->as.call.args, num_params);
        ast_free(node);
        return res;
      }
    }
    return node;
  }
  }
  return node;
}

static int has_matrix(const AstNode *node) {
  if (!node)
    return 0;
  if (node->type == AST_MATRIX)
    return 1;
  if (node->type == AST_BINOP)
    return has_matrix(node->as.binop.left) || has_matrix(node->as.binop.right);
  if (node->type == AST_UNARY_NEG)
    return has_matrix(node->as.unary.operand);
  if (node->type == AST_FUNC_CALL)
    for (size_t i = 0; i < node->as.call.nargs; i++)
      if (has_matrix(node->as.call.args[i]))
        return 1;
  return 0;
}

/* Evaluate an AST that may contain matrix nodes.
 * Returns a new Matrix on success, or NULL on failure.
 * Supports +, -, *, scalar scaling, and unary negation. */
static Matrix *eval_matrix_expr(const AstNode *node) {
  if (!node)
    return NULL;

  if (node->type == AST_MATRIX) {
    size_t rows = node->as.matrix.rows;
    size_t cols = node->as.matrix.cols;
    Matrix *m = matrix_create(rows, cols);
    for (size_t i = 0; i < rows * cols; i++) {
      EvalResult er = eval(node->as.matrix.elements[i], &global_symtab);
      if (!er.ok) {
        eval_result_free(&er);
        matrix_free(m);
        return NULL;
      }
      m->data[i] = er.value;
      eval_result_free(&er);
    }
    return m;
  }

  if (node->type == AST_UNARY_NEG) {
    Matrix *m = eval_matrix_expr(node->as.unary.operand);
    if (!m)
      return NULL;
    Matrix *r = matrix_scale(m, -1.0);
    matrix_free(m);
    return r;
  }

  if (node->type != AST_BINOP)
    return NULL;

  const AstNode *L = node->as.binop.left;
  const AstNode *R = node->as.binop.right;

  switch (node->as.binop.op) {
  case OP_ADD: {
    Matrix *ml = eval_matrix_expr(L);
    Matrix *mr = eval_matrix_expr(R);
    if (!ml || !mr) {
      matrix_free(ml);
      matrix_free(mr);
      return NULL;
    }
    Matrix *r = matrix_add(ml, mr);
    matrix_free(ml);
    matrix_free(mr);
    return r;
  }
  case OP_SUB: {
    Matrix *ml = eval_matrix_expr(L);
    Matrix *mr = eval_matrix_expr(R);
    if (!ml || !mr) {
      matrix_free(ml);
      matrix_free(mr);
      return NULL;
    }
    Matrix *r = matrix_sub(ml, mr);
    matrix_free(ml);
    matrix_free(mr);
    return r;
  }
  case OP_MUL: {
    /* scalar * matrix */
    if (L->type == AST_NUMBER && has_matrix(R)) {
      Matrix *mr = eval_matrix_expr(R);
      if (!mr)
        return NULL;
      Matrix *r = matrix_scale(mr, L->as.number);
      matrix_free(mr);
      return r;
    }
    /* matrix * scalar */
    if (R->type == AST_NUMBER && has_matrix(L)) {
      Matrix *ml = eval_matrix_expr(L);
      if (!ml)
        return NULL;
      Matrix *r = matrix_scale(ml, R->as.number);
      matrix_free(ml);
      return r;
    }
    /* matrix * matrix */
    Matrix *ml = eval_matrix_expr(L);
    Matrix *mr = eval_matrix_expr(R);
    if (!ml || !mr) {
      matrix_free(ml);
      matrix_free(mr);
      return NULL;
    }
    Matrix *r = matrix_mul(ml, mr);
    matrix_free(ml);
    matrix_free(mr);
    return r;
  }
  default:
    return NULL;
  }
}

static int handle_let(const char *input) {
  /* "let NAME = EXPR" or "let f(x, ...) = EXPR" */
  const char *p = input;
  while (*p == ' ')
    p++;
  if (strncmp(p, "let ", 4) != 0)
    return 0;
  p += 4;
  while (*p == ' ')
    p++;

  const char *name_start = p;
  while (*p && *p != ' ' && *p != '=' && *p != '(')
    p++;
  size_t name_len = (size_t)(p - name_start);
  if (name_len == 0)
    return 0;

  char *name = malloc(name_len + 1);
  memcpy(name, name_start, name_len);
  name[name_len] = '\0';

  char **params = NULL;
  size_t num_params = 0;

  if (*p == '(') {
    p++;
    size_t cap = 4;
    params = malloc(cap * sizeof(char *));
    while (*p && *p != ')') {
      while (*p == ' ')
        p++;
      const char *param_start = p;
      while (*p && *p != ' ' && *p != ',' && *p != ')')
        p++;
      size_t param_len = (size_t)(p - param_start);
      if (param_len > 0) {
        if (num_params >= cap) {
          cap *= 2;
          params = realloc(params, cap * sizeof(char *));
        }
        params[num_params] = malloc(param_len + 1);
        memcpy(params[num_params], param_start, param_len);
        params[num_params][param_len] = '\0';
        num_params++;
      }
      while (*p == ' ')
        p++;
      if (*p == ',')
        p++;
    }
    if (*p == ')')
      p++;
  }

  while (*p == ' ')
    p++;
  if (*p != '=') {
    free(name);
    if (params) {
      for (size_t i = 0; i < num_params; i++)
        free(params[i]);
      free(params);
    }
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
    if (params) {
      for (size_t i = 0; i < num_params; i++)
        free(params[i]);
      free(params);
    }
    return 1;
  }

  if (params) {
    symtab_set_func(&global_symtab, name, params, num_params,
                    ast_clone(pr.root));
    char *s = ast_to_string(pr.root);
    char out[512];
    snprintf(out, sizeof out, "%s = %s", name, s);
    print_bold(out);
    putchar('\n');
    free(s);
  } else {
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
  }

  parse_result_free(&pr);
  free(name);
  return 1;
}

static int process_input(const char *input, int batch_mode) {
  /* handle let bindings (REPL only) */
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

      /* substitute known variables before differentiation */
      AstNode *expr =
          substitute_vars(ast_clone(pr.root->as.call.args[0]), &global_symtab);
      AstNode *result = sym_diff(expr, pr.root->as.call.args[1]->as.variable);
      ast_free(expr);
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

      /* substitute known variables and ast_canonicalize before integration */
      AstNode *expr =
          substitute_vars(ast_clone(pr.root->as.call.args[0]), &global_symtab);
      expr = ast_canonicalize(expr);
      expr = sym_simplify(expr);
      AstNode *result =
          sym_integrate(expr, pr.root->as.call.args[1]->as.variable);
      ast_free(expr);
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

      /* substitute known variables before simplification */
      AstNode *expr =
          substitute_vars(ast_clone(pr.root->as.call.args[0]), &global_symtab);
      AstNode *simplified = sym_simplify(expr);
      char *s = ast_to_string(simplified);
      output_str(s, batch_mode);
      free(s);
      ast_free(simplified);
      parse_result_free(&pr);
      return 0;
    }

    if (strcmp(pr.root->as.call.name, "expand") == 0 &&
        pr.root->as.call.nargs == 1) {

      /* substitute known variables before expansion */
      AstNode *expr =
          substitute_vars(ast_clone(pr.root->as.call.args[0]), &global_symtab);
      AstNode *expanded = sym_expand(expr);
      expanded = ast_canonicalize(expanded);
      expanded = sym_collect_terms(expanded);
      char *s = ast_to_string(expanded);
      output_str(s, batch_mode);
      free(s);
      ast_free(expanded);
      parse_result_free(&pr);
      return 0;
    }
  }

  /* substitute known variables and check for matrix expressions */
  AstNode *resolved = substitute_vars(ast_clone(pr.root), &global_symtab);

  if (has_matrix(resolved)) {
    Matrix *m = eval_matrix_expr(resolved);
    if (m) {
      char *s = matrix_to_string(m);
      output_str(s, batch_mode);
      free(s);
      matrix_free(m);
    } else {
      /* symbolic matrix: display as-is */
      char *s = ast_to_string(resolved);
      output_str(s, batch_mode);
      free(s);
    }
    ast_free(resolved);
    parse_result_free(&pr);
    return 0;
  }

  /* numeric evaluation */
  EvalResult er = eval(resolved, &global_symtab);
  if (er.ok) {
    char buf[64];
    format_number(buf, sizeof buf, er.value);
    output_str(buf, batch_mode);
  } else {
    /* fallback to symbolic simplification */
    AstNode *simplified = sym_simplify(resolved);
    char *s = ast_to_string(simplified);
    output_str(s, batch_mode);
    free(s);
    ast_free(simplified);
    resolved = NULL;
  }

  if (resolved)
    ast_free(resolved);
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
