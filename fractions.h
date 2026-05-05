#pragma once

typedef struct {
  long long num;
  long long den;
} Fraction;

/* Converts a double to the nearest simple fraction using continued fractions.
 * Returns {v, 1} if no simple fraction is found within the precision. */
Fraction fraction_from_double(double v);
