#ifndef CUMO_SCOMPLEX_KERNEL_H
#define CUMO_SCOMPLEX_KERNEL_H

typedef scomplex dtype;
typedef float rtype;

#include "complex_macro_kernel.h"
#include "cumo/cuda/cumo_thrust_complex.hpp"

__device__ static inline bool c_nearly_eq(dtype x, dtype y) {
    return c_abs(c_sub(x,y)) <= (c_abs(x)+c_abs(y))*FLT_EPSILON*2;
}

#endif // CUMO_SCOMPLEX_KERNEL_H
