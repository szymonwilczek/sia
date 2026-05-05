#include "fractions.h"
#include <math.h>

Fraction fraction_from_double(double x) {
  Fraction f;
  if (isnan(x) || isinf(x)) {
    f.num = (long long)x;
    f.den = 1;
    return f;
  }

  int sign = 1;
  if (x < 0) {
    sign = -1;
    x = -x;
  }

  /* continued fraction algorithm to find the best rational approximation */
  const double tolerance = 1.0E-9;
  double z = x;
  long long d_prev = 0;
  long long d_curr = 1;
  long long n_prev = 1;
  long long n_curr = (long long)x;

  for (int i = 0; i < 64; i++) {
    double frac = z - floor(z);
    if (frac < tolerance) {
      break;
    }

    z = 1.0 / frac;
    long long a = (long long)floor(z);

    long long n_next = a * n_curr + n_prev;
    long long d_next = a * d_curr + d_prev;

    /* prevent overflow or ridiculously large denominators */
    if (d_next > 1000000 || d_next < 0 || n_next < 0) {
      break;
    }

    n_prev = n_curr;
    d_prev = d_curr;
    n_curr = n_next;
    d_curr = d_next;

    if (fabs((double)n_curr / d_curr - x) < tolerance) {
      break;
    }
  }

  /* only return as fraction if its a nice fraction and highly accurate */
  if (d_curr > 1 && d_curr <= 100000 &&
      fabs((double)n_curr / d_curr - x) < tolerance * 10) {
    f.num = sign * n_curr;
    f.den = d_curr;
  } else {
    /* fallback to simple value */
    f.num = (long long)(sign * x);
    f.den = 1;
  }

  return f;
}
