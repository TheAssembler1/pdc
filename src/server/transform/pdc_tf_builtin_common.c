#include "pdc_tf_builtin_common.h"
#include "pdc_logger.h"

bool
pdc_tf_builtin_double_to_float(void *params, uint8_t ndim, uint32_t *dims, uint32_t *sizes, void *input,
                               void **output)
{
    LOG_INFO("pdc_tf_builtin_double_to_float was called\n");

    float *buf = (float *)input;

    for (int i = 0; i < 10; i++)
        printf("float[%d] = %f\n", i, buf[i]);

    return true;
}

bool
pdc_tf_builtin_float_to_double(void *params, uint8_t ndim, uint32_t *dims, uint32_t *sizes, void *input,
                               void **output)
{
    LOG_INFO("pdc_tf_builtin_float_to_double was called\n");
    return true;
}

bool
pdc_tf_builtin_zfp_compress(void *params, uint8_t ndim, uint32_t *dims, uint32_t *sizes, void *input,
                            void **output)
{
    LOG_INFO("pdc_tf_builtin_zfp_compress was called\n");
    return true;
}

bool
pdc_tf_builtin_zfp_decompress(void *params, uint8_t ndim, uint32_t *dims, uint32_t *sizes, void *input,
                              void **output)
{
    LOG_INFO("pdc_tf_builtin_zfp_decompress was called\n");
    return true;
}
