#include "pdc_tf.h"
#include "pdc_timing.h"

// creates a new directed graph and returns ID
pdcid_t
PDCtf_create_dg(char *dg_name)
{
    FUNC_ENTER(NULL);

    pdcid_t dg_id = 0;

    FUNC_LEAVE(dg_id);
}

// attach transformation to DG
void
PDCtf_add_func(pdcid_t dg_id, pdcid_t func_id)
{
    FUNC_ENTER(NULL);

    FUNC_LEAVE_VOID();
}

/**
 * creates a new data state and returns ID
 * the state_name is used when attaching the directed graph to a PDC resources
 * to specify the source state and the destination state.
 */
pdcid_t
PDCtf_create_state(char *state_name)
{
    FUNC_ENTER(NULL);

    pdcid_t state_id = 0;

    FUNC_LEAVE(state_id);
}

/**
 * creates a new function and returns ID
 * the lib:func is the path to the library and function to execute
 */
pdcid_t
PDCtf_create_func(char *path_colon_name, pdc_tf_dev_t dev, pdcid_t input_data_state,
                  pdcid_t output_data_state)
{
    FUNC_ENTER(NULL);

    pdcid_t func_id = 0;

    FUNC_LEAVE(func_id);
}

/**
 * specifies how the output of a transformation function should be handled
 * the default is PDC_TF_OVERWRITE if this function is not called
 */
perr_t
PDCtf_set_output_mode(pdcid_t dg_id, pdc_tf_output_mode_t mode, pdcid_t *obj_ids, char **result_names,
                      int num_ids)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}

/**
 * free resources used by directed graph
 * this includes resources managed by functions/data states
 */
perr_t
PDCtf_close_dg(pdcid_t dg_id)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}