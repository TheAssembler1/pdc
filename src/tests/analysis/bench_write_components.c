/**
 * Posthoc benchmark, phase 1 of 2: write the three vector-component
 * objects (vx, vy, vz) as plain PDC objects (no graph attached) and exit.
 *
 * A real posthoc workflow writes data in one job, then some time later a
 * separate job comes back to read it, compute a derived product, and
 * persist that too -- unlike eager/lazy, which stay in one continuous
 * client session. This binary is only the first half: run it, let every
 * client rank fully exit, restart the PDC server with the `restart`
 * argument so it reloads the checkpointed metadata, then run
 * bench_posthoc_analyze against the same container/objects.
 *
 * Usage: bench_write_components <n_elem_per_rank>
 *
 * Prints one CSV line from rank 0:
 *   mode,n_client_ranks,n_elem,setup_s,write_s
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mpi.h>

#include "pdc.h"

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
    size_t i;

    double t_setup0, t_setup1, t_write0, t_write1;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <n_elem_per_rank>\n", argv[0]);
        return 1;
    }
    n_elem = atol(argv[1]);

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    float *vx = (float *)malloc(sizeof(float) * n_elem);
    float *vy = (float *)malloc(sizeof(float) * n_elem);
    float *vz = (float *)malloc(sizeof(float) * n_elem);

    /* Same deterministic pattern bench_posthoc_analyze uses to
     * independently regenerate expected values for its correctness
     * check. */
    for (i = 0; i < (size_t)n_elem; ++i) {
        vx[i] = (float)((i % 1000) + 1);
        vy[i] = (float)(((i + 137) % 1000) + 1);
        vz[i] = (float)(((i + 613) % 1000) + 1);
    }

    /* See bench_magnitude.c's comment on REGION_STATIC vs OBJ_STATIC for
     * why a shared object with region-offset-based routing is used here
     * instead of one object per rank. */
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
    pdcid_t     obj_prop_in = 0;
    pdcid_t     vx_obj = 0, vy_obj = 0, vz_obj = 0;

    /* Only rank 0 creates the shared container and objects; every other
     * rank opens them by name after the barrier below (see
     * bench_magnitude.c's comment on the metadata dedup path). */
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
        PDCprop_set_obj_transfer_region_type(obj_prop_in, PDC_REGION_STATIC);

        vx_obj = PDCobj_create(cont, "vx", obj_prop_in);
        vy_obj = PDCobj_create(cont, "vy", obj_prop_in);
        vz_obj = PDCobj_create(cont, "vz", obj_prop_in);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank != 0) {
        cont   = PDCcont_open(cont_name, pdc);
        vx_obj = PDCobj_open("vx", pdc);
        vy_obj = PDCobj_open("vy", pdc);
        vz_obj = PDCobj_open("vz", pdc);
    }

    pdcid_t reg        = PDCregion_create(1, local_offset, region_len);
    pdcid_t reg_global = PDCregion_create(1, global_offset, region_len);

    MPI_Barrier(MPI_COMM_WORLD);
    t_setup1 = MPI_Wtime();

    t_write0 = MPI_Wtime();
    do_transfer(vx, PDC_WRITE, vx_obj, reg, reg_global, "write vx");
    do_transfer(vy, PDC_WRITE, vy_obj, reg, reg_global, "write vy");
    do_transfer(vz, PDC_WRITE, vz_obj, reg, reg_global, "write vz");
    MPI_Barrier(MPI_COMM_WORLD);
    t_write1 = MPI_Wtime();

    PDCregion_close(reg);
    PDCregion_close(reg_global);
    PDCobj_close(vx_obj);
    PDCobj_close(vy_obj);
    PDCobj_close(vz_obj);
    PDCcont_close(cont);
    if (rank == 0) {
        PDCprop_close(obj_prop_in);
        PDCprop_close(cont_prop);
    }
    PDCclose(pdc);

    double local_setup = t_setup1 - t_setup0;
    double local_write = t_write1 - t_write0;

    double max_setup, max_write;
    MPI_Reduce(&local_setup, &max_setup, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_write, &max_write, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("posthoc_write,%d,%ld,%.6f,%.6f\n", nranks, n_elem, max_setup, max_write);
        fflush(stdout);
    }

    free(vx);
    free(vy);
    free(vz);

    MPI_Finalize();
    return 0;
}
