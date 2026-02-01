#include <stdint.h>

#include "pdc_tf_builtin_common.h"
#include "pdc_tf_common.h"
#include "pdc_tf_helper.h"

#ifdef ENABLE_TF_ZFP_COMPRESSION
#include <zfp.h>
#endif

#ifdef ENABLE_TF_SECRET_BOX_ENCRYPTION
#include <sodium.h>
#endif

#ifdef CUDA_ENABLED
#include <cuda_runtime.h>
#endif

#include "pdc_logger.h"

typedef struct zfp_compress_params_t {
    pdc_tf_region_t decompressed_region;
} zfp_compress_params_t;

typedef struct sz_compress_params_t {
    pdc_tf_region_t decompressed_region;
} sz_compress_params_t;

typedef struct turbo_compress_params_t {
    pdc_tf_region_t decompressed_region;
} turbo_compress_params_t;

static void
print_ztype(zfp_type z_type)
{
    switch (z_type) {
        case zfp_type_int32:
            LOG_DEBUG("ZFP type: int32\n");
            break;
        case zfp_type_int64:
            LOG_DEBUG("ZFP type: int64\n");
            break;
        case zfp_type_float:
            LOG_DEBUG("ZFP type: float\n");
            break;
        case zfp_type_double:
            LOG_DEBUG("ZFP type: double\n");
            break;
        case zfp_type_none:
        default:
            LOG_ERROR("ZFP type: none/unknown (%d)\n", (int)z_type);
            break;
    }
}

#define ENABLE_TF_SZ_GPU_COMPRESSSION
#ifdef ENABLE_TF_SZ_GPU_COMPRESSSION
#include <stdbool.h>
#include <stdint.h>
#include "cusz.h"

#define CUDA_CHECK(call)                                                                                     \
    do {                                                                                                     \
        cudaError_t err = call;                                                                              \
        if (err != cudaSuccess) {                                                                            \
            fprintf(stderr, "[CUDA ERROR] %s:%d: %s (code %d)\n", __FILE__, __LINE__,                        \
                    cudaGetErrorString(err), err);                                                           \
            exit(EXIT_FAILURE);                                                                              \
        }                                                                                                    \
    } while (0)

bool
pdc_tf_builtin_sz_compress_cuda(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                                pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_DEBUG("pdc_tf_builtin_szecompress_cuda called\n");
    PDCtf_log_pdc_region_t(input_region);

    psz_dtype dtype;
    switch (input_region.pdc_var_type) {
        case PDC_FLOAT:
            dtype = F4;
            break;
        case PDC_DOUBLE:
            dtype = F8;
            break;
        case PDC_INT32:
            dtype = F4;
            break;
        case PDC_INT64:
            dtype = F8;
            break;
        default:
            LOG_ERROR("Unsupported element type for SZ: %d\n", input_region.pdc_var_type);
            return false;
    }

    uint64_t decompressed_bytes = PDC_get_region_desc_size_bytes(
        input_region.size, PDC_get_var_type_size(input_region.pdc_var_type), input_region.ndim);

    float *d_data = NULL;
    cudaMalloc((void **)&d_data, decompressed_bytes);
    cudaMemcpy(d_data, *region_data, decompressed_bytes, cudaMemcpyHostToDevice);

    // --- create SZ compressor ---
    psz_len3 len = {1, 1, 1};
    if (input_region.ndim == 1) {
        len.x = input_region.size[0];
    }
    else if (input_region.ndim == 2) {
        len.x = input_region.size[0];
        len.y = input_region.size[1];
    }
    else if (input_region.ndim == 3) {
        len.x = input_region.size[0];
        len.y = input_region.size[1];
        len.z = input_region.size[2];
    }
    else {
        LOG_ERROR("Invalid input region ndim: %d\n", input_region.ndim);
        return false;
    }
    LOG_INFO("1\n");
    // FIXME: for now just hardcode this
    psz_compressor *comp = psz_create_default(dtype, len);

    uint8_t *  d_compressed    = NULL;
    size_t     compressed_size = 0;
    psz_header header;
    void *     record = psz_make_timerecord();
    // Compress
    pszerror err =
        psz_compress(comp, d_data, len, 0.01, Abs, &d_compressed, &compressed_size, &header, record, 0);
    if (err != PSZ_SUCCESS) {
        printf("Compression failed!\n");
        return false;
    }
    printf("Compressed %lu bytes\n", compressed_size);

    // Copy back to host
    *region_data = (uint8_t *)malloc(compressed_size);
    cudaMemcpy(*region_data, d_compressed, compressed_size, cudaMemcpyDeviceToHost);
    // Clean up
    cudaFree(d_data);
    cudaFree(d_compressed);
    psz_release(comp);

    output_region->ndim         = 1;
    output_region->pdc_var_type = PDC_CHAR;
    output_region->size[0]      = compressed_size;

    // Set output params
    sz_compress_params_t *out_params = (sz_compress_params_t *)PDC_malloc(sizeof(sz_compress_params_t));
    PDCtf_copy_tf_region_t(&input_region, &out_params->decompressed_region);
    SET_FUNC_PARAMS("zfp_compress", PDC_TF_GPU_DEVICE, out_params, sizeof(sz_compress_params_t));

    return true;
}

