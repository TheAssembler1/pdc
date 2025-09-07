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

#ifdef CUDA_ENABLED
#include <cuda_runtime.h>
#endif

#include "pdc_logger.h"

typedef struct zfp_compress_params_t {
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
static bool
pdc_tf_builtin_zfp_compress_helper(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                                   pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_INFO("pdc_tf_builtin_zfp_compress was called\n");
    LOG_INFO("Input region (zfp_compress):\n");
    PDCtf_log_pdc_region_t(input_region);

    // set datatype based on params
    zfp_type z_type;
    uint8_t  bits_per_value = 0;
    switch (input_region.pdc_var_type) {
        case PDC_FLOAT:
            z_type         = zfp_type_float;
            bits_per_value = 16; // ~2:1 from 32
            break;
        case PDC_DOUBLE:
            z_type         = zfp_type_double;
            bits_per_value = 32; // ~2:1 from 64
            break;
        case PDC_INT:
        case PDC_INT32:
            z_type         = zfp_type_int32;
            bits_per_value = 16; // ~2:1 from 32
            break;
        case PDC_INT64:
            z_type         = zfp_type_int64;
            bits_per_value = 32; // ~2:1 from 64
            break;
        default:
            LOG_ERROR("Invalid element type\n");
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

    // Allocate buffer for compressed data
    size_t bufsize           = zfp_stream_maximum_size(zfp, field);
    LOG_INFO("Max compressed size: %zu bytes\n", bufsize);
    void * compressed_buffer = PDC_malloc(bufsize);

    // Create bitstream backed by the compressed buffer
    bitstream *stream = stream_open(compressed_buffer, bufsize);

    // FIXME: get compression config from param str
    zfp_stream_set_reversible(zfp);
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    // Perform compression
    size_t compressed_size = zfp_compress(zfp, field);

    // Update region_data to point to compressed buffer
    *region_data = compressed_buffer;

    // Update output region dims to reflect compressed data size (1D)
    output_region->ndim         = 1;
    output_region->pdc_var_type = PDC_CHAR;
    output_region->size[0] = compressed_size;

    // Free zfp structures
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);

    // Set output params
    zfp_compress_params_t *out_params = (zfp_compress_params_t *)PDC_malloc(sizeof(zfp_compress_params_t));
    PDCtf_copy_tf_region_t(&input_region, &out_params->decompressed_region);
    SET_FUNC_PARAMS("zfp_compress", PDC_TF_CPU_DEVICE, out_params, sizeof(zfp_compress_params_t));

    LOG_INFO("ZFP compression succeeded, compressed size bytes: %zu bytes\n", compressed_size);

    return true;
}

static bool
pdc_tf_builtin_zfp_decompress_helper(pdc_tf_internal_param internal_param, char *params_str,
                                     void **region_data, pdc_tf_region_t input_region,
                                     pdc_tf_region_t *output_region)
{
    LOG_INFO("pdc_tf_builtin_zfp_decompress was called\n");
    LOG_INFO("Input region (zfp_decompress):\n");
    PDCtf_log_pdc_region_t(input_region);

    // Get params
    zfp_compress_params_t *in_params = NULL;
    uint64_t               in_params_size;
    GET_FUNC_PARAMS("zfp_compress", PDC_TF_CPU_DEVICE, (void **)&in_params, &in_params_size);
    LOG_INFO("Input region (zfp_decompress):\n");
    PDCtf_log_pdc_region_t(in_params->decompressed_region);
    // set datatype based on params
    zfp_type z_type;
    uint8_t  bits_per_value = 0;
    switch (in_params->decompressed_region.pdc_var_type) {
        case PDC_FLOAT:
            z_type         = zfp_type_float;
            bits_per_value = 16; // ~2:1 from 32
            break;
        case PDC_DOUBLE:
            z_type         = zfp_type_double;
            bits_per_value = 32; // ~2:1 from 64
            break;
        case PDC_INT:
        case PDC_INT32:
            z_type         = zfp_type_int32;
            bits_per_value = 16; // ~2:1 from 32
            break;
        case PDC_INT64:
            z_type         = zfp_type_int64;
            bits_per_value = 32; // ~2:1 from 64
            break;
        default:
            LOG_ERROR("Invalid element type %d\n", in_params->decompressed_region.pdc_var_type);
            return false;
    }
    print_ztype(z_type);
    size_t compressed_size = input_region.size[0];
    LOG_INFO("Compressed size: %zu bytes\n", compressed_size);

    // Create bitstream from compressed data
    bitstream *stream = stream_open(*region_data, compressed_size);

    // Create zfp stream and associate it with bitstream
    zfp_stream *zfp = zfp_stream_open(NULL);

    // FIXME: get compression config from param str
    zfp_stream_set_reversible(zfp);
    // zfp_stream_set_reversible(zfp);
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
            field = zfp_field_1d(buf, z_type, in_params->decompressed_region.size[0]);
            break;
        case 2:
            field = zfp_field_2d(buf, z_type, in_params->decompressed_region.size[0],
                                 in_params->decompressed_region.size[1]);
            break;
        case 3:
            field =
                zfp_field_3d(buf, z_type, in_params->decompressed_region.size[0],
                             in_params->decompressed_region.size[1], in_params->decompressed_region.size[2]);
            break;
        case 4:
            field = zfp_field_4d(
                buf, z_type, in_params->decompressed_region.size[0], in_params->decompressed_region.size[1],
                in_params->decompressed_region.size[2], in_params->decompressed_region.size[3]);
            break;
        default:
            LOG_ERROR("Unsupported ndim: %d\n", in_params->decompressed_region.ndim);
            return false;
    }

    // Decompress
    size_t decompressed_size = zfp_decompress(zfp, field);

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

#ifdef CUDA_ENABLED
static bool
pdc_tf_builtin_zfp_compress_cuda_helper(pdc_tf_internal_param internal_param, char *params_str,
                                        void **region_data, pdc_tf_region_t input_region,
                                        pdc_tf_region_t *output_region)
{
    LOG_INFO("pdc_tf_builtin_zfp_compress_cuda was called\n");
    LOG_INFO("Input region (zfp_compress_cuda):\n");
    PDCtf_log_pdc_region_t(input_region);

    // Determine type
    zfp_type z_type;
    uint8_t  bits_per_value = 0;
    switch (input_region.pdc_var_type) {
        case PDC_FLOAT:
            z_type         = zfp_type_float;
            bits_per_value = 16;
            break;
        case PDC_DOUBLE:
            z_type         = zfp_type_double;
            bits_per_value = 32;
            break;
        case PDC_INT:
        case PDC_INT32:
            z_type         = zfp_type_int32;
            bits_per_value = 16;
            break;
        case PDC_INT64:
            z_type         = zfp_type_int64;
            bits_per_value = 32;
            break;
        default:
            LOG_ERROR("Invalid element type\n");
            return false;
    }
    print_ztype(z_type);

    size_t num_bytes = PDCtf_get_pdc_region_t_bytes(input_region);

    // Allocate device memory for input and output
    void *dev_in  = NULL;
    void *dev_out = NULL;
    cudaMalloc(&dev_in, num_bytes);
    cudaMemcpy(dev_in, *region_data, num_bytes, cudaMemcpyHostToDevice);

    // Create zfp field on device
    zfp_field *field = NULL;
    switch (input_region.ndim) {
        case 1:
            field = zfp_field_1d(dev_in, z_type, input_region.size[0]);
            break;
        case 2:
            field = zfp_field_2d(dev_in, z_type, input_region.size[0], input_region.size[1]);
            break;
        case 3:
            field = zfp_field_3d(dev_in, z_type, input_region.size[0], input_region.size[1],
                                 input_region.size[2]);
            break;
        case 4:
            field = zfp_field_4d(dev_in, z_type, input_region.size[0], input_region.size[1],
                                 input_region.size[2], input_region.size[3]);
            break;
        default:
            LOG_ERROR("Unsupported ndim: %d\n", input_region.ndim);
            cudaFree(dev_in);
            return false;
    }

    zfp_stream *zfp     = zfp_stream_open(NULL);
    size_t      bufsize = zfp_stream_maximum_size(zfp, field);
    cudaMalloc(&dev_out, bufsize);

    bitstream *stream = stream_open(dev_out, bufsize);
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_set_rate(zfp, (double)bits_per_value, z_type, input_region.ndim, 0);
    zfp_stream_set_execution(zfp, zfp_exec_cuda);
    zfp_stream_rewind(zfp);

    size_t compressed_size = zfp_compress(zfp, field);
    assert(compressed_size > 0);

    // Copy compressed data back to host
    void *host_compressed = malloc(compressed_size);
    cudaMemcpy(host_compressed, dev_out, compressed_size, cudaMemcpyDeviceToHost);

    // Cleanup device
    cudaFree(dev_in);
    cudaFree(dev_out);

    // Update output region
    *region_data                = host_compressed;
    output_region->ndim         = 1;
    output_region->pdc_var_type = PDC_CHAR;
    output_region->size[0]      = compressed_size;

    // Free zfp objects
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);

    // Store params
    zfp_compress_params_t *out_params = (zfp_compress_params_t *)malloc(sizeof(zfp_compress_params_t));
    PDCtf_copy_tf_region_t(&input_region, &out_params->decompressed_region);
    SET_FUNC_PARAMS("zfp_compress", PDC_TF_GPU_DEVICE, out_params, sizeof(zfp_compress_params_t));

    LOG_INFO("CUDA ZFP compression succeeded, %zu bytes\n", compressed_size);
    return true;
}

