/**
 * Description: This workflow is identical to workflow 4
 * other than the way it attaches the DG to the PDC resources.
 * It attaches the DG to multiple objects.
 */

#include "pdc.h"
#include "test_helper.h"

int
main(int argc, char **argv)
{
    pdcid_t pdc, create_prop, cont;
    int     rank = 0, size = 1;

    int ret_value = TSUCCEED;

#ifdef ENABLE_MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
#endif

    // create a pdc
    TASSERT((pdc = PDCinit("pdc")) != 0, "Call to PDCinit succeeded", "Call to PDCinit failed");
    // create a container property
    TASSERT((create_prop = PDCprop_create(PDC_CONT_CREATE, pdc)) != 0, "Call to PDCprop_create succeeded",
            "Call to PDCprop_create failed");
    // create a container
    TASSERT((cont = PDCcont_create("c1", create_prop)) != 0, "Call to PDCcont_create succeeded",
            "Call to PDCcont_create failed");
    // close a container
    TASSERT(PDCcont_close(cont) >= 0, "Call to PDCcont_close succeeded", "Call to PDCcont_close failed");
    // close a container property
    TASSERT(PDCprop_close(create_prop) >= 0, "Call to PDCprop_close succeeded",
            "Call to PDCprop_close failed");
    // close pdc
    TASSERT(PDCclose(pdc) >= 0, "Call to PDCclose succeeded", "Call to PDCclose failed");

done:
#ifdef ENABLE_MPI
    MPI_Finalize();
#endif
    return ret_value;
}