#include <stdint.h>

#include "pdc_tf_builtin_common.h"
#include "pdc_tf_common.h"
#include "pdc_tf_helper.h"

#ifdef ENABLE_TF_ZFP_COMPRESSION
#include <zfp.h>
#endif

#ifdef ENABLE_SECRET_BOX_ENCRYPTION
#include <sodium.h>
#endif

#include "pdc_logger.h"

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
pdc_tf_builtin_zfp_compress(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                            pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_INFO("pdc_tf_builtin_zfp_compress was called\n");

    PDCtf_log_pdc_region_t(input_region);

    // set datatype based on params
    zfp_type z_type;

    switch (input_region.pdc_var_type) {
        case PDC_FLOAT:
            z_type = zfp_type_float;
            break;
        case PDC_DOUBLE:
            z_type = zfp_type_double;
            break;
        case PDC_INT32:
            z_type = zfp_type_int32;
            break;
        case PDC_INT64:
            z_type = zfp_type_int64;
            break;
        default:
            LOG_ERROR("Unsupported datatype: %s\n", params_str);
            return false;
    }
    print_ztype(z_type);

    zfp_field *field = NULL;
    switch (input_region.ndim) {
        case 1:
            field = zfp_field_1d(*region_data, z_type, input_region.size[0]);
            break;
        case 2:
            field = zfp_field_2d(*region_data, z_type, input_region.size[0], input_region.size[1]);
            break;
        case 3:
            field = zfp_field_3d(*region_data, z_type, input_region.size[0], input_region.size[1],
                                 input_region.size[2]);
            break;
        case 4:
            field = zfp_field_4d(*region_data, z_type, input_region.size[0], input_region.size[1],
                                 input_region.size[2], input_region.size[3]);
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
    output_region->ndim         = 1;
    output_region->pdc_var_type = PDC_CHAR;
    output_region->size[0]      = compressed_size;

    // Free zfp structures
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);

    // Set output params
    zfp_compress_params_t *out_params = (zfp_compress_params_t *)malloc(sizeof(zfp_compress_params_t));
    out_params->z_type                = z_type;
    PDCtf_copy_tf_region_t(&input_region, &out_params->decompressed_region);
    SET_STATE_PARAMS("compressed", out_params, sizeof(zfp_compress_params_t));

    LOG_INFO("ZFP compression succeeded, compressed size bytes: %zu bytes\n", compressed_size);

    return true;
}

bool
pdc_tf_builtin_zfp_decompress(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                              pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_INFO("pdc_tf_builtin_zfp_decompress was called\n");

    PDCtf_log_pdc_region_t(input_region);

    // Get params
    zfp_compress_params_t *in_params;
    uint64_t               in_params_size;
    GET_STATE_PARAMS("compressed", (void **)&in_params, &in_params_size);

    print_ztype(in_params->z_type);
    size_t compressed_size = input_region.size[0];
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
    PDCtf_copy_tf_region_t(&in_params->decompressed_region, output_region);

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
    LOG_INFO("Expected decompressed size: %zu bytes\n", PDCtf_get_pdc_region_t_bytes(*output_region));

    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);

    return true;
}
#endif // ENABLE_TF_ZFP_COMPRESSION

#ifdef ENABLE_SECRET_BOX_ENCRYPTION

// FIXME: should be picked up by params_str
unsigned char key[crypto_secretbox_KEYBYTES]     = {0};
unsigned char nonce[crypto_secretbox_NONCEBYTES] = {0};

typedef struct encrypt_params_t {
    size_t original_plaintext_size;
} encrypt_params_t;

bool
pdc_tf_builtin_encrypt(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                       pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_INFO("pdc_tf_builtin_encrypt called\n");

    size_t plaintext_len = PDCtf_get_pdc_region_t_bytes(input_region);

    size_t         ciphertext_len = plaintext_len + crypto_secretbox_MACBYTES;
    unsigned char *ciphertext     = malloc(ciphertext_len);
    if (!ciphertext) {
        LOG_ERROR("Failed to allocate ciphertext buffer\n");
        return false;
    }

    if (crypto_secretbox_easy(ciphertext, (unsigned char *)*region_data, plaintext_len, nonce, key) != 0) {
        LOG_ERROR("Encryption failed\n");
        free(ciphertext);
        return false;
    }

    // Output region is 1D bytes (ciphertext)
    output_region->ndim         = 1;
    output_region->pdc_var_type = PDC_CHAR;
    output_region->size[0]      = ciphertext_len;

    // Save original plaintext size in output_params
    encrypt_params_t *out_params = malloc(sizeof(encrypt_params_t));
    if (!out_params) {
        LOG_ERROR("Failed to allocate output params\n");
        free(ciphertext);
        return false;
    }
    out_params->original_plaintext_size = plaintext_len;
    SET_STATE_PARAMS("encrypted", out_params, sizeof(encrypt_params_t));

    // Update data pointer
    *region_data = ciphertext;

    LOG_INFO("Encryption succeeded, ciphertext length: %zu bytes\n", ciphertext_len);
    return true;
}

bool
pdc_tf_builtin_decrypt(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                       pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_INFO("pdc_tf_builtin_decrypt called\n");

    size_t ciphertext_len = PDCtf_get_pdc_region_t_bytes(input_region);

    encrypt_params_t *in_params;
    uint64_t          in_params_size;
    GET_STATE_PARAMS("encrypted", (void **)&in_params, &in_params_size);

    if (ciphertext_len < crypto_secretbox_MACBYTES) {
        LOG_ERROR("Ciphertext too short\n");
        return false;
    }

    size_t plaintext_len = in_params->original_plaintext_size;

    unsigned char *plaintext = malloc(plaintext_len);
    if (!plaintext) {
        LOG_ERROR("Failed to allocate plaintext buffer\n");
        return false;
    }

    if (crypto_secretbox_open_easy(plaintext, (unsigned char *)*region_data, ciphertext_len, nonce, key) !=
        0) {
        LOG_ERROR("Decryption failed or ciphertext tampered\n");
        free(plaintext);
        return false;
    }

    // Set output region dims: restore original plaintext region
    output_region->ndim         = 1;
    output_region->pdc_var_type = PDC_CHAR;
    output_region->size[0]      = plaintext_len;

    // Update data pointer
    *region_data = plaintext;

    LOG_INFO("Decryption succeeded, plaintext length: %zu bytes\n", plaintext_len);
    return true;
}
#endif // NABLE_SECRET_BOX_ENCRYPTION