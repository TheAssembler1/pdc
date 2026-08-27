/**
 * Parallel HDF5 counterpart to src/tests/analysis/bench_magnitude.c's
 * "posthoc" mode: there is no in-flight transform framework in plain HDF5,
 * so this is the only strategy that applies -- write three vector-component
 * datasets (vx, vy, vz), read them back, compute magnitude client-side, and
 * write the result back as a fourth dataset. Same problem size, same
 * per-rank disjoint hyperslab decomposition, same timed phases as the PDC
 * benchmark, so the two are directly comparable.
 *
 * Usage: hdf5_bench_magnitude <n_elem_per_rank> [out_file]
 *
 * Prints one CSV line from rank 0:
 *   mode,n_client_ranks,n_elem,setup_s,write_s,readback_s,compute_s,writeback_s,total_s,bad
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <mpi.h>
#include <hdf5.h>

#define EPSILON 1e-3

static void
check(hid_t id, const char *what)
{
    if (id < 0) {
        fprintf(stderr, "%s failed\n", what);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
}

/* Collective write of a rank-local buffer into its disjoint slice of a
 * dataset spanning the whole nranks*n_elem domain. */
static void
write_dataset(hid_t file, const char *name, hid_t mem_type, hid_t file_type, void *buf, hsize_t dims[1],
              hsize_t offset[1], hsize_t count[1])
{
    hid_t filespace = H5Screate_simple(1, dims, NULL);
    check(filespace, "H5Screate_simple (file)");
    hid_t dset = H5Dcreate2(file, name, file_type, filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    check(dset, "H5Dcreate2");
    H5Sclose(filespace);

    hid_t memspace = H5Screate_simple(1, count, NULL);
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
read_dataset(hid_t file, const char *name, hid_t mem_type, void *buf, hsize_t offset[1], hsize_t count[1])
{
    hid_t dset = H5Dopen2(file, name, H5P_DEFAULT);
    check(dset, "H5Dopen2");

    hid_t memspace = H5Screate_simple(1, count, NULL);
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
    long   n_elem;
    size_t i;

    double t_setup0, t_setup1, t_write0, t_write1;
    double t_readback0, t_readback1, t_compute0, t_compute1, t_writeback0, t_writeback1;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <n_elem_per_rank> [out_file]\n", argv[0]);
        return 1;
    }
    n_elem               = atol(argv[1]);
    const char *out_file = (argc >= 3) ? argv[2] : "hdf5_bench_magnitude.h5";

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    float * vx    = (float *)malloc(sizeof(float) * n_elem);
    float * vy    = (float *)malloc(sizeof(float) * n_elem);
    float * vz    = (float *)malloc(sizeof(float) * n_elem);
    float * vx_rb = (float *)malloc(sizeof(float) * n_elem);
    float * vy_rb = (float *)malloc(sizeof(float) * n_elem);
    float * vz_rb = (float *)malloc(sizeof(float) * n_elem);
    double *mag   = (double *)malloc(sizeof(double) * n_elem);

    for (i = 0; i < (size_t)n_elem; ++i) {
        vx[i] = (float)((i % 1000) + 1);
        vy[i] = (float)(((i + 137) % 1000) + 1);
        vz[i] = (float)(((i + 613) % 1000) + 1);
    }
    memset(mag, 0, sizeof(double) * n_elem);

    hsize_t dims[1]   = {(hsize_t)nranks * (hsize_t)n_elem};
    hsize_t offset[1] = {(hsize_t)rank * (hsize_t)n_elem};
    hsize_t count[1]  = {(hsize_t)n_elem};

    MPI_Barrier(MPI_COMM_WORLD);
    t_setup0 = MPI_Wtime();

    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    check(H5Pset_fapl_mpio(fapl, MPI_COMM_WORLD, MPI_INFO_NULL), "H5Pset_fapl_mpio");

    hid_t file = H5Fcreate(out_file, H5F_ACC_TRUNC, H5P_DEFAULT, fapl);
    check(file, "H5Fcreate");
    H5Pclose(fapl);

    MPI_Barrier(MPI_COMM_WORLD);
    t_setup1 = MPI_Wtime();

    /* Write vector components. */
    t_write0 = MPI_Wtime();
    write_dataset(file, "vx", H5T_NATIVE_FLOAT, H5T_IEEE_F32LE, vx, dims, offset, count);
    write_dataset(file, "vy", H5T_NATIVE_FLOAT, H5T_IEEE_F32LE, vy, dims, offset, count);
    write_dataset(file, "vz", H5T_NATIVE_FLOAT, H5T_IEEE_F32LE, vz, dims, offset, count);
    MPI_Barrier(MPI_COMM_WORLD);
    t_write1 = MPI_Wtime();

    /* Read the components back to the client. */
    t_readback0 = MPI_Wtime();
    read_dataset(file, "vx", H5T_NATIVE_FLOAT, vx_rb, offset, count);
    read_dataset(file, "vy", H5T_NATIVE_FLOAT, vy_rb, offset, count);
    read_dataset(file, "vz", H5T_NATIVE_FLOAT, vz_rb, offset, count);
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

    /* Write the result back, like a hand-built materialized view. */
    t_writeback0 = MPI_Wtime();
    write_dataset(file, "magnitude", H5T_NATIVE_DOUBLE, H5T_IEEE_F64LE, mag, dims, offset, count);
    MPI_Barrier(MPI_COMM_WORLD);
    t_writeback1 = MPI_Wtime();

    H5Fclose(file);

    /* Correctness check (not timed). */
    int local_bad = 0;
    for (i = 0; i < (size_t)n_elem; ++i) {
        double x = (double)vx[i], y = (double)vy[i], z = (double)vz[i];
        double expected = sqrt(x * x + y * y + z * z);
        if (fabs(mag[i] - expected) > EPSILON) {
            local_bad++;
            break;
        }
    }
    int global_bad = 0;
    MPI_Reduce(&local_bad, &global_bad, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    double local_setup     = t_setup1 - t_setup0;
    double local_write     = t_write1 - t_write0;
    double local_readback  = t_readback1 - t_readback0;
    double local_compute   = t_compute1 - t_compute0;
    double local_writeback = t_writeback1 - t_writeback0;

    double max_setup, max_write, max_readback, max_compute, max_writeback;
    MPI_Reduce(&local_setup, &max_setup, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_write, &max_write, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_readback, &max_readback, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_compute, &max_compute, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_writeback, &max_writeback, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        double total = max_setup + max_write + max_readback + max_compute + max_writeback;
        printf("hdf5,%d,%ld,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d\n", nranks, n_elem, max_setup, max_write,
               max_readback, max_compute, max_writeback, total, global_bad);
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
