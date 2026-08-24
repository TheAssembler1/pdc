/**
 * Scale benchmark comparing two strategies for producing a "magnitude"
 * object from three vector-component objects (vx, vy, vz):
 *
 *   eager   - components are written through the region-analysis
 *             framework (DataFlyway) with the vector_magnitude graph
 *             attached; the last component write transparently triggers
 *             server-side eager computation of magnitude in the write
 *             path. A confirmation read of magnitude follows.
 *
 *   posthoc - components are written as plain PDC objects (no graph
 *             attached), then read back to the client, magnitude is
 *             computed client-side, and the result is written back as a
 *             plain PDC object.
 *
 * Usage: bench_magnitude <eager|posthoc> <n_elem_per_rank>
 *
 * Prints one CSV line from rank 0:
 *   mode,n_client_ranks,n_elem,setup_s,write_s,readback_s,compute_s,writeback_s,confirm_read_s,total_s
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <mpi.h>

#include "pdc.h"

#define EPSILON 1e-3

static void
do_transfer(void *buf, pdc_access_t access, pdcid_t obj, pdcid_t reg, pdcid_t reg_global, const char *what)
{
    pdcid_t tr = PDCregion_transfer_create(buf, access, obj, reg, reg_global);
    if (tr == 0) {
        fprintf(stderr, "PDCregion_transfer_create failed for %s\n", what);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if (PDCregion_transfer_start(tr) < 0) {
        fprintf(stderr, "PDCregion_transfer_start failed for %s\n", what);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if (PDCregion_transfer_wait(tr) < 0) {
        fprintf(stderr, "PDCregion_transfer_wait failed for %s\n", what);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if (PDCregion_transfer_close(tr) < 0) {
        fprintf(stderr, "PDCregion_transfer_close failed for %s\n", what);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
}

int
main(int argc, char **argv)
{
    int    rank, nranks;
    long   n_elem;
    int    is_eager;
    size_t i;

    double t_setup0, t_setup1, t_write0, t_write1;
    double t_readback0 = 0, t_readback1 = 0, t_compute0 = 0, t_compute1 = 0;
    double t_writeback0 = 0, t_writeback1 = 0, t_read0 = 0, t_read1 = 0;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <eager|posthoc> <n_elem_per_rank>\n", argv[0]);
        return 1;
    }
    is_eager = (strcmp(argv[1], "eager") == 0);
    n_elem   = atol(argv[2]);

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    float * vx      = (float *)malloc(sizeof(float) * n_elem);
    float * vy      = (float *)malloc(sizeof(float) * n_elem);
    float * vz      = (float *)malloc(sizeof(float) * n_elem);
    float * vx_rb   = (float *)malloc(sizeof(float) * n_elem);
    float * vy_rb   = (float *)malloc(sizeof(float) * n_elem);
    float * vz_rb   = (float *)malloc(sizeof(float) * n_elem);
    double *mag     = (double *)malloc(sizeof(double) * n_elem);

    for (i = 0; i < (size_t)n_elem; ++i) {
        vx[i] = (float)((i % 1000) + 1);
        vy[i] = (float)(((i + 137) % 1000) + 1);
        vz[i] = (float)(((i + 613) % 1000) + 1);
    }
    memset(mag, 0, sizeof(double) * n_elem);

    /* Single shared object per variable, spanning the whole nranks*n_elem
     * problem domain. Each rank manages a distinct, disjoint subset of
     * every object: rank i's local region is [i*n_elem, (i+1)*n_elem). The
     * local buffer region (reg) is always 0-indexed -- it describes the
     * client's own memory layout -- while the global region (reg_global)
     * is the rank's slice within the shared object. */
    uint64_t local_offset[1], global_offset[1], region_len[1], dims[1];
    local_offset[0]  = 0;
    global_offset[0] = (uint64_t)rank * (uint64_t)n_elem;
    region_len[0]    = (uint64_t)n_elem;
    dims[0]          = (uint64_t)nranks * (uint64_t)n_elem;

    MPI_Barrier(MPI_COMM_WORLD);
    t_setup0 = MPI_Wtime();

    pdcid_t pdc = PDCinit("pdc");
    if (pdc == 0) {
        fprintf(stderr, "PDCinit failed\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    const char *cont_name = "bench_shared";
    pdcid_t     cont_prop = 0, cont = 0;
    pdcid_t     obj_prop_in = 0, obj_prop_out = 0;
    pdcid_t     vx_obj = 0, vy_obj = 0, vz_obj = 0, mag_obj = 0;

    /* Only rank 0 creates the shared container and objects; every other
     * rank opens them by name after the barrier below. Every rank calling
     * PDCobj_create for the same name would hit the server's metadata
     * dedup path (rejected as "identical metadata"), since it keys purely
     * on (obj_name, time_step) -- this is a create-once-per-object, not a
     * create-once-per-rank, API. */
    if (rank == 0) {
        cont_prop = PDCprop_create(PDC_CONT_CREATE, pdc);
        cont      = PDCcont_create(cont_name, cont_prop);

        obj_prop_in = PDCprop_create(PDC_OBJ_CREATE, pdc);
        PDCprop_set_obj_type(obj_prop_in, PDC_FLOAT);
        PDCprop_set_obj_dims(obj_prop_in, 1, dims);
        PDCprop_set_obj_user_id(obj_prop_in, getuid());
        PDCprop_set_obj_time_step(obj_prop_in, 0);
        PDCprop_set_obj_app_name(obj_prop_in, "BenchMagnitude");
        PDCprop_set_obj_tags(obj_prop_in, "tag0=1");
        PDCprop_set_obj_transfer_region_type(obj_prop_in, PDC_OBJ_STATIC);

        obj_prop_out = PDCprop_create(PDC_OBJ_CREATE, pdc);
        PDCprop_set_obj_type(obj_prop_out, PDC_DOUBLE);
        PDCprop_set_obj_dims(obj_prop_out, 1, dims);
        PDCprop_set_obj_user_id(obj_prop_out, getuid());
        PDCprop_set_obj_time_step(obj_prop_out, 0);
        PDCprop_set_obj_app_name(obj_prop_out, "BenchMagnitude");
        PDCprop_set_obj_tags(obj_prop_out, "tag0=1");
        PDCprop_set_obj_transfer_region_type(obj_prop_out, PDC_OBJ_STATIC);

        vx_obj  = PDCobj_create(cont, "vx", obj_prop_in);
        vy_obj  = PDCobj_create(cont, "vy", obj_prop_in);
        vz_obj  = PDCobj_create(cont, "vz", obj_prop_in);
        mag_obj = PDCobj_create(cont, "magnitude", obj_prop_out);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank != 0) {
        cont    = PDCcont_open(cont_name, pdc);
        vx_obj  = PDCobj_open("vx", pdc);
        vy_obj  = PDCobj_open("vy", pdc);
        vz_obj  = PDCobj_open("vz", pdc);
        mag_obj = PDCobj_open("magnitude", pdc);
    }

    pdcid_t reg        = PDCregion_create(1, local_offset, region_len);
    pdcid_t reg_global  = PDCregion_create(1, global_offset, region_len);

    pdcid_t dg_id = 0;
    if (is_eager) {
        dg_id = PDCan_dg_json_create(AN_GRAPHS_DIR "vector_magnitude.json");
        if (dg_id == 0) {
            fprintf(stderr, "PDCan_dg_json_create failed\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        PDCan_attach_to_region(dg_id, "vx", vx_obj, reg_global);
        PDCan_attach_to_region(dg_id, "vy", vy_obj, reg_global);
        PDCan_attach_to_region(dg_id, "vz", vz_obj, reg_global);
        PDCan_attach_to_region(dg_id, "magnitude", mag_obj, reg_global);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    t_setup1 = MPI_Wtime();

    /* Write vector components -- for eager mode, the final component
     * write transparently triggers server-side computation+persistence
     * of magnitude before this phase's wait/close return. */
    t_write0 = MPI_Wtime();
    do_transfer(vx, PDC_WRITE, vx_obj, reg, reg_global, "write vx");
    do_transfer(vy, PDC_WRITE, vy_obj, reg, reg_global, "write vy");
    do_transfer(vz, PDC_WRITE, vz_obj, reg, reg_global, "write vz");
    MPI_Barrier(MPI_COMM_WORLD);
    t_write1 = MPI_Wtime();

    if (is_eager) {
        /* Confirmation read: magnitude should already be materialized. */
        t_read0 = MPI_Wtime();
        do_transfer(mag, PDC_READ, mag_obj, reg, reg_global, "read magnitude");
        MPI_Barrier(MPI_COMM_WORLD);
        t_read1 = MPI_Wtime();
    }
    else {
        /* Read the components back to the client. */
        t_readback0 = MPI_Wtime();
        do_transfer(vx_rb, PDC_READ, vx_obj, reg, reg_global, "readback vx");
        do_transfer(vy_rb, PDC_READ, vy_obj, reg, reg_global, "readback vy");
        do_transfer(vz_rb, PDC_READ, vz_obj, reg, reg_global, "readback vz");
        MPI_Barrier(MPI_COMM_WORLD);
        t_readback1 = MPI_Wtime();

        /* Compute magnitude client-side. */
        t_compute0 = MPI_Wtime();
        for (i = 0; i < (size_t)n_elem; ++i) {
            double x = (double)vx_rb[i], y = (double)vy_rb[i], z = (double)vz_rb[i];
            mag[i] = sqrt(x * x + y * y + z * z);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        t_compute1 = MPI_Wtime();

        /* Write the result back as an ordinary PDC object. */
        t_writeback0 = MPI_Wtime();
        do_transfer(mag, PDC_WRITE, mag_obj, reg, reg_global, "writeback magnitude");
        MPI_Barrier(MPI_COMM_WORLD);
        t_writeback1 = MPI_Wtime();
    }

    /* Correctness check (not timed). */
    int local_bad = 0;
    for (i = 0; i < (size_t)n_elem; ++i) {
        double x        = (double)vx[i], y = (double)vy[i], z = (double)vz[i];
        double expected = sqrt(x * x + y * y + z * z);
        if (fabs(mag[i] - expected) > EPSILON) {
            local_bad++;
            break;
        }
    }
    int global_bad = 0;
    MPI_Reduce(&local_bad, &global_bad, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (is_eager)
        PDCan_close_dg(dg_id);
    PDCregion_close(reg);
    PDCregion_close(reg_global);
    PDCobj_close(vx_obj);
    PDCobj_close(vy_obj);
    PDCobj_close(vz_obj);
    PDCobj_close(mag_obj);
    PDCcont_close(cont);
    if (rank == 0) {
        PDCprop_close(obj_prop_in);
        PDCprop_close(obj_prop_out);
        PDCprop_close(cont_prop);
    }
    PDCclose(pdc);

    double local_setup    = t_setup1 - t_setup0;
    double local_write    = t_write1 - t_write0;
    double local_readback = t_readback1 - t_readback0;
    double local_compute  = t_compute1 - t_compute0;
    double local_writeback = t_writeback1 - t_writeback0;
    double local_read     = t_read1 - t_read0;

    double max_setup, max_write, max_readback, max_compute, max_writeback, max_read;
    MPI_Reduce(&local_setup, &max_setup, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_write, &max_write, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_readback, &max_readback, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_compute, &max_compute, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_writeback, &max_writeback, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_read, &max_read, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        double total = is_eager ? (max_write + max_read) : (max_write + max_readback + max_compute + max_writeback);
        printf("%s,%d,%ld,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d\n", is_eager ? "eager" : "posthoc", nranks,
               n_elem, max_setup, max_write, max_readback, max_compute, max_writeback, max_read, total,
               global_bad);
        fflush(stdout);
    }

    free(vx);
    free(vy);
    free(vz);
    free(vx_rb);
    free(vy_rb);
    free(vz_rb);
    free(mag);

    MPI_Finalize();
    return global_bad != 0;
}
