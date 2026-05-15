#include "ast.h"
#include "canonical.h"
#include "eval.h"
#include "fractions.h"
#include "latex.h"
#include "matrix.h"
#include "parser.h"
#include "solve.h"
#include "symbolic.h"
#include "symtab.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int is_tty;
static int latex_mode;
static int full_doc_mode;
static int late_binding_mode;
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

static void format_number(char *buf, size_t size, Complex z) {
  if (z.exact && fraction_is_zero(z.im_q)) {
    Fraction f = z.re_q;
    if (f.den != 1) {
      if (f.num < 0)
        snprintf(buf, size, "-%lld/%lld", -f.num, f.den);
      else
        snprintf(buf, size, "%lld/%lld", f.num, f.den);
    } else {
      snprintf(buf, size, "%lld", f.num);
    }
  } else if (fabs(z.im) < 1e-15) {
    double v = z.re;
    Fraction f = fraction_from_double(v);
    if (f.den != 1) {
      if (f.num < 0)
        snprintf(buf, size, "-%lld/%lld", -f.num, f.den);
      else
        snprintf(buf, size, "%lld/%lld", f.num, f.den);
    } else if (v == (long long)v && fabs(v) < 1e15) {
      snprintf(buf, size, "%lld", (long long)v);
    } else {
      snprintf(buf, size, "%g", v);
    }
  } else if (z.exact && fraction_is_zero(z.re_q)) {
    if (fraction_is_one(z.im_q))
      snprintf(buf, size, "i");
    else if (fraction_eq(z.im_q, fraction_neg((Fraction){1, 1})))
      snprintf(buf, size, "-i");
    else if (z.im_q.den == 1)
      snprintf(buf, size, "%lldi", z.im_q.num);
    else
      snprintf(buf, size, "%lld/%lldi", z.im_q.num, z.im_q.den);
  } else if (fabs(z.re) < 1e-15) {
    if (z.im == 1.0)
      snprintf(buf, size, "i");
    else if (z.im == -1.0)
      snprintf(buf, size, "-i");
    else if (z.im == (long long)z.im && fabs(z.im) < 1e15)
      snprintf(buf, size, "%lldi", (long long)z.im);
    else
      snprintf(buf, size, "%gi", z.im);
  } else {
    char im_buf[32];
    if (z.exact) {
      if (fraction_is_one(z.im_q))
        snprintf(im_buf, sizeof im_buf, "i");
      else if (fraction_is_zero(z.im_q))
        snprintf(im_buf, sizeof im_buf, "0");
      else if (z.im_q.den == 1)
        snprintf(im_buf, sizeof im_buf, "%lldi",
                 z.im_q.num < 0 ? -z.im_q.num : z.im_q.num);
      else
        snprintf(im_buf, sizeof im_buf, "%lld/%lldi", llabs(z.im_q.num),
                 z.im_q.den);

      if (z.re_q.den == 1)
        snprintf(buf, size, "%lld %c %s", z.re_q.num,
                 z.im_q.num >= 0 ? '+' : '-', im_buf);
      else
        snprintf(buf, size, "%lld/%lld %c %s", z.re_q.num, z.re_q.den,
                 z.im_q.num >= 0 ? '+' : '-', im_buf);
    } else {
      double aim = fabs(z.im);
      if (aim == 1.0)
        snprintf(im_buf, sizeof im_buf, "i");
      else if (aim == (long long)aim && aim < 1e15)
        snprintf(im_buf, sizeof im_buf, "%lldi", (long long)aim);
      else
        snprintf(im_buf, sizeof im_buf, "%gi", aim);

      if (z.re == (long long)z.re && fabs(z.re) < 1e15)
        snprintf(buf, size, "%lld %c %s", (long long)z.re, z.im > 0 ? '+' : '-',
                 im_buf);
      else
        snprintf(buf, size, "%g %c %s", z.re, z.im > 0 ? '+' : '-', im_buf);
    }
  }
}

static void output_result(const AstNode *node, const char *plain,
                          int batch_mode) {
  if (latex_mode) {
    char *tex = node ? ast_to_latex(node) : NULL;
    const char *out = tex ? tex : plain;
    if (full_doc_mode) {
      latex_print_document(out);
    } else if (batch_mode) {
      printf("%s", out);
    } else {
      print_bold(out);
      putchar('\n');
    }
    free(tex);
  } else {
    if (batch_mode)
      printf("%s", plain);
    else {
      print_bold(plain);
      putchar('\n');
    }
  }
}

