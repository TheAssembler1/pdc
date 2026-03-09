#include <assert.h>

#include "pdc_tf_server.h"
#include "pdc_malloc.h"
#include "pdc_client_server_common.h"
#include "pdc_vector.h"
#include "pdc_tf_user.h"

double   __timer_totals[NUM_TIMER_TARGETS]      = {0};
uint64_t __timer_totals_freq[NUM_TIMER_TARGETS] = {0};

double __timer_start       = 0.0f;
double __graph_timer_start = 0.0f;

PDC_VECTOR *tf_obj_id_to_dg_vector_g = NULL;

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

    if (tf_obj_id_to_dg_vector_g == NULL)
        tf_obj_id_to_dg_vector_g = pdc_vector_create(8, 2.0);
    if (tf_obj_id_to_dg_vector_g == NULL)
        PGOTO_ERROR(FAIL, "tf_obj_id_to_dg_vector_g was NULL");

    // Find object in mapping if it exists
    pdc_tf_obj_id_to_dg_t *obj_id_to_dg                = NULL;
    PDC_VECTOR_ITERATOR *  tf_obj_id_to_dg_vector_iter = pdc_vector_iterator_new(tf_obj_id_to_dg_vector_g);
    while (pdc_vector_iterator_has_next(tf_obj_id_to_dg_vector_iter)) {
        pdc_tf_obj_id_to_dg_t *cur_obj_id_to_dg =
            (pdc_tf_obj_id_to_dg_t *)pdc_vector_iterator_next(tf_obj_id_to_dg_vector_iter);
        if (cur_obj_id_to_dg->obj_id == obj_id) {
            obj_id_to_dg = cur_obj_id_to_dg;
            break;
        }
    }
    pdc_vector_iterator_destroy(tf_obj_id_to_dg_vector_iter);

    // If object has attached graph make sure it is the same as the passed in graph
    if (obj_id_to_dg != NULL) {
        char *graph_json_filepath = (char *)(obj_id_to_dg->dg->data);
        if (strcmp(json_filepath, graph_json_filepath)) {
            PGOTO_ERROR(FAIL, "Passed graph filepath %s didn't match stored filepath %s", json_filepath,
                        graph_json_filepath);
        }
    }

    // Region mappings for passed in region and object
    pdc_tf_region_mapping_t *region_mapping = NULL;

    // If object doesn't have a directed graph create a new one
    if (obj_id_to_dg == NULL) {
        LOG_DEBUG("Creating directed graph for object\n");

        pdc_dg_t *dg = PDCtf_dg_json_create_common(json_filepath);
        if (dg == NULL)
            PGOTO_ERROR(FAIL, "Failed to load JSON\n");

        // Create new obj id to dg and append to vector
        obj_id_to_dg = PDC_malloc(sizeof(pdc_tf_obj_id_to_dg_t));
        pdc_vector_add(tf_obj_id_to_dg_vector_g, obj_id_to_dg);

        obj_id_to_dg->dg     = dg;
        obj_id_to_dg->obj_id = obj_id;

        // Create a new region mapping vector and mapping entry
        obj_id_to_dg->pdc_tf_obj.region_mappings_vector = pdc_vector_create(8, 2.0);
        region_mapping                                  = PDC_calloc(1, sizeof(pdc_tf_region_mapping_t));
        pdc_vector_add(obj_id_to_dg->pdc_tf_obj.region_mappings_vector, region_mapping);
    }

    // Check if this mapping already exists
    if (region_mapping == NULL) {
        PDC_VECTOR_ITERATOR *region_mapping_iter =
            pdc_vector_iterator_new(obj_id_to_dg->pdc_tf_obj.region_mappings_vector);
        while (pdc_vector_iterator_has_next(region_mapping_iter)) {
            pdc_tf_region_mapping_t *cur_region_mapping = pdc_vector_iterator_next(region_mapping_iter);
            if (cur_region_mapping == NULL)
                PGOTO_ERROR(FAIL, "cur_region_mapping was NULL");
            uint64_t *conceptual_offset = cur_region_mapping->conceptual_offset;
            if (memcmp(offset, conceptual_offset, ndim * sizeof(uint64_t)) == 0) {
                region_mapping = cur_region_mapping;
                break;
            }
        }
        pdc_vector_iterator_destroy(region_mapping_iter);
    }

    // If this is null we need to append this mapping
    if (region_mapping == NULL) {
        region_mapping = PDC_calloc(1, sizeof(pdc_tf_region_mapping_t));
        pdc_vector_add(obj_id_to_dg->pdc_tf_obj.region_mappings_vector, region_mapping);
    }

    pdc_tf_region_t *conceptual_region = &region_mapping->conceptual_region;
    uint64_t *       conceptual_offset = region_mapping->conceptual_offset;

    PDC_get_var_type_size(pdc_var_type);

    // copy region information into conceptual region
    conceptual_region->ndim         = ndim;
    conceptual_region->pdc_var_type = pdc_var_type;
    memcpy(conceptual_offset, offset, ndim * sizeof(uint64_t));
    memcpy(conceptual_region->size, size, ndim * sizeof(uint64_t));

    LOG_DEBUG("obj_id=%" PRIu64 " ndim=%u\n", obj_id, ndim);
    for (int i = 0; i < ndim; i++) {
        LOG_DEBUG("  offset[%d]=%" PRIu64 " size[%d]=%" PRIu64 "\n", i, conceptual_offset[i], i,
                  conceptual_region->size[i]);
    }

    // FIXME: need to free these strings later
    region_mapping->region_state.cur_state    = strdup(cur_state);
    region_mapping->region_state.client_state = strdup(client_state);
    region_mapping->region_state.store_state  = strdup(store_state);
    region_mapping->region_state.dg_id        = cur_graph;

done:
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

            GRAPH_TIMER_START();
            if (f->c_func(internal_params, f->params_str, input, input_region, output_region) == false)
                PGOTO_ERROR(FAIL, "Error when running transformation, %s", f->name);
            else
                LOG_DEBUG("Transformation %s(%s) = %s ran successfully\n", f->name, v1->name, v2->name);
            GRAPH_TIMER_STOP(TOTAL_GRAPH_EXEC_TIME);
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
    else {
        LOG_ERROR("JSON filepath %s\n", (char *)dg->data);
        LOG_ERROR("Curent state %s, desired state %s\n", cur_state, desired_state);
        PGOTO_ERROR(FAIL, "No path to desired state");
    }

done:
    if (edges_out != NULL)
        edges_out = PDC_free(edges_out);

    FUNC_LEAVE(ret_value);
}
#endif // IS_PDC_SERVER
