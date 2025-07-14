#include <stdint.h>

#include "pdc_tf_builtin_common.h"

#ifdef ENABLE_TF_ZFP_COMPRESSION
#include <zfp.h>
#endif

#include "pdc_logger.h"

static size_t get_pdc_region_t_elements(pdc_tf_region_t input_reg) {
    size_t num_elements = 1;
    for (int i = 0; i < input_reg.ndim; ++i) {
        num_elements *= input_reg.dims[i];
    }
    return num_elements;
}

static size_t get_pdc_region_t_bytes(pdc_tf_region_t input_reg) {
    return get_pdc_region_t_elements(input_reg) * input_reg.unit;
}

static void log_pdc_region_t(pdc_tf_region_t input_reg) {
    LOG_INFO("input region ndim: %lu\n", input_reg.ndim);
    LOG_INFO("input region unit: %lu\n", input_reg.unit);
    for(int i = 0; i < input_reg.ndim; i++)
        LOG_INFO("\tdim %d = %lu\n", i + 1, input_reg.dims[0]);
    LOG_INFO("Total input region bytes: %zu\n", get_pdc_region_t_bytes(input_reg));
}

bool
pdc_tf_builtin_double_to_float(void *params, void **region_data,
                               pdc_tf_region_t input_reg, pdc_tf_region_t* output_reg)
{
    LOG_INFO("pdc_tf_builtin_double_to_float was called\n");

    log_pdc_region_t(input_reg);
    size_t num_elements = get_pdc_region_t_elements(input_reg);

    double *buf = *((double **)region_data);

    // copy into new buffer
    float* new_buf = (float*)PDC_malloc(get_pdc_region_t_bytes(input_reg));
    for(int i = 0; i < get_pdc_region_t_elements(input_reg); i++)
        new_buf[i] = (float)buf[i];

    *region_data = new_buf;

    // resize output region
    output_reg->unit = sizeof(float);

    return true;
}

bool
pdc_tf_builtin_float_to_double(void *params, void **region_data,
                               pdc_tf_region_t input_reg, pdc_tf_region_t* output_reg)
{
    LOG_INFO("pdc_tf_builtin_float_to_double was called\n");
    log_pdc_region_t(input_reg);
    return true;
}

#ifdef ENABLE_TF_ZFP_COMPRESSION
bool
pdc_tf_builtin_zfp_compress(void *params, void **region_data,
                         pdc_tf_region_t input_reg, pdc_tf_region_t* output_reg)
{
    LOG_INFO("pdc_tf_builtin_zfp_compress was called\n");

    log_pdc_region_t(input_reg);

    /**
     * FIXME: we can get the zfp type through the *params for now hardcoding
     * we can also get the compression rate and align parameters
     */
    zfp_type z_type = zfp_type_float;
    uint8_t comp_rate = 16;
    uint8_t align = 0;

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

    // Create a zfp stream
    zfp_stream* zfp = zfp_stream_open(NULL);
    if (!zfp) {
        LOG_ERROR("Failed to create zfp stream\n");
        zfp_field_free(field);
        return false;
    }

    // Set compression rate (bits per value)
    if (!zfp_stream_set_rate(zfp, comp_rate, z_type, input_reg.ndim, align)) {
        LOG_ERROR("Failed to set zfp compression rate\n");
        zfp_field_free(field);
        zfp_stream_close(zfp);
        return false;
    }

    // Allocate buffer for compressed data
    size_t bufsize = zfp_stream_maximum_size(zfp, field);
    void* compressed_buffer = PDC_malloc(bufsize);
    if (!compressed_buffer) {
        LOG_ERROR("Failed to allocate memory for compressed data\n");
        zfp_field_free(field);
        zfp_stream_close(zfp);
        return false;
    }

    // Create bitstream backed by the compressed buffer
    bitstream* stream = stream_open(compressed_buffer, bufsize);
    if (!stream) {
        LOG_ERROR("Failed to create bitstream\n");
        free(compressed_buffer);
        zfp_field_free(field);
        zfp_stream_close(zfp);
        return false;
    }

    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    // Perform compression
    size_t compressed_size = zfp_compress(zfp, field);
    if (compressed_size == 0) {
        LOG_ERROR("ZFP compression failed\n");
        stream_close(stream);
        free(compressed_buffer);
        zfp_field_free(field);
        zfp_stream_close(zfp);
        return false;
    }

    // Update region_data to point to compressed buffer
    *region_data = compressed_buffer;

    // Update output region dims to reflect compressed data size (1D)
    output_reg->ndim = 1;
    output_reg->dims[0] = compressed_size;

    // Free zfp structures
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);

    LOG_INFO("ZFP compression succeeded, compressed size: %zu bytes\n", compressed_size);

    return true;
}

bool
pdc_tf_builtin_zfp_decompress(void *params, void **region_data,
                              pdc_tf_region_t input_reg, pdc_tf_region_t* output_reg)
{
    LOG_INFO("pdc_tf_builtin_zfp_decompress was called\n");
    log_pdc_region_t(input_reg);
    return true;
}
#endif // ENABLE_TF_ZFP_COMPRESSION