bool
pdc_tf_builtin_sz_decompress_cuda(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                                  pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_DEBUG("pdc_tf_builtin_sz_decompress_cuda called\n");

    // Create compressor from header
    psz_header      header; // You need to save/load this header from compress step
    psz_compressor *comp = psz_create_from_header(&header);
    if (!comp) {
        LOG_ERROR("Failed to create cuSZ decompressor\n");
        return false;
    }

    // Allocate device memory for compressed input
    uint8_t *d_compressed = NULL;
    CUDA_CHECK(cudaMalloc((void **)&d_compressed, input_region.size[0]));
    CUDA_CHECK(cudaMemcpy(d_compressed, *region_data, input_region.size[0], cudaMemcpyHostToDevice));

    // Allocate device memory for decompressed data
    psz_len3 len            = {output_region->size[0], output_region->ndim > 1 ? output_region->size[1] : 1,
                    output_region->ndim > 2 ? output_region->size[2] : 1};
    void *   d_decompressed = NULL;
    size_t   nbytes         = len.x * len.y * len.z * sizeof(float); // assume float, adjust if needed
    CUDA_CHECK(cudaMalloc(&d_decompressed, nbytes));

    void *   record = capi_psz_make_timerecord();
    pszerror p_err = psz_decompress(comp, d_compressed, input_region.size[0], d_decompressed, len, record, 0);
    if (p_err != PSZ_SUCCESS) {
        LOG_ERROR("cuSZ decompression failed\n");
        cudaFree(d_compressed);
        cudaFree(d_decompressed);
        psz_release(comp);
        return false;
    }
    CUDA_CHECK(cudaDeviceSynchronize());

    // Copy decompressed data back to host
    void *h_decompressed = malloc(nbytes);
    CUDA_CHECK(cudaMemcpy(h_decompressed, d_decompressed, nbytes, cudaMemcpyDeviceToHost));

    *region_data = h_decompressed;

    // Clean up
    cudaFree(d_compressed);
    cudaFree(d_decompressed);
    p_err = psz_clear_buffer(comp);
    if (p_err != PSZ_SUCCESS) {
        LOG_ERROR("cuSZ psz_clear_buffer failed\n");
        return false;
    }
    p_err = psz_release(comp);
    if (p_err != PSZ_SUCCESS) {
        LOG_ERROR("cuSZ psz_release failed\n");
        return false;
    }

    LOG_INFO("cuSZ decompression succeeded\n");
    return true;
}
#endif

