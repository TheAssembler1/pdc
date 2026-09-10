/**
 * Parallel HDF5 curl/vorticity-magnitude benchmark, posthoc phase 2 of 3:
 * opens the file a prior, already-exited hdf5_bench_curl_write run
 * created, reads the u/v/w datasets back, computes curl client-side via
 * the shared kernel in curl_math.h (the identical kernel the PDC side's
 * builtin and bench_curl_compute.c use, so results are directly
 * comparable), and writes the result out as three datasets (curl_x,
 * curl_y, curl_z). See hdf5_bench_curl_analyze.c for phase 3.
 *
 * Usage: hdf5_bench_curl_compute <nx> <ny> <nz_per_rank> [out_file]
 *
 * Prints one CSV line from rank 0:
 *   mode,n_client_ranks,nx,ny,nz_per_rank,setup_s,readback_s,compute_s,writeback_s,total_s
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mpi.h>
#include <hdf5.h>

#include "curl_math.h"

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

    float * u_rb   = (float *)malloc(sizeof(float) * n_elem);
    float * v_rb   = (float *)malloc(sizeof(float) * n_elem);
    float * w_rb   = (float *)malloc(sizeof(float) * n_elem);
    double *curl_x = (double *)malloc(sizeof(double) * n_elem);
    double *curl_y = (double *)malloc(sizeof(double) * n_elem);
    double *curl_z = (double *)malloc(sizeof(double) * n_elem);

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
    read_dataset(file, "u", H5T_NATIVE_FLOAT, u_rb, offset, count);
    read_dataset(file, "v", H5T_NATIVE_FLOAT, v_rb, offset, count);
    read_dataset(file, "w", H5T_NATIVE_FLOAT, w_rb, offset, count);
    MPI_Barrier(MPI_COMM_WORLD);
    t_readback1 = MPI_Wtime();

    t_compute0 = MPI_Wtime();
    curl_math_compute(u_rb, v_rb, w_rb, (size_t)nx, (size_t)ny, (size_t)nz_per_rank, curl_x, curl_y, curl_z);
    MPI_Barrier(MPI_COMM_WORLD);
    t_compute1 = MPI_Wtime();

    t_writeback0 = MPI_Wtime();
    write_dataset(file, "curl_x", H5T_NATIVE_DOUBLE, H5T_IEEE_F64LE, curl_x, dims, offset, count);
    write_dataset(file, "curl_y", H5T_NATIVE_DOUBLE, H5T_IEEE_F64LE, curl_y, dims, offset, count);
    write_dataset(file, "curl_z", H5T_NATIVE_DOUBLE, H5T_IEEE_F64LE, curl_z, dims, offset, count);
    MPI_Barrier(MPI_COMM_WORLD);
    t_writeback1 = MPI_Wtime();

    H5Fclose(file);

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
