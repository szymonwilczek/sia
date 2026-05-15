#include "fractions.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>

static long long ll_gcd(long long a, long long b) {
  while (b != 0) {
    long long t = a % b;
    a = b;
    b = t;
  }
  return a < 0 ? -a : a;
}

Fraction fraction_make(long long num, long long den) {
  if (den == 0)
    return (Fraction){num, den};
  if (num == 0)
    return (Fraction){0, 1};
  if (den < 0) {
    num = -num;
    den = -den;
  }
  long long g = ll_gcd(num, den);
  if (g == 0)
    return (Fraction){num, den};
  return (Fraction){num / g, den / g};
}

Fraction fraction_neg(Fraction f) { return (Fraction){-f.num, f.den}; }

int fraction_is_zero(Fraction f) { return f.num == 0; }

int fraction_is_one(Fraction f) { return f.num == f.den && f.den != 0; }

int fraction_eq(Fraction a, Fraction b) {
  return a.num == b.num && a.den == b.den;
}

static int safe_add_ll(long long a, long long b, long long *out) {
  return !__builtin_add_overflow(a, b, out);
}

static int safe_mul_ll(long long a, long long b, long long *out) {
  return !__builtin_mul_overflow(a, b, out);
}

static Fraction fraction_from_math(long long num, long long den) {
  if (den == 0)
    return (Fraction){0, 0};
  return fraction_make(num, den);
}

Fraction fraction_add(Fraction a, Fraction b) {
  long long left = 0;
  long long right = 0;
  long long den = 0;
  if (safe_mul_ll(a.num, b.den, &left) && safe_mul_ll(b.num, a.den, &right) &&
      safe_add_ll(left, right, &left) && safe_mul_ll(a.den, b.den, &den)) {
    return fraction_from_math(left, den);
  }
  return fraction_from_double((double)a.num / (double)a.den +
                              (double)b.num / (double)b.den);
}

Fraction fraction_sub(Fraction a, Fraction b) {
  long long left = 0;
  long long right = 0;
  long long den = 0;
  if (safe_mul_ll(a.num, b.den, &left) && safe_mul_ll(b.num, a.den, &right) &&
      safe_add_ll(left, -right, &left) && safe_mul_ll(a.den, b.den, &den)) {
    return fraction_from_math(left, den);
  }
  return fraction_from_double((double)a.num / (double)a.den -
                              (double)b.num / (double)b.den);
}

Fraction fraction_mul(Fraction a, Fraction b) {
  long long num = 0;
  long long den = 0;
  if (safe_mul_ll(a.num, b.num, &num) && safe_mul_ll(a.den, b.den, &den))
    return fraction_from_math(num, den);
  return fraction_from_double((double)a.num / (double)a.den *
                              ((double)b.num / (double)b.den));
}

Fraction fraction_div(Fraction a, Fraction b) {
  if (b.num == 0)
    return (Fraction){0, 0};
  long long num = 0;
  long long den = 0;
  if (safe_mul_ll(a.num, b.den, &num) && safe_mul_ll(a.den, b.num, &den))
    return fraction_from_math(num, den);
  return fraction_from_double((double)a.num / (double)a.den /
                              ((double)b.num / (double)b.den));
}

Fraction fraction_exact_from_double(double v, int *exact) {
  Fraction f = fraction_from_double(v);
  int is_exact = 0;
  if (isfinite(v)) {
    if (f.den == 1) {
      is_exact = v == (long long)v;
    } else {
      long long den = f.den;
      int power_of_two = 1;
      while (den > 1) {
        if (den % 2 != 0) {
          power_of_two = 0;
          break;
        }
        den /= 2;
      }
      is_exact = power_of_two && fabs((double)f.num / f.den - v) < 1e-12;
    }
  }
  if (exact)
    *exact = is_exact;
  return f;
}

Fraction fraction_from_double(double x) {
  Fraction f;
  static const long long fallback_scales[] = {
      1000000000000000000LL,
      100000000000000000LL,
      10000000000000000LL,
      1000000000000000LL,
      100000000000000LL,
      10000000000000LL,
      1000000000000LL,
      100000000000LL,
      10000000000LL,
      1000000000LL,
      100000000LL,
      10000000LL,
      1000000LL,
      100000LL,
      10000LL,
      1000LL,
      100LL,
      10LL,
  };
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
    /* fallback to a scaled decimal approximation without collapsing tiny
     * non-zero values to zero */
    for (size_t i = 0; i < sizeof(fallback_scales) / sizeof(fallback_scales[0]);
         i++) {
      long long scale = fallback_scales[i];
      if (x > (double)LLONG_MAX / (double)scale)
        continue;
      long long scaled = llround(x * (double)scale);
      if (scaled != 0) {
        f = fraction_make(sign * scaled, scale);
        return f;
      }
    }

    f.num = sign * ((x >= 0.5) ? 1 : 0);
    f.den = 1;
  }

  return f;
}