#ifdef ENABLE_TF_SZ_COMPRESSION
#include "sz3c.h"
bool
pdc_tf_builtin_sz_compress(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                           pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_DEBUG("pdc_tf_builtin_sz_compress was called\n");
    PDCtf_log_pdc_region_t(input_region);

    // Determine SZ3 data type
    int sz_type;
    // Doesn't seem to support any sized ints: int32, int64, uint8, uint32, uint64
    switch (input_region.pdc_var_type) {
        case PDC_FLOAT:
            sz_type = SZ_FLOAT;
            break;
        case PDC_DOUBLE:
            sz_type = SZ_DOUBLE;
            break;
        case PDC_INT:
        case PDC_INT32:
            sz_type = SZ_FLOAT;
            break;
        case PDC_INT64:
            sz_type = SZ_DOUBLE;
            break;
        default:
            LOG_ERROR("Unsupported element type for SZ3: %d\n", input_region.pdc_var_type);
            return false;
    }

    // Set reasonable error bound values
    int    err_bound_mode = ABS;  // Start with absolute error
    double absErrBound    = 0.01; // ±0.01 for floats
    double relBoundRatio  = 0.0;  // not used for ABS
    double pwrBoundRatio  = 0.0;  // not used

    // Fill dimensions (up to 5D)
    size_t r1 = input_region.size[0];
    size_t r2 = (input_region.ndim > 1) ? input_region.size[1] : 1;
    size_t r3 = (input_region.ndim > 2) ? input_region.size[2] : 1;
    size_t r4 = (input_region.ndim > 3) ? input_region.size[3] : 1;
    size_t r5 = (input_region.ndim > 4) ? input_region.size[4] : 1;

    size_t         compressed_size = 0;
    unsigned char *compressed =
        SZ_compress_args(sz_type, *region_data, &compressed_size, err_bound_mode, absErrBound, relBoundRatio,
                         pwrBoundRatio, r5, r4, r3, r2, r1);

    if (!compressed) {
        LOG_ERROR("SZ3 compression failed\n");
        return false;
    }

    *region_data = compressed;

    // Output region is 1D array of bytes
    output_region->ndim         = 1;
    output_region->pdc_var_type = PDC_CHAR;
    output_region->size[0]      = compressed_size;

    sz_compress_params_t *out_params = (sz_compress_params_t *)PDC_malloc(sizeof(sz_compress_params_t));
    PDCtf_copy_tf_region_t(&input_region, &out_params->decompressed_region);
    SET_FUNC_PARAMS("sz_compress", PDC_TF_CPU_DEVICE, out_params, sizeof(sz_compress_params_t));

    LOG_INFO("SZ3 compression succeeded, compressed size: %zu bytes\n", compressed_size);
    return true;
}

bool
pdc_tf_builtin_sz_decompress(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                             pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_DEBUG("pdc_tf_builtin_sz_decompress was called\n");
    PDCtf_log_pdc_region_t(input_region);

    // Get decompression params
    sz_compress_params_t *in_params = NULL;
    uint64_t              in_params_size;
    GET_FUNC_PARAMS("sz_compress", PDC_TF_CPU_DEVICE, (void **)&in_params, &in_params_size);
    PDCtf_log_pdc_region_t(in_params->decompressed_region);

    int sz_type;
    switch (in_params->decompressed_region.pdc_var_type) {
        case PDC_FLOAT:
            sz_type = SZ_FLOAT;
            break;
        case PDC_DOUBLE:
            sz_type = SZ_DOUBLE;
            break;
        case PDC_INT:
        case PDC_INT32:
            sz_type = SZ_FLOAT;
            break;
        case PDC_INT64:
            sz_type = SZ_DOUBLE;
            break;
        default:
            LOG_ERROR("Unsupported element type for SZ3: %d\n",
                      in_params->decompressed_region.pdc_var_type); // <-- use decompressed_region
            return false;
    }

    // Fill dimensions
    size_t r1 = in_params->decompressed_region.size[0];
    size_t r2 = (in_params->decompressed_region.ndim > 1) ? in_params->decompressed_region.size[1] : 1;
    size_t r3 = (in_params->decompressed_region.ndim > 2) ? in_params->decompressed_region.size[2] : 1;
    size_t r4 = (in_params->decompressed_region.ndim > 3) ? in_params->decompressed_region.size[3] : 1;
    size_t r5 = (in_params->decompressed_region.ndim > 4) ? in_params->decompressed_region.size[4] : 1;

    // Decompress
    void *decompressed = SZ_decompress(sz_type, *region_data, input_region.size[0], r5, r4, r3, r2, r1);
    if (!decompressed) {
        LOG_ERROR("SZ3 decompression failed\n");
        return false;
    }

    *region_data = decompressed;

    // Set output region
    PDCtf_copy_tf_region_t(&in_params->decompressed_region, output_region);

    LOG_INFO("SZ3 decompression succeeded\n");
    return true;
}
#endif

