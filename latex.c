#include "latex.h"
#include "factorial.h"
#include "fractions.h"
#include "logarithm.h"
#include "number_theory.h"
#include "trigonometry/trigonometry.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} StrBuf;

static void sb_init(StrBuf *sb) {
  sb->cap = 128;
  sb->len = 0;
  sb->buf = malloc(sb->cap);
  sb->buf[0] = '\0';
}

static void sb_append(StrBuf *sb, const char *s, size_t n) {
  while (sb->len + n + 1 > sb->cap) {
    sb->cap *= 2;
    sb->buf = realloc(sb->buf, sb->cap);
  }
  memcpy(sb->buf + sb->len, s, n);
  sb->len += n;
  sb->buf[sb->len] = '\0';
}

static void sb_puts(StrBuf *sb, const char *s) { sb_append(sb, s, strlen(s)); }

static void sb_printf(StrBuf *sb, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

static void sb_printf(StrBuf *sb, const char *fmt, ...) {
  char tmp[128];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
  va_end(ap);
  if (n > 0)
    sb_append(sb, tmp, (size_t)n);
}

static int op_prec(BinOpKind op) {
  switch (op) {
  case OP_ADD:
  case OP_SUB:
    return 1;
  case OP_MUL:
  case OP_DIV:
    return 2;
  case OP_POW:
    return 3;
  }
  return 0;
}

static int is_latex_func(const char *name) {
  static const char *funcs[] = {"sinh", "cosh", "tanh", "exp",
                                "log",  "ln",   "det",  NULL};
  for (int i = 0; funcs[i]; i++)
    if (strcmp(name, funcs[i]) == 0)
      return 1;
  return 0;
}
static int is_negative_number(const AstNode *n) {
  return n->type == AST_NUMBER && c_is_real(n->as.number) &&
         n->as.number.re < 0;
}

static void latex_node(const AstNode *node, StrBuf *sb, const AstNode *parent,
                       int is_right);

static void latex_child(const AstNode *child, StrBuf *sb, const AstNode *parent,
                        int is_right, int force_parens) {
  int parens = force_parens;
  if (!parens && child->type == AST_BINOP && parent &&
      parent->type == AST_BINOP) {
    int cp = op_prec(child->as.binop.op);
    int pp = op_prec(parent->as.binop.op);
    if (cp < pp)
      parens = 1;
    if (cp == pp && is_right &&
        (parent->as.binop.op == OP_SUB || parent->as.binop.op == OP_DIV))
      parens = 1;
  }
  if (parens)
    sb_puts(sb, "\\left(");
  latex_node(child, sb, parent, is_right);
  if (parens)
    sb_puts(sb, "\\right)");
}

static int needs_mul_dot(const AstNode *left, const AstNode *right) {
  /* number * number: 3 \cdot 4 */
  if (left->type == AST_NUMBER && right->type == AST_NUMBER) {
    /* avoid \cdot for 2i */
    if (fabs(right->as.number.re) < 1e-15 &&
        fabs(right->as.number.im - 1.0) < 1e-15)
      return 0;
    return 1;
  }
  /* number * identifier: 3x (no dot) */
  if (left->type == AST_NUMBER && right->type == AST_VARIABLE)
    return 0;
  /* number * (expr with leading number): avoid 23 ambiguity */
  if (left->type == AST_NUMBER && right->type == AST_BINOP &&
      right->as.binop.op == OP_MUL && right->as.binop.left->type == AST_NUMBER)
    return 1;
  return 0;
}

static void latex_node(const AstNode *node, StrBuf *sb, const AstNode *parent,
                       int is_right) {
  if (!node)
    return;

  switch (node->type) {
  case AST_NUMBER: {
    Complex z = node->as.number;
    if (z.im == 0.0) {
      double v = z.re;
      Fraction f = fraction_from_double(v);
      if (f.den != 1) {
        if (f.num < 0) {
          sb_printf(sb, "-\\frac{%lld}{%lld}", -f.num, f.den);
        } else {
          sb_printf(sb, "\\frac{%lld}{%lld}", f.num, f.den);
        }
      } else {
        if (v == (long long)v && fabs(v) < 1e15)
          sb_printf(sb, "%lld", (long long)v);
        else
          sb_printf(sb, "%g", v);
      }
    } else if (z.re == 0.0) {
      if (z.im == 1.0)
        sb_puts(sb, "i");
      else if (z.im == -1.0)
        sb_puts(sb, "-i");
      else {
        if (z.im == (long long)z.im && fabs(z.im) < 1e15)
          sb_printf(sb, "%lldi", (long long)z.im);
        else
          sb_printf(sb, "%gi", z.im);
      }
    } else {
      /* a + bi form */
      if (z.re == (long long)z.re && fabs(z.re) < 1e15)
        sb_printf(sb, "%lld", (long long)z.re);
      else
        sb_printf(sb, "%g", z.re);

      if (z.im > 0) {
        sb_puts(sb, " + ");
        if (z.im == 1.0)
          sb_puts(sb, "i");
        else {
          if (z.im == (long long)z.im && fabs(z.im) < 1e15)
            sb_printf(sb, "%lldi", (long long)z.im);
          else
            sb_printf(sb, "%gi", z.im);
        }
      } else {
        sb_puts(sb, " - ");
        double aim = -z.im;
        if (aim == 1.0)
          sb_puts(sb, "i");
        else {
          if (aim == (long long)aim && fabs(aim) < 1e15)
            sb_printf(sb, "%lldi", (long long)aim);
          else
            sb_printf(sb, "%gi", aim);
        }
      }
    }
    break;
  }

  case AST_VARIABLE: {
    const char *name = node->as.variable;
    if (strcmp(name, "pi") == 0)
      sb_puts(sb, "\\pi");
    else if (strcmp(name, "e") == 0)
      sb_puts(sb, "e");
    else if (strlen(name) > 1)
      sb_printf(sb, "\\mathrm{%s}", name);
    else
      sb_puts(sb, name);
    break;
  }

  case AST_UNARY_NEG:
    sb_puts(sb, "-");
    if (node->as.unary.operand->type == AST_BINOP) {
      sb_puts(sb, "\\left(");
      latex_node(node->as.unary.operand, sb, node, 0);
      sb_puts(sb, "\\right)");
    } else {
      latex_node(node->as.unary.operand, sb, node, 0);
    }
    break;

  case AST_BINOP: {
    BinOpKind op = node->as.binop.op;
    const AstNode *L = node->as.binop.left;
    const AstNode *R = node->as.binop.right;

    if (op == OP_DIV) {
      sb_puts(sb, "\\frac{");
      latex_node(L, sb, NULL, 0);
      sb_puts(sb, "}{");
      latex_node(R, sb, NULL, 0);
      sb_puts(sb, "}");
      break;
    }

    if (op == OP_POW) {
      if (R->type == AST_NUMBER && c_is_real(R->as.number) &&
          R->as.number.re == -1.0) {
        sb_puts(sb, "\\frac{1}{");
        latex_node(L, sb, NULL, 0);
        sb_puts(sb, "}");
        break;
      }

      int base_parens = L->type == AST_BINOP || L->type == AST_UNARY_NEG;
      if (base_parens)
        sb_puts(sb, "\\left(");
      latex_node(L, sb, node, 0);
      if (base_parens)
        sb_puts(sb, "\\right)");
      sb_puts(sb, "^{");
      latex_node(R, sb, NULL, 0);
      sb_puts(sb, "}");
      break;
    }

    if (op == OP_MUL) {
      if (L->type == AST_NUMBER && c_is_real(L->as.number)) {
        Fraction f = fraction_from_double(L->as.number.re);
        if (f.den != 1) {
          if (f.num == 1) {
            sb_puts(sb, "\\frac{");
            latex_child(R, sb, node, 1, 0);
            sb_printf(sb, "}{%lld}", f.den);
            break;
          } else if (f.num == -1) {
            sb_puts(sb, "-\\frac{");
            latex_child(R, sb, node, 1, 0);
            sb_printf(sb, "}{%lld}", f.den);
            break;
          } else if (f.num < 0) {
            sb_printf(sb, "-\\frac{%lld ", -f.num);
            latex_child(R, sb, node, 1, 0);
            sb_printf(sb, "}{%lld}", f.den);
            break;
          } else {
            sb_printf(sb, "\\frac{%lld ", f.num);
            latex_child(R, sb, node, 1, 0);
            sb_printf(sb, "}{%lld}", f.den);
            break;
          }
        }
      }
      latex_child(L, sb, node, 0, 0);
      if (needs_mul_dot(L, R))
        sb_puts(sb, " \\cdot ");
      else
        sb_puts(sb, " ");
      latex_child(R, sb, node, 1, 0);
      break;
    }

    /* OP_ADD, OP_SUB */
    latex_child(L, sb, node, 0, 0);
    if (op == OP_ADD) {
      if (R->type == AST_UNARY_NEG) {
        sb_puts(sb, " - ");
        latex_node(R->as.unary.operand, sb, node, 1);
      } else if (is_negative_number(R)) {
        sb_printf(sb, " - %g", -R->as.number.re);
      } else {
        sb_puts(sb, " + ");
        latex_child(R, sb, node, 1, 0);
      }
    } else {
      sb_puts(sb, " - ");
      latex_child(R, sb, node, 1, 0);
    }
    break;
  }

  case AST_FUNC_CALL: {
    if (factorial_is_call(node)) {
      const AstNode *arg = node->as.call.args[0];
      if (arg->type == AST_BINOP || arg->type == AST_UNARY_NEG) {
        sb_puts(sb, "\\left(");
        latex_node(arg, sb, NULL, 0);
        sb_puts(sb, "\\right)");
      } else {
        latex_node(arg, sb, NULL, 0);
      }
      sb_puts(sb, "!");
      break;
    }

    TrigKind trig = trig_kind(node);
    if (trig != TRIG_KIND_NONE) {
      const char *latex_name = NULL;
      switch (trig) {
      case TRIG_KIND_SIN:
        latex_name = "sin";
        break;
      case TRIG_KIND_COS:
        latex_name = "cos";
        break;
      case TRIG_KIND_TAN:
        latex_name = "tan";
        break;
      case TRIG_KIND_ASIN:
        latex_name = "asin";
        break;
      case TRIG_KIND_ACOS:
        latex_name = "acos";
        break;
      case TRIG_KIND_ATAN:
        latex_name = "atan";
        break;
      case TRIG_KIND_SINH:
        latex_name = "sinh";
        break;
      case TRIG_KIND_COSH:
        latex_name = "cosh";
        break;
      case TRIG_KIND_TANH:
        latex_name = "tanh";
        break;
      default:
        break;
      }
      sb_printf(sb, "\\%s", latex_name ? latex_name : node->as.call.name);
      sb_puts(sb, "\\left(");
      latex_node(node->as.call.args[0], sb, NULL, 0);
      sb_puts(sb, "\\right)");
      break;
    }

    NumberTheoryKind nt_kind = number_theory_kind(node);
    if (nt_kind != NT_KIND_NONE) {
      if (nt_kind == NT_KIND_GCD) {
        sb_puts(sb, "\\gcd");
      } else {
        sb_puts(sb, "\\operatorname{lcm}");
      }
      sb_puts(sb, "\\left(");
      latex_node(node->as.call.args[0], sb, NULL, 0);
      sb_puts(sb, ", ");
      latex_node(node->as.call.args[1], sb, NULL, 0);
      sb_puts(sb, "\\right)");
      break;
    }

    LogKind kind = log_kind(node);

    if (kind == LOG_KIND_LN) {
      sb_puts(sb, "\\ln");
      sb_puts(sb, "\\left(");
      latex_node(node->as.call.args[0], sb, NULL, 0);
      sb_puts(sb, "\\right)");
      break;
    }

    if (kind == LOG_KIND_BASE10 || kind == LOG_KIND_BASE2 ||
        kind == LOG_KIND_GENERIC) {
      sb_puts(sb, "\\log_{");
      if (kind == LOG_KIND_BASE10) {
        sb_puts(sb, "10");
      } else if (kind == LOG_KIND_BASE2) {
        sb_puts(sb, "2");
      } else {
        latex_node(node->as.call.args[1], sb, NULL, 0);
      }
      sb_puts(sb, "}");
      sb_puts(sb, "\\left(");
      latex_node(node->as.call.args[0], sb, NULL, 0);
      sb_puts(sb, "\\right)");
      break;
    }

    const char *name = node->as.call.name;

    if (strcmp(name, "sqrt") == 0 && node->as.call.nargs == 1) {
      sb_puts(sb, "\\sqrt{");
      latex_node(node->as.call.args[0], sb, NULL, 0);
      sb_puts(sb, "}");
      break;
    }

    if (strcmp(name, "abs") == 0 && node->as.call.nargs == 1) {
      sb_puts(sb, "\\left|");
      latex_node(node->as.call.args[0], sb, NULL, 0);
      sb_puts(sb, "\\right|");
      break;
    }

    if (is_latex_func(name)) {
      sb_printf(sb, "\\%s", name);
    } else {
      sb_printf(sb, "\\operatorname{%s}", name);
    }
    sb_puts(sb, "\\left(");
    for (size_t i = 0; i < node->as.call.nargs; i++) {
      if (i > 0)
        sb_puts(sb, ", ");
      latex_node(node->as.call.args[i], sb, NULL, 0);
    }
    sb_puts(sb, "\\right)");
    break;
  }

  case AST_LIMIT:
    sb_puts(sb, "\\lim_{");
    sb_puts(sb, node->as.limit.var);
    sb_puts(sb, " \\to ");
    latex_node(node->as.limit.target, sb, NULL, 0);
    sb_puts(sb, "} ");
    latex_node(node->as.limit.expr, sb, NULL, 0);
    break;

  case AST_MATRIX: {
    sb_puts(sb, "\\begin{pmatrix}\n");
    for (size_t r = 0; r < node->as.matrix.rows; r++) {
      if (r > 0)
        sb_puts(sb, " \\\\\n");
      for (size_t c = 0; c < node->as.matrix.cols; c++) {
        if (c > 0)
          sb_puts(sb, " & ");
        size_t idx = r * node->as.matrix.cols + c;
        latex_node(node->as.matrix.elements[idx], sb, NULL, 0);
      }
    }
    sb_puts(sb, "\n\\end{pmatrix}");
    break;
  }
  }
}

char *ast_to_latex(const AstNode *node) {
  StrBuf sb;
  sb_init(&sb);
  latex_node(node, &sb, NULL, 0);
  return sb.buf;
}

void latex_print_document(const char *latex_body) {
  printf("\\documentclass{article}\n");
  printf("\\usepackage[utf8]{inputenc}\n");
  printf("\\usepackage{amsmath}\n");
  printf("\\begin{document}\n");
  printf("\\[\n%s\n\\]\n", latex_body);
  printf("\\end{document}\n");
}
