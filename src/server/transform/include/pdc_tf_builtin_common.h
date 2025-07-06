#ifndef PDC_TF_BUILTIN_COMMON_H
#define PDC_TF_BUILTIN_COMMON_H

#include <stdbool.h>

bool pdc_tf_builtin_double_to_float(void *input, void **output);
bool pdc_tf_builtin_float_to_double(void *input, void **output);
bool pdc_tf_builtin_zfp_compress(void *input, void **output);
bool pdc_tf_builtin_zfp_decompress(void *input, void **output);

#endif
