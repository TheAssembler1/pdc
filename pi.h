#ifndef PI_GPU_H
#define PI_GPU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cuda_runtime.h>

// Function to run Pi calculation on GPU
void run_gemm_compute(int rank);

#ifdef __cplusplus
}
#endif

#endif // PI_GPU_H
