/*
 * Copyright Notice for
 * Proactive Data Containers (PDC) Software Library and Utilities
 * -----------------------------------------------------------------------------

 *** Copyright Notice ***

 * Proactive Data Containers (PDC) Copyright (c) 2017, The Regents of the
 * University of California, through Lawrence Berkeley National Laboratory,
 * UChicago Argonne, LLC, operator of Argonne National Laboratory, and The HDF
 * Group (subject to receipt of any required approvals from the U.S. Dept. of
 * Energy).  All rights reserved.

 * If you have questions about your rights to use or distribute this software,
 * please contact Berkeley Lab's Innovation & Partnerships Office at  IPO@lbl.gov.

 * NOTICE.  This Software was developed under funding from the U.S. Department of
 * Energy and the U.S. Government consequently retains certain rights. As such, the
 * U.S. Government has been granted for itself and others acting on its behalf a
 * paid-up, nonexclusive, irrevocable, worldwide license in the Software to
 * reproduce, distribute copies to the public, prepare derivative works, and
 * perform publicly and display publicly, and to permit other to do so.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <inttypes.h>
#include "pdc.h"
#include "pdc_timing.h"

#define NPARTICLES 8388608

double
uniform_random_number()
{
    return (((double)rand()) / ((double)(RAND_MAX)));
}

void
print_usage()
{
    LOG_JUST_PRINT("Usage: srun -n ./vpicio #particles #steps sleep_time(s)\n");
}

int
main(int argc, char **argv)
{
    int     rank = 0, size = 1;
    pdcid_t pdc_id, cont_prop, cont_id, region_local, region_remote;
    pdcid_t obj_prop_float, obj_prop_int;
    pdcid_t obj_xx, obj_yy, obj_zz, obj_pxx, obj_pyy, obj_pzz, obj_id11, obj_id22;
#ifdef ENABLE_MPI
    MPI_Comm comm;
#else
    int comm = 1;
#endif
    float     *x, *y, *z, *px, *py, *pz, *x_r, *y_r, *z_r, *px_r, *py_r, *pz_r;
    int       *id1, *id2, *id1_r, *id2_r;
    int        x_dim = 64, y_dim = 64, z_dim = 64, ndim = 1, steps = 1, sleeptime = 0;
    uint64_t   numparticles, dims[1], offset_local[1], offset_remote[1], mysize[1];
    double     t0, t1;
    char       cur_time[64];
    time_t     t;
    struct tm *tm;

    pdcid_t transfer_request_x, transfer_request_y, transfer_request_z, transfer_request_px,
        transfer_request_py, transfer_request_pz, transfer_request_id1, transfer_request_id2;
    pdcid_t transfer_requests[8];

#ifdef ENABLE_MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_dup(MPI_COMM_WORLD, &comm);
#endif

    numparticles = NPARTICLES;
    if (argc == 5) {
        numparticles = atoll(argv[1]);
        steps        = atoi(argv[2]);
        sleeptime    = atoi(argv[3]);
    }
    else {
        LOG_ERROR("5 paramters are required\n");
        print_usage();
        return FAIL;
    }
    if (rank == 0)
        LOG_INFO("Writing/Reading %" PRIu64
                 " number of particles for %d steps with %d clients with sleep time %ds.\n",
                 numparticles, steps, size, sleeptime);

    dims[0] = numparticles * size;

    // write buffers
    x   = (float *)malloc(numparticles * sizeof(float));
    y   = (float *)malloc(numparticles * sizeof(float));
    z   = (float *)malloc(numparticles * sizeof(float));
    px  = (float *)malloc(numparticles * sizeof(float));
    py  = (float *)malloc(numparticles * sizeof(float));
    pz  = (float *)malloc(numparticles * sizeof(float));
    id1 = (int *)malloc(numparticles * sizeof(int));
    id2 = (int *)malloc(numparticles * sizeof(int));

    // read buffers
    x_r   = (float *)malloc(numparticles * sizeof(float));
    y_r   = (float *)malloc(numparticles * sizeof(float));
    z_r   = (float *)malloc(numparticles * sizeof(float));
    px_r  = (float *)malloc(numparticles * sizeof(float));
    py_r  = (float *)malloc(numparticles * sizeof(float));
    pz_r  = (float *)malloc(numparticles * sizeof(float));
    id1_r = (int *)malloc(numparticles * sizeof(int));
    id2_r = (int *)malloc(numparticles * sizeof(int));

    // create a pdc
    pdc_id = PDCinit("pdc");

    // create a container property
    cont_prop = PDCprop_create(PDC_CONT_CREATE, pdc_id);
    if (cont_prop <= 0) {
        LOG_ERROR("Failed to create container property");
        return FAIL;
    }
    // create a container
    cont_id = PDCcont_create_col("c1", cont_prop);
    if (cont_id <= 0) {
        LOG_ERROR("Failed to create container");
        return FAIL;
    }
    // create an object property
    obj_prop_float = PDCprop_create(PDC_OBJ_CREATE, pdc_id);
    PDCprop_set_obj_dims(obj_prop_float, 1, dims);
    PDCprop_set_obj_type(obj_prop_float, PDC_FLOAT);
    PDCprop_set_obj_user_id(obj_prop_float, getuid());
    PDCprop_set_obj_app_name(obj_prop_float, "VPICIO");
    PDCprop_set_obj_tags(obj_prop_float, "tag0=1");
    PDCprop_set_obj_transfer_region_type(obj_prop_float, PDC_REGION_LOCAL);

    obj_prop_int = PDCprop_obj_dup(obj_prop_float);
    PDCprop_set_obj_type(obj_prop_int, PDC_INT);
    PDCprop_set_obj_transfer_region_type(obj_prop_int, PDC_REGION_LOCAL);

    for (uint64_t i = 0; i < numparticles; i++) {
        id1[i] = i;
        id2[i] = i * 2;
        x[i]   = uniform_random_number() * x_dim;
        y[i]   = uniform_random_number() * y_dim;
        z[i]   = ((float)id1[i] / numparticles) * z_dim;
        px[i]  = uniform_random_number() * x_dim;
        py[i]  = uniform_random_number() * y_dim;
        pz[i]  = ((float)id2[i] / numparticles) * z_dim;
    }

    offset_local[0]  = 0;
    offset_remote[0] = rank * numparticles;
    mysize[0]        = numparticles;

    // create local and remote region
    region_local  = PDCregion_create(ndim, offset_local, mysize);
    region_remote = PDCregion_create(ndim, offset_remote, mysize);

    for (int iter = 0; iter < steps; iter++) {
#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        PDC_get_time_str(cur_time);
        if (rank == 0)
            LOG_INFO("\n[%s] #Step  %d\n", cur_time, iter);
        t0 = MPI_Wtime();
#endif
        PDCprop_set_obj_time_step(obj_prop_float, iter);
        PDCprop_set_obj_time_step(obj_prop_int, iter);

        obj_xx = PDCobj_create_mpi(cont_id, "obj-var-xx", obj_prop_float, 0, comm);
        if (obj_xx == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "x");
            return FAIL;
        }

        obj_yy = PDCobj_create_mpi(cont_id, "obj-var-yy", obj_prop_float, 0, comm);
        if (obj_yy == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "y");
            return FAIL;
        }
        obj_zz = PDCobj_create_mpi(cont_id, "obj-var-zz", obj_prop_float, 0, comm);
        if (obj_zz == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "z");
            return FAIL;
        }
        obj_pxx = PDCobj_create_mpi(cont_id, "obj-var-pxx", obj_prop_float, 0, comm);
        if (obj_pxx == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "px");
            return FAIL;
        }
        obj_pyy = PDCobj_create_mpi(cont_id, "obj-var-pyy", obj_prop_float, 0, comm);
        if (obj_pyy == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "py");
            return FAIL;
        }
        obj_pzz = PDCobj_create_mpi(cont_id, "obj-var-pzz", obj_prop_float, 0, comm);
        if (obj_pzz == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "pz");
            return FAIL;
        }

        obj_id11 = PDCobj_create_mpi(cont_id, "id11", obj_prop_int, 0, comm);
        if (obj_id11 == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "id1");
            return FAIL;
        }
        obj_id22 = PDCobj_create_mpi(cont_id, "id22", obj_prop_int, 0, comm);
        if (obj_id22 == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "id2");
            return FAIL;
        }

        // pdcid_t obj_xx, obj_yy, obj_zz, obj_pxx, obj_pyy, obj_pzz, obj_id11, obj_id22;
        if (!strcmp(argv[4], "zfp")) {
            pdcid_t dg_id = PDCtf_dg_json_create(TF_GRAPHS_DIR "compression.json");
            PDCtf_attach_to_obj(dg_id, obj_xx, "decompressed", "compressed");
            PDCtf_attach_to_obj(dg_id, obj_yy, "decompressed", "compressed");
            PDCtf_attach_to_obj(dg_id, obj_zz, "decompressed", "compressed");
            PDCtf_attach_to_obj(dg_id, obj_pxx, "decompressed", "compressed");
            PDCtf_attach_to_obj(dg_id, obj_pyy, "decompressed", "compressed");
            PDCtf_attach_to_obj(dg_id, obj_pzz, "decompressed", "compressed");
            PDCtf_attach_to_obj(dg_id, obj_id11, "decompressed", "compressed");
            PDCtf_attach_to_obj(dg_id, obj_id22, "decompressed", "compressed");
        }
        else if (!strcmp(argv[4], "zfp_encrypt")) {
            pdcid_t dg_id = PDCtf_dg_json_create(TF_GRAPHS_DIR "compression_encryption.json");
            PDCtf_attach_to_obj(dg_id, obj_xx, "decompressed", "encrypted");
            PDCtf_attach_to_obj(dg_id, obj_yy, "decompressed", "encrypted");
            PDCtf_attach_to_obj(dg_id, obj_zz, "decompressed", "encrypted");
            PDCtf_attach_to_obj(dg_id, obj_pxx, "decompressed", "encrypted");
            PDCtf_attach_to_obj(dg_id, obj_pyy, "decompressed", "encrypted");
            PDCtf_attach_to_obj(dg_id, obj_pzz, "decompressed", "encrypted");
            PDCtf_attach_to_obj(dg_id, obj_id11, "decompressed", "encrypted");
            PDCtf_attach_to_obj(dg_id, obj_id22, "decompressed", "encrypted");
        }
        else if (strcmp(argv[4], "raw")) {
            LOG_ERROR("Invalid transform requested\n");
            print_usage();
            return FAIL;
        }

#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        t1 = MPI_Wtime();
        PDC_get_time_str(cur_time);
        if (rank == 0)
            LOG_INFO("[%s] Obj create time: %.5e\n", cur_time, t1 - t0);
#endif

        transfer_requests[0] =
            PDCregion_transfer_create(&x[0], PDC_WRITE, obj_xx, region_local, region_remote);
        if (transfer_requests[0] == 0) {
            LOG_ERROR("x transfer request creation failed\n");
            return FAIL;
        }
        transfer_requests[1] =
            PDCregion_transfer_create(&y[0], PDC_WRITE, obj_yy, region_local, region_remote);
        if (transfer_requests[1] == 0) {
            LOG_ERROR("y transfer request creation failed\n");
            return FAIL;
        }
        transfer_requests[2] =
            PDCregion_transfer_create(&z[0], PDC_WRITE, obj_zz, region_local, region_remote);
        if (transfer_requests[2] == 0) {
            LOG_ERROR("z transfer request creation failed\n");
            return FAIL;
        }
        transfer_requests[3] =
            PDCregion_transfer_create(&px[0], PDC_WRITE, obj_pxx, region_local, region_remote);
        if (transfer_requests[3] == 0) {
            LOG_ERROR("px transfer request creation failed\n");
            return FAIL;
        }
        transfer_requests[4] =
            PDCregion_transfer_create(&py[0], PDC_WRITE, obj_pyy, region_local, region_remote);
        if (transfer_requests[4] == 0) {
            LOG_ERROR("py transfer request creation failed\n");
            return FAIL;
        }
        transfer_requests[5] =
            PDCregion_transfer_create(&pz[0], PDC_WRITE, obj_pzz, region_local, region_remote);
        if (transfer_requests[5] == 0) {
            LOG_ERROR("pz transfer request creation failed\n");
            return FAIL;
        }
        transfer_requests[6] =
            PDCregion_transfer_create(&id1[0], PDC_WRITE, obj_id11, region_local, region_remote);
        if (transfer_requests[6] == 0) {
            LOG_ERROR("id1 transfer request creation failed\n");
            return FAIL;
        }
        transfer_requests[7] =
            PDCregion_transfer_create(&id2[0], PDC_WRITE, obj_id22, region_local, region_remote);
        if (transfer_requests[7] == 0) {
            LOG_ERROR("id2 transfer request creation failed\n");
            return FAIL;
        }

#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        t0 = MPI_Wtime();
        PDC_get_time_str(cur_time);
        if (rank == 0)
            LOG_INFO("[%s] Transfer create time: %.5e\n", cur_time, t0 - t1);
#endif

#ifdef ENABLE_MPI
        if (PDCregion_transfer_start_all_mpi(transfer_requests, 8, MPI_COMM_WORLD) != SUCCEED) {
#else
        if (PDCregion_transfer_start_all(transfer_requests, 8) != SUCCEED) {
#endif
            LOG_ERROR("Failed to start transfer requests\n");
            return FAIL;
        }

#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        t1 = MPI_Wtime();
        PDC_get_time_str(cur_time);
        if (rank == 0)
            LOG_INFO("[%s] Transfer start time: %.5e\n", cur_time, t1 - t0);
#endif
        // Emulate compute with sleep
        if (iter != steps - 1) {
            PDC_get_time_str(cur_time);
            if (rank == 0)
                LOG_INFO("[%s] Sleep start: %llu.00\n", cur_time, sleeptime);
            sleep(sleeptime);
            PDC_get_time_str(cur_time);
            if (rank == 0)
                LOG_INFO("[%s] Sleep end: %llu.00\n", cur_time, sleeptime);
        }

#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        t0 = MPI_Wtime();
#endif

        if (PDCregion_transfer_wait_all(transfer_requests, 8) != SUCCEED) {
            LOG_ERROR("Failed to transfer wait all\n");
            return FAIL;
        }

#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        t1 = MPI_Wtime();
        PDC_get_time_str(cur_time);
        if (rank == 0)
            LOG_INFO("[%s] Transfer wait time: %.5e\n", cur_time, t1 - t0);
#endif

        for (int j = 0; j < 8; j++) {
            if (PDCregion_transfer_close(transfer_requests[j]) != SUCCEED) {
                LOG_ERROR("region transfer close failed\n");
                return FAIL;
            }
        }

#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        t0 = MPI_Wtime();
        PDC_get_time_str(cur_time);
        if (rank == 0)
            LOG_INFO("[%s] Transfer close time: %.5e\n", cur_time, t0 - t1);
#endif

        if (PDCobj_close(obj_xx) != SUCCEED) {
            LOG_ERROR("Failed to close obj_xx\n");
            return FAIL;
        }
        if (PDCobj_close(obj_yy) != SUCCEED) {
            LOG_ERROR("Failed to close object obj_yy\n");
            return FAIL;
        }
        if (PDCobj_close(obj_zz) != SUCCEED) {
            LOG_ERROR("Failed to close object obj_zz\n");
            return FAIL;
        }
        if (PDCobj_close(obj_pxx) != SUCCEED) {
            LOG_ERROR("Failed to close object obj_pxx\n");
            return FAIL;
        }
        if (PDCobj_close(obj_pyy) != SUCCEED) {
            LOG_ERROR("Failed to close object obj_pyy\n");
            return FAIL;
        }
        if (PDCobj_close(obj_pzz) != SUCCEED) {
            LOG_ERROR("Failed to close object obj_pzz\n");
            return FAIL;
        }
        if (PDCobj_close(obj_id11) != SUCCEED) {
            LOG_ERROR("Failed to close object obj_id11\n");
            return FAIL;
        }
        if (PDCobj_close(obj_id22) != SUCCEED) {
            LOG_ERROR("Failed to close object obj_id22\n");
            return FAIL;
        }

#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        t1 = MPI_Wtime();
        PDC_get_time_str(cur_time);
        if (rank == 0)
            LOG_INFO("[%s] Obj close time: %.5e\n", cur_time, t1 - t0);
#endif
    } // End for steps

    for (int iter = 0; iter < steps; iter++) {
#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        PDC_get_time_str(cur_time);
        if (rank == 0)
            LOG_INFO("\n[%s] #Step  %d\n", cur_time, iter);
        t0 = MPI_Wtime();
#endif
        PDCprop_set_obj_time_step(obj_prop_float, iter);
        PDCprop_set_obj_time_step(obj_prop_int, iter);

        obj_xx = PDCobj_open_col("obj-var-xx", pdc_id);
        if (obj_xx == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "x");
            return FAIL;
        }

        obj_yy = PDCobj_open_col("obj-var-yy", pdc_id);
        if (obj_yy == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "y");
            return FAIL;
        }
        obj_zz = PDCobj_open_col("obj-var-zz", pdc_id);
        if (obj_zz == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "z");
            return FAIL;
        }
        obj_pxx = PDCobj_open_col("obj-var-pxx", pdc_id);
        if (obj_pxx == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "px");
            return FAIL;
        }
        obj_pyy = PDCobj_open_col("obj-var-pyy", pdc_id);
        if (obj_pyy == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "py");
            return FAIL;
        }
        obj_pzz = PDCobj_open_col("obj-var-pzz", pdc_id);
        if (obj_pzz == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "pz");
            return FAIL;
        }

        obj_id11 = PDCobj_open_col("id11", pdc_id);
        if (obj_id11 == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "id1");
            return FAIL;
        }
        obj_id22 = PDCobj_open_col("id22", pdc_id);
        if (obj_id22 == 0) {
            LOG_ERROR("Error getting an object id of %s from server\n", "id2");
            return FAIL;
        }

        // pdcid_t obj_xx, obj_yy, obj_zz, obj_pxx, obj_pyy, obj_pzz, obj_id11, obj_id22;
        if (!strcmp(argv[4], "zfp")) {
            pdcid_t dg_id = PDCtf_dg_json_create(TF_GRAPHS_DIR "compression.json");
            PDCtf_attach_to_obj(dg_id, obj_xx, "decompressed", "compressed");
            PDCtf_attach_to_obj(dg_id, obj_yy, "decompressed", "compressed");
            PDCtf_attach_to_obj(dg_id, obj_zz, "decompressed", "compressed");
            PDCtf_attach_to_obj(dg_id, obj_pxx, "decompressed", "compressed");
            PDCtf_attach_to_obj(dg_id, obj_pyy, "decompressed", "compressed");
            PDCtf_attach_to_obj(dg_id, obj_pzz, "decompressed", "compressed");
            PDCtf_attach_to_obj(dg_id, obj_id11, "decompressed", "compressed");
            PDCtf_attach_to_obj(dg_id, obj_id22, "decompressed", "compressed");
        }
        else if (!strcmp(argv[4], "zfp_encrypt")) {
            pdcid_t dg_id = PDCtf_dg_json_create(TF_GRAPHS_DIR "compression_encryption.json");
            PDCtf_attach_to_obj(dg_id, obj_xx, "decompressed", "encrypted");
            PDCtf_attach_to_obj(dg_id, obj_yy, "decompressed", "encrypted");
            PDCtf_attach_to_obj(dg_id, obj_zz, "decompressed", "encrypted");
            PDCtf_attach_to_obj(dg_id, obj_pxx, "decompressed", "encrypted");
            PDCtf_attach_to_obj(dg_id, obj_pyy, "decompressed", "encrypted");
            PDCtf_attach_to_obj(dg_id, obj_pzz, "decompressed", "encrypted");
            PDCtf_attach_to_obj(dg_id, obj_id11, "decompressed", "encrypted");
            PDCtf_attach_to_obj(dg_id, obj_id22, "decompressed", "encrypted");
        }
        else if (strcmp(argv[4], "raw")) {
            LOG_ERROR("Invalid transform requested\n");
            print_usage();
            return FAIL;
        }

#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        t1 = MPI_Wtime();
        PDC_get_time_str(cur_time);
        if (rank == 0)
            LOG_INFO("[%s] Obj create time: %.5e\n", cur_time, t1 - t0);
#endif

        transfer_requests[0] =
            PDCregion_transfer_create(&x_r[0], PDC_READ, obj_xx, region_local, region_remote);
        if (transfer_requests[0] == 0) {
            LOG_ERROR("x transfer request creation failed\n");
            return FAIL;
        }
        transfer_requests[1] =
            PDCregion_transfer_create(&y_r[0], PDC_READ, obj_yy, region_local, region_remote);
        if (transfer_requests[1] == 0) {
            LOG_ERROR("y transfer request creation failed\n");
            return FAIL;
        }
        transfer_requests[2] =
            PDCregion_transfer_create(&z_r[0], PDC_READ, obj_zz, region_local, region_remote);
        if (transfer_requests[2] == 0) {
            LOG_ERROR("z transfer request creation failed\n");
            return FAIL;
        }
        transfer_requests[3] =
            PDCregion_transfer_create(&px_r[0], PDC_READ, obj_pxx, region_local, region_remote);
        if (transfer_requests[3] == 0) {
            LOG_ERROR("px transfer request creation failed\n");
            return FAIL;
        }
        transfer_requests[4] =
            PDCregion_transfer_create(&py_r[0], PDC_READ, obj_pyy, region_local, region_remote);
        if (transfer_requests[4] == 0) {
            LOG_ERROR("py transfer request creation failed\n");
            return FAIL;
        }
        transfer_requests[5] =
            PDCregion_transfer_create(&pz_r[0], PDC_READ, obj_pzz, region_local, region_remote);
        if (transfer_requests[5] == 0) {
            LOG_ERROR("pz transfer request creation failed\n");
            return FAIL;
        }
        transfer_requests[6] =
            PDCregion_transfer_create(&id1_r[0], PDC_READ, obj_id11, region_local, region_remote);
        if (transfer_requests[6] == 0) {
            LOG_ERROR("id1 transfer request creation failed\n");
            return FAIL;
        }
        transfer_requests[7] =
            PDCregion_transfer_create(&id2_r[0], PDC_READ, obj_id22, region_local, region_remote);
        if (transfer_requests[7] == 0) {
            LOG_ERROR("id2 transfer request creation failed\n");
            return FAIL;
        }

#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        t0 = MPI_Wtime();
        PDC_get_time_str(cur_time);
        if (rank == 0)
            LOG_INFO("[%s] Transfer create time: %.5e\n", cur_time, t0 - t1);
#endif

#ifdef ENABLE_MPI
        if (PDCregion_transfer_start_all_mpi(transfer_requests, 8, MPI_COMM_WORLD) != SUCCEED) {
#else
        if (PDCregion_transfer_start_all(transfer_requests, 8) != SUCCEED) {
#endif
            LOG_ERROR("Failed to start transfer requests\n");
            return FAIL;
        }

#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        t1 = MPI_Wtime();
        PDC_get_time_str(cur_time);
        if (rank == 0)
            LOG_INFO("[%s] Transfer start time: %.5e\n", cur_time, t1 - t0);
#endif
        // Emulate compute with sleep
        if (iter != steps - 1) {
            PDC_get_time_str(cur_time);
            if (rank == 0)
                LOG_INFO("[%s] Sleep start: %llu.00\n", cur_time, sleeptime);
            sleep(sleeptime);
            PDC_get_time_str(cur_time);
            if (rank == 0)
                LOG_INFO("[%s] Sleep end: %llu.00\n", cur_time, sleeptime);
        }

#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        t0 = MPI_Wtime();
#endif

        if (PDCregion_transfer_wait_all(transfer_requests, 8) != SUCCEED) {
            LOG_ERROR("Failed to transfer wait all\n");
            return FAIL;
        }

#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        t1 = MPI_Wtime();
        PDC_get_time_str(cur_time);
        if (rank == 0)
            LOG_INFO("[%s] Transfer wait time: %.5e\n", cur_time, t1 - t0);
#endif

        for (int j = 0; j < 8; j++) {
            if (PDCregion_transfer_close(transfer_requests[j]) != SUCCEED) {
                LOG_ERROR("region transfer close failed\n");
                return FAIL;
            }
        }

#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        t0 = MPI_Wtime();
        PDC_get_time_str(cur_time);
        if (rank == 0)
            LOG_INFO("[%s] Transfer close time: %.5e\n", cur_time, t0 - t1);
#endif

        if (PDCobj_close(obj_xx) != SUCCEED) {
            LOG_ERROR("Failed to close obj_xx\n");
            return FAIL;
        }
        if (PDCobj_close(obj_yy) != SUCCEED) {
            LOG_ERROR("Failed to close object obj_yy\n");
            return FAIL;
        }
        if (PDCobj_close(obj_zz) != SUCCEED) {
            LOG_ERROR("Failed to close object obj_zz\n");
            return FAIL;
        }
        if (PDCobj_close(obj_pxx) != SUCCEED) {
            LOG_ERROR("Failed to close object obj_pxx\n");
            return FAIL;
        }
        if (PDCobj_close(obj_pyy) != SUCCEED) {
            LOG_ERROR("Failed to close object obj_pyy\n");
            return FAIL;
        }
        if (PDCobj_close(obj_pzz) != SUCCEED) {
            LOG_ERROR("Failed to close object obj_pzz\n");
            return FAIL;
        }
        if (PDCobj_close(obj_id11) != SUCCEED) {
            LOG_ERROR("Failed to close object obj_id11\n");
            return FAIL;
        }
        if (PDCobj_close(obj_id22) != SUCCEED) {
            LOG_ERROR("Failed to close object obj_id22\n");
            return FAIL;
        }

#ifdef ENABLE_MPI
        MPI_Barrier(MPI_COMM_WORLD);
        t1 = MPI_Wtime();
        PDC_get_time_str(cur_time);
        if (rank == 0)
            LOG_INFO("[%s] Obj close time: %.5e\n", cur_time, t1 - t0);
#endif
    } // End for steps

    for (uint64_t i = 0; i < numparticles; i++) {
        if (id1[i] != id1_r[i]) {
            LOG_ERROR("id1 mismatch at index %lu: wrote %u, read %u\n", i, id1[i], id1_r[i]);
            return FAIL;
        }
        if (id2[i] != id2_r[i]) {
            LOG_ERROR("id2 mismatch at index %lu: wrote %u, read %u\n", i, id2[i], id2_r[i]);
            return FAIL;
        }
        if (x[i] != x_r[i]) {
            LOG_ERROR("x mismatch at index %lu: wrote %f, read %f\n", i, x[i], x_r[i]);
            return FAIL;
        }
        if (y[i] != y_r[i]) {
            LOG_ERROR("y mismatch at index %lu: wrote %f, read %f\n", i, y[i], y_r[i]);
            return FAIL;
        }
        if (z[i] != z_r[i]) {
            LOG_ERROR("z mismatch at index %lu: wrote %f, read %f\n", i, z[i], z_r[i]);
            return FAIL;
        }
        if (px[i] != px_r[i]) {
            LOG_ERROR("px mismatch at index %lu: wrote %f, read %f\n", i, px[i], px_r[i]);
            return FAIL;
        }
        if (py[i] != py_r[i]) {
            LOG_ERROR("py mismatch at index %lu: wrote %f, read %f\n", i, py[i], py_r[i]);
            return FAIL;
        }
        if (pz[i] != pz_r[i]) {
            LOG_ERROR("pz mismatch at index %lu: wrote %f, read %f\n", i, pz[i], pz_r[i]);
            return FAIL;
        }
    }

    if (PDCprop_close(obj_prop_float) != SUCCEED) {
        LOG_ERROR("Failed to close obj_prop_float\n");
        return FAIL;
    }
    if (PDCprop_close(obj_prop_int) != SUCCEED) {
        LOG_ERROR("Failed to close obj_prop_int\n");
        return FAIL;
    }
    if (PDCregion_close(region_local) != SUCCEED) {
        LOG_ERROR("Failed to close local region \n");
        return FAIL;
    }
    if (PDCobj_close(region_remote) != SUCCEED) {
        LOG_ERROR("Failed to close remote region\n");
        return FAIL;
    }
    if (PDCcont_close(cont_id) != SUCCEED) {
        LOG_ERROR("Failed to close container\n");
        return FAIL;
    }
    if (PDCprop_close(cont_prop) != SUCCEED) {
        LOG_ERROR("Failed to close property\n");
        return FAIL;
    }
    if (PDCclose(pdc_id) != SUCCEED) {
        LOG_ERROR("Failed to close PDC\n");
        return FAIL;
    }

    if (rank == 0)
        LOG_INFO("Data was validated successfully\n");

    free(x);
    free(y);
    free(z);
    free(px);
    free(py);
    free(pz);
    free(id1);
    free(id2);
    free(x_r);
    free(y_r);
    free(z_r);
    free(px_r);
    free(py_r);
    free(pz_r);
    free(id1_r);
    free(id2_r);
#ifdef ENABLE_MPI
    MPI_Finalize();
#endif

    return 0;
}
