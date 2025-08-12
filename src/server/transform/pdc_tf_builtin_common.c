#include <stdint.h>

#include "pdc_tf_builtin_common.h"
#include "pdc_tf_common.h"

#ifdef ENABLE_TF_ZFP_COMPRESSION
#include <zfp.h>
#endif

#include "pdc_logger.h"

bool
pdc_tf_builtin_double_to_float(void *params, void **region_data, pdc_tf_region_t input_reg,
                               pdc_tf_region_t *output_reg)
{
    LOG_INFO("pdc_tf_builtin_double_to_float was called\n");

    PDCtf_log_pdc_region_t(input_reg);
    size_t num_elements = PDCtf_get_pdc_region_t_elements(input_reg);

    double *buf = *((double **)region_data);

    // copy into new buffer
    float *new_buf = (float *)malloc(PDCtf_get_pdc_region_t_bytes(input_reg));
    for (int i = 0; i < PDCtf_get_pdc_region_t_elements(input_reg); i++)
        new_buf[i] = (float)buf[i];

    *region_data = new_buf;

    // resize output region
    output_reg->unit = sizeof(float);

    return true;
}

bool
pdc_tf_builtin_float_to_double(void *params, void **region_data, pdc_tf_region_t input_reg,
                               pdc_tf_region_t *output_reg)
{
    LOG_INFO("pdc_tf_builtin_float_to_double was called\n");
    PDCtf_log_pdc_region_t(input_reg);
    return true;
}

#ifdef ENABLE_TF_ZFP_COMPRESSION
bool
pdc_tf_builtin_zfp_compress(void *params, void **region_data, pdc_tf_region_t input_reg,
                            pdc_tf_region_t *output_reg)
{
    LOG_INFO("pdc_tf_builtin_zfp_compress was called\n");

    PDCtf_log_pdc_region_t(input_reg);

    /**
     * FIXME: we can get the zfp type through the *params for now hardcoding
     * we can also get the compression rate and align parameters
     */
    zfp_type z_type    = zfp_type_float;
    uint8_t  comp_rate = 16;
    uint8_t  align     = 0;

    float *buf = *((float **)region_data);

    if (buf == NULL) {
        LOG_ERROR("ZFP compress passed NULL buf\n");
        return false;
    }

    zfp_field *field = NULL;

    switch (input_reg.ndim) {
        case 1:
            field = zfp_field_1d(buf, z_type, input_reg.size[0]);
            break;
        case 2:
            field = zfp_field_2d(buf, z_type, input_reg.size[0], input_reg.size[1]);
            break;
        case 3:
            field = zfp_field_3d(buf, z_type, input_reg.size[0], input_reg.size[1], input_reg.size[2]);
            break;
        case 4:
            field = zfp_field_4d(buf, z_type, input_reg.size[0], input_reg.size[1], input_reg.size[2],
                                 input_reg.size[3]);
            break;
        case 0:
            LOG_ERROR("ZFP compression not supported for 0 dimensions\n");
            return false;
        default:
            LOG_ERROR("ZFP compression not supported for > 4 dimensions\n");
            return false;
    }

    if (field == NULL) {
        LOG_ERROR("field was NULL\n");
        return false;
    }

    // Create a zfp stream
    zfp_stream *zfp = zfp_stream_open(NULL);
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
    size_t bufsize           = zfp_stream_maximum_size(zfp, field);
    void  *compressed_buffer = malloc(bufsize);
    if (!compressed_buffer) {
        LOG_ERROR("Failed to allocate memory for compressed data\n");
        zfp_field_free(field);
        zfp_stream_close(zfp);
        return false;
    }

    // Create bitstream backed by the compressed buffer
    bitstream *stream = stream_open(compressed_buffer, bufsize);
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
    output_reg->ndim    = 1;
    output_reg->unit    = 1;
    output_reg->size[0] = compressed_size;

    // Free zfp structures
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);

    LOG_INFO("ZFP compression succeeded, compressed size bytes: %zu bytes\n", compressed_size);

    return true;
}

bool
pdc_tf_builtin_zfp_decompress(void *params, void **region_data, pdc_tf_region_t input_reg,
                              pdc_tf_region_t *output_reg)
{
    LOG_INFO("pdc_tf_builtin_zfp_decompress was called\n");

    PDCtf_log_pdc_region_t(input_reg);

    // Hardcoded for now to match compress path
    zfp_type z_type = zfp_type_float;
    uint8_t  align  = 0;

    // Compressed buffer from input
    void  *compressed_buffer = *region_data;
    size_t compressed_size   = input_reg.size[0];

    // Create bitstream from compressed data
    bitstream *stream = stream_open(compressed_buffer, compressed_size);
    if (!stream) {
        LOG_ERROR("Failed to open bitstream for decompression\n");
        return false;
    }

    // Create zfp stream and associate it with bitstream
    zfp_stream *zfp = zfp_stream_open(NULL);
    if (!zfp) {
        LOG_ERROR("Failed to create zfp stream\n");
        stream_close(stream);
        return false;
    }

    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    // Allocate uncompressed buffer
    size_t total_elems = 1;
    for (int i = 0; i < input_reg.ndim; ++i)
        total_elems *= input_reg.size[i];

    float *decompressed_data = (float *)malloc(total_elems * sizeof(float));
    if (!decompressed_data) {
        LOG_ERROR("Failed to allocate memory for decompressed data\n");
        zfp_stream_close(zfp);
        stream_close(stream);
        return false;
    }

    // Create ZFP field for decompression
    zfp_field *field = NULL;
    switch (input_reg.ndim) {
        case 1:
            field = zfp_field_1d(decompressed_data, z_type, input_reg.size[0]);
            break;
        case 2:
            field = zfp_field_2d(decompressed_data, z_type, input_reg.size[0], input_reg.size[1]);
            break;
        case 3:
            field = zfp_field_3d(decompressed_data, z_type, input_reg.size[0], input_reg.size[1],
                                 input_reg.size[2]);
            break;
        case 4:
            field = zfp_field_4d(decompressed_data, z_type, input_reg.size[0], input_reg.size[1],
                                 input_reg.size[2], input_reg.size[3]);
            break;
        default:
            LOG_ERROR("Unsupported ndim: %d\n", input_reg.size);
            free(decompressed_data);
            zfp_stream_close(zfp);
            stream_close(stream);
            return false;
    }

    if (!field) {
        LOG_ERROR("Failed to create zfp field\n");
        free(decompressed_data);
        zfp_stream_close(zfp);
        stream_close(stream);
        return false;
    }

    // Decompress
    size_t decompressed_size = zfp_decompress(zfp, field);
    if (decompressed_size == 0) {
        LOG_ERROR("ZFP decompression failed\n");
        free(decompressed_data);
        zfp_field_free(field);
        zfp_stream_close(zfp);
        stream_close(stream);
        return false;
    }

    // Set output region unit and dims (same as input)
    output_reg->unit = 4;
    output_reg->ndim = 1;
    memcpy(output_reg->size, input_reg.size, input_reg.ndim * sizeof(uint64_t));

    // Update region_data to point to decompressed buffer
    *region_data = decompressed_data;

    LOG_INFO("ZFP decompression succeeded, decompressed size bytes: %zu\n", decompressed_size);

    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);

    return true;
}

#endif // ENABLE_TF_ZFP_COMPRESSION
