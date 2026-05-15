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
  m->data = xmalloc(rows * cols * sizeof(Complex));
  memset(m->data, 0, rows * cols * sizeof(Complex));
  return m;
}

Matrix *matrix_clone(const Matrix *m) {
  if (!m)
    return NULL;
  Matrix *c = matrix_create(m->rows, m->cols);
  memcpy(c->data, m->data, m->rows * m->cols * sizeof(Complex));
  return c;
}

void matrix_free(Matrix *m) {
  if (!m)
    return;
  free(m->data);
  free(m);
}

Complex matrix_get(const Matrix *m, size_t r, size_t c) {
  return m->data[r * m->cols + c];
}

void matrix_set(Matrix *m, size_t r, size_t c, Complex v) {
  m->data[r * m->cols + c] = v;
}

Matrix *matrix_add(const Matrix *a, const Matrix *b) {
  if (a->rows != b->rows || a->cols != b->cols)
    return NULL;
  Matrix *r = matrix_create(a->rows, a->cols);
  size_t n = a->rows * a->cols;
  for (size_t i = 0; i < n; i++)
    r->data[i] = c_add(a->data[i], b->data[i]);
  return r;
}

Matrix *matrix_sub(const Matrix *a, const Matrix *b) {
  if (a->rows != b->rows || a->cols != b->cols)
    return NULL;
  Matrix *r = matrix_create(a->rows, a->cols);
  size_t n = a->rows * a->cols;
  for (size_t i = 0; i < n; i++)
    r->data[i] = c_sub(a->data[i], b->data[i]);
  return r;
}

Matrix *matrix_mul(const Matrix *a, const Matrix *b) {
  if (a->cols != b->rows)
    return NULL;
  Matrix *r = matrix_create(a->rows, b->cols);
  for (size_t i = 0; i < a->rows; i++)
    for (size_t j = 0; j < b->cols; j++) {
      Complex sum = c_real(0.0);
      for (size_t k = 0; k < a->cols; k++)
        sum = c_add(sum, c_mul(matrix_get(a, i, k), matrix_get(b, k, j)));
      matrix_set(r, i, j, sum);
    }
  return r;
}

Matrix *matrix_scale(const Matrix *m, Complex s) {
  Matrix *r = matrix_create(m->rows, m->cols);
  size_t n = m->rows * m->cols;
  for (size_t i = 0; i < n; i++)
    r->data[i] = c_mul(m->data[i], s);
  return r;
}

Matrix *matrix_transpose(const Matrix *m) {
  Matrix *r = matrix_create(m->cols, m->rows);
  for (size_t i = 0; i < m->rows; i++)
    for (size_t j = 0; j < m->cols; j++)
      matrix_set(r, j, i, matrix_get(m, i, j));
  return r;
}

static Complex det_recursive(const Complex *data, size_t n, size_t stride) {
  if (n == 1)
    return data[0];
  if (n == 2) {
    /* ad - bc */
    Complex ad = c_mul(data[0], data[stride + 1]);
    Complex bc = c_mul(data[1], data[stride]);
    return c_sub(ad, bc);
  }

  Complex *sub = xmalloc((n - 1) * (n - 1) * sizeof(Complex));
  Complex det = c_real(0.0);

  for (size_t col = 0; col < n; col++) {
    size_t si = 0;
    for (size_t r = 1; r < n; r++)
      for (size_t c = 0; c < n; c++) {
        if (c == col)
          continue;
        sub[si++] = data[r * stride + c];
      }
    Complex cofactor = det_recursive(sub, n - 1, n - 1);
    Complex sign = (col % 2 == 0) ? c_real(1.0) : c_real(-1.0);
    det = c_add(det, c_mul(sign, c_mul(data[col], cofactor)));
  }

  free(sub);
  return det;
}

Complex matrix_det(const Matrix *m) {
  if (m->rows != m->cols)
    return c_real(0.0);
  return det_recursive(m->data, m->rows, m->cols);
}

Matrix *matrix_inverse(const Matrix *m) {
  if (m->rows != m->cols)
    return NULL;
  size_t n = m->rows;
  Complex d = matrix_det(m);
  if (c_abs(d) < 1e-15)
    return NULL;

  if (n == 1) {
    Matrix *inv = matrix_create(1, 1);
    matrix_set(inv, 0, 0, c_div(c_real(1.0), matrix_get(m, 0, 0)));
    return inv;
  }

  Matrix *adj = matrix_create(n, n);

  for (size_t i = 0; i < n; i++)
    for (size_t j = 0; j < n; j++) {
      Complex *sub = xmalloc((n - 1) * (n - 1) * sizeof(Complex));
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
      Complex minor = det_recursive(sub, n - 1, n - 1);
      free(sub);
      Complex sign = ((i + j) % 2 == 0) ? c_real(1.0) : c_real(-1.0);
      Complex cofactor = c_mul(sign, minor);
      matrix_set(adj, j, i, c_div(cofactor, d));
    }

  return adj;
}

Matrix *matrix_identity(size_t n) {
  Matrix *m = matrix_create(n, n);
  for (size_t i = 0; i < n; i++)
    matrix_set(m, i, i, c_real(1.0));
  return m;
}

Complex matrix_trace(const Matrix *m) {
  if (m->rows != m->cols)
    return c_real(0.0);
  Complex t = c_real(0.0);
  for (size_t i = 0; i < m->rows; i++)
    t = c_add(t, matrix_get(m, i, i));
  return t;
}

static void format_complex_plain(char *buf, size_t size, Complex z) {
  if (z.im == 0.0) {
    if (z.re == (long long)z.re && fabs(z.re) < 1e15)
      snprintf(buf, size, "%lld", (long long)z.re);
    else
      snprintf(buf, size, "%g", z.re);
  } else if (z.re == 0.0) {
    if (z.im == 1.0)
      snprintf(buf, size, "i");
    else if (z.im == -1.0)
      snprintf(buf, size, "-i");
    else if (z.im == (long long)z.im && fabs(z.im) < 1e15)
      snprintf(buf, size, "%lldi", (long long)z.im);
    else
      snprintf(buf, size, "%gi", z.im);
  } else {
    double aim = fabs(z.im);
    char im_part[32];
    if (aim == 1.0)
      strcpy(im_part, "i");
    else if (aim == (long long)aim && aim < 1e15)
      snprintf(im_part, sizeof im_part, "%lldi", (long long)aim);
    else
      snprintf(im_part, sizeof im_part, "%gi", aim);

    if (z.re == (long long)z.re && fabs(z.re) < 1e15)
      snprintf(buf, size, "%lld%c%s", (long long)z.re, z.im > 0 ? '+' : '-',
               im_part);
    else
      snprintf(buf, size, "%g%c%s", z.re, z.im > 0 ? '+' : '-', im_part);
  }
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
      char tmp[128];
      Complex v = matrix_get(m, i, j);
      format_complex_plain(tmp, sizeof tmp, v);
      int n = strlen(tmp);

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
