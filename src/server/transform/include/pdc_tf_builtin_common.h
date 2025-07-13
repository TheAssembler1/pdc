#ifndef PDC_TF_BUILTIN_COMMON_H
#define PDC_TF_BUILTIN_COMMON_H

#include <stdbool.h>
#include <stdint.h>

// FIXME: this should be picked up from the cmakelists
#define ENABLE_TF_ZFP_COMPRESSION

#include "pdc_tf_common.h"

bool pdc_tf_builtin_double_to_float(void *params, void **region_data,
                                    pdc_tf_region_t input_reg, pdc_tf_region_t* output_reg);
bool pdc_tf_builtin_float_to_double(void *params, void **region_data,
                                    pdc_tf_region_t input_reg, pdc_tf_region_t* output_reg);

#ifdef ENABLE_TF_ZFP_COMPRESSION
bool pdc_tf_builtin_zfp_compress(void *params, void **region_data,
                         pdc_tf_region_t input_reg, pdc_tf_region_t* output_reg);
bool pdc_tf_builtin_zfp_decompress(void *params, void **region_data,
                         pdc_tf_region_t input_reg, pdc_tf_region_t* output_reg);
#endif

#endif
