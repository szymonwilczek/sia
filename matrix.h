#ifndef SIA_MATRIX_H
#define SIA_MATRIX_H

#include "complex.h"
#include <stddef.h>

typedef struct {
  Complex *data;
  size_t rows;
  size_t cols;
} Matrix;

Matrix *matrix_create(size_t rows, size_t cols);
Matrix *matrix_clone(const Matrix *m);
void matrix_free(Matrix *m);

Complex matrix_get(const Matrix *m, size_t r, size_t c);
void matrix_set(Matrix *m, size_t r, size_t c, Complex v);

Matrix *matrix_add(const Matrix *a, const Matrix *b);
Matrix *matrix_sub(const Matrix *a, const Matrix *b);
Matrix *matrix_mul(const Matrix *a, const Matrix *b);
Matrix *matrix_scale(const Matrix *m, Complex s);
Matrix *matrix_transpose(const Matrix *m);

Complex matrix_det(const Matrix *m);
Matrix *matrix_inverse(const Matrix *m);

Matrix *matrix_identity(size_t n);
Complex matrix_trace(const Matrix *m);

char *matrix_to_string(const Matrix *m);

#endif
