#ifndef PDC_TF_BUILTIN_COMMON_H
#define PDC_TF_BUILTIN_COMMON_H

#include <stdbool.h>
#include <stdint.h>

// FIXME: this should be picked up from the cmakelists
#define ENABLE_TF_ZFP_COMPRESSION
// FIMXE: same
#define ENABLE_SECRET_BOX_ENCRYPTION

#include "pdc_tf_common.h"

bool pdc_tf_builtin_double_to_float(pdc_tf_internal_param internal_param, char *params_str,
                                    void **region_data, pdc_tf_region_t input_region,
                                    pdc_tf_region_t *output_region);
bool pdc_tf_builtin_float_to_double(pdc_tf_internal_param internal_param, char *params_str,
                                    void **region_data, pdc_tf_region_t input_region,
                                    pdc_tf_region_t *output_region);

#ifdef ENABLE_TF_ZFP_COMPRESSION
bool pdc_tf_builtin_zfp_compress(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                                 pdc_tf_region_t input_region, pdc_tf_region_t *output_region);
bool pdc_tf_builtin_zfp_decompress(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                                   pdc_tf_region_t input_region, pdc_tf_region_t *output_region);
#endif

#ifdef ENABLE_SECRET_BOX_ENCRYPTION
bool pdc_tf_builtin_encrypt(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                            pdc_tf_region_t input_region, pdc_tf_region_t *output_region);
bool pdc_tf_builtin_decrypt(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                            pdc_tf_region_t input_region, pdc_tf_region_t *output_region);
#endif

#endif
