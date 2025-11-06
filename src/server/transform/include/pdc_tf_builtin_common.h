#ifndef PDC_TF_BUILTIN_COMMON_H
#define PDC_TF_BUILTIN_COMMON_H

#include <stdbool.h>
#include <stdint.h>

// FIXME: this should be picked up from the cmakelists
#define ENABLE_TF_ZFP_COMPRESSION
// FIMXE: same
#define ENABLE_SECRET_BOX_ENCRYPTION
// FIXME: same
#define ENABLE_TF_SZ_COMPRESSION

#include "pdc_tf_common.h"

#ifdef ENABLE_TF_SZ_COMPRESSION
bool pdc_tf_builtin_sz_compress(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                                 pdc_tf_region_t input_region, pdc_tf_region_t *output_region);
bool pdc_tf_builtin_sz_decompress(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                                   pdc_tf_region_t input_region, pdc_tf_region_t *output_region);
#endif

#ifdef ENABLE_TF_ZFP_COMPRESSION
bool pdc_tf_builtin_zfp_compress(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                                 pdc_tf_region_t input_region, pdc_tf_region_t *output_region);
bool pdc_tf_builtin_zfp_decompress(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                                   pdc_tf_region_t input_region, pdc_tf_region_t *output_region);
#endif
#if defined(ENABLE_TF_ZFP_COMPRESSION) && defined(CUDA_ENABLED)
bool pdc_tf_builtin_zfp_compress_cuda(pdc_tf_internal_param internal_param, char *params_str,
                                      void **region_data, pdc_tf_region_t input_region,
                                      pdc_tf_region_t *output_region);
bool pdc_tf_builtin_zfp_decompress_cuda(pdc_tf_internal_param internal_param, char *params_str,
                                        void **region_data, pdc_tf_region_t input_region,
                                        pdc_tf_region_t *output_region);
#endif

#ifdef ENABLE_SECRET_BOX_ENCRYPTION
bool pdc_tf_builtin_encrypt(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                            pdc_tf_region_t input_region, pdc_tf_region_t *output_region);
bool pdc_tf_builtin_decrypt(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                            pdc_tf_region_t input_region, pdc_tf_region_t *output_region);
#endif

#endif
