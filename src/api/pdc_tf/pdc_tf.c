#include "pdc_tf.h"
#include "pdc_timing.h"

pdcid_t
PDCtf_create_dg(char *dg_name)
{
    FUNC_ENTER(NULL);

    pdcid_t dg_id = 0;

    FUNC_LEAVE(dg_id);
}

void
PDCtf_add_func(pdcid_t dg_id, pdcid_t func_id)
{
    FUNC_ENTER(NULL);

    FUNC_LEAVE_VOID();
}

pdcid_t
PDCtf_create_state(char *state_name)
{
    FUNC_ENTER(NULL);

    pdcid_t state_id = 0;

    FUNC_LEAVE(state_id);
}

pdcid_t
PDCtf_create_func(char *path_colon_name, pdc_tf_dev_t dev, pdcid_t input_data_state,
                  pdcid_t output_data_state)
{
    FUNC_ENTER(NULL);

    pdcid_t func_id = 0;

    FUNC_LEAVE(func_id);
}

perr_t
PDCtf_set_output_mode(pdcid_t dg_id, pdc_tf_output_mode_t mode, pdcid_t *obj_ids, char **result_names,
                      int num_ids)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_close_dg(pdcid_t dg_id)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}

// region transfer to/from the specified obj_id, global_reg_id follow DG
perr_t
PDCtf_attach_to_region(pdcid_t dg_id, pdcid_t obj_id, pdcid_t global_reg_id, pdcid_t client_state_id,
                       pdcid_t server_state_id)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}

// all region transfers for obj_id follow DG
perr_t
PDCtf_attach_to_obj(pdcid_t dg_id, pdcid_t obj_id, pdcid_t client_state_id, pdcid_t server_state_id)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}

// all region transfers for specified obj_ids follow DG
perr_t
PDCtf_attach_to_objs(pdcid_t dg_id, pdcid_t *obj_ids, int num_ids, pdcid_t client_state_id,
                     pdcid_t server_state_id)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}