#ifdef ENABLE_TF_ZFP_COMPRESSION
#define FIXED_CR_RATIO 1
static bool
pdc_tf_builtin_zfp_compress_helper(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                                   pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_DEBUG("pdc_tf_builtin_zfp_compress was called\n");
    LOG_DEBUG("Input region (zfp_compress):\n");
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
        case PDC_INT:
        case PDC_INT32:
            z_type = zfp_type_int32;
            break;
        case PDC_INT64:
            z_type = zfp_type_int64;
            break;
        default:
            LOG_ERROR("Invalid element type\n");
            return false;
    }
    print_ztype(z_type);
    size_t bits_per_value = (8 * PDC_get_var_type_size(input_region.pdc_var_type)) / FIXED_CR_RATIO;
    LOG_INFO("BITS PER VALUE %d\n", bits_per_value);

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
    size_t bufsize = zfp_stream_maximum_size(zfp, field);
    LOG_DEBUG("Max compressed size: %zu bytes\n", bufsize);
    void *compressed_buffer = PDC_malloc(bufsize);

    // Create bitstream backed by the compressed buffer
    bitstream *stream = stream_open(compressed_buffer, bufsize);
    zfp_stream_set_rate(zfp, bits_per_value, z_type, input_region.ndim, 1);
    // zfp_stream_set_reversible(zfp);
    // zfp_stream_set_accuracy(zfp, 1e-3);
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    // Perform compression
    size_t compressed_size = zfp_compress(zfp, field);

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
    LOG_DEBUG("pdc_tf_builtin_zfp_decompress was called\n");
    LOG_DEBUG("Input region (zfp_decompress):\n");
    PDCtf_log_pdc_region_t(input_region);

    // Get params
    zfp_compress_params_t *in_params = NULL;
    uint64_t               in_params_size;
    GET_FUNC_PARAMS("zfp_compress", PDC_TF_CPU_DEVICE, (void **)&in_params, &in_params_size);
    LOG_DEBUG("Input region (zfp_decompress):\n");
    PDCtf_log_pdc_region_t(in_params->decompressed_region);
    // set datatype based on params
    zfp_type z_type;
    switch (in_params->decompressed_region.pdc_var_type) {
        case PDC_FLOAT:
            z_type = zfp_type_float;
            break;
        case PDC_DOUBLE:
            z_type = zfp_type_double;
            break;
        case PDC_INT:
        case PDC_INT32:
            z_type = zfp_type_int32;
            break;
        case PDC_INT64:
            z_type = zfp_type_int64;
            break;
        default:
            LOG_ERROR("Invalid element type %d\n", in_params->decompressed_region.pdc_var_type);
            return false;
    }
    print_ztype(z_type);
    size_t bits_per_value =
        (8 * PDC_get_var_type_size(in_params->decompressed_region.pdc_var_type)) / FIXED_CR_RATIO;
    LOG_INFO("BITS PER VALUE %d\n", bits_per_value);
    size_t compressed_size = input_region.size[0];
    LOG_DEBUG("Compressed size: %zu bytes\n", compressed_size);

    // Create bitstream from compressed data
    bitstream *stream = stream_open(*region_data, compressed_size);

    // Create zfp stream and associate it with bitstream
    zfp_stream *zfp = zfp_stream_open(NULL);
    zfp_stream_set_rate(zfp, bits_per_value, z_type, input_region.ndim, 1);
    // zfp_stream_set_reversible(zfp);
    // zfp_stream_set_accuracy(zfp, 1e-3);
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    // Allocate uncompressed buffer
    size_t total_bytes = PDCtf_get_pdc_region_t_bytes(in_params->decompressed_region);
    LOG_DEBUG("Decompressed region %zu bytes\n", total_bytes);
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
    LOG_DEBUG("Scalar size: %d\n", elem_size);
    LOG_DEBUG("Actual decompressed size: %zu bytes\n", decompressed_bytes);
    LOG_DEBUG("Expected decompressed size: %zu bytes\n", PDCtf_get_pdc_region_t_bytes(*output_region));

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
    LOG_DEBUG("pdc_tf_builtin_zfp_compress_cuda was called\n");
    LOG_DEBUG("Input region (zfp_compress_cuda):\n");
    PDCtf_log_pdc_region_t(input_region);

    // Determine type
    zfp_type z_type;
    switch (input_region.pdc_var_type) {
        case PDC_FLOAT:
            z_type = zfp_type_float;
            break;
        case PDC_DOUBLE:
            z_type = zfp_type_double;
            break;
        case PDC_INT:
        case PDC_INT32:
            z_type = zfp_type_int32;
            break;
        case PDC_INT64:
            z_type = zfp_type_int64;
            break;
        default:
            LOG_ERROR("Invalid element type\n");
            return false;
    }
    size_t bits_per_value = (8 * PDC_get_var_type_size(input_region.pdc_var_type)) / FIXED_CR_RATIO;
    print_ztype(z_type);

    size_t num_bytes = PDCtf_get_pdc_region_t_bytes(input_region);

    // Allocate device memory for input and output
    void *dev_in  = NULL;
    void *dev_out = NULL;
    CUDA_CHECK(cudaMalloc(&dev_in, num_bytes));
    CUDA_CHECK(cudaMemcpy(dev_in, *region_data, num_bytes, cudaMemcpyHostToDevice));

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
    zfp_stream_set_rate(zfp, bits_per_value, z_type, input_region.ndim, 1);
    zfp_stream_set_execution(zfp, zfp_exec_cuda);
    zfp_stream_rewind(zfp);

    size_t compressed_size = zfp_compress(zfp, field);
    CUDA_CHECK(cudaDeviceSynchronize());
    assert(compressed_size > 0);

    // Copy compressed data back to host
    void *host_compressed = malloc(compressed_size);
    CUDA_CHECK(cudaMemcpy(host_compressed, dev_out, compressed_size, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaFree(dev_in));
    CUDA_CHECK(cudaFree(dev_out));

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

    LOG_DEBUG("CUDA ZFP compression succeeded, %zu bytes\n", compressed_size);
    return true;
}

