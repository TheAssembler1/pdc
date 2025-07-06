#ifndef PDC_TF_BUILTIN_COMMON_H
#define PDC_TF_BUILTIN_COMMON_H

#include <stdbool.h>
#include <stdint.h>

bool pdc_tf_builtin_double_to_float(void* params, uint8_t ndim, uint32_t* dims,
                                    uint32_t* sizes, void *input, void **output);
bool pdc_tf_builtin_float_to_double(void* params, uint8_t ndim, uint32_t* dims,
                                    uint32_t* sizes, void *input, void **output);
bool pdc_tf_builtin_zfp_compress(void* params, uint8_t ndim, uint32_t* dims,
                                 uint32_t* sizes, void *input, void **output);
bool pdc_tf_builtin_zfp_decompress(void* params, uint8_t ndim, uint32_t* dims,
                                   uint32_t* sizes, void *input, void **output);

#endif
