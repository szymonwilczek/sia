#include "matrix.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *xmalloc(size_t n) {
  void *p = malloc(n);
  if (!p) {
    fprintf(stderr, "sia: matrix: out of memory\n");
    abort();
  }
  return p;
}

Matrix *matrix_create(size_t rows, size_t cols) {
  Matrix *m = xmalloc(sizeof *m);
  m->rows = rows;
  m->cols = cols;
  m->data = xmalloc(rows * cols * sizeof(double));
  memset(m->data, 0, rows * cols * sizeof(double));
  return m;
}

Matrix *matrix_clone(const Matrix *m) {
  if (!m)
    return NULL;
  Matrix *c = matrix_create(m->rows, m->cols);
  memcpy(c->data, m->data, m->rows * m->cols * sizeof(double));
  return c;
}

void matrix_free(Matrix *m) {
  if (!m)
    return;
  free(m->data);
  free(m);
}

double matrix_get(const Matrix *m, size_t r, size_t c) {
  return m->data[r * m->cols + c];
}

void matrix_set(Matrix *m, size_t r, size_t c, double v) {
  m->data[r * m->cols + c] = v;
}

Matrix *matrix_add(const Matrix *a, const Matrix *b) {
  if (a->rows != b->rows || a->cols != b->cols)
    return NULL;
  Matrix *r = matrix_create(a->rows, a->cols);
  size_t n = a->rows * a->cols;
  for (size_t i = 0; i < n; i++)
    r->data[i] = a->data[i] + b->data[i];
  return r;
}

Matrix *matrix_sub(const Matrix *a, const Matrix *b) {
  if (a->rows != b->rows || a->cols != b->cols)
    return NULL;
  Matrix *r = matrix_create(a->rows, a->cols);
  size_t n = a->rows * a->cols;
  for (size_t i = 0; i < n; i++)
    r->data[i] = a->data[i] - b->data[i];
  return r;
}

Matrix *matrix_mul(const Matrix *a, const Matrix *b) {
  if (a->cols != b->rows)
    return NULL;
  Matrix *r = matrix_create(a->rows, b->cols);
  for (size_t i = 0; i < a->rows; i++)
    for (size_t j = 0; j < b->cols; j++) {
      double sum = 0.0;
      for (size_t k = 0; k < a->cols; k++)
        sum += matrix_get(a, i, k) * matrix_get(b, k, j);
      matrix_set(r, i, j, sum);
    }
  return r;
}

Matrix *matrix_scale(const Matrix *m, double s) {
  Matrix *r = matrix_create(m->rows, m->cols);
  size_t n = m->rows * m->cols;
  for (size_t i = 0; i < n; i++)
    r->data[i] = m->data[i] * s;
  return r;
}

Matrix *matrix_transpose(const Matrix *m) {
  Matrix *r = matrix_create(m->cols, m->rows);
  for (size_t i = 0; i < m->rows; i++)
    for (size_t j = 0; j < m->cols; j++)
      matrix_set(r, j, i, matrix_get(m, i, j));
  return r;
}

/* cofactor expansion for determinant */
static double det_recursive(const double *data, size_t n, size_t stride) {
  if (n == 1)
    return data[0];
  if (n == 2)
    return data[0] * data[stride + 1] - data[1] * data[stride];

  double *sub = xmalloc((n - 1) * (n - 1) * sizeof(double));
  double det = 0.0;

  for (size_t col = 0; col < n; col++) {
    size_t si = 0;
    for (size_t r = 1; r < n; r++)
      for (size_t c = 0; c < n; c++) {
        if (c == col)
          continue;
        sub[si++] = data[r * stride + c];
      }
    double cofactor = det_recursive(sub, n - 1, n - 1);
    det += (col % 2 == 0 ? 1.0 : -1.0) * data[col] * cofactor;
  }

  free(sub);
  return det;
}

double matrix_det(const Matrix *m) {
  if (m->rows != m->cols)
    return 0.0;
  return det_recursive(m->data, m->rows, m->cols);
}

Matrix *matrix_inverse(const Matrix *m) {
  if (m->rows != m->cols)
    return NULL;
  size_t n = m->rows;
  double d = matrix_det(m);
  if (fabs(d) < 1e-15)
    return NULL;

  Matrix *adj = matrix_create(n, n);

  for (size_t i = 0; i < n; i++)
    for (size_t j = 0; j < n; j++) {
      /* minor (i,j) */
      double *sub = xmalloc((n - 1) * (n - 1) * sizeof(double));
      size_t si = 0;
      for (size_t r = 0; r < n; r++) {
        if (r == i)
          continue;
        for (size_t c = 0; c < n; c++) {
          if (c == j)
            continue;
          sub[si++] = matrix_get(m, r, c);
        }
      }
      double minor = det_recursive(sub, n - 1, n - 1);
      free(sub);
      double cofactor = ((i + j) % 2 == 0 ? 1.0 : -1.0) * minor;
      matrix_set(adj, j, i, cofactor / d);
    }

  return adj;
}

Matrix *matrix_identity(size_t n) {
  Matrix *m = matrix_create(n, n);
  for (size_t i = 0; i < n; i++)
    matrix_set(m, i, i, 1.0);
  return m;
}

double matrix_trace(const Matrix *m) {
  if (m->rows != m->cols)
    return 0.0;
  double t = 0.0;
  for (size_t i = 0; i < m->rows; i++)
    t += matrix_get(m, i, i);
  return t;
}

char *matrix_to_string(const Matrix *m) {
  size_t cap = 256;
  char *buf = xmalloc(cap);
  size_t len = 0;

  buf[0] = '[';
  len = 1;

  for (size_t i = 0; i < m->rows; i++) {
    if (i > 0) {
      while (len + 3 > cap) {
        cap *= 2;
        buf = realloc(buf, cap);
      }
      buf[len++] = ';';
      buf[len++] = ' ';
    }
    for (size_t j = 0; j < m->cols; j++) {
      char tmp[64];
      double v = matrix_get(m, i, j);
      int n;
      if (v == (long long)v && fabs(v) < 1e15)
        n = snprintf(tmp, sizeof tmp, "%lld", (long long)v);
      else
        n = snprintf(tmp, sizeof tmp, "%g", v);

      if (j > 0) {
        while (len + 3 > cap) {
          cap *= 2;
          buf = realloc(buf, cap);
        }
        buf[len++] = ',';
        buf[len++] = ' ';
      }
      while (len + (size_t)n + 2 > cap) {
        cap *= 2;
        buf = realloc(buf, cap);
      }
      memcpy(buf + len, tmp, (size_t)n);
      len += (size_t)n;
    }
  }

  while (len + 2 > cap) {
    cap *= 2;
    buf = realloc(buf, cap);
  }
  buf[len++] = ']';
  buf[len] = '\0';
  return buf;
}