static bool
pdc_tf_builtin_zfp_decompress_cuda_helper(pdc_tf_internal_param internal_param, char *params_str,
                                          void **region_data, pdc_tf_region_t input_region,
                                          pdc_tf_region_t *output_region)
{
    LOG_DEBUG("pdc_tf_builtin_zfp_decompress_cuda_safe called\n");
    PDCtf_log_pdc_region_t(input_region);

    // Get compression params
    zfp_compress_params_t *in_params      = NULL;
    uint64_t               in_params_size = 0;
    GET_FUNC_PARAMS("zfp_compress", PDC_TF_GPU_DEVICE, (void **)&in_params, &in_params_size);
    if (!in_params) {
        LOG_ERROR("Failed to get ZFP compression parameters.\n");
        return false;
    }

    // Determine type
    zfp_type z_type;
    size_t   type_size = 0;
    switch (in_params->decompressed_region.pdc_var_type) {
        case PDC_FLOAT:
            z_type    = zfp_type_float;
            type_size = sizeof(float);
            break;
        case PDC_DOUBLE:
            z_type    = zfp_type_double;
            type_size = sizeof(double);
            break;
        case PDC_INT:
        case PDC_INT32:
            z_type    = zfp_type_int32;
            type_size = sizeof(int32_t);
            break;
        case PDC_INT64:
            z_type    = zfp_type_int64;
            type_size = sizeof(int64_t);
            break;
        default:
            LOG_ERROR("Unsupported element type %d\n", in_params->decompressed_region.pdc_var_type);
            return false;
    }
    size_t bits_per_value =
        (8 * PDC_get_var_type_size(in_params->decompressed_region.pdc_var_type)) / FIXED_CR_RATIO;

    // Compute expected uncompressed size
    size_t total_elements = 1;
    for (int i = 0; i < in_params->decompressed_region.ndim; i++)
        total_elements *= in_params->decompressed_region.size[i];

    size_t uncompressed_size = total_elements * type_size;
    LOG_DEBUG("Uncompressed size: %zu bytes\n", uncompressed_size);

    size_t compressed_size = input_region.size[0]; // already in bytes
    LOG_DEBUG("Compressed size: %zu bytes\n", compressed_size);

    // Allocate device buffers safely
    void *dev_compressed   = NULL;
    void *dev_uncompressed = NULL;
    CUDA_CHECK(cudaMalloc(&dev_compressed, compressed_size + 64));
    CUDA_CHECK(cudaMalloc(&dev_uncompressed, uncompressed_size));
    CUDA_CHECK(cudaMemcpy(dev_compressed, *region_data, compressed_size, cudaMemcpyHostToDevice));

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
            CUDA_CHECK(cudaFree(dev_compressed));
            CUDA_CHECK(cudaFree(dev_uncompressed));
            return false;
    }

    // Setup zfp stream
    zfp_stream *zfp = zfp_stream_open(NULL);
    if (!zfp) {
        LOG_ERROR("Failed to open zfp stream\n");
        CUDA_CHECK(cudaFree(dev_compressed));
        CUDA_CHECK(cudaFree(dev_uncompressed));
        zfp_field_free(field);
        return false;
    }

    bitstream *stream = stream_open(dev_compressed, compressed_size);
    if (!stream) {
        LOG_ERROR("Failed to open bitstream\n");
        zfp_stream_close(zfp);
        zfp_field_free(field);
        CUDA_CHECK(cudaFree(dev_compressed));
        CUDA_CHECK(cudaFree(dev_uncompressed));
        return false;
    }

    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_set_rate(zfp, bits_per_value, z_type, input_region.ndim, 1);
    zfp_stream_set_execution(zfp, zfp_exec_cuda);
    zfp_stream_rewind(zfp);

    // Decompress
    size_t decompressed_size = zfp_decompress(zfp, field);
    CUDA_CHECK(cudaDeviceSynchronize());
    if (decompressed_size == 0) {
        LOG_ERROR("ZFP decompression failed\n");
        stream_close(stream);
        zfp_stream_close(zfp);
        zfp_field_free(field);
        CUDA_CHECK(cudaFree(dev_compressed));
        CUDA_CHECK(cudaFree(dev_uncompressed));
        return false;
    }

    // Copy data back to host
    void *host_buf = malloc(uncompressed_size);
    if (!host_buf) {
        LOG_ERROR("Failed to allocate host buffer\n");
        stream_close(stream);
        zfp_stream_close(zfp);
        zfp_field_free(field);
        CUDA_CHECK(cudaFree(dev_compressed));
        CUDA_CHECK(cudaFree(dev_uncompressed));
        return false;
    }

    CUDA_CHECK(cudaMemcpy(host_buf, dev_uncompressed, uncompressed_size, cudaMemcpyDeviceToHost));

    // Free device memory
    CUDA_CHECK(cudaFree(dev_compressed));
    CUDA_CHECK(cudaFree(dev_uncompressed));

    // Update region data and output
    *region_data = host_buf;
    PDCtf_copy_tf_region_t(&in_params->decompressed_region, output_region);

    // Cleanup ZFP
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);

    LOG_DEBUG("CUDA ZFP decompression succeeded: %zu bytes\n", decompressed_size);
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

