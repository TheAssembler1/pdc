/**
 * Description: This workflow builds on Workflow 2 by introducing
 * multiple transformation paths that can execute on different hardware
 * devices. Compression and decompression can now be performed using
 * either CPU or GPU, allowing PDC to choose the best available option
 * at runtime.
 */

#include "pdc.h"
#include "test_helper.h"

#define NUM_PARTICLES_PER_DIM 400
#define NUM_DIMS              1
#define TYPE                  PDC_DOUBLE
#define INIT_VAL              2.0
#define INIT_READ_VAL         0.0
#define FINAL_VAL             2.0

static void
set_buf_double(double *buf, int val, uint64_t num)
{
    for (uint64_t i = 0; i < num; i++)
        buf[i] = val;
}

static void
set_buf_float(float *buf, int val, uint64_t num)
{
    for (uint64_t i = 0; i < num; i++)
        buf[i] = val;
}

static int
workflow1(pdcid_t pdc, pdcid_t cont)
{
    int     ret_value = TSUCCEED;
    pdcid_t obj_id, obj_prop, reg, reg_global, transfer_id;

    uint64_t total_particles = NUM_PARTICLES_PER_DIM;
    for (int i = 2; i <= NUM_DIMS; i++)
        total_particles *= NUM_PARTICLES_PER_DIM;
    double *data = (double *)malloc(total_particles * sizeof(double));
    float *data_read = (float *)malloc(total_particles * sizeof(float));

    set_buf_double(data, INIT_VAL, total_particles);
    set_buf_float(data_read, INIT_READ_VAL, total_particles);

    pdcid_t dg_id = PDCtf_create_dg("example_dg");

    pdcid_t float_id                = PDCtf_create_state("decompressed_floats");
    pdcid_t compressed_id           = PDCtf_create_state("compressed_floats");
    pdcid_t decompressed_doubles_id = PDCtf_create_state("decompressed_doubles");

    PDCtf_add_func(dg_id, "builtin:double_to_float", PDC_TF_CPU_DEVICE, decompressed_doubles_id, float_id);
    PDCtf_add_func(dg_id, "builtin:float_to_double", PDC_TF_CPU_DEVICE, float_id, decompressed_doubles_id);
    PDCtf_add_func(dg_id, "builtin:zfp_compress", PDC_TF_CPU_DEVICE, float_id, compressed_id);
    PDCtf_add_func(dg_id, "builtin:zfp_decompress", PDC_TF_CPU_DEVICE, compressed_id, float_id);

    PDCtf_print_dg(dg_id);

    TASSERT((obj_prop = PDCprop_create(PDC_OBJ_CREATE, pdc)) != 0, "obj_prop_create succeeded",
            "obj_prop_create failed");
    uint64_t dims[NUM_DIMS];
    uint64_t offset[NUM_DIMS];

    for (int i = 0; i < NUM_DIMS; i++) {
        offset[i] = 0;
        dims[i]   = NUM_PARTICLES_PER_DIM;
    }

    TASSERT(PDCprop_set_obj_type(obj_prop, TYPE) >= 0, "obj_prop_set_obj_type succeeded",
            "obj_prop_set_obj_type failed");
    TASSERT(PDCprop_set_obj_dims(obj_prop, NUM_DIMS, dims) >= 0, "obj_prop_set_obj_dims succeeded",
            "obj_prop_set_obj_dims failed");
    TASSERT((obj_id = PDCobj_create(cont, "obj_transform", obj_prop)) != 0, "obj_create succeeded",
            "obj_create failed");
    // for now local and global region are the same
    TASSERT((reg = PDCregion_create(NUM_DIMS, offset, dims)) != 0, "region_create succeeded",
            "region_create failed");
    TASSERT((reg_global = PDCregion_create(NUM_DIMS, offset, dims)) != 0, "region_create succeeded",
            "region_create failed");

    // attach graph to region
    PDCtf_attach_to_region(dg_id, obj_id, reg_global, decompressed_doubles_id, TYPE, compressed_id);

    for (int i = 0; i < total_particles; i++) {
        LOG_JUST_PRINT("%.1f ", i, data[i]);
        if(i != 0 && (i + 1) % 20 == 0)
            LOG_JUST_PRINT("\n");
    }
    LOG_JUST_PRINT("\n");

    // write transfer
    LOG_INFO("Starting region transfer write\n");
    TASSERT((transfer_id = PDCregion_transfer_create(data, PDC_WRITE, obj_id, reg, reg)) != 0,
            "region_transfer_create succeeded", "region_transfer_create failed");
    TASSERT(PDCregion_transfer_start(transfer_id) >= 0, "region_transfer_start succeeded",
            "region_transfer_start failed");
    TASSERT(PDCregion_transfer_wait(transfer_id) >= 0, "region_transfer_wait succeeded",
            "region_transfer_wait failed");
    TASSERT(PDCregion_transfer_close(transfer_id) >= 0, "region_transfer_close succeeded",
            "region_transfer_close failed");

    // read transfer
    LOG_INFO("Starting region transfer read\n");
    TASSERT((transfer_id = PDCregion_transfer_create(data_read, PDC_READ, obj_id, reg, reg)) != 0,
            "region_transfer_create succeeded", "region_transfer_create failed");
    TASSERT(PDCregion_transfer_start(transfer_id) >= 0, "region_transfer_start succeeded",
            "region_transfer_start failed");
    TASSERT(PDCregion_transfer_wait(transfer_id) >= 0, "region_transfer_wait succeeded",
            "region_transfer_wait failed");
    TASSERT(PDCregion_transfer_close(transfer_id) >= 0, "region_transfer_close succeeded",
            "region_transfer_close failed");

    for (int i = 0; i < total_particles; i++) {
        LOG_JUST_PRINT("%.1f ", i, data_read[i]);
        if(i != 0 && (i + 1) % 20 == 0)
            LOG_JUST_PRINT("\n");
    }
    LOG_JUST_PRINT("\n");

    PDCtf_close_dg(dg_id);
    TASSERT(PDCregion_close(reg) >= 0, "Call to PDCregion_close succeeded", "Call to PDCregion_close failed");
    TASSERT(PDCregion_close(reg_global) >= 0, "Call to PDCregion_close succeeded",
            "Call to PDCregion_close failed");
    // close object
    TASSERT(PDCobj_close(obj_id) >= 0, "Call to PDCobj_close succeeded", "Call to PDCobj_close failed");
    // close a object property
    TASSERT(PDCprop_close(obj_prop) >= 0, "Call to PDCprop_close succeeded", "Call to PDCprop_close failed");
done:
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
