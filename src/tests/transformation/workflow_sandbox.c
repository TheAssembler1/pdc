/**
 * Description: Sandbox for testing transformations
 */

#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include "pdc.h"
#include "test_helper.h"

#define NUM_DIMS              1
#define NUM_PARTICLES_PER_DIM 4096
#define TYPE                  PDC_FLOAT

#define REG_1_INIT_VAL  3.14f
#define REG_1_FINAL_VAL 3.14f
#define REG_2_INIT_VAL  6.28f
#define REG_2_FINAL_VAL 6.28f

static void
set_buf(float *buf, float val, uint64_t num)
{
    for (uint64_t i = 0; i < num; i++)
        buf[i] = val;
}

static int
workflow1(pdcid_t pdc, pdcid_t cont)
{
    int      ret_value = TSUCCEED;
    pdcid_t  obj_prop = 0, obj = 0;
    pdcid_t  reg1 = 0, reg2 = 0, reg_global1 = 0, reg_global2 = 0;
    uint64_t total_particles = 1;
    uint64_t dims[NUM_DIMS];
    uint64_t region_dims[NUM_DIMS];
    uint64_t offset1[NUM_DIMS];
    uint64_t offset2[NUM_DIMS];

    // Calculate total particles
    for (int i = 0; i < NUM_DIMS; i++)
        total_particles *= NUM_PARTICLES_PER_DIM;

    // Allocate buffers for two regions (half each)
    uint64_t half_particles = total_particles / 2;
    float   *data1          = (float *)malloc(sizeof(float) * half_particles);
    float   *data2          = (float *)malloc(sizeof(float) * half_particles);
    if (!data1 || !data2) {
        fprintf(stderr, "Failed to allocate data buffers\n");
        ret_value = TFAIL;
        goto cleanup;
    }

    // Initialize buffers with distinct values
    set_buf(data1, REG_1_INIT_VAL, half_particles);
    set_buf(data2, REG_2_INIT_VAL, half_particles);

    // Create object property and set type and full dims
    obj_prop = PDCprop_create(PDC_OBJ_CREATE, pdc);
    if (obj_prop == 0) {
        fprintf(stderr, "Failed to create object property\n");
        ret_value = TFAIL;
        goto cleanup;
    }
    dims[0] = NUM_PARTICLES_PER_DIM;
    if (PDCprop_set_obj_type(obj_prop, TYPE) < 0 || PDCprop_set_obj_dims(obj_prop, NUM_DIMS, dims) < 0) {
        fprintf(stderr, "Failed to set object property attributes\n");
        ret_value = TFAIL;
        goto cleanup;
    }

    // Create the object
    obj = PDCobj_create(cont, "obj_consecutive_regions", obj_prop);
    if (obj == 0) {
        fprintf(stderr, "Failed to create object\n");
        ret_value = TFAIL;
        goto cleanup;
    }

    // Define first region offset and size (first half)
    offset1[0]     = 0;
    region_dims[0] = NUM_PARTICLES_PER_DIM / 2;

    // Define second region offset and size (second half)
    offset2[0] = NUM_PARTICLES_PER_DIM / 2;

    // Create local and global regions for both
    reg1        = PDCregion_create(NUM_DIMS, offset1, region_dims);
    reg_global1 = PDCregion_create(NUM_DIMS, offset1, region_dims);
    reg2        = PDCregion_create(NUM_DIMS, offset1, region_dims);
    reg_global2 = PDCregion_create(NUM_DIMS, offset2, region_dims);
    if (!reg1 || !reg_global1 || !reg2 || !reg_global2) {
        fprintf(stderr, "Failed to create regions\n");
        ret_value = TFAIL;
        goto cleanup;
    }

    pdcid_t dg_id = PDCtf_open_dg_json("/home/ta1/src/workspace/source/pdc/tf_graphs/test.json");
    PDCtf_print_dg(dg_id, true);
    PDCtf_print_exec_path(dg_id, "decompressed_floats", "compressed_floats");
    PDCtf_print_exec_path(dg_id, "compressed_floats", "decompressed_floats");

    PDCtf_attach_to_region(dg_id, obj, reg_global1, "decompressed_floats", "compressed_floats");
    PDCtf_attach_to_region(dg_id, obj, reg_global2, "decompressed_floats", "compressed_floats");

    // Write first region
    pdcid_t transfer_id = PDCregion_transfer_create(data1, PDC_WRITE, obj, reg1, reg_global1);
    if (transfer_id == 0 || PDCregion_transfer_start(transfer_id) < 0 ||
        PDCregion_transfer_wait(transfer_id) < 0 || PDCregion_transfer_close(transfer_id) < 0) {
        fprintf(stderr, "Failed to write first region\n");
        ret_value = TFAIL;
        goto cleanup;
    }

    // Write second region
    transfer_id = PDCregion_transfer_create(data2, PDC_WRITE, obj, reg2, reg_global2);
    if (transfer_id == 0 || PDCregion_transfer_start(transfer_id) < 0 ||
        PDCregion_transfer_wait(transfer_id) < 0 || PDCregion_transfer_close(transfer_id) < 0) {
        fprintf(stderr, "Failed to write second region\n");
        ret_value = TFAIL;
        goto cleanup;
    }

    // Reset buffers to zero before read
    set_buf(data1, 0, half_particles);
    set_buf(data2, 0, half_particles);

    // Read first region
    transfer_id = PDCregion_transfer_create(data1, PDC_READ, obj, reg1, reg_global1);
    if (transfer_id == 0 || PDCregion_transfer_start(transfer_id) < 0 ||
        PDCregion_transfer_wait(transfer_id) < 0 || PDCregion_transfer_close(transfer_id) < 0) {
        fprintf(stderr, "Failed to read first region\n");
        ret_value = TFAIL;
        goto cleanup;
    }

    // Read second region
    transfer_id = PDCregion_transfer_create(data2, PDC_READ, obj, reg2, reg_global2);
    if (transfer_id == 0 || PDCregion_transfer_start(transfer_id) < 0 ||
        PDCregion_transfer_wait(transfer_id) < 0 || PDCregion_transfer_close(transfer_id) < 0) {
        fprintf(stderr, "Failed to read second region\n");
        ret_value = TFAIL;
        goto cleanup;
    }

    // Validate first buffer
    for (uint64_t i = 0; i < half_particles; i++) {
        if (fabsf(data1[i] - REG_1_FINAL_VAL) > 1e-6) {
            fprintf(stderr, "Validation failed for data1 at %llu: expected %f got %f\n",
                    (unsigned long long)i, REG_1_FINAL_VAL, data1[i]);
            ret_value = TFAIL;
            goto cleanup;
        }
    }

    // Validate second buffer
    for (uint64_t i = 0; i < half_particles; i++) {
        if (fabsf(data2[i] - REG_2_FINAL_VAL) > 1e-6) {
            fprintf(stderr, "Validation failed for data2 at %llu: expected %f got %f\n",
                    (unsigned long long)i, REG_2_FINAL_VAL, data2[i]);
            ret_value = TFAIL;
            goto cleanup;
        }
    }

cleanup:
    if (reg1)
        PDCregion_close(reg1);
    if (reg_global1)
        PDCregion_close(reg_global1);
    if (reg2)
        PDCregion_close(reg2);
    if (reg_global2)
        PDCregion_close(reg_global2);
    if (obj)
        PDCobj_close(obj);
    if (obj_prop)
        PDCprop_close(obj_prop);
    if (data1)
        free(data1);
    if (data2)
        free(data2);

    return ret_value;
}

