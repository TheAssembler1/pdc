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
#include "pdc.h"
#include "test_helper.h"

int
main(int argc, char **argv)
{
    pdcid_t              pdc, cont_prop, cont, obj_prop;
    perr_t               ret;
    pdcid_t              obj1, obj2;
    struct pdc_obj_info *obj1_info, *obj2_info;
    char                 cont_name[128], obj_name1[128], obj_name2[128];

    int rank = 0, size = 1;
    int ret_value = 0;

    size_t   ndim = 3;
    uint64_t dims[3];
    dims[0] = 64;
    dims[1] = 3;
    dims[2] = 4;

#ifdef ENABLE_MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
#endif
    // create a pdc
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
    TASSERT(PDCprop_set_obj_dims(obj_prop, ndim, dims) >= 0, "Call to PDCprop_set_obj_dims succeeded",
            "Call to PDCprop_set_obj_dims failed");
    TASSERT(PDCprop_set_obj_type(obj_prop, PDC_DOUBLE) >= 0, "Call to PDCprop_set_obj_type succeeded",
            "Call to PDCprop_set_obj_type failed");

    // create first object
    sprintf(obj_name1, "o1_%d", rank);
    TASSERT((obj1 = PDCobj_create(cont, obj_name1, obj_prop)) != 0, "Call to PDCobj_create succeeded",
            "Call to PDCobj_create failed");
    // create second object
    sprintf(obj_name2, "o2_%d", rank);
    TASSERT((obj2 = PDCobj_create(cont, obj_name2, obj_prop)) != 0, "Call to PDCobj_create succeeded",
            "Call to PDCobj_create failed");
    TASSERT((obj1_info = PDCobj_get_info(obj1)) != NULL, "Call to PDCobj_get_info succeeded",
            "Call to PDCobj_get_info failed");
    TASSERT((obj2_info = PDCobj_get_info(obj2)) != NULL, "Call to PDCobj_get_info succeeded",
            "Call to PDCobj_get_info failed");
    TASSERT(strcmp(obj1_info->name, obj_name1) == 0, "obj1 name matched", "obj1 name did NOT match");

    TASSERT(obj1_info->obj_pt->type == PDC_DOUBLE, "Type is properly inherited from object property",
            "Type is NOT properly inherited from object property");
    TASSERT(obj1_info->obj_pt->ndim == ndim,
            "Number of dimensions is properly inherited from object property",
            "Number of dimensions is NOT properly inherited from object property");
    TASSERT(obj1_info->obj_pt->dims[0] == dims[0],
            "First dimension is properly inherited from object property",
            "First dimension is NOT properly inherited from object property");
    TASSERT(obj1_info->obj_pt->dims[1] == dims[1],
            "Second dimension is properly inherited from object property",
            "Second dimension is NOT properly inherited from object property");
    TASSERT(obj1_info->obj_pt->dims[2] == dims[2],
            "Third dimension is properly inherited from object property",
            "Third dimension is NOT properly inherited from object property");

    TASSERT(strcmp(obj2_info->name, obj_name2) == 0, "obj2 name matched", "obj2 name did NOT match");

    TASSERT(obj2_info->obj_pt->type == PDC_DOUBLE, "Type is properly inherited from object property",
            "Type is NOT properly inherited from object property");
    TASSERT(obj2_info->obj_pt->ndim == ndim,
            "Number of dimensions is properly inherited from object property",
            "Number of dimensions is NOT properly inherited from object property");
    TASSERT(obj2_info->obj_pt->dims[0] == dims[0],
            "First dimension is properly inherited from object property",
            "First dimension is NOT properly inherited from object property");
    TASSERT(obj2_info->obj_pt->dims[1] == dims[1],
            "Second dimension is properly inherited from object property",
            "Second dimension is NOT properly inherited from object property");
    TASSERT(obj2_info->obj_pt->dims[2] == dims[2],
            "Third dimension is properly inherited from object property",
            "Third dimension is NOT properly inherited from object property");

    // close object
    TASSERT(PDCobj_close(obj1) >= 0, "Call to PDCobj_close succeeded", "Call to PDCobj_close failed");
    TASSERT(PDCobj_close(obj2) >= 0, "Call to PDCobj_close succeeded", "Call to PDCobj_close failed");
    // close a container
    TASSERT(PDCcont_close(cont) >= 0, "Call to PDCcont_close succeeded", "Call to PDCcont_close failed");
    // close a object property
    TASSERT(PDCprop_close(obj_prop) >= 0, "Call to PDCprop_close succeeded", "Call to PDCprop_close failed");
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
