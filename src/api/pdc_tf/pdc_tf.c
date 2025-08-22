#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "pdc_tf.h"
#include "pdc_timing.h"
#include "pdc_interface.h"
#include "pdc_prop.h"
#include "pdc_obj_pkg.h"
#include "pdc_dg.h"
#include "pdc_malloc.h"
#include "pdc_region.h"
#include "pdc_tf_common.h"
#include "pdc_client_server_common.h"

pdc_dg_t *
PDCtf_get_dg(pdcid_t dg_id)
{
    FUNC_ENTER(NULL);

    pdc_dg_t *ret_value = NULL;

    struct _pdc_id_info *id_info = PDC_find_id(dg_id);
    if (id_info == NULL)
        PGOTO_ERROR(NULL, "Failed to find dg_id");

    ret_value = (pdc_dg_t *)id_info->obj_ptr;
    if (ret_value == NULL)
        PGOTO_ERROR(NULL, "Failed to find dg");

done:
    FUNC_LEAVE(ret_value);
}

pdcid_t
PDCtf_dg_json_create(char *json_filepath)
{
    FUNC_ENTER(NULL);
    pdcid_t ret_value = 0;

    pdc_dg_t *dg = PDCtf_dg_json_create_common(json_filepath);
    if (dg == NULL) {
        abort();
        PGOTO_ERROR(FAIL, "Error with PDCtf_open_dg_json_common");
    }

    ret_value = PDC_id_register(PDC_TF_DG, dg);

done:
    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_close_dg(pdcid_t dg_id)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;
    if (PDC_dec_ref(dg_id) < 0)
        PGOTO_ERROR(FAIL, "Directed graph: problem of freeing ID");

done:
    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_set_output_mode(pdcid_t dg_id, pdc_tf_output_mode_t mode, pdcid_t *obj_ids, char **result_names,
                      int num_ids)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}

// Region transfer to/from the specified obj_id, remote_reg_id follow DG
perr_t
PDCtf_attach_to_region(pdcid_t dg_id, pdcid_t obj_id, pdcid_t remote_reg, char *client_state,
                       char *store_state)
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCtf_attach_to_region was called\n");

    perr_t ret_value = SUCCEED;

    // First locate object
    const struct _pdc_id_info *obj_id_info = PDC_find_id(obj_id);
    if (obj_id_info == NULL)
        PGOTO_ERROR(FAIL, "Failed to find object using pdcid");
    struct _pdc_obj_info *obj_info = obj_id_info->obj_ptr;

    // Validate partition strategy is supported with transformations
    if (obj_info->obj_pt->obj_prop_pub->region_partition == PDC_REGION_STATIC) {
        LOG_ERROR("PDC_REGION_STATIC partition strategy not supported for transformations\n");
        abort();
        PGOTO_ERROR(FAIL, "The following partitions strategies are supported: PDC_REGION_LOCAL, "
                          "PDC_REGION_DYNAMIC, or PDC_OBJ_STATIC\n");
    }

    // Validate user has set the datatype on the object
    if (PDC_get_var_type_size(obj_info->obj_pt->obj_prop_pub->type) == 0) {
        LOG_ERROR("Invalid data type for object transformation\n");
        PGOTO_ERROR(FAIL, "Data type must be set on object before attaching transformations");
    }

    // Pull out pdc obj transform information and allocate first if NULL
    if (obj_info->pdc_tf_obj == NULL)
        obj_info->pdc_tf_obj = PDC_calloc(1, sizeof(struct pdc_tf_obj_t));
    struct pdc_tf_obj_t *pdc_tf_obj     = obj_info->pdc_tf_obj;
    const uint32_t       cur_region_map = pdc_tf_obj->num_region_mappings;

    // Get region information
    struct _pdc_id_info *region_id_info = PDC_find_id(remote_reg);
    if (region_id_info == NULL)
        PGOTO_ERROR(FAIL, "Cannot locate remote region ID");
    struct pdc_region_info *region_info = region_id_info->obj_ptr;

    // get region mapping fields from object
    pdc_tf_region_mapping_t *region_mapping    = &pdc_tf_obj->region_mappings[cur_region_map];
    pdc_tf_region_t *        conceptual_region = &region_mapping->conceptual_region;
    uint64_t *               conceptual_offset = region_mapping->conceptual_offset;

    // Copy region information into conceptual region
    PDCtf_set_tf_region_t(conceptual_region, region_info->ndim, obj_info->obj_pt->obj_prop_pub->type,
                          region_info->size);
    memcpy(conceptual_offset, region_info->offset, region_info->ndim * sizeof(uint64_t));

    // FIXME: need to free these strings later
    region_mapping->region_state.client_state = strdup(client_state);
    region_mapping->region_state.cur_state    = strdup(client_state);
    region_mapping->region_state.store_state  = strdup(store_state);
    region_mapping->region_state.dg_id        = dg_id;

    // increase the current region mapping
    pdc_tf_obj->num_region_mappings++;
done:
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

static perr_t
PDCtf_dg_free(void *dg)
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCtf_dg_free called\n");

    perr_t ret_value = SUCCEED;
    PDCdg_destroy((pdc_dg_t *)dg);

    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_init()
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCtf_init called\n");

    perr_t ret_value = SUCCEED;

    if (PDC_register_type(PDC_TF_DG, (PDC_free_t)PDCtf_dg_free) < 0)
        PGOTO_ERROR(FAIL, "Failed to register PDC_TF_DG type");
    if (PDCtf_init_builtin_funcs() != SUCCEED)
        PGOTO_ERROR(FAIL, "Error with PDCtf_init_builtin_funcs");

done:
    FUNC_LEAVE(ret_value);
}

void
PDCtf_print_dg(pdcid_t dg_id, bool write_to_file)
{
    pdc_dg_t *dg = PDCtf_get_dg(dg_id);
    assert(dg != NULL);
    PDCtf_print_dg_common(dg, write_to_file);
}

void
PDCtf_print_exec_path(pdcid_t dg_id, char *cur_state, char *desired_state)
{
    PDCtf_print_exec_path_common(PDCtf_get_dg(dg_id), cur_state, desired_state);
}