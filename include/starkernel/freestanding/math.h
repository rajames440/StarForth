/* Freestanding math.h shim — floating-point math for kernel builds.
 * These functions are declared but should not be called; physics feedback
 * loops use Q48.16 integer arithmetic, not float. */
#ifndef FREESTANDING_MATH_H
#define FREESTANDING_MATH_H
double sqrt(double x);
double pow(double base, double exp);
double fabs(double x);
double ceil(double x);
double floor(double x);
double log(double x);
double exp(double x);
double sin(double x);
double cos(double x);
double atan2(double y, double x);
#define HUGE_VAL __builtin_huge_val()
#define NAN      __builtin_nanf("")
#define INFINITY __builtin_inff()
#endif /* FREESTANDING_MATH_H */
