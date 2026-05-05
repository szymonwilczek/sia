#pragma once

#include <stddef.h>

typedef struct {
  long long num;
  long long den;
} Fraction;

/* Converts a double to the nearest simple fraction using continued fractions.
 * Returns {v, 1} if no simple fraction is found within the precision. */
Fraction fraction_from_double(double v);

/* Converts a double to a fraction and reports whether the conversion is exact.
 */
Fraction fraction_exact_from_double(double v, int *exact);

Fraction fraction_make(long long num, long long den);
Fraction fraction_neg(Fraction f);
Fraction fraction_add(Fraction a, Fraction b);
Fraction fraction_sub(Fraction a, Fraction b);
Fraction fraction_mul(Fraction a, Fraction b);
Fraction fraction_div(Fraction a, Fraction b);
int fraction_is_zero(Fraction f);
int fraction_is_one(Fraction f);
int fraction_eq(Fraction a, Fraction b);