static bool
pdc_tf_builtin_zfp_decompress_cuda_helper(pdc_tf_internal_param internal_param, char *params_str,
                                          void **region_data, pdc_tf_region_t input_region,
                                          pdc_tf_region_t *output_region)
{
    LOG_INFO("pdc_tf_builtin_zfp_decompress_cuda was called\n");
    LOG_INFO("Input region (zfp_decompress_cuda):\n");
    PDCtf_log_pdc_region_t(input_region);

    // Get params
    zfp_compress_params_t *in_params;
    uint64_t               in_params_size;
    GET_FUNC_PARAMS("zfp_compress", PDC_TF_GPU_DEVICE, (void **)&in_params, &in_params_size);

    // Determine type
    zfp_type z_type;
    uint8_t  bits_per_value = 0;
    switch (in_params->decompressed_region.pdc_var_type) {
        case PDC_FLOAT:
            z_type         = zfp_type_float;
            bits_per_value = 16;
            break;
        case PDC_DOUBLE:
            z_type         = zfp_type_double;
            bits_per_value = 32;
            break;
        case PDC_INT:
        case PDC_INT32:
            z_type         = zfp_type_int32;
            bits_per_value = 16;
            break;
        case PDC_INT64:
            z_type         = zfp_type_int64;
            bits_per_value = 32;
            break;
        default:
            LOG_ERROR("Invalid element type\n");
            return false;
    }
    print_ztype(z_type);

    size_t compressed_size   = input_region.size[0];
    size_t uncompressed_size = PDCtf_get_pdc_region_t_bytes(in_params->decompressed_region);

    // Allocate device buffers
    void *dev_compressed   = NULL;
    void *dev_uncompressed = NULL;
    cudaMalloc(&dev_compressed, compressed_size);
    cudaMalloc(&dev_uncompressed, uncompressed_size);
    cudaMemcpy(dev_compressed, *region_data, compressed_size, cudaMemcpyHostToDevice);

    // Create zfp field on device
    zfp_field *field  = NULL;
    void *     target = dev_uncompressed;
    switch (in_params->decompressed_region.ndim) {
        case 1:
            field = zfp_field_1d(target, z_type, in_params->decompressed_region.size[0]);
            break;
        case 2:
            field = zfp_field_2d(target, z_type, in_params->decompressed_region.size[0],
                                 in_params->decompressed_region.size[1]);
            break;
        case 3:
            field =
                zfp_field_3d(target, z_type, in_params->decompressed_region.size[0],
                             in_params->decompressed_region.size[1], in_params->decompressed_region.size[2]);
            break;
        case 4:
            field =
                zfp_field_4d(target, z_type, in_params->decompressed_region.size[0],
                             in_params->decompressed_region.size[1], in_params->decompressed_region.size[2],
                             in_params->decompressed_region.size[3]);
            break;
        default:
            LOG_ERROR("Unsupported ndim: %d\n", in_params->decompressed_region.ndim);
            cudaFree(dev_compressed);
            cudaFree(dev_uncompressed);
            return false;
    }

    zfp_stream *zfp    = zfp_stream_open(NULL);
    bitstream * stream = stream_open(dev_compressed, compressed_size);
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_set_rate(zfp, bits_per_value, z_type, in_params->decompressed_region.ndim, 0);
    zfp_stream_set_execution(zfp, zfp_exec_cuda);
    zfp_stream_rewind(zfp);

    size_t decompressed_size = zfp_decompress(zfp, field);
    assert(decompressed_size > 0);

    // Copy decompressed data back to host
    void *host_buf = malloc(uncompressed_size);
    cudaMemcpy(host_buf, dev_uncompressed, uncompressed_size, cudaMemcpyDeviceToHost);

    // Cleanup device
    cudaFree(dev_compressed);
    cudaFree(dev_uncompressed);

    // Update region_data and output region
    *region_data = host_buf;
    PDCtf_copy_tf_region_t(&in_params->decompressed_region, output_region);

    // Cleanup zfp
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);

    LOG_INFO("CUDA ZFP decompression succeeded, %zu bytes\n", decompressed_size);
    return true;
}
#endif // CUDA_ENABLED

