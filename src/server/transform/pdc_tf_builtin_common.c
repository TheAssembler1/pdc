#include <stdint.h>

#include "pdc_tf_builtin_common.h"
#include "pdc_tf_common.h"

#ifdef ENABLE_TF_ZFP_COMPRESSION
#include <zfp.h>
#endif

#include "pdc_logger.h"

bool
pdc_tf_builtin_double_to_float(pdc_tf_params_t *tf_params, void **region_data, pdc_tf_region_t input_reg,
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
pdc_tf_builtin_float_to_double(pdc_tf_params_t *tf_params, void **region_data, pdc_tf_region_t input_reg,
                               pdc_tf_region_t *output_reg)
{
    LOG_INFO("pdc_tf_builtin_float_to_double was called\n");
    PDCtf_log_pdc_region_t(input_reg);
    return true;
}

typedef struct zfp_compress_params_t {
    zfp_type        z_type;
    pdc_var_type_t  data_type;
    pdc_tf_region_t decompressed_region;
} zfp_compress_params_t;

static void
print_ztype(zfp_type z_type)
{
    switch (z_type) {
        case zfp_type_int32:
            LOG_INFO("ZFP type: int32\n");
            break;
        case zfp_type_int64:
            LOG_INFO("ZFP type: int64\n");
            break;
        case zfp_type_float:
            LOG_INFO("ZFP type: float\n");
            break;
        case zfp_type_double:
            LOG_INFO("ZFP type: double\n");
            break;
        case zfp_type_none:
        default:
            LOG_INFO("ZFP type: none/unknown (%d)\n", (int)z_type);
            break;
    }
}

#ifdef ENABLE_TF_ZFP_COMPRESSION
bool
pdc_tf_builtin_zfp_compress(pdc_tf_params_t *tf_params, void **region_data, pdc_tf_region_t input_reg,
                            pdc_tf_region_t *output_reg)
{
    LOG_INFO("pdc_tf_builtin_zfp_compress was called\n");

    PDCtf_log_pdc_region_t(input_reg);

    LOG_INFO("ZFP COMPRESS INPUT\n");
    for (int i = 0; i < 128; i++)
        LOG_JUST_PRINT("%d ", ((int *)*region_data)[i]);
    LOG_JUST_PRINT("\n");

    // set datatype based on params
    zfp_type z_type;

    switch (tf_params->params_str[0]) {
        case 'f':
            z_type = zfp_type_float;
            break;
        case 'd':
            z_type = zfp_type_double;
            break;
        case 'i':
            z_type = zfp_type_int32;
            break;
        case 'l':
            z_type = zfp_type_int64;
            break;
        default:
            LOG_ERROR("Unsupported datatype: %s\n", tf_params->params_str);
            return false;
    }
    print_ztype(z_type);

    zfp_field *field = NULL;
    switch (input_reg.ndim) {
        case 1:
            field = zfp_field_1d(*region_data, z_type, input_reg.size[0]);
            break;
        case 2:
            field = zfp_field_2d(*region_data, z_type, input_reg.size[0], input_reg.size[1]);
            break;
        case 3:
            field =
                zfp_field_3d(*region_data, z_type, input_reg.size[0], input_reg.size[1], input_reg.size[2]);
            break;
        case 4:
            field = zfp_field_4d(*region_data, z_type, input_reg.size[0], input_reg.size[1],
                                 input_reg.size[2], input_reg.size[3]);
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

    // Allocate buffer for compressed data
    size_t bufsize           = zfp_stream_maximum_size(zfp, field);
    void * compressed_buffer = malloc(bufsize);
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

    zfp_stream_set_reversible(zfp);
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

    // Set output params
    zfp_compress_params_t *out_params = (zfp_compress_params_t *)malloc(sizeof(zfp_compress_params_t));
    out_params->z_type                = z_type;
    PDCtf_copy_tf_region_t(&input_reg, &out_params->decompressed_region);

    tf_params->output_params      = out_params;
    tf_params->output_params_size = sizeof(zfp_compress_params_t);

    LOG_INFO("ZFP compression succeeded, compressed size bytes: %zu bytes\n", compressed_size);

    LOG_INFO("ZFP COMPRESS OUTPUT\n");
    for (int i = 0; i < 189; i++)
        LOG_JUST_PRINT("%02x ", ((char *)*region_data)[i]);
    LOG_JUST_PRINT("\n");

    return true;
}

bool
pdc_tf_builtin_zfp_decompress(pdc_tf_params_t *tf_params, void **region_data, pdc_tf_region_t input_reg,
                              pdc_tf_region_t *output_reg)
{
    LOG_INFO("pdc_tf_builtin_zfp_decompress was called\n");

    PDCtf_log_pdc_region_t(input_reg);

    LOG_INFO("ZFP DECOMPRESS INPUT\n");
    for (int i = 0; i < 189; i++)
        LOG_JUST_PRINT("%02x ", ((char *)*region_data)[i]);
    LOG_JUST_PRINT("\n");

    // set datatype based on params
    zfp_compress_params_t *in_params = (zfp_compress_params_t *)tf_params->input_params;
    if (in_params == NULL) {
        LOG_ERROR("ZFP decompress passed NULL params\n");
        return false;
    }

    print_ztype(in_params->z_type);
    size_t compressed_size = input_reg.size[0];
    LOG_INFO("Compressed size: %zu bytes\n", compressed_size);

    // Create bitstream from compressed data
    bitstream *stream = stream_open(*region_data, compressed_size);
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

    zfp_stream_set_reversible(zfp);
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    // Allocate uncompressed buffer
    size_t total_bytes = PDCtf_get_pdc_region_t_bytes(in_params->decompressed_region);
    LOG_INFO("Decompressed region %zu bytes\n", total_bytes);
    void *buf = malloc(total_bytes);

    // Create ZFP field for decompression
    zfp_field *field = NULL;
    switch (in_params->decompressed_region.ndim) {
        case 1:
            field = zfp_field_1d(buf, in_params->z_type, in_params->decompressed_region.size[0]);
            break;
        case 2:
            field = zfp_field_2d(buf, in_params->z_type, in_params->decompressed_region.size[0],
                                 in_params->decompressed_region.size[1]);
            break;
        case 3:
            field =
                zfp_field_3d(buf, in_params->z_type, in_params->decompressed_region.size[0],
                             in_params->decompressed_region.size[1], in_params->decompressed_region.size[2]);
            break;
        case 4:
            field =
                zfp_field_4d(buf, in_params->z_type, in_params->decompressed_region.size[0],
                             in_params->decompressed_region.size[1], in_params->decompressed_region.size[2],
                             in_params->decompressed_region.size[3]);
            break;
        default:
            LOG_ERROR("Unsupported ndim: %d\n", in_params->decompressed_region.ndim);
            free(buf);
            zfp_stream_close(zfp);
            stream_close(stream);
            return false;
    }

    if (!field) {
        LOG_ERROR("Failed to create zfp field\n");
        free(buf);
        zfp_stream_close(zfp);
        stream_close(stream);
        return false;
    }

    // Decompress
    size_t decompressed_size = zfp_decompress(zfp, field);
    if (decompressed_size == 0) {
        LOG_ERROR("ZFP decompression failed\n");
        free(buf);
        zfp_field_free(field);
        zfp_stream_close(zfp);
        stream_close(stream);
        return false;
    }

    // Set output region unit and dims (same as input)
    PDCtf_copy_tf_region_t(&in_params->decompressed_region, output_reg);

    // Update region_data to point to decompressed buffer
    *region_data = buf;

    size_t sizes[4];
    size_t num_elements = zfp_field_size(field, sizes);

    size_t elem_size = 0;
    switch (zfp_field_type(field)) {
        case zfp_type_int32:
            elem_size = 4;
            break;
        case zfp_type_int64:
            elem_size = 8;
            break;
        case zfp_type_float:
            elem_size = 4;
            break;
        case zfp_type_double:
            elem_size = 8;
            break;
        default:
            elem_size = 0;
            break;
    }

    size_t decompressed_bytes = num_elements * elem_size;
    LOG_INFO("Actual decompressed size: %zu bytes\n", decompressed_bytes);
    LOG_INFO("Expected decompressed size: %zu bytes\n", PDCtf_get_pdc_region_t_bytes(*output_reg));

    LOG_INFO("ZFP DECOMPRESS OUTPUT\n");
    for (int i = 0; i < 128; i++)
        LOG_JUST_PRINT("%d ", ((int *)*region_data)[i]);
    LOG_JUST_PRINT("\n");

    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);

    return true;
}
#endif // ENABLE_TF_ZFP_COMPRESSION
