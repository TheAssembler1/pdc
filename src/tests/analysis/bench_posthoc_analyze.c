/**
 * Posthoc benchmark, phase 2 of 2: opens the vx/vy/vz objects written by a
 * prior, already-exited bench_write_components run (against a PDC server
 * that has since been restarted with the `restart` argument to reload
 * that data from its checkpoint), reads them back, computes magnitude
 * client-side, and writes the result back as a plain PDC object -- a
 * hand-built materialized view outside the transform framework.
 *
 * This models the actual posthoc workflow: someone comes back later, in a
 * separate job, to compute and persist a derived product from data
 * someone else already wrote. Unlike eager, none of the write-side cost
 * or client/server relaunch cost is hidden inside a single continuous
 * session -- see srun_server_restart.sh and posthoc_analysis.sbatch for
 * how the relaunch is timed and folded into the combined results row.
 *
 * Usage: bench_posthoc_analyze <n_elem_per_rank>
 *
 * Prints one CSV line from rank 0:
 *   mode,n_client_ranks,n_elem,setup_s,readback_s,compute_s,writeback_s,total_s,bad
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
    size_t i;

    double t_setup0, t_setup1, t_readback0, t_readback1;
    double t_compute0, t_compute1, t_writeback0, t_writeback1;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <n_elem_per_rank>\n", argv[0]);
        return 1;
    }
    n_elem = atol(argv[1]);

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    float * vx_rb = (float *)malloc(sizeof(float) * n_elem);
    float * vy_rb = (float *)malloc(sizeof(float) * n_elem);
    float * vz_rb = (float *)malloc(sizeof(float) * n_elem);
    double *mag   = (double *)malloc(sizeof(double) * n_elem);
    memset(mag, 0, sizeof(double) * n_elem);

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

    const char *cont_name    = "bench_shared";
    pdcid_t     obj_prop_out = 0;
    pdcid_t     cont = 0, vx_obj = 0, vy_obj = 0, vz_obj = 0, mag_obj = 0;

    /* vx/vy/vz/container already exist from the prior write-phase job.
     * The magnitude object doesn't exist yet, so rank 0 creates it here,
     * same create-once-per-object pattern as bench_magnitude.c. */
    cont   = PDCcont_open(cont_name, pdc);
    vx_obj = PDCobj_open("vx", pdc);
    vy_obj = PDCobj_open("vy", pdc);
    vz_obj = PDCobj_open("vz", pdc);

    if (rank == 0) {
        obj_prop_out = PDCprop_create(PDC_OBJ_CREATE, pdc);
        PDCprop_set_obj_type(obj_prop_out, PDC_DOUBLE);
        PDCprop_set_obj_dims(obj_prop_out, 1, dims);
        PDCprop_set_obj_user_id(obj_prop_out, getuid());
        PDCprop_set_obj_time_step(obj_prop_out, 0);
        PDCprop_set_obj_app_name(obj_prop_out, "BenchMagnitude");
        PDCprop_set_obj_tags(obj_prop_out, "tag0=1");
        PDCprop_set_obj_transfer_region_type(obj_prop_out, PDC_REGION_STATIC);

        mag_obj = PDCobj_create(cont, "magnitude", obj_prop_out);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank != 0)
        mag_obj = PDCobj_open("magnitude", pdc);

    pdcid_t reg        = PDCregion_create(1, local_offset, region_len);
    pdcid_t reg_global = PDCregion_create(1, global_offset, region_len);

    MPI_Barrier(MPI_COMM_WORLD);
    t_setup1 = MPI_Wtime();

    t_readback0 = MPI_Wtime();
    do_transfer(vx_rb, PDC_READ, vx_obj, reg, reg_global, "readback vx");
    do_transfer(vy_rb, PDC_READ, vy_obj, reg, reg_global, "readback vy");
    do_transfer(vz_rb, PDC_READ, vz_obj, reg, reg_global, "readback vz");
    MPI_Barrier(MPI_COMM_WORLD);
    t_readback1 = MPI_Wtime();

    t_compute0 = MPI_Wtime();
    for (i = 0; i < (size_t)n_elem; ++i) {
        double x = (double)vx_rb[i], y = (double)vy_rb[i], z = (double)vz_rb[i];
        mag[i] = sqrt(x * x + y * y + z * z);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    t_compute1 = MPI_Wtime();

    t_writeback0 = MPI_Wtime();
    do_transfer(mag, PDC_WRITE, mag_obj, reg, reg_global, "writeback magnitude");
    MPI_Barrier(MPI_COMM_WORLD);
    t_writeback1 = MPI_Wtime();

    /* Correctness check (not timed): vx/vy/vz were generated with the
     * same deterministic pattern by bench_write_components, so it's
     * regenerated locally here rather than read back a second time. */
    int local_bad = 0;
    for (i = 0; i < (size_t)n_elem; ++i) {
        float  ex       = (float)((i % 1000) + 1);
        float  ey       = (float)(((i + 137) % 1000) + 1);
        float  ez       = (float)(((i + 613) % 1000) + 1);
        double expected = sqrt((double)ex * ex + (double)ey * ey + (double)ez * ez);
        if (fabs(mag[i] - expected) > EPSILON) {
            local_bad++;
            break;
        }
    }
    int global_bad = 0;
    MPI_Reduce(&local_bad, &global_bad, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    PDCregion_close(reg);
    PDCregion_close(reg_global);
    PDCobj_close(vx_obj);
    PDCobj_close(vy_obj);
    PDCobj_close(vz_obj);
    PDCobj_close(mag_obj);
    PDCcont_close(cont);
    if (rank == 0)
        PDCprop_close(obj_prop_out);
    PDCclose(pdc);

    double local_setup     = t_setup1 - t_setup0;
    double local_readback  = t_readback1 - t_readback0;
    double local_compute   = t_compute1 - t_compute0;
    double local_writeback = t_writeback1 - t_writeback0;

    double max_setup, max_readback, max_compute, max_writeback;
    MPI_Reduce(&local_setup, &max_setup, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_readback, &max_readback, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_compute, &max_compute, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_writeback, &max_writeback, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        double total = max_setup + max_readback + max_compute + max_writeback;
        printf("posthoc_analyze,%d,%ld,%.6f,%.6f,%.6f,%.6f,%.6f,%d\n", nranks, n_elem, max_setup,
               max_readback, max_compute, max_writeback, total, global_bad);
        fflush(stdout);
    }

    free(vx_rb);
    free(vy_rb);
    free(vz_rb);
    free(mag);

    MPI_Finalize();
    return global_bad != 0;
}