bool
pdc_tf_builtin_zfp_compress(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                            pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    return pdc_tf_builtin_zfp_compress_helper(internal_param, params_str, region_data, input_region,
                                              output_region);
}
bool
pdc_tf_builtin_zfp_decompress(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                              pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    return pdc_tf_builtin_zfp_decompress_helper(internal_param, params_str, region_data, input_region,
                                                output_region);
}
#ifdef CUDA_ENABLED
bool
pdc_tf_builtin_zfp_compress_cuda(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                                 pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    return pdc_tf_builtin_zfp_compress_cuda_helper(internal_param, params_str, region_data, input_region,
                                                   output_region);
}
bool
pdc_tf_builtin_zfp_decompress_cuda(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                                   pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    return pdc_tf_builtin_zfp_decompress_cuda_helper(internal_param, params_str, region_data, input_region,
                                                     output_region);
}
#endif // CUDA_ENABLED
#endif // ENABLE_TF_ZFP_COMPRESSION

#ifdef ENABLE_SECRET_BOX_ENCRYPTION

// FIXME: should be picked up by params_str
unsigned char key[crypto_secretbox_KEYBYTES]     = {0};
unsigned char nonce[crypto_secretbox_NONCEBYTES] = {0};

typedef struct encrypt_params_t {
    pdc_tf_region_t decompressed_region;
} encrypt_params_t;