#ifdef ENABLE_TF_SECRET_BOX_ENCRYPTION

// FIXME: should be picked up by params_str
unsigned char key[crypto_secretbox_KEYBYTES]     = {0};
unsigned char nonce[crypto_secretbox_NONCEBYTES] = {0};

typedef struct encrypt_params_t {
    pdc_tf_region_t unencrypted_region;
} encrypt_params_t;

bool
pdc_tf_builtin_encrypt(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                       pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_DEBUG("pdc_tf_builtin_encrypt called\n");
    LOG_DEBUG("Input region (encrypt):\n");
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
    encrypt_params_t *out_params                = (encrypt_params_t *)malloc(sizeof(encrypt_params_t));
    out_params->unencrypted_region.pdc_var_type = input_region.pdc_var_type;
    PDCtf_copy_tf_region_t(&input_region, &out_params->unencrypted_region);
    SET_FUNC_PARAMS("secret_box_encrypt", PDC_TF_CPU_DEVICE, out_params, sizeof(encrypt_params_t));

    // Update data pointer
    *region_data = ciphertext;

    LOG_DEBUG("Encryption succeeded, ciphertext length: %zu bytes\n", ciphertext_len);
    return true;
}

bool
pdc_tf_builtin_decrypt(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                       pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_DEBUG("pdc_tf_builtin_decrypt called\n");
    LOG_DEBUG("Input region (decrypt):\n");
    PDCtf_log_pdc_region_t(input_region);

    size_t ciphertext_len = PDCtf_get_pdc_region_t_bytes(input_region);

    encrypt_params_t *in_params;
    uint64_t          in_params_size;
    GET_FUNC_PARAMS("secret_box_encrypt", PDC_TF_CPU_DEVICE, (void **)&in_params, &in_params_size);

    if (ciphertext_len < crypto_secretbox_MACBYTES) {
        LOG_ERROR("Ciphertext too short\n");
        return false;
    }

    size_t plaintext_len = PDC_get_region_desc_size_bytes(
        in_params->unencrypted_region.size, PDC_get_var_type_size(in_params->unencrypted_region.pdc_var_type),
        in_params->unencrypted_region.ndim);

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
    PDCtf_copy_tf_region_t(&in_params->unencrypted_region, output_region);

    // Update data pointer
    *region_data = plaintext;

    LOG_DEBUG("Decryption succeeded, plaintext length: %zu bytes\n", plaintext_len);
    return true;
}
#endif // ENABLE_TF_SECRET_BOX_ENCRYPTION

