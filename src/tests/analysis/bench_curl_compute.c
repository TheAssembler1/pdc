/**
 * E3SM-shaped curl/vorticity-magnitude benchmark, posthoc phase 2 of 3:
 * opens the u/v/w objects a prior, already-exited bench_curl_write run
 * created (against a PDC server that's since been closed and restarted
 * with the `restart` argument to reload that data from its checkpoint),
 * reads them back, computes curl client-side via the shared kernel in
 * curl_math.h, and writes the result out as three plain PDC objects
 * (curl_x, curl_y, curl_z). See bench_curl_analyze.c for phase 3 (read
 * curl back, compute magnitude, write it out).
 *
 * Usage: bench_curl_compute <nx> <ny> <nz_per_rank>
 *
 * Prints one CSV line from rank 0:
 *   mode,n_client_ranks,nx,ny,nz_per_rank,setup_s,readback_s,compute_s,writeback_s,total_s
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mpi.h>

#include "pdc.h"
#include "curl_math.h"

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
    int  rank, nranks;
    long nx, ny, nz_per_rank;

    double t_setup0, t_setup1, t_readback0, t_readback1, t_compute0, t_compute1, t_writeback0, t_writeback1;

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <nx> <ny> <nz_per_rank>\n", argv[0]);
        return 1;
    }
    nx          = atol(argv[1]);
    ny          = atol(argv[2]);
    nz_per_rank = atol(argv[3]);

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    size_t n_elem = (size_t)nx * (size_t)ny * (size_t)nz_per_rank;

    float * u_rb   = (float *)malloc(sizeof(float) * n_elem);
    float * v_rb   = (float *)malloc(sizeof(float) * n_elem);
    float * w_rb   = (float *)malloc(sizeof(float) * n_elem);
    double *curl_x = (double *)malloc(sizeof(double) * n_elem);
    double *curl_y = (double *)malloc(sizeof(double) * n_elem);
    double *curl_z = (double *)malloc(sizeof(double) * n_elem);

    uint64_t local_offset[3], global_offset[3], region_len[3], dims[3];
    local_offset[0]  = 0;
    local_offset[1]  = 0;
    local_offset[2]  = 0;
    global_offset[0] = 0;
    global_offset[1] = 0;
    global_offset[2] = (uint64_t)rank * (uint64_t)nz_per_rank;
    region_len[0]    = (uint64_t)nx;
    region_len[1]    = (uint64_t)ny;
    region_len[2]    = (uint64_t)nz_per_rank;
    dims[0]          = (uint64_t)nx;
    dims[1]          = (uint64_t)ny;
    dims[2]          = (uint64_t)nranks * (uint64_t)nz_per_rank;

    MPI_Barrier(MPI_COMM_WORLD);
    t_setup0 = MPI_Wtime();

    pdcid_t pdc = PDCinit("pdc");
    if (pdc == 0) {
        fprintf(stderr, "PDCinit failed\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    const char *cont_name   = "curl_bench_shared";
    pdcid_t     prop_double = 0;
    pdcid_t     cont = 0, u_obj = 0, v_obj = 0, w_obj = 0;
    pdcid_t     curl_x_obj = 0, curl_y_obj = 0, curl_z_obj = 0;

    /* u/v/w/container already exist from the prior write-phase job.
     * curl_x/y/z don't exist yet, so rank 0 creates them here, same
     * create-once-per-object pattern as bench_curl_write.c. */
    cont  = PDCcont_open(cont_name, pdc);
    u_obj = PDCobj_open("u", pdc);
    v_obj = PDCobj_open("v", pdc);
    w_obj = PDCobj_open("w", pdc);

    if (rank == 0) {
        prop_double = PDCprop_create(PDC_OBJ_CREATE, pdc);
        PDCprop_set_obj_type(prop_double, PDC_DOUBLE);
        PDCprop_set_obj_dims(prop_double, 3, dims);
        PDCprop_set_obj_user_id(prop_double, getuid());
        PDCprop_set_obj_time_step(prop_double, 0);
        PDCprop_set_obj_app_name(prop_double, "BenchCurlVorticity");
        PDCprop_set_obj_tags(prop_double, "tag0=1");
        PDCprop_set_obj_transfer_region_type(prop_double, PDC_REGION_STATIC);

        curl_x_obj = PDCobj_create(cont, "curl_x", prop_double);
        curl_y_obj = PDCobj_create(cont, "curl_y", prop_double);
        curl_z_obj = PDCobj_create(cont, "curl_z", prop_double);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank != 0) {
        curl_x_obj = PDCobj_open("curl_x", pdc);
        curl_y_obj = PDCobj_open("curl_y", pdc);
        curl_z_obj = PDCobj_open("curl_z", pdc);
    }

    pdcid_t reg        = PDCregion_create(3, local_offset, region_len);
    pdcid_t reg_global = PDCregion_create(3, global_offset, region_len);

    MPI_Barrier(MPI_COMM_WORLD);
    t_setup1 = MPI_Wtime();

    t_readback0 = MPI_Wtime();
    do_transfer(u_rb, PDC_READ, u_obj, reg, reg_global, "readback u");
    do_transfer(v_rb, PDC_READ, v_obj, reg, reg_global, "readback v");
    do_transfer(w_rb, PDC_READ, w_obj, reg, reg_global, "readback w");
    MPI_Barrier(MPI_COMM_WORLD);
    t_readback1 = MPI_Wtime();

    t_compute0 = MPI_Wtime();
    curl_math_compute(u_rb, v_rb, w_rb, (size_t)nx, (size_t)ny, (size_t)nz_per_rank, curl_x, curl_y, curl_z);
    MPI_Barrier(MPI_COMM_WORLD);
    t_compute1 = MPI_Wtime();

    t_writeback0 = MPI_Wtime();
    do_transfer(curl_x, PDC_WRITE, curl_x_obj, reg, reg_global, "writeback curl_x");
    do_transfer(curl_y, PDC_WRITE, curl_y_obj, reg, reg_global, "writeback curl_y");
    do_transfer(curl_z, PDC_WRITE, curl_z_obj, reg, reg_global, "writeback curl_z");
    MPI_Barrier(MPI_COMM_WORLD);
    t_writeback1 = MPI_Wtime();

    PDCregion_close(reg);
    PDCregion_close(reg_global);
    PDCobj_close(u_obj);
    PDCobj_close(v_obj);
    PDCobj_close(w_obj);
    PDCobj_close(curl_x_obj);
    PDCobj_close(curl_y_obj);
    PDCobj_close(curl_z_obj);
    PDCcont_close(cont);
    if (rank == 0)
        PDCprop_close(prop_double);
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
        printf("curl_posthoc_compute,%d,%ld,%ld,%ld,%.6f,%.6f,%.6f,%.6f,%.6f\n", nranks, nx, ny, nz_per_rank,
               max_setup, max_readback, max_compute, max_writeback, total);
        fflush(stdout);
    }

    free(u_rb);
    free(v_rb);
    free(w_rb);
    free(curl_x);
    free(curl_y);
    free(curl_z);

    MPI_Finalize();
    return 0;
}
