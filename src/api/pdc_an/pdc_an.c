#include <string.h>

#include "pdc_an.h"
#include "pdc_timing.h"
#include "pdc_interface.h"
#include "pdc_prop.h"
#include "pdc_obj_pkg.h"
#include "pdc_dg.h"
#include "pdc_malloc.h"
#include "pdc_region.h"
#include "pdc_an_common.h"
#include "pdc_client_connect.h"
#include "pdc_tf_common.h"
#include "pdc_tf.h"

pdc_dg_t *
PDCan_get_dg(pdcid_t dg_id)
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
PDCan_dg_json_create(char *json_filepath)
{
    FUNC_ENTER(NULL);

    pdcid_t ret_value = 0;

    pdc_dg_t *dg = PDCan_dg_json_create_common(json_filepath);
    if (dg == NULL)
        PGOTO_ERROR(0, "Failed to parse analysis graph JSON \"%s\"\n", json_filepath);

    ret_value = PDC_id_register(PDC_AN_DG, dg);

done:
    FUNC_LEAVE(ret_value);
}

static perr_t
PDCan_dg_free(void *dg)
{
    FUNC_ENTER(NULL);
    PDCdg_destroy((pdc_dg_t *)dg);
    FUNC_LEAVE(SUCCEED);
}

perr_t
PDCan_close_dg(pdcid_t dg_id)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;
    if (PDC_dec_ref(dg_id) < 0)
        PGOTO_ERROR(FAIL, "Analysis graph: problem freeing ID");

done:
    FUNC_LEAVE(ret_value);
}

/**
 * Checks whether tf_obj already has a PDC TF graph attached to exactly
 * region_info (matching PDCtf_region_has_attached_graph's per-region
 * matching, plus the whole-object attach_to_all_regions case that
 * function doesn't check). If so, resolves the state's client-local dg_id
 * to the underlying graph's json_filepath and returns true with
 * *json_filepath_out/*client_state_out/*store_state_out set (borrowed
 * pointers, not owned by the caller).
 */
static bool
find_attached_tf_info(struct pdc_tf_obj_t *tf_obj, struct pdc_region_info *region_info,
                      char **json_filepath_out, char **client_state_out, char **store_state_out)
{
    if (tf_obj == NULL)
        return false;

    pdc_tf_region_state_t *state = NULL;

    if (tf_obj->attach_to_all_regions) {
        state = &tf_obj->all_regions_state;
    }
    else if (tf_obj->region_mappings_vector != NULL) {
        PDC_VECTOR_ITERATOR *iter = pdc_vector_iterator_new(tf_obj->region_mappings_vector);
        while (pdc_vector_iterator_has_next(iter)) {
            pdc_tf_region_mapping_t *m = (pdc_tf_region_mapping_t *)pdc_vector_iterator_next(iter);
            if (m == NULL || m->conceptual_region.ndim != region_info->ndim)
                continue;

            bool match = true;
            for (size_t i = 0; i < region_info->ndim; i++) {
                if (m->conceptual_offset[i] != region_info->offset[i] ||
                    m->conceptual_region.size[i] != region_info->size[i]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                state = &m->region_state;
                break;
            }
        }
        pdc_vector_iterator_destroy(iter);
    }

    if (state == NULL)
        return false;

    pdc_dg_t *tf_dg = PDCtf_get_dg(state->dg_id);
    if (tf_dg == NULL)
        return false;

    *json_filepath_out = (char *)tf_dg->data;
    *client_state_out  = state->client_state;
    *store_state_out   = state->store_state;
    return true;
}

perr_t
PDCan_attach_to_region(pdcid_t dg_id, char *state_name, pdcid_t obj_id, pdcid_t region)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    pdc_dg_t *dg = PDCan_get_dg(dg_id);
    if (dg == NULL)
        PGOTO_ERROR(FAIL, "Failed to find analysis graph");

    struct _pdc_id_info *obj_id_info = PDC_find_id(obj_id);
    if (obj_id_info == NULL)
        PGOTO_ERROR(FAIL, "Failed to find object using pdcid");
    struct _pdc_obj_info *pdc_obj_info = obj_id_info->obj_ptr;

    struct _pdc_id_info *region_id_info = PDC_find_id(region);
    if (region_id_info == NULL)
        PGOTO_ERROR(FAIL, "Cannot locate region ID");
    struct pdc_region_info *region_info = region_id_info->obj_ptr;

    /* obj_id here is the client-local PDC_id_register() handle
     * (pdc_obj_info->local_id) -- the server has no notion of it. Region
     * transfers identify objects by the server-assigned global id,
     * obj_info_pub->meta_id (see pdc_region_transfer.c:313), so that's what
     * has to cross the wire for the binding to match at read time. */

    /* If this object/region also has a PDC TF graph attached client-side
     * (PDCtf_attach_to_region/_to_obj), the server needs to be told about
     * it explicitly here: that registration is normally a side effect of
     * a client write RPC's pdc_tf_pkg piggyback, but an analysis
     * input/output is written and read by PDCan_exec_graph running
     * server-side, never by a client write RPC, so that piggyback would
     * otherwise never happen. */
    char *tf_json_filepath = NULL;
    char *tf_client_state  = NULL;
    char *tf_store_state   = NULL;
    find_attached_tf_info(pdc_obj_info->pdc_tf_obj, region_info, &tf_json_filepath, &tf_client_state,
                          &tf_store_state);

    if (PDC_Client_an_attach_region(
            (char *)dg->data, state_name, pdc_obj_info->obj_info_pub->meta_id, (uint8_t)region_info->ndim,
            region_info->offset, region_info->size, (int32_t)pdc_obj_info->obj_pt->obj_prop_pub->ndim,
            pdc_obj_info->obj_pt->obj_prop_pub->dims, pdc_obj_info->obj_pt->obj_prop_pub->type,
            tf_json_filepath, tf_client_state, tf_store_state) != SUCCEED)
        PGOTO_ERROR(FAIL, "Server failed to attach region to analysis state \"%s\"", state_name);

done:
    FUNC_LEAVE(ret_value);
}

perr_t
PDCan_init(void)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    if (PDC_register_type(PDC_AN_DG, (PDC_free_t)PDCan_dg_free) < 0)
        PGOTO_ERROR(FAIL, "Failed to register PDC_AN_DG type");
    if (PDCan_init_builtin_funcs() != SUCCEED)
        PGOTO_ERROR(FAIL, "Error with PDCan_init_builtin_funcs");

done:
    FUNC_LEAVE(ret_value);
}
