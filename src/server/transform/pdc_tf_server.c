#include <assert.h>

#include "pdc_tf_server.h"
#include "pdc_malloc.h"
#include "pdc_client_server_common.h"

pdc_tf_obj_id_to_dg_t pdc_tf_obj_id_to_dg_list[MAX_TF_OBJECT_ID_TO_DG_MAPPINGS];
uint32_t              num_objs_with_dg = 0;

#ifndef IS_PDC_SERVER
perr_t
PDCtf_store_json_mapping(pdcid_t obj_id, char *json_filepath, char *cur_state, char *client_state,
                         char *store_state, uint64_t *offset, uint64_t *size, uint8_t ndim,
                         pdc_var_type_t pdc_var_type)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(SUCCEED);
}
perr_t
PDCtf_exec_graph(pdc_dg_t *dg, uint64_t flat_conceptual_offset, char *cur_state, char *desired_state,
                 pdc_tf_region_t input_region, pdc_tf_region_t *output_region, void **input, int is_write)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(SUCCEED);
}
#else
static pdcid_t cur_graph = 1;

perr_t
PDCtf_store_json_mapping(pdcid_t obj_id, char *json_filepath, char *cur_state, char *client_state,
                         char *store_state, uint64_t *offset, uint64_t *size, uint8_t ndim,
                         pdc_var_type_t pdc_var_type)
{
    FUNC_ENTER(NULL);

    LOG_DEBUG("PDCtf_store_json_mapping was called\n");

    perr_t ret_value = SUCCEED;

    // check if object has directed graph
    bool obj_id_has_dg        = false;
    int  obj_to_dg_list_index = 0;
    for (obj_to_dg_list_index = 0; obj_to_dg_list_index < num_objs_with_dg; obj_to_dg_list_index++) {
        if (pdc_tf_obj_id_to_dg_list[obj_to_dg_list_index].obj_id == obj_id) {
            obj_id_has_dg = true;
            break;
        }
    }

    // If object has attached graph make sure it is the same as the passed in graph
    if (obj_id_has_dg) {
        char *graph_json_filepath = (char *)(pdc_tf_obj_id_to_dg_list[obj_to_dg_list_index].dg->data);
        assert(graph_json_filepath != NULL);
        if (strcmp(json_filepath, graph_json_filepath)) {
            PGOTO_ERROR(FAIL, "Passed graph filepath %s didn't match stored filepath %s", json_filepath,
                        graph_json_filepath);
        }
    }

    // Region mappings for passed in region and object
    pdc_tf_region_mapping_t *region_mapping = NULL;

    // If object doesn't have a directed graph create a new one
    if (!obj_id_has_dg) {
        LOG_DEBUG("Creating directed graph for object\n");

        pdc_dg_t *dg = PDCtf_dg_json_create_common(json_filepath);
        if (dg == NULL)
            PGOTO_ERROR(FAIL, "Failed to load JSON\n");

        pdc_tf_obj_id_to_dg_list[num_objs_with_dg].dg                             = dg;
        pdc_tf_obj_id_to_dg_list[num_objs_with_dg].obj_id                         = obj_id;
        pdc_tf_obj_id_to_dg_list[num_objs_with_dg].pdc_tf_obj.num_region_mappings = 1;
        region_mapping = &pdc_tf_obj_id_to_dg_list[num_objs_with_dg].pdc_tf_obj.region_mappings[0];

        // Set this as the current obj_to_dg_list index
        obj_to_dg_list_index = num_objs_with_dg;

        // Increment the number of objects with attached graphs
        num_objs_with_dg++;
    }

    // At this point there is a mapping from object to graph
    pdc_tf_obj_id_to_dg_t *obj_to_dg = &pdc_tf_obj_id_to_dg_list[obj_to_dg_list_index];

    LOG_DEBUG("Cur num region mappings for object: %lu\n", obj_to_dg->pdc_tf_obj.num_region_mappings);

    // Check if this mapping already exists
    if (region_mapping == NULL) {
        for (int i = 0; i < obj_to_dg->pdc_tf_obj.num_region_mappings; i++) {
            uint64_t *conceptual_offset = obj_to_dg->pdc_tf_obj.region_mappings[i].conceptual_offset;
            if (memcmp(offset, conceptual_offset, ndim * sizeof(uint64_t)) == 0) {
                region_mapping = &obj_to_dg->pdc_tf_obj.region_mappings[i];
                break;
            }
        }
    }

    // If this is null we need to append this mapping
    if (region_mapping == NULL) {
        region_mapping = &obj_to_dg->pdc_tf_obj.region_mappings[obj_to_dg->pdc_tf_obj.num_region_mappings];
        obj_to_dg->pdc_tf_obj.num_region_mappings++;
    }

    pdc_tf_region_t *conceptual_region = &region_mapping->conceptual_region;
    uint64_t *       conceptual_offset = region_mapping->conceptual_offset;

    PDC_get_var_type_size(pdc_var_type);

    // copy region information into conceptual region
    conceptual_region->ndim         = ndim;
    conceptual_region->pdc_var_type = pdc_var_type;
    memcpy(conceptual_offset, offset, ndim * sizeof(uint64_t));
    memcpy(conceptual_region->size, size, ndim * sizeof(uint64_t));

    // FIXME: need to free these strings later
    region_mapping->region_state.cur_state    = strdup(cur_state);
    region_mapping->region_state.client_state = strdup(client_state);
    region_mapping->region_state.store_state  = strdup(store_state);
    region_mapping->region_state.dg_id        = cur_graph;

    PDC_get_var_type_size(conceptual_region->pdc_var_type);

done:
    LOG_DEBUG("Cur number of objs with region mappings: %d\n", num_objs_with_dg);
    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_exec_graph(pdc_dg_t *dg, uint64_t flat_conceptual_offset, char *cur_state, char *desired_state,
                 pdc_tf_region_t input_region, pdc_tf_region_t *output_region, void **input, int is_write)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    LOG_DEBUG("PDCtf_exec_graph was called\n");

    PDC_get_var_type_size(input_region.pdc_var_type);

    /**
     * Setup input and output states
     * NOTE: The vertices are check for equality based on the name alone
     */
    pdc_tf_state_t tf_input_state  = {.name = cur_state};
    pdc_tf_state_t tf_output_state = {.name = desired_state};
    void *         input_state     = (void *)&tf_input_state;
    void *         output_state    = (void *)&tf_output_state;

    pdc_dg_edge_t *edges_out = NULL;
    uint32_t       num_edges;

    if (PDCdg_shortest_path(dg, input_state, output_state, &edges_out, &num_edges)) {
        LOG_DEBUG("Path was found:\n");
        for (uint32_t j = 0; j < num_edges; j++) {
            pdc_dg_edge_t   e  = edges_out[j];
            pdc_tf_state_t *v1 = (pdc_tf_state_t *)(dg->vertices[e.v1_id]->data);
            pdc_tf_state_t *v2 = (pdc_tf_state_t *)(dg->vertices[e.v2_id]->data);
            pdc_tf_func_t * f  = (pdc_tf_func_t *)(e.data);

            // Setup internal paramters for helper macros
            pdc_tf_internal_param internal_params;
            internal_params.dg                     = dg;
            internal_params.flat_conceptual_offset = flat_conceptual_offset;

            // Run the transformation
            LOG_DEBUG("--------------------------TRANSFORM_START--------------------------\n");
            void *prev_input = *input;

            if (f->c_func(internal_params, f->params_str, input, input_region, output_region) == false)
                PGOTO_ERROR(FAIL, "Error when running transformation, %s", f->name);
            else
                LOG_DEBUG("Transformation %s(%s) = %s ran successfully\n", f->name, v1->name, v2->name);
            /**
             * The transformation malloced a new buffer
             * The buffer associated with the original bulk handle (i.e. j != 0)
             * should not be freed as this is freed by a higher up caller
             * only on a write
             */
            if (!(is_write && j == 0) && prev_input != *input)
                prev_input = PDC_free(prev_input);

            LOG_DEBUG("--------------------------TRANSFORM_DONE--------------------------\n");

            // Set previous output region as input region for next transformation
            if (j + 1 != num_edges)
                memcpy(&input_region, output_region, sizeof(pdc_tf_region_t));
        }
        LOG_DEBUG("Done running transformations\n");
    }
    else
        PGOTO_ERROR(FAIL, "No path to desired states");

done:
    if (edges_out != NULL)
        edges_out = PDC_free(edges_out);

    FUNC_LEAVE(ret_value);
}
#endif // IS_PDC_SERVER