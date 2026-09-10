/**
 * E3SM-shaped curl/vorticity-magnitude benchmark, posthoc phase 1 of 3:
 * write a local (nx, ny, nz_per_rank) block of a synthetic wind-velocity
 * field (u, v, w) per rank as plain PDC objects (no graph attached) and
 * exit. See bench_curl_compute.c (phase 2: read u,v,w back, compute
 * curl, write it out) and bench_curl_analyze.c (phase 3: read curl back,
 * compute magnitude, write it out) -- three genuinely separate processes,
 * with the PDC server closed (checkpointing) and restarted (reloading
 * that checkpoint) between each, matching a real posthoc workflow.
 *
 * Usage: bench_curl_write <nx> <ny> <nz_per_rank>
 *
 * Prints one CSV line from rank 0:
 *   mode,n_client_ranks,nx,ny,nz_per_rank,setup_s,write_s
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
    long   nx, ny, nz_per_rank;
    size_t i;

    double t_setup0, t_setup1, t_write0, t_write1;

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

    float *u = (float *)malloc(sizeof(float) * n_elem);
    float *v = (float *)malloc(sizeof(float) * n_elem);
    float *w = (float *)malloc(sizeof(float) * n_elem);

    /* Same deterministic pattern bench_curl_compute.c uses to
     * independently regenerate expected values for its correctness
     * check. */
    for (i = 0; i < n_elem; ++i) {
        u[i] = (float)((i % 1000) + 1);
        v[i] = (float)(((i + 137) % 1000) + 1);
        w[i] = (float)(((i + 613) % 1000) + 1);
    }

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

    const char *cont_name = "curl_bench_shared";
    pdcid_t     cont_prop = 0, cont = 0;
    pdcid_t     prop_float = 0;
    pdcid_t     u_obj = 0, v_obj = 0, w_obj = 0;

    if (rank == 0) {
        cont_prop = PDCprop_create(PDC_CONT_CREATE, pdc);
        cont      = PDCcont_create(cont_name, cont_prop);

        prop_float = PDCprop_create(PDC_OBJ_CREATE, pdc);
        PDCprop_set_obj_type(prop_float, PDC_FLOAT);
        PDCprop_set_obj_dims(prop_float, 3, dims);
        PDCprop_set_obj_user_id(prop_float, getuid());
        PDCprop_set_obj_time_step(prop_float, 0);
        PDCprop_set_obj_app_name(prop_float, "BenchCurlVorticity");
        PDCprop_set_obj_tags(prop_float, "tag0=1");
        PDCprop_set_obj_transfer_region_type(prop_float, PDC_REGION_STATIC);

        u_obj = PDCobj_create(cont, "u", prop_float);
        v_obj = PDCobj_create(cont, "v", prop_float);
        w_obj = PDCobj_create(cont, "w", prop_float);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank != 0) {
        cont  = PDCcont_open(cont_name, pdc);
        u_obj = PDCobj_open("u", pdc);
        v_obj = PDCobj_open("v", pdc);
        w_obj = PDCobj_open("w", pdc);
    }

    pdcid_t reg        = PDCregion_create(3, local_offset, region_len);
    pdcid_t reg_global = PDCregion_create(3, global_offset, region_len);

    MPI_Barrier(MPI_COMM_WORLD);
    t_setup1 = MPI_Wtime();

    t_write0 = MPI_Wtime();
    do_transfer(u, PDC_WRITE, u_obj, reg, reg_global, "write u");
    do_transfer(v, PDC_WRITE, v_obj, reg, reg_global, "write v");
    do_transfer(w, PDC_WRITE, w_obj, reg, reg_global, "write w");
    MPI_Barrier(MPI_COMM_WORLD);
    t_write1 = MPI_Wtime();

    PDCregion_close(reg);
    PDCregion_close(reg_global);
    PDCobj_close(u_obj);
    PDCobj_close(v_obj);
    PDCobj_close(w_obj);
    PDCcont_close(cont);
    if (rank == 0) {
        PDCprop_close(prop_float);
        PDCprop_close(cont_prop);
    }
    PDCclose(pdc);

    double local_setup = t_setup1 - t_setup0;
    double local_write  = t_write1 - t_write0;

    double max_setup, max_write;
    MPI_Reduce(&local_setup, &max_setup, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_write, &max_write, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("curl_posthoc_write,%d,%ld,%ld,%ld,%.6f,%.6f\n", nranks, nx, ny, nz_per_rank, max_setup,
               max_write);
        fflush(stdout);
    }

    free(u);
    free(v);
    free(w);

    MPI_Finalize();
    return 0;
}
