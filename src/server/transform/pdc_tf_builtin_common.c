#include <stdint.h>

#include "pdc_tf_builtin_common.h"

#ifdef ENABLE_TF_ZFP_COMPRESSION
#include <zfp.h>
#endif

#include "pdc_logger.h"

bool
pdc_tf_builtin_double_to_float(void *params, void **region_data,
                               pdc_tf_region_t input_reg, pdc_tf_region_t* output_reg)
{
    LOG_INFO("pdc_tf_builtin_double_to_float was called\n");

    float *buf = *((float **)region_data);

    for (int i = 0; i < 10; i++)
        printf("float[%d] = %f\n", i, buf[i]);

    return true;
}

bool
pdc_tf_builtin_float_to_double(void *params, void **region_data,
                               pdc_tf_region_t input_reg, pdc_tf_region_t* output_reg)
{
    LOG_INFO("pdc_tf_builtin_float_to_double was called\n");
    return true;
}

#ifdef ENABLE_TF_ZFP_COMPRESSION
bool
pdc_tf_builtin_zfp_compress(void *params, void **region_data,
                         pdc_tf_region_t input_reg, pdc_tf_region_t* output_reg)
{
    LOG_INFO("pdc_tf_builtin_zfp_compress was called\n");

    /**
     * FIXME: we can get the zfp type through the *params for now hardcoding
     */
    zfp_type z_type = zfp_type_float;
    float *buf = *((float **)region_data);

    zfp_field* field = NULL;

    switch (input_reg.ndim) {
        case 1:
            field = zfp_field_1d(buf, z_type, input_reg.dims[0]);
            break;
        case 2:
            field = zfp_field_2d(buf, z_type, input_reg.dims[0], input_reg.dims[1]);
            break;
        case 3:
            field = zfp_field_3d(buf, z_type, input_reg.dims[0], input_reg.dims[1], input_reg.dims[2]);
            break;
        case 4:
            field = zfp_field_4d(buf, z_type, input_reg.dims[0], input_reg.dims[1], input_reg.dims[2], input_reg.dims[3]);
            break;
        case 0:
            LOG_ERROR("ZFP compression not supported for 0 dimensions\n");
            return false;
        default:
            LOG_ERROR("ZFP compression not supported for > 4 dimensions\n");
            return false;
    }

    if(field == NULL) {
        LOG_ERROR("field was NULL\n");
        return false;
    }

    return true;
}

bool
pdc_tf_builtin_zfp_decompress(void *params, void **region_data,
                              pdc_tf_region_t input_reg, pdc_tf_region_t* output_reg)
{
    LOG_INFO("pdc_tf_builtin_zfp_decompress was called\n");
    return true;
}
#endif // ENABLE_TF_ZFP_COMPRESSION