bool
pdc_tf_builtin_encrypt(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                       pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_INFO("pdc_tf_builtin_encrypt called\n");
    LOG_INFO("Input region (encrypt):\n");
    PDCtf_log_pdc_region_t(input_region);

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

    // Save original region size in output_params
    encrypt_params_t *out_params                 = (encrypt_params_t *)malloc(sizeof(encrypt_params_t));
    out_params->decompressed_region.pdc_var_type = input_region.pdc_var_type;
    PDCtf_copy_tf_region_t(&input_region, &out_params->decompressed_region);
    SET_FUNC_PARAMS("secret_box_encrypt", PDC_TF_CPU_DEVICE, out_params, sizeof(zfp_compress_params_t));

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
    LOG_INFO("Input region (decrypt):\n");
    PDCtf_log_pdc_region_t(input_region);

    size_t ciphertext_len = PDCtf_get_pdc_region_t_bytes(input_region);

    encrypt_params_t *in_params;
    uint64_t          in_params_size;
    GET_FUNC_PARAMS("secret_box_encrypt", PDC_TF_CPU_DEVICE, (void **)&in_params, &in_params_size);

    if (ciphertext_len < crypto_secretbox_MACBYTES) {
        LOG_ERROR("Ciphertext too short\n");
        return false;
    }

    size_t plaintext_len =
        PDC_get_region_desc_size_bytes(in_params->decompressed_region.size,
                                       PDC_get_var_type_size(in_params->decompressed_region.pdc_var_type),
                                       in_params->decompressed_region.ndim);

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
    PDCtf_copy_tf_region_t(&in_params->decompressed_region, output_region);

    // Update data pointer
    *region_data = plaintext;

    LOG_INFO("Decryption succeeded, plaintext length: %zu bytes\n", plaintext_len);
    return true;
}
#endif // ENABLE_SECRET_BOX_ENCRYPTION