int
main(int argc, char **argv)
{
    pdcid_t pdc, create_prop, cont;
    int     rank = 0, size = 1;

    int ret_value = TSUCCEED;

#ifdef ENABLE_MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
#endif

    // create a pdc
    TASSERT((pdc = PDCinit("pdc")) != 0, "Call to PDCinit succeeded", "Call to PDCinit failed");
    // create a container property
    TASSERT((create_prop = PDCprop_create(PDC_CONT_CREATE, pdc)) != 0, "Call to PDCprop_create succeeded",
            "Call to PDCprop_create failed");
    // create a container
    TASSERT((cont = PDCcont_create("c1", create_prop)) != 0, "Call to PDCcont_create succeeded",
            "Call to PDCcont_create failed");

    TASSERT(workflow1(pdc, cont) == TSUCCEED, "workflow1 succeeded", "workflow1 failed");

    // close a container
    TASSERT(PDCcont_close(cont) >= 0, "Call to PDCcont_close succeeded", "Call to PDCcont_close failed");
    // close a container property
    TASSERT(PDCprop_close(create_prop) >= 0, "Call to PDCprop_close succeeded",
            "Call to PDCprop_close failed");
    // close pdc
    TASSERT(PDCclose(pdc) >= 0, "Call to PDCclose succeeded", "Call to PDCclose failed");

done:
#ifdef ENABLE_MPI
    MPI_Finalize();
#endif
    return ret_value;
}