static void output_str(const char *s, int batch_mode) {
  if (batch_mode)
    printf("%s", s);
  else {
    print_bold(s);
    putchar('\n');
  }
}

static int parse_nonnegative_int_arg(const AstNode *node, const SymTab *st,
                                     int *out) {
  EvalResult er;
  double rounded = 0.0;

  if (!node || !out)
    return 0;

  er = eval(node, st);
  if (!er.ok) {
    eval_result_free(&er);
    return 0;
  }

  if (!c_is_real(er.value)) {
    eval_result_free(&er);
    return 0;
  }

  rounded = round(er.value.re);
  if (fabs(er.value.re - rounded) > 1e-9 || rounded < 0.0 ||
      rounded > (double)INT_MAX) {
    eval_result_free(&er);
    return 0;
  }

  *out = (int)rounded;
  eval_result_free(&er);
  return 1;
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
    Complex val;
    if (!late_binding_mode && symtab_get(st, node->as.variable, &val)) {
      ast_free(node);
      return ast_complex(val.re, val.im);
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

static AstNode *resolve_symbolic(AstNode *node, const SymTab *st) {
  if (!node)
    return NULL;

  if (node->type == AST_FUNC_CALL) {
    /* diff(expr, var[, order]) */
    if (strcmp(node->as.call.name, "diff") == 0 &&
        (node->as.call.nargs == 2 || node->as.call.nargs == 3) &&
        node->as.call.args[1]->type == AST_VARIABLE) {
      int order = 1;
      AstNode *expr = resolve_symbolic(ast_clone(node->as.call.args[0]), st);
      expr = substitute_vars(expr, st);
      if (node->as.call.nargs == 3 &&
          !parse_nonnegative_int_arg(node->as.call.args[2], st, &order)) {
        ast_free(expr);
      } else {
        AstNode *res =
            sym_diff_n(expr, node->as.call.args[1]->as.variable, order);
        ast_free(expr);
        if (res) {
          ast_free(node);
          return resolve_symbolic(res, st);
        }
      }
    }
    /* grad(expr, [x, y, ...]) */
    if (strcmp(node->as.call.name, "grad") == 0 && node->as.call.nargs == 2 &&
        node->as.call.args[1]->type == AST_MATRIX) {
      AstNode *expr = resolve_symbolic(ast_clone(node->as.call.args[0]), st);
      AstNode *res = NULL;
      expr = substitute_vars(expr, st);
      res = sym_grad(expr, node->as.call.args[1]);
      ast_free(expr);
      if (res) {
        ast_free(node);
        return resolve_symbolic(res, st);
      }
    }
    /* taylor(expr, var, a, order) */
    if (strcmp(node->as.call.name, "taylor") == 0 && node->as.call.nargs == 4 &&
        node->as.call.args[1]->type == AST_VARIABLE) {
      int order = 0;
      AstNode *expr = resolve_symbolic(ast_clone(node->as.call.args[0]), st);
      AstNode *point = resolve_symbolic(ast_clone(node->as.call.args[2]), st);

      expr = substitute_vars(expr, st);
      point = substitute_vars(point, st);

      if (!parse_nonnegative_int_arg(node->as.call.args[3], st, &order)) {
        ast_free(expr);
        ast_free(point);
      } else {
        AstNode *res =
            sym_taylor(expr, node->as.call.args[1]->as.variable, point, order);
        ast_free(expr);
        ast_free(point);
        if (res) {
          ast_free(node);
          return resolve_symbolic(res, st);
        }
      }
    }
    /* int(expr, var) or int(expr, var, a, b) */
    if ((strcmp(node->as.call.name, "int") == 0 ||
         strcmp(node->as.call.name, "integrate") == 0) &&
        (node->as.call.nargs == 2 || node->as.call.nargs == 4) &&
        node->as.call.args[1]->type == AST_VARIABLE) {
      AstNode *expr = resolve_symbolic(ast_clone(node->as.call.args[0]), st);
      expr = substitute_vars(expr, st);
      expr = ast_canonicalize(expr);
      expr = sym_simplify(expr);
      AstNode *result = sym_integrate(expr, node->as.call.args[1]->as.variable);
      ast_free(expr);
      if (result) {
        result = sym_simplify(result);
        if (node->as.call.nargs == 4) {
          char *var_name = node->as.call.args[1]->as.variable;
          AstNode *lower =
              resolve_symbolic(ast_clone(node->as.call.args[2]), st);
          lower = substitute_vars(lower, st);
          AstNode *upper =
              resolve_symbolic(ast_clone(node->as.call.args[3]), st);
          upper = substitute_vars(upper, st);

          AstNode *res_upper = substitute_params(result, &var_name, &upper, 1);
          AstNode *res_lower = substitute_params(result, &var_name, &lower, 1);

          AstNode *def_res = ast_binop(OP_SUB, res_upper, res_lower);
          def_res = sym_full_simplify(def_res);

          ast_free(result);
          ast_free(upper);
          ast_free(lower);
          ast_free(node);
          return resolve_symbolic(def_res, st);
        } else {
          ast_free(node);
          return resolve_symbolic(result, st);
        }
      }
    }

    /* det(matrix) */
    if (strcmp(node->as.call.name, "det") == 0 && node->as.call.nargs == 1) {
      AstNode *arg = resolve_symbolic(ast_clone(node->as.call.args[0]), st);
      arg = substitute_vars(arg, st);
      AstNode *result = sym_det(arg);
      ast_free(arg);
      if (result) {
        ast_free(node);
        return resolve_symbolic(result, st);
      }
    }
    /* expand(expr) */
    if (strcmp(node->as.call.name, "expand") == 0 && node->as.call.nargs == 1) {
      AstNode *expr = resolve_symbolic(ast_clone(node->as.call.args[0]), st);
      expr = substitute_vars(expr, st);
      AstNode *expanded = sym_expand(expr);
      expanded = ast_canonicalize(expanded);
      expanded = sym_collect_terms(expanded);
      expanded = sym_simplify(expanded);
      ast_free(node);
      return resolve_symbolic(expanded, st);
    }

    /* other functions: resolve arguments */
    for (size_t i = 0; i < node->as.call.nargs; i++) {
      node->as.call.args[i] = resolve_symbolic(node->as.call.args[i], st);
    }
  } else if (node->type == AST_BINOP) {
    node->as.binop.left = resolve_symbolic(node->as.binop.left, st);
    node->as.binop.right = resolve_symbolic(node->as.binop.right, st);
  } else if (node->type == AST_UNARY_NEG) {
    node->as.unary.operand = resolve_symbolic(node->as.unary.operand, st);
  } else if (node->type == AST_MATRIX) {
    size_t total = node->as.matrix.rows * node->as.matrix.cols;
    for (size_t i = 0; i < total; i++) {
      node->as.matrix.elements[i] =
          resolve_symbolic(node->as.matrix.elements[i], st);
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
    Matrix *r = matrix_scale(m, c_real(-1.0));
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
    late_binding_mode = 1;
    AstNode *body = resolve_symbolic(ast_clone(pr.root), &global_symtab);
    late_binding_mode = 0;
    symtab_set_func(&global_symtab, name, params, num_params, body);
    char *s = ast_to_string(body);
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
      AstNode *resolved = resolve_symbolic(ast_clone(pr.root), &global_symtab);
      symtab_set_expr(&global_symtab, name, resolved);
      char *s = ast_to_string(resolved);
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

  pr.root = resolve_symbolic(pr.root, &global_symtab);

  /* symbolic function calls: diff, grad, taylor, int, simplify */
  if (pr.root->type == AST_FUNC_CALL) {
    if (strcmp(pr.root->as.call.name, "diff") == 0 &&
        (pr.root->as.call.nargs == 2 || pr.root->as.call.nargs == 3) &&
        pr.root->as.call.args[1]->type == AST_VARIABLE) {
      int order = 1;
      AstNode *result = NULL;

      /* substitute known variables before differentiation */
      AstNode *expr =
          substitute_vars(ast_clone(pr.root->as.call.args[0]), &global_symtab);
      if (pr.root->as.call.nargs == 3 &&
          !parse_nonnegative_int_arg(pr.root->as.call.args[2], &global_symtab,
                                     &order)) {
        ast_free(expr);
        print_error("diff order must be a non-negative integer constant");
        parse_result_free(&pr);
        return 1;
      }
      result = sym_diff_n(expr, pr.root->as.call.args[1]->as.variable, order);
      ast_free(expr);
      if (result) {
        char *s = ast_to_string(result);
        output_result(result, s, batch_mode);
        free(s);
        ast_free(result);
      } else {
        print_error("cannot differentiate expression");
      }
      parse_result_free(&pr);
      return result ? 0 : 1;
    }

    if (strcmp(pr.root->as.call.name, "grad") == 0 &&
        pr.root->as.call.nargs == 2) {
      AstNode *expr = NULL;
      AstNode *result = NULL;

      if (pr.root->as.call.args[1]->type != AST_MATRIX) {
        print_error(
            "grad expects a matrix of variables as the second argument");
        parse_result_free(&pr);
        return 1;
      }

      expr =
          substitute_vars(ast_clone(pr.root->as.call.args[0]), &global_symtab);
      result = sym_grad(expr, pr.root->as.call.args[1]);
      ast_free(expr);

      if (result) {
        char *s = ast_to_string(result);
        output_result(result, s, batch_mode);
        free(s);
        ast_free(result);
      } else {
        print_error(
            "cannot compute gradient (matrix must contain only variables)");
      }
      parse_result_free(&pr);
      return result ? 0 : 1;
    }

    if (strcmp(pr.root->as.call.name, "taylor") == 0 &&
        pr.root->as.call.nargs == 4 &&
        pr.root->as.call.args[1]->type == AST_VARIABLE) {
      int order = 0;
      AstNode *expr = NULL;
      AstNode *point = NULL;
      AstNode *result = NULL;

      if (!parse_nonnegative_int_arg(pr.root->as.call.args[3], &global_symtab,
                                     &order)) {
        print_error("taylor order must be a non-negative integer constant");
        parse_result_free(&pr);
        return 1;
      }

      expr =
          substitute_vars(ast_clone(pr.root->as.call.args[0]), &global_symtab);
      point =
          substitute_vars(ast_clone(pr.root->as.call.args[2]), &global_symtab);
      result =
          sym_taylor(expr, pr.root->as.call.args[1]->as.variable, point, order);
      ast_free(expr);
      ast_free(point);

      if (result) {
        char *s = ast_to_string(result);
        output_result(result, s, batch_mode);
        free(s);
        ast_free(result);
      } else {
        print_error("cannot compute Taylor series");
      }
      parse_result_free(&pr);
      return result ? 0 : 1;
    }

    if ((strcmp(pr.root->as.call.name, "int") == 0 ||
         strcmp(pr.root->as.call.name, "integrate") == 0) &&
        (pr.root->as.call.nargs == 2 || pr.root->as.call.nargs == 4) &&
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
        if (pr.root->as.call.nargs == 4) {
          char *var_name = pr.root->as.call.args[1]->as.variable;
          AstNode *lower = substitute_vars(ast_clone(pr.root->as.call.args[2]),
                                           &global_symtab);
          AstNode *upper = substitute_vars(ast_clone(pr.root->as.call.args[3]),
                                           &global_symtab);

          AstNode *res_upper = substitute_params(result, &var_name, &upper, 1);
          AstNode *res_lower = substitute_params(result, &var_name, &lower, 1);

          AstNode *def_res = ast_binop(OP_SUB, res_upper, res_lower);
          def_res = sym_full_simplify(def_res);

          char *s = ast_to_string(def_res);
          output_result(def_res, s, batch_mode);
          free(s);

          ast_free(def_res);
          ast_free(upper);
          ast_free(lower);
        } else {
          char *s = ast_to_string(result);
          if (latex_mode) {
            char *tex = ast_to_latex(result);
            size_t tlen = strlen(tex);
            char *out = malloc(tlen + 5);
            memcpy(out, tex, tlen);
            memcpy(out + tlen, " + C", 5);
            if (full_doc_mode) {
              latex_print_document(out);
            } else if (batch_mode) {
              printf("%s", out);
            } else {
              print_bold(out);
              putchar('\n');
            }
            free(tex);
            free(out);
          } else {
            size_t len = strlen(s);
            char *out = malloc(len + 5);
            memcpy(out, s, len);
            memcpy(out + len, " + C", 5);
            output_str(out, batch_mode);
            free(out);
          }
          free(s);
        }
        ast_free(result);
      } else {
        print_error("cannot integrate expression");
      }
      parse_result_free(&pr);
      return result ? 0 : 1;
    }

    if (strcmp(pr.root->as.call.name, "det") == 0 &&
        pr.root->as.call.nargs == 1) {
      AstNode *arg =
          substitute_vars(ast_clone(pr.root->as.call.args[0]), &global_symtab);
      arg = resolve_symbolic(arg, &global_symtab);
      arg = substitute_vars(arg, &global_symtab);
      AstNode *result = sym_det(arg);
      ast_free(arg);
      if (result) {
        result = sym_simplify(result);
        char *s = ast_to_string(result);
        output_result(result, s, batch_mode);
        free(s);
        ast_free(result);
      } else {
        print_error("cannot compute determinant (not a square matrix)");
      }
      parse_result_free(&pr);
      return result ? 0 : 1;
    }

    if (strcmp(pr.root->as.call.name, "solve") == 0 &&
        (pr.root->as.call.nargs == 2 || pr.root->as.call.nargs == 3) &&
        pr.root->as.call.args[1]->type == AST_VARIABLE) {

      AstNode *expr =
          substitute_vars(ast_clone(pr.root->as.call.args[0]), &global_symtab);
      const char *var = pr.root->as.call.args[1]->as.variable;
      Complex x0 = c_real(0.0);
      if (pr.root->as.call.nargs == 3) {
        EvalResult er = eval(pr.root->as.call.args[2], &global_symtab);
        if (er.ok)
          x0 = er.value;
        eval_result_free(&er);
      }

      SolveResult sr = sym_solve(expr, var, x0, &global_symtab);
      ast_free(expr);

      if (sr.ok) {
        for (size_t i = 0; i < sr.count; i++) {
          char buf[64];
          format_number(buf, sizeof buf, sr.roots[i]);
          char out[128];
          snprintf(out, sizeof out, "%s = %s", var, buf);
          if (batch_mode)
            printf("%s%s", out, i + 1 < sr.count ? "\n" : "");
          else {
            print_bold(out);
            putchar('\n');
          }
        }
      } else {
        print_error(sr.error);
      }
      solve_result_free(&sr);
      parse_result_free(&pr);
      return sr.ok ? 0 : 1;
    }

    if (strcmp(pr.root->as.call.name, "simplify") == 0 &&
        pr.root->as.call.nargs == 1) {

      /* substitute known variables before simplification */
      AstNode *expr =
          substitute_vars(ast_clone(pr.root->as.call.args[0]), &global_symtab);
      AstNode *simplified = sym_full_simplify(expr);
      char *s = ast_to_string(simplified);
      output_result(simplified, s, batch_mode);
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
      output_result(expanded, s, batch_mode);
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
      if (latex_mode) {
        char *tex = ast_to_latex(resolved);
        output_result(NULL, tex, batch_mode);
        free(tex);
      } else {
        output_str(s, batch_mode);
      }
      free(s);
      matrix_free(m);
    } else {
      /* symbolic matrix: display as-is */
      char *s = ast_to_string(resolved);
      output_result(resolved, s, batch_mode);
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
    output_result(resolved, buf, batch_mode);
  } else {
    /* fallback to symbolic simplification */
    AstNode *simplified = sym_simplify(resolved);
    char *s = ast_to_string(simplified);
    output_result(simplified, s, batch_mode);
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
  fprintf(stderr, "sia v0.6.7 - type an expression, or 'quit' to exit\n");

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

  /* parse flags */
  int first_expr_arg = 1;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--latex") == 0) {
      latex_mode = 1;
      first_expr_arg = i + 1;
    } else if (strcmp(argv[i], "--full") == 0) {
      full_doc_mode = 1;
      first_expr_arg = i + 1;
    } else {
      break;
    }
  }

  if (first_expr_arg < argc) {
    size_t total = 0;
    for (int i = first_expr_arg; i < argc; i++)
      total += strlen(argv[i]) + 1;

    char *expr = malloc(total + 1);
    expr[0] = '\0';
    for (int i = first_expr_arg; i < argc; i++) {
      if (i > first_expr_arg)
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
