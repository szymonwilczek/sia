#ifndef SIA_COMPLEX_H
#define SIA_COMPLEX_H

#include <math.h>

typedef struct {
  double re;
  double im;
} Complex;

static inline Complex c_make(double re, double im) { return (Complex){re, im}; }

static inline Complex c_real(double re) { return (Complex){re, 0.0}; }

static inline int c_is_real(Complex z) { return z.im == 0.0; }

static inline int c_is_zero(Complex z) { return z.re == 0.0 && z.im == 0.0; }

static inline Complex c_add(Complex a, Complex b) {
  return c_make(a.re + b.re, a.im + b.im);
}

static inline Complex c_sub(Complex a, Complex b) {
  return c_make(a.re - b.re, a.im - b.im);
}

static inline Complex c_mul(Complex a, Complex b) {
  return c_make(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
}

static inline Complex c_neg(Complex z) { return c_make(-z.re, -z.im); }

static inline double c_abs2(Complex z) { return z.re * z.re + z.im * z.im; }

static inline double c_abs(Complex z) { return sqrt(c_abs2(z)); }

static inline Complex c_conj(Complex z) { return c_make(z.re, -z.im); }

static inline Complex c_div(Complex a, Complex b) {
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

static inline Complex c_sin(Complex z) {
  return c_make(sin(z.re) * cosh(z.im), cos(z.re) * sinh(z.im));
}

static inline Complex c_cos(Complex z) {
  return c_make(cos(z.re) * cosh(z.im), -sin(z.re) * sinh(z.im));
}

static inline Complex c_tan(Complex z) { return c_div(c_sin(z), c_cos(z)); }

static inline Complex c_sinh(Complex z) {
  return c_make(sinh(z.re) * cos(z.im), cosh(z.re) * sin(z.im));
}

static inline Complex c_cosh(Complex z) {
  return c_make(cosh(z.re) * cos(z.im), sinh(z.re) * sin(z.im));
}

static inline Complex c_tanh(Complex z) { return c_div(c_sinh(z), c_cosh(z)); }

static inline Complex c_asin(Complex z) {
  /* asin(z) = -i * ln(iz + sqrt(1 - z^2)) */
  Complex i_ = c_make(0, 1);
  Complex mi = c_make(0, -1);
  Complex one = c_real(1);
  Complex arg = c_add(c_mul(i_, z), c_sqrt(c_sub(one, c_mul(z, z))));
  return c_mul(mi, c_log(arg));
}

static inline Complex c_acos(Complex z) {
  /* acos(z) = pi/2 - asin(z) */
  Complex half_pi = c_real(M_PI / 2.0);
  return c_sub(half_pi, c_asin(z));
}

static inline Complex c_atan(Complex z) {
  /* atan(z) = (i/2) * ln((1-iz)/(1+iz)) */
  Complex i_ = c_make(0, 1);
  Complex half_i = c_make(0, 0.5);
  Complex iz = c_mul(i_, z);
  Complex one = c_real(1);
  return c_mul(half_i, c_log(c_div(c_sub(one, iz), c_add(one, iz))));
}

#endif
