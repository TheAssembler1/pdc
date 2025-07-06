#include "pdc_tf_builtin_common.h"
#include "pdc_logger.h"

bool
pdc_tf_builtin_double_to_float(void *input, void **output)
{
    LOG_INFO("pdc_tf_builtin_double_to_float was called\n");
    return true;
}

bool
pdc_tf_builtin_float_to_double(void *input, void **output)
{
    LOG_INFO("pdc_tf_builtin_float_to_double was called\n");
    return true;
}

bool
pdc_tf_builtin_zfp_compress(void *input, void **output)
{
    LOG_INFO("pdc_tf_builtin_zfp_compress was called\n");
    return true;
}

bool
pdc_tf_builtin_zfp_decompress(void *input, void **output)
{
    LOG_INFO("pdc_tf_builtin_zfp_decompress was called\n");
    return true;
}
