#include "pdc_tf_common.h"
#include "pdc_tf_builtin_common.h"
#include "pdc_tf.h"
#include "pdc_timing.h"

pdc_dg_t *pdc_tf_graphs[200];
state *   pdc_tf_states[200];

pdc_tf_builtin_func_t pdc_tf_builtin_funcs_g[PDC_TF_MAX_BUILTIN_FUNCS];
uint32_t              pdc_tf_builtin_cur_func_g = 0;

bool pdc_tf_has_init_g = false;

perr_t
PDCtf_exec_graph(pdcid_t dg_id, pdcid_t current_state_id, pdcid_t desired_state_id,
                 pdc_tf_region_t input_region, pdc_tf_region_t *output_region, void **input)
{
    FUNC_ENTER(NULL);

    int ret_value = SUCCEED;

    LOG_INFO("PDCtf_exec_graph was called\n");

    void *         input_state  = pdc_tf_states[current_state_id];
    void *         output_state = pdc_tf_states[desired_state_id];
    pdc_dg_edge_t *edges_out;
    uint32_t       num_edges;

    memcpy(output_region, &input_region, sizeof(pdc_tf_region_t));

    if (PDCdg_shortest_path(pdc_tf_graphs[dg_id], input_state, output_state, &edges_out, &num_edges)) {
        LOG_INFO("Path was found:\n");
        for (uint32_t j = 0; j < num_edges; j++) {
            pdc_dg_edge_t e  = edges_out[j];
            state *       v1 = (state *)(pdc_tf_graphs[dg_id]->vertices[e.v1_id]->data);
            state *       v2 = (state *)(pdc_tf_graphs[dg_id]->vertices[e.v2_id]->data);
            func *        f  = (func *)(e.data);

            // run the transformation
            if (f->c_func(NULL, input, input_region, output_region) == false)
                PGOTO_ERROR(FAIL, "Error when running transformation, %s", f->type_func_name);
            else
                LOG_INFO("Transformation %s(%s) = %s ran successfully\n", f->type_func_name, v1->name,
                         v2->name);

            // set previous output region as input region for next transformation
            if (j + 1 != num_edges)
                memcpy(&input_region, output_region, sizeof(pdc_tf_region_t));
        }
    }
    else
        PGOTO_ERROR(FAIL, "No path to desired states");

done:
    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_add_builtin_func(char *func_name, c_func_t c_func)
{
    FUNC_ENTER(NULL);

    int ret_value = SUCCEED;

    if (func_name == NULL)
        PGOTO_ERROR(FAIL, "func_name was NULL");

    strcpy(pdc_tf_builtin_funcs_g[pdc_tf_builtin_cur_func_g].name, func_name);
    pdc_tf_builtin_funcs_g[pdc_tf_builtin_cur_func_g].c_func = c_func;

    pdc_tf_builtin_cur_func_g++;

    LOG_INFO("Successfully added builtin function %s\n", func_name);

done:
    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_link_builtin_func(char *func_name, func *f)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;
    bool   found     = false;

    if (func_name == NULL)
        PGOTO_ERROR(FAIL, "func_name was NULL");
    if (f == NULL)
        PGOTO_ERROR(FAIL, "f was NULL");

    for (int i = 0; i < pdc_tf_builtin_cur_func_g; i++) {
        if (strcmp(pdc_tf_builtin_funcs_g[i].name, func_name) == 0) {
            found     = true;
            f->c_func = pdc_tf_builtin_funcs_g[i].c_func;
        }
    }

    if (!found)
        PGOTO_ERROR(FAIL, "Builtin function not found");

done:
    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_init_builtin_funcs()
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    if (PDCtf_add_builtin_func("double_to_float", pdc_tf_builtin_double_to_float) != SUCCEED)
        PGOTO_ERROR(FAIL, "Failed to add builtin func double_to_float");
    if (PDCtf_add_builtin_func("float_to_double", pdc_tf_builtin_float_to_double) != SUCCEED)
        PGOTO_ERROR(FAIL, "Failed to add builtin func float_to_double");
#ifdef ENABLE_TF_ZFP_COMPRESSION
    if (PDCtf_add_builtin_func("zfp_compress", pdc_tf_builtin_zfp_compress) != SUCCEED)
        PGOTO_ERROR(FAIL, "Failed to add builtin func zfp_compress");
    if (PDCtf_add_builtin_func("zfp_decompress", pdc_tf_builtin_zfp_decompress) != SUCCEED)
        PGOTO_ERROR(FAIL, "Failed to add builtin func zfp_decompress");
#endif

done:
    FUNC_LEAVE(ret_value);
}

bool
PDCtf_should_exec_graph(struct _pdc_obj_info *obj_info, pdcid_t *region_exec_graph_id, int ndim,
                        uint8_t unit, uint64_t *offset, uint64_t *dims, bool check_client)
{
    bool ret_value = false;

    /**
     * loop through attached graphs
     * if this is NULL then no graphs are attached to the object
     */
    if (obj_info->pdc_tf_obj != NULL) {
        for (*region_exec_graph_id = 0; *region_exec_graph_id < obj_info->pdc_tf_obj->num_regions;
             (*region_exec_graph_id)++) {
            pdc_tf_absolute_region_t abs_reg;
            
            if(check_client)
                abs_reg = obj_info->pdc_tf_obj->client_regions[*region_exec_graph_id];
            else 
                abs_reg = obj_info->pdc_tf_obj->remote_regions[*region_exec_graph_id];

            // check if client ndim, offset, dims, unit match
            bool ndim_matches = abs_reg.ndim == ndim;
            bool unit_matches = abs_reg.unit == unit;
            // note these return 0 on match so ! is needed
            bool offset_matches = !memcmp(abs_reg.offset, offset, ndim * sizeof(uint64_t));
            bool dims_matches   = !memcmp(abs_reg.dims, dims, ndim * sizeof(uint64_t));

            // debug logging for matching
            if(check_client)
                LOG_INFO("Checking against client regions\n");
            else 
                LOG_INFO("Checking against remote regions\n");
            
            LOG_INFO("\tpassed ndim %d, checked ndim %d\n", abs_reg.ndim, ndim);
            LOG_INFO("\tpassed unit %d, checked unit %d\n", abs_reg.unit, unit);
            for(int i = 0; i < ndim; i++)
                LOG_INFO("\tpassed dims[%d]=%d, checked dims[%d]=%d\n", i, abs_reg.dims[i], i, dims[i]);
            for(int i = 0; i < ndim; i++)
                LOG_INFO("\tpassed offset[%d]=%d, checked offset[%d]=%d\n", i, abs_reg.offset[i], i, offset[i]);

            if (ndim_matches && offset_matches && dims_matches && unit_matches)
                PGOTO_DONE(true);
        }
    }

done:
    FUNC_LEAVE(ret_value);
}

size_t
PDCtf_get_pdc_region_t_elements(pdc_tf_region_t reg)
{
    size_t num_elements = 1;
    for (int i = 0; i < reg.ndim; ++i) {
        num_elements *= reg.dims[i];
    }
    return num_elements;
}

size_t
PDCtf_get_pdc_region_t_bytes(pdc_tf_region_t reg)
{
    return PDCtf_get_pdc_region_t_elements(reg) * reg.unit;
}

void
PDCtf_log_pdc_region_t(pdc_tf_region_t reg)
{
    LOG_INFO("region ndim: %lu\n", reg.ndim);
    LOG_INFO("region unit: %lu\n", reg.unit);
    for (int i = 0; i < reg.ndim; i++)
        LOG_INFO("\tdim %d = %lu\n", i + 1, reg.dims[0]);
    LOG_INFO("region bytes: %zu\n", PDCtf_get_pdc_region_t_bytes(reg));
}
