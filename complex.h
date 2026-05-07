#ifndef SIA_COMPLEX_H
#define SIA_COMPLEX_H

#include "fractions.h"
#include <math.h>

typedef struct {
  double re;
  double im;
  Fraction re_q;
  Fraction im_q;
  unsigned char exact;
} Complex;

static inline Complex c_from_fractions(Fraction re, Fraction im) {
  return (Complex){
      .re = (double)re.num / (double)re.den,
      .im = (double)im.num / (double)im.den,
      .re_q = fraction_make(re.num, re.den),
      .im_q = fraction_make(im.num, im.den),
      .exact = 1,
  };
}

static inline Complex c_make(double re, double im) {
  int re_exact = 0;
  int im_exact = 0;
  Fraction re_q = fraction_exact_from_double(re, &re_exact);
  Fraction im_q = fraction_exact_from_double(im, &im_exact);
  return (Complex){
      .re = re,
      .im = im,
      .re_q = re_q,
      .im_q = im_q,
      .exact = (unsigned char)(re_exact && im_exact),
  };
}

static inline Complex c_real(double re) { return c_make(re, 0.0); }

static inline int c_is_real(Complex z) {
  return z.exact ? fraction_is_zero(z.im_q) : z.im == 0.0;
}

static inline int c_is_zero(Complex z) {
  return z.exact ? fraction_is_zero(z.re_q) && fraction_is_zero(z.im_q)
                 : z.re == 0.0 && z.im == 0.0;
}

static inline int c_is_one(Complex z) {
  return z.exact ? fraction_is_one(z.re_q) && fraction_is_zero(z.im_q)
                 : z.re == 1.0 && z.im == 0.0;
}

static inline int c_is_minus_one(Complex z) {
  return z.exact ? z.re_q.num == -z.re_q.den && fraction_is_zero(z.im_q)
                 : z.re == -1.0 && z.im == 0.0;
}

static inline int c_eq(Complex a, Complex b) {
  if (a.exact && b.exact)
    return fraction_eq(a.re_q, b.re_q) && fraction_eq(a.im_q, b.im_q);
  return a.re == b.re && a.im == b.im;
}

static inline int c_real_fraction(Complex z, Fraction *out) {
  if (!c_is_real(z))
    return 0;
  if (z.exact) {
    if (out)
      *out = z.re_q;
    return 1;
  }
  return 0;
}

static inline Complex c_add(Complex a, Complex b) {
  if (a.exact && b.exact)
    return c_from_fractions(fraction_add(a.re_q, b.re_q),
                            fraction_add(a.im_q, b.im_q));
  return c_make(a.re + b.re, a.im + b.im);
}

static inline Complex c_sub(Complex a, Complex b) {
  if (a.exact && b.exact)
    return c_from_fractions(fraction_sub(a.re_q, b.re_q),
                            fraction_sub(a.im_q, b.im_q));
  return c_make(a.re - b.re, a.im - b.im);
}

static inline Complex c_mul(Complex a, Complex b) {
  if (a.exact && b.exact) {
    Fraction re = fraction_sub(fraction_mul(a.re_q, b.re_q),
                               fraction_mul(a.im_q, b.im_q));
    Fraction im = fraction_add(fraction_mul(a.re_q, b.im_q),
                               fraction_mul(a.im_q, b.re_q));
    return c_from_fractions(re, im);
  }
  return c_make(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
}

static inline Complex c_neg(Complex z) {
  if (z.exact)
    return c_from_fractions(fraction_neg(z.re_q), fraction_neg(z.im_q));
  return c_make(-z.re, -z.im);
}

static inline double c_abs2(Complex z) { return z.re * z.re + z.im * z.im; }

static inline double c_abs(Complex z) { return sqrt(c_abs2(z)); }

static inline Complex c_conj(Complex z) {
  if (z.exact)
    return c_from_fractions(z.re_q, fraction_neg(z.im_q));
  return c_make(z.re, -z.im);
}

static inline Complex c_div(Complex a, Complex b) {
  if (a.exact && b.exact && !c_is_zero(b)) {
    Fraction d = fraction_add(fraction_mul(b.re_q, b.re_q),
                              fraction_mul(b.im_q, b.im_q));
    if (fraction_is_zero(d))
      return c_make(NAN, NAN);
    Fraction re = fraction_div(fraction_add(fraction_mul(a.re_q, b.re_q),
                                            fraction_mul(a.im_q, b.im_q)),
                               d);
    Fraction im = fraction_div(fraction_sub(fraction_mul(a.im_q, b.re_q),
                                            fraction_mul(a.re_q, b.im_q)),
                               d);
    return c_from_fractions(re, im);
  }
  double d = c_abs2(b);
  if (d == 0.0)
    return c_make(NAN, NAN);
  return c_make((a.re * b.re + a.im * b.im) / d,
                (a.im * b.re - a.re * b.im) / d);
}

static inline Complex c_exp(Complex z) {
  double er = exp(z.re);
  return c_make(er * cos(z.im), er * sin(z.im));
}

static inline Complex c_log(Complex z) {
  return c_make(log(c_abs(z)), atan2(z.im, z.re));
}

static inline Complex c_pow(Complex base, Complex exp_) {
  if (base.exact && exp_.exact && c_is_real(exp_) && exp_.im_q.num == 0 &&
      exp_.re_q.den == 1) {
    long long n = exp_.re_q.num;
    if (n == 0)
      return c_real(1.0);
    if (n < 0) {
      if (c_is_zero(base))
        return c_make(NAN, NAN);
      base = c_div(c_real(1.0), base);
      n = -n;
    }
    Complex result = c_real(1.0);
    while (n > 0) {
      if (n & 1)
        result = c_mul(result, base);
      n >>= 1;
      if (n)
        base = c_mul(base, base);
    }
    return result;
  }
  if (c_is_zero(base)) {
    if (exp_.re > 0)
      return c_real(0.0);
    return c_make(NAN, NAN);
  }
  /* base^exp = exp(exp * ln(base)) */
  Complex lb = c_log(base);
  return c_exp(c_mul(exp_, lb));
}

static inline Complex c_sqrt(Complex z) {
  if (c_is_real(z) && z.re >= 0)
    return c_real(sqrt(z.re));
  double r = c_abs(z);
  double re = sqrt((r + z.re) / 2.0);
  double im = (z.im >= 0 ? 1.0 : -1.0) * sqrt((r - z.re) / 2.0);
  return c_make(re, im);
}

static inline Complex c_sinh(Complex z) {
  return c_make(sinh(z.re) * cos(z.im), cosh(z.re) * sin(z.im));
}

static inline Complex c_cosh(Complex z) {
  return c_make(cosh(z.re) * cos(z.im), sinh(z.re) * sin(z.im));
}

static inline Complex c_tanh(Complex z) { return c_div(c_sinh(z), c_cosh(z)); }

Complex c_sin(Complex z);
Complex c_cos(Complex z);
Complex c_tan(Complex z);
Complex c_asin(Complex z);
Complex c_acos(Complex z);
Complex c_atan(Complex z);

#endif