#ifdef ENABLE_TF_TURBO_COMPRESSION
#include <ic.h>

bool
pdc_tf_builtin_turbo_compress(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                              pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_DEBUG("pdc_tf_builtin_turbo compress called\n");
    LOG_DEBUG("Input region (compress):\n");
    PDCtf_log_pdc_region_t(input_region);

    size_t size       = PDCtf_get_pdc_region_t_bytes(input_region);
    void * compressed = malloc(size);
    size_t ret;

    switch (input_region.pdc_var_type) {
        case PDC_INT:
        case PDC_INT32:
            ret = p4nenc32((uint32_t *)*region_data, PDCtf_get_pdc_region_t_elements(input_region),
                           (unsigned char *)compressed);
            break;
        case PDC_INT64:
            ret = p4nenc64((uint64_t *)*region_data, PDCtf_get_pdc_region_t_elements(input_region),
                           (unsigned char *)compressed);
            break;
        default:
            LOG_ERROR("Invalid element type\n");
            return false;
    }

    output_region->ndim         = 1;
    output_region->pdc_var_type = PDC_CHAR;
    output_region->size[0]      = ret;
    *region_data                = compressed;

    turbo_compress_params_t *out_params =
        (turbo_compress_params_t *)PDC_malloc(sizeof(turbo_compress_params_t));
    PDCtf_copy_tf_region_t(&input_region, &out_params->decompressed_region);
    SET_FUNC_PARAMS("turbo_compress", PDC_TF_CPU_DEVICE, out_params, sizeof(turbo_compress_params_t));

    LOG_DEBUG("Turbo compression succeeded: %zu bytes\n", ret);

    return true;
}

bool
pdc_tf_builtin_turbo_decompress(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                                pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    LOG_DEBUG("pdc_tf_builtin_turbo_decompress called\n");
    LOG_DEBUG("Input region (decompress):\n");
    PDCtf_log_pdc_region_t(input_region);

    turbo_compress_params_t *in_params      = NULL;
    uint64_t                 in_params_size = 0;
    GET_FUNC_PARAMS("turbo_compress", PDC_TF_CPU_DEVICE, (void **)&in_params, &in_params_size);

    size_t size         = PDCtf_get_pdc_region_t_bytes(in_params->decompressed_region);
    void * decompressed = malloc(size);
    size_t ret;

    switch (in_params->decompressed_region.pdc_var_type) {
        case PDC_INT:
        case PDC_INT32:
            ret = p4ndec32((unsigned char *)*region_data,
                           PDCtf_get_pdc_region_t_elements(in_params->decompressed_region),
                           (uint32_t *)decompressed);
            break;
        case PDC_INT64:
            ret = p4ndec64((unsigned char *)*region_data,
                           PDCtf_get_pdc_region_t_elements(in_params->decompressed_region),
                           (uint64_t *)decompressed);
            break;
        default:
            LOG_ERROR("Invalid element type\n");
            return false;
    }

    *region_data = decompressed;
    PDCtf_copy_tf_region_t(&in_params->decompressed_region, output_region);

    LOG_DEBUG("Turbo decompression succeeded: %zu bytes\n", ret);

    return true;
}
#endif