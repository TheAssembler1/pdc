#include "pdc_tf_server.h"
#include "pdc_malloc.h"

pdc_dg_t *pdc_tf_graphs[200];
uint32_t  num_tf_obj_with_obj_ids_g = 0;

#ifndef IS_PDC_SERVER
perr_t
PDCtf_store_json_mapping(pdcid_t obj_id, char *json_filepath, char *cur_state, char *client_state,
                         char *store_state, uint64_t *offset, uint64_t *size, uint8_t ndim, uint8_t unit)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(SUCCEED);
}
perr_t
PDCtf_exec_graph(pdcid_t dg_id, char *cur_state, char *desired_state, pdc_tf_region_t input_region,
                 pdc_tf_region_t *output_region, void **input, int is_write)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(SUCCEED);
}
#else
static pdcid_t cur_graph = 1;

perr_t
PDCtf_store_json_mapping(pdcid_t obj_id, char *json_filepath, char *cur_state, char *client_state,
                         char *store_state, uint64_t *offset, uint64_t *size, uint8_t ndim, uint8_t unit)
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCtf_attach_to_region was called\n");

    perr_t ret_value = SUCCEED;

    pdc_dg_t *dg             = PDCtf_dg_json_create_common(json_filepath);
    pdc_tf_graphs[cur_graph] = dg;

    if (dg == NULL)
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
    uint64_t        *conceptual_offset = region_mapping->conceptual_offset;

    // copy region information into conceptual region
    conceptual_region->ndim = ndim;
    conceptual_region->unit = unit;
    memcpy(conceptual_offset, offset, ndim * sizeof(uint64_t));
    memcpy(conceptual_region->size, size, ndim * sizeof(uint64_t));

    // FIXME: need to free these strings later
    region_mapping->region_state.cur_state    = strdup(cur_state);
    region_mapping->region_state.client_state = strdup(client_state);
    region_mapping->region_state.store_state  = strdup(store_state);
    region_mapping->region_state.dg_id        = cur_graph;

    // increase the current region mapping
    cur_tf_server_obj_info->pdc_tf_obj_t.num_region_mappings++;
    num_tf_obj_with_obj_ids_g++;
    cur_graph++;
done:
    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_exec_graph(pdcid_t dg_id, char *cur_state, char *desired_state, pdc_tf_region_t input_region,
                 pdc_tf_region_t *output_region, void **input, int is_write)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    LOG_INFO("PDCtf_exec_graph was called\n");

    /**
     * Setup input and output states
     * NOTE: The vertices are check for equality based on the name alone
     */
    pdc_tf_state_t tf_input_state  = {.name = cur_state};
    pdc_tf_state_t tf_output_state = {.name = desired_state};
    void          *input_state     = (void *)&tf_input_state;
    void          *output_state    = (void *)&tf_output_state;

    pdc_dg_edge_t *edges_out;
    uint32_t       num_edges;

    memcpy(output_region, &input_region, sizeof(pdc_tf_region_t));

    if (PDCdg_shortest_path(pdc_tf_graphs[dg_id], input_state, output_state, &edges_out, &num_edges)) {
        LOG_INFO("Planned Execution path\n");
        for (uint32_t j = 0; j < num_edges; j++) {
            pdc_dg_edge_t   e  = edges_out[j];
            pdc_tf_state_t *v1 = (pdc_tf_state_t *)(pdc_tf_graphs[dg_id]->vertices[e.v1_id]->data);
            pdc_tf_state_t *v2 = (pdc_tf_state_t *)(pdc_tf_graphs[dg_id]->vertices[e.v2_id]->data);
            pdc_tf_func_t  *f  = (pdc_tf_func_t *)(e.data);

            LOG_INFO("Transformation %s(%s) = %s\n", f->name, v1->name, v2->name);
        }
    }

    if (PDCdg_shortest_path(pdc_tf_graphs[dg_id], input_state, output_state, &edges_out, &num_edges)) {
        LOG_INFO("Path was found:\n");
        for (uint32_t j = 0; j < num_edges; j++) {
            pdc_dg_edge_t   e  = edges_out[j];
            pdc_tf_state_t *v1 = (pdc_tf_state_t *)(pdc_tf_graphs[dg_id]->vertices[e.v1_id]->data);
            pdc_tf_state_t *v2 = (pdc_tf_state_t *)(pdc_tf_graphs[dg_id]->vertices[e.v2_id]->data);
            pdc_tf_func_t  *f  = (pdc_tf_func_t *)(e.data);

            // Setup internal paramters for helper macros
            pdc_tf_internal_param internal_params;
            internal_params.dg = pdc_tf_graphs[dg_id];

            // Run the transformation
            LOG_JUST_PRINT("--------------------------TRANSFORM_START--------------------------\n");
            void *prev_input = *input;
            if (f->c_func(internal_params, f->params_str, input, input_region, output_region) == false)
                PGOTO_ERROR(FAIL, "Error when running transformation, %s", f->name);
            else
                LOG_INFO("Transformation %s(%s) = %s ran successfully\n", f->name, v1->name, v2->name);
            /**
             * The transformation malloced a new buffer
             * The buffer associated with the original bulk handle (i.e. j != 0)
             * should not be freed as this is freed by a higher up caller
             * only on a write
             */
            if (!(is_write && j == 0) && prev_input != *input) {
                prev_input = PDC_free(prev_input);
            }

            LOG_JUST_PRINT("--------------------------TRANSFORM_DONE--------------------------\n");

            // Set previous output region as input region for next transformation
            if (j + 1 != num_edges)
                memcpy(&input_region, output_region, sizeof(pdc_tf_region_t));
        }

        LOG_INFO("Done running transformations\n");
    }
    else
        PGOTO_ERROR(FAIL, "No path to desired states");

done:
    FUNC_LEAVE(ret_value);
}
#endif // #ifndef IS_PDC_SERVER