#ifndef LHDC_TEST_MATH_HOST_REFERENCE_H
#define LHDC_TEST_MATH_HOST_REFERENCE_H
#include <stdint.h>
#define LHDC_TEST_MATH_HOST_OPERATIONS 12
typedef uint64_t (*LhdcMathHostFunction)(uint64_t, uint64_t);
extern LhdcMathHostFunction const lhdc_test_math_host_reference[LHDC_TEST_MATH_HOST_OPERATIONS];
#endif
