#include "pdc_tf_server.h"

uint32_t num_tf_obj_with_obj_ids_g = 0;

#ifndef IS_PDC_SERVER
perr_t
PDCtf_store_json_mapping(pdcid_t obj_id, char *json_filepath, char *cur_state, char *store_state,
                         uint64_t *offset, uint64_t *size, uint8_t ndim, uint8_t unit)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(SUCCEED);
}
#else
perr_t
PDCtf_store_json_mapping(pdcid_t obj_id, char *json_filepath, char *cur_state, char *store_state,
                         uint64_t *offset, uint64_t *size, uint8_t ndim, uint8_t unit)
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCtf_attach_to_region was called\n");

    perr_t ret_value = SUCCEED;

    pdcid_t dg_id = PDCtf_open_dg_json_common(json_filepath);

    if (dg_id == 0)
        PGOTO_ERROR(FAIL, "Failed to load JSON\n");

    const uint32_t            cur_num_tf_obj_with_obj_id = num_tf_obj_with_obj_ids_g;
    pdc_tf_obj_with_obj_id_t *cur_tf_server_obj_info = &pdc_tf_obj_with_obj_ids[cur_num_tf_obj_with_obj_id];
    const uint32_t            cur_region_map =
        pdc_tf_obj_with_obj_ids[cur_num_tf_obj_with_obj_id].pdc_tf_obj_t.num_region_mappings;

    cur_tf_server_obj_info->obj_id = obj_id;

    // get region mapping fields from object
    pdc_tf_region_mapping_t *region_mapping =
        &cur_tf_server_obj_info->pdc_tf_obj_t.region_mappings[cur_region_map];
    pdc_tf_region_t *conceptual_region = &region_mapping->conceptual_region;
    uint64_t *       conceptual_offset = region_mapping->conceptual_offset;

    // copy region information into conceptual region
    conceptual_region->ndim = ndim;
    conceptual_region->unit = unit;
    memcpy(conceptual_offset, offset, ndim * sizeof(uint64_t));
    memcpy(conceptual_region->size, size, ndim * sizeof(uint64_t));

    // FIXME: need to free these strings later
    region_mapping->region_state.cur_state     = strdup(cur_state);
    region_mapping->region_state.desired_state = strdup(store_state);
    region_mapping->region_state.dg_id         = dg_id;

    // increase the current region mapping
    cur_tf_server_obj_info->pdc_tf_obj_t.num_region_mappings++;
    num_tf_obj_with_obj_ids_g++;
done:
    FUNC_LEAVE(ret_value);
}
#endif // #ifndef IS_PDC_SERVER