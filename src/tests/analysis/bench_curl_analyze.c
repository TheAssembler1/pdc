/**
 * E3SM-shaped curl/vorticity-magnitude benchmark, posthoc phase 3 of 3:
 * opens the curl_x/y/z objects a prior, already-exited bench_curl_compute
 * run created (against a PDC server that's since been closed and
 * restarted again to reload that data from its checkpoint), reads them
 * back, computes vorticity magnitude client-side, and writes the result
 * out as a plain PDC object (vorticity_magnitude).
 *
 * With --compress, vorticity_magnitude is created with the existing GPU
 * ZFP compression transform (tf_client/graphs/zfp_gpu.json) composed onto
 * it before the write -- the transformation framework already applies
 * transparently to a plain object write, no analysis graph involved here
 * at all (posthoc never uses one).
 *
 * Usage: bench_curl_analyze <nx> <ny> <nz_per_rank> <compress:0|1>
 *
 * Prints one CSV line from rank 0:
 *   mode,n_client_ranks,nx,ny,nz_per_rank,compress,setup_s,readback_s,compute_s,writeback_s,total_s,bad
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <mpi.h>

#include "pdc.h"
#include "curl_math.h"

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
    long   nx, ny, nz_per_rank;
    int    compress;
    size_t i;

    double t_setup0, t_setup1, t_readback0, t_readback1, t_compute0, t_compute1, t_writeback0, t_writeback1;

    if (argc < 5) {
        fprintf(stderr, "Usage: %s <nx> <ny> <nz_per_rank> <compress:0|1>\n", argv[0]);
        return 1;
    }
    nx          = atol(argv[1]);
    ny          = atol(argv[2]);
    nz_per_rank = atol(argv[3]);
    compress    = atoi(argv[4]);

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    size_t n_elem = (size_t)nx * (size_t)ny * (size_t)nz_per_rank;

    double *curl_x_rb = (double *)malloc(sizeof(double) * n_elem);
    double *curl_y_rb = (double *)malloc(sizeof(double) * n_elem);
    double *curl_z_rb = (double *)malloc(sizeof(double) * n_elem);
    double *mag       = (double *)malloc(sizeof(double) * n_elem);

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
    pdcid_t     cont = 0, curl_x_obj = 0, curl_y_obj = 0, curl_z_obj = 0, mag_obj = 0;

    /* curl_x/y/z/container already exist from the prior compute-phase
     * job. vorticity_magnitude doesn't exist yet, so rank 0 creates it
     * here. */
    cont       = PDCcont_open(cont_name, pdc);
    curl_x_obj = PDCobj_open("curl_x", pdc);
    curl_y_obj = PDCobj_open("curl_y", pdc);
    curl_z_obj = PDCobj_open("curl_z", pdc);

    if (rank == 0) {
        prop_double = PDCprop_create(PDC_OBJ_CREATE, pdc);
        PDCprop_set_obj_type(prop_double, PDC_DOUBLE);
        PDCprop_set_obj_dims(prop_double, 3, dims);
        PDCprop_set_obj_user_id(prop_double, getuid());
        PDCprop_set_obj_time_step(prop_double, 0);
        PDCprop_set_obj_app_name(prop_double, "BenchCurlVorticity");
        PDCprop_set_obj_tags(prop_double, "tag0=1");
        PDCprop_set_obj_transfer_region_type(prop_double, PDC_REGION_STATIC);

        mag_obj = PDCobj_create(cont, "vorticity_magnitude", prop_double);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank != 0)
        mag_obj = PDCobj_open("vorticity_magnitude", pdc);

    pdcid_t reg        = PDCregion_create(3, local_offset, region_len);
    pdcid_t reg_global = PDCregion_create(3, global_offset, region_len);

    pdcid_t tf_dg_id = 0;
    if (compress) {
        tf_dg_id = PDCtf_dg_json_create(TF_GRAPHS_DIR "zfp_gpu.json");
        if (tf_dg_id == 0) {
            fprintf(stderr, "PDCtf_dg_json_create (zfp_gpu) failed\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        PDCtf_attach_to_region(tf_dg_id, mag_obj, reg_global, "decompressed", "compressed");
    }

    MPI_Barrier(MPI_COMM_WORLD);
    t_setup1 = MPI_Wtime();

    t_readback0 = MPI_Wtime();
    do_transfer(curl_x_rb, PDC_READ, curl_x_obj, reg, reg_global, "readback curl_x");
    do_transfer(curl_y_rb, PDC_READ, curl_y_obj, reg, reg_global, "readback curl_y");
    do_transfer(curl_z_rb, PDC_READ, curl_z_obj, reg, reg_global, "readback curl_z");
    MPI_Barrier(MPI_COMM_WORLD);
    t_readback1 = MPI_Wtime();

    t_compute0 = MPI_Wtime();
    for (i = 0; i < n_elem; ++i) {
        double cx = curl_x_rb[i], cy = curl_y_rb[i], cz = curl_z_rb[i];
        mag[i] = sqrt(cx * cx + cy * cy + cz * cz);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    t_compute1 = MPI_Wtime();

    t_writeback0 = MPI_Wtime();
    do_transfer(mag, PDC_WRITE, mag_obj, reg, reg_global, "writeback vorticity_magnitude");
    MPI_Barrier(MPI_COMM_WORLD);
    t_writeback1 = MPI_Wtime();

    /* Correctness check (not timed): u/v/w were generated with the same
     * deterministic pattern by bench_curl_write, and curl from them by
     * bench_curl_compute using the identical shared kernel -- recompute
     * both locally here rather than reading everything back a second
     * time. */
    float *u = (float *)malloc(sizeof(float) * n_elem);
    float *v = (float *)malloc(sizeof(float) * n_elem);
    float *w = (float *)malloc(sizeof(float) * n_elem);
    for (i = 0; i < n_elem; ++i) {
        u[i] = (float)((i % 1000) + 1);
        v[i] = (float)(((i + 137) % 1000) + 1);
        w[i] = (float)(((i + 613) % 1000) + 1);
    }
    double *curl_x_expected = (double *)malloc(sizeof(double) * n_elem);
    double *curl_y_expected = (double *)malloc(sizeof(double) * n_elem);
    double *curl_z_expected = (double *)malloc(sizeof(double) * n_elem);
    curl_math_compute(u, v, w, (size_t)nx, (size_t)ny, (size_t)nz_per_rank, curl_x_expected, curl_y_expected,
                      curl_z_expected);

    int local_bad = 0;
    for (i = 0; i < n_elem; ++i) {
        double expected =
            sqrt(curl_x_expected[i] * curl_x_expected[i] + curl_y_expected[i] * curl_y_expected[i] +
                 curl_z_expected[i] * curl_z_expected[i]);
        if (fabs(mag[i] - expected) > EPSILON) {
            local_bad++;
            break;
        }
    }
    int global_bad = 0;
    MPI_Reduce(&local_bad, &global_bad, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (compress)
        PDCtf_close_dg(tf_dg_id);
    PDCregion_close(reg);
    PDCregion_close(reg_global);
    PDCobj_close(curl_x_obj);
    PDCobj_close(curl_y_obj);
    PDCobj_close(curl_z_obj);
    PDCobj_close(mag_obj);
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
        printf("curl_posthoc_analyze,%d,%ld,%ld,%ld,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%d\n", nranks, nx, ny,
               nz_per_rank, compress, max_setup, max_readback, max_compute, max_writeback, total, global_bad);
        fflush(stdout);
    }

    free(curl_x_rb);
    free(curl_y_rb);
    free(curl_z_rb);
    free(mag);
    free(u);
    free(v);
    free(w);
    free(curl_x_expected);
    free(curl_y_expected);
    free(curl_z_expected);

    MPI_Finalize();
    return global_bad != 0;
}
