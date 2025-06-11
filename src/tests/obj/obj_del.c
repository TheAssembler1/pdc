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
#include <getopt.h>
#include <time.h>
#include "pdc.h"
#include "test_helper.h"

int
main(int argc, char **argv)
{
    pdcid_t pdc, cont_prop, cont, obj_prop, obj1, obj2;
    int     ret_value = TSUCCEED;
    int     rank      = 0;
    char    cont_name[128], obj_name1[128], obj_name2[128];
    // create a pdc
#ifdef ENABLE_MPI
    int size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
#endif

    TASSERT((pdc = PDCinit("pdc")) != 0, "Call to PDCinit succeeded", "Call to PDCinit failed");

    // create a container property
    TASSERT((cont_prop = PDCprop_create(PDC_CONT_CREATE, pdc)) != 0, "Call to PDCprop_create succeeded",
            "Call to PDCprop_create failed");
    // create a container
    sprintf(cont_name, "c%d", rank);
    TASSERT((cont = PDCcont_create(cont_name, cont_prop)) != 0, "Call to PDCcont_create succeeded",
            "Call to PDCcont_create failed");
    // create an object property
    TASSERT((obj_prop = PDCprop_create(PDC_OBJ_CREATE, pdc)) != 0, "Call to PDCprop_create succeeded",
            "Call to PDCprop_create failed");
    // create first object
    sprintf(obj_name1, "o1_%d", rank);
    TASSERT((obj1 = PDCobj_create(cont, obj_name1, obj_prop)) != 0,
            "Call to PDCobj_create succeeded for obj1", "Call to PDCobj_create failed for obj1");
    // create second object
    sprintf(obj_name2, "o2_%d", rank);
    TASSERT((obj2 = PDCobj_create(cont, obj_name2, obj_prop)) != 0,
            "Call to PDCobj_create succeeded for obj2", "Call to PDCobj_create failed for obj2");

    // delete objects
    TASSERT(PDCobj_del(obj1) >= 0, "Call to PDCobj_del succeeded for obj1",
            "Call to PDCobj_del failed for obj1");
    TASSERT(PDCobj_del(obj2) >= 0, "Call to PDCobj_del succeeded for obj2",
            "Call to PDCobj_del failed for obj2");

    // close first object
    TASSERT(PDCobj_close(obj1) >= 0, "Call to PDCobj_close succeeded for obj1",
            "Call to PDCobj_close failed for obj1");
    // close second object
    TASSERT(PDCobj_close(obj2) >= 0, "Call to PDCobj_close succeeded for obj2",
            "Call to PDCobj_close failed for obj2");
    // close a container
    TASSERT(PDCcont_close(cont) >= 0, "Call to PDCcont_close succeeded", "Call to PDCcont_close failed");
    // close a container property
    TASSERT(PDCprop_close(cont_prop) >= 0, "Call to PDCprop_close succeeded", "Call to PDCprop_close failed");
    // close pdc
    TASSERT(PDCclose(pdc) >= 0, "Call to PDCclose succeeded", "Call to PDCclose failed");

done:
#ifdef ENABLE_MPI
    MPI_Finalize();
#endif
    return ret_value;
}
