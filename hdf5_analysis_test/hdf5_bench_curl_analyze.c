/**
 * Parallel HDF5 curl/vorticity-magnitude benchmark, posthoc phase 3 of 3:
 * opens the file a prior, already-exited hdf5_bench_curl_compute run
 * wrote curl_x/y/z into, reads them back, computes vorticity magnitude
 * client-side, and writes the result out as a fourth dataset
 * (vorticity_magnitude).
 *
 * Usage: hdf5_bench_curl_analyze <nx> <ny> <nz_per_rank> [out_file]
 *
 * Prints one CSV line from rank 0:
 *   mode,n_client_ranks,nx,ny,nz_per_rank,setup_s,readback_s,compute_s,writeback_s,total_s,bad
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <mpi.h>
#include <hdf5.h>

#include "curl_math.h"

#define EPSILON 1e-3

static void
check(hid_t id, const char *what)
{
    if (id < 0) {
        fprintf(stderr, "%s failed\n", what);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
}

static void
write_dataset(hid_t file, const char *name, hid_t mem_type, hid_t file_type, void *buf, hsize_t dims[3],
              hsize_t offset[3], hsize_t count[3])
{
    hid_t filespace = H5Screate_simple(3, dims, NULL);
    check(filespace, "H5Screate_simple (file)");

    hid_t dcpl = H5Pcreate(H5P_DATASET_CREATE);
    check(H5Pset_chunk(dcpl, 3, count), "H5Pset_chunk");

    hid_t dset = H5Dcreate2(file, name, file_type, filespace, H5P_DEFAULT, dcpl, H5P_DEFAULT);
    check(dset, "H5Dcreate2");
    H5Pclose(dcpl);
    H5Sclose(filespace);

    hid_t memspace = H5Screate_simple(3, count, NULL);
    check(memspace, "H5Screate_simple (mem)");

    filespace = H5Dget_space(dset);
    check(H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, count, NULL), "H5Sselect_hyperslab");

    hid_t dxpl = H5Pcreate(H5P_DATASET_XFER);
    H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_COLLECTIVE);

    check(H5Dwrite(dset, mem_type, memspace, filespace, dxpl, buf), "H5Dwrite");

    H5Pclose(dxpl);
    H5Sclose(filespace);
    H5Sclose(memspace);
    H5Dclose(dset);
}

static void
read_dataset(hid_t file, const char *name, hid_t mem_type, void *buf, hsize_t offset[3], hsize_t count[3])
{
    hid_t dset = H5Dopen2(file, name, H5P_DEFAULT);
    check(dset, "H5Dopen2");

    hid_t memspace = H5Screate_simple(3, count, NULL);
    check(memspace, "H5Screate_simple (mem)");

    hid_t filespace = H5Dget_space(dset);
    check(H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, count, NULL), "H5Sselect_hyperslab");

    hid_t dxpl = H5Pcreate(H5P_DATASET_XFER);
    H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_COLLECTIVE);

    check(H5Dread(dset, mem_type, memspace, filespace, dxpl, buf), "H5Dread");

    H5Pclose(dxpl);
    H5Sclose(filespace);
    H5Sclose(memspace);
    H5Dclose(dset);
}

int
main(int argc, char **argv)
{
    int    rank, nranks;
    long   nx, ny, nz_per_rank;
    size_t i;

    double t_setup0, t_setup1, t_readback0, t_readback1, t_compute0, t_compute1, t_writeback0, t_writeback1;

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <nx> <ny> <nz_per_rank> [out_file]\n", argv[0]);
        return 1;
    }
    nx                   = atol(argv[1]);
    ny                   = atol(argv[2]);
    nz_per_rank          = atol(argv[3]);
    const char *out_file = (argc >= 5) ? argv[4] : "hdf5_bench_curl.h5";

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    size_t n_elem = (size_t)nx * (size_t)ny * (size_t)nz_per_rank;

    double *curl_x_rb = (double *)malloc(sizeof(double) * n_elem);
    double *curl_y_rb = (double *)malloc(sizeof(double) * n_elem);
    double *curl_z_rb = (double *)malloc(sizeof(double) * n_elem);
    double *mag       = (double *)malloc(sizeof(double) * n_elem);

    hsize_t dims[3]   = {(hsize_t)nx, (hsize_t)ny, (hsize_t)nranks * (hsize_t)nz_per_rank};
    hsize_t offset[3] = {0, 0, (hsize_t)rank * (hsize_t)nz_per_rank};
    hsize_t count[3]  = {(hsize_t)nx, (hsize_t)ny, (hsize_t)nz_per_rank};

    MPI_Barrier(MPI_COMM_WORLD);
    t_setup0 = MPI_Wtime();

    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    check(H5Pset_fapl_mpio(fapl, MPI_COMM_WORLD, MPI_INFO_NULL), "H5Pset_fapl_mpio");

    hid_t file = H5Fopen(out_file, H5F_ACC_RDWR, fapl);
    check(file, "H5Fopen");
    H5Pclose(fapl);

    MPI_Barrier(MPI_COMM_WORLD);
    t_setup1 = MPI_Wtime();

    t_readback0 = MPI_Wtime();
    read_dataset(file, "curl_x", H5T_NATIVE_DOUBLE, curl_x_rb, offset, count);
    read_dataset(file, "curl_y", H5T_NATIVE_DOUBLE, curl_y_rb, offset, count);
    read_dataset(file, "curl_z", H5T_NATIVE_DOUBLE, curl_z_rb, offset, count);
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
    write_dataset(file, "vorticity_magnitude", H5T_NATIVE_DOUBLE, H5T_IEEE_F64LE, mag, dims, offset, count);
    MPI_Barrier(MPI_COMM_WORLD);
    t_writeback1 = MPI_Wtime();

    H5Fclose(file);

    /* Correctness check (not timed): u/v/w were generated with the same
     * deterministic pattern by hdf5_bench_curl_write, and curl from them
     * by hdf5_bench_curl_compute using the identical shared kernel --
     * recompute both locally here rather than reading everything back a
     * second time. */
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
        printf("curl_posthoc_analyze,%d,%ld,%ld,%ld,%.6f,%.6f,%.6f,%.6f,%.6f,%d\n", nranks, nx, ny,
               nz_per_rank, max_setup, max_readback, max_compute, max_writeback, total, global_bad);
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
