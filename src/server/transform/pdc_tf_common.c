#include "pdc_tf_common.h"
#include "pdc_tf_builtin_common.h"
#include "pdc_tf.h"
#include "pdc_timing.h"

pdc_dg_t *graphs[200];
state *   states[200];

pdc_tf_builtin_func_t pdc_tf_builtin_funcs_g[PDC_TF_MAX_BUILTIN_FUNCS];
uint32_t              pdc_tf_builtin_cur_func_g = 0;

bool pdc_tf_has_init_g = false;

perr_t
PDCtf_exec_graph(pdcid_t dg_id, pdcid_t current_state_id, pdcid_t desired_state_id, void *input)
{
    FUNC_ENTER(NULL);

    int ret_value = SUCCEED;

    LOG_INFO("PDCtf_exec_graph was called\n");
    pdc_dg_t *dg = graphs[dg_id];

    void *         input_state  = states[current_state_id];
    void *         output_state = states[desired_state_id];
    pdc_dg_edge_t *edges_out;
    uint32_t       num_edges;

    if (PDCdg_shortest_path(graphs[dg_id], input_state, output_state, &edges_out, &num_edges)) {
        LOG_INFO("Path was found:\n");
        for (uint32_t j = 0; j < num_edges; j++) {
            pdc_dg_edge_t e = edges_out[j];

            state *v1 = (state *)(graphs[dg_id]->vertices[e.v1_id]->data);
            state *v2 = (state *)(graphs[dg_id]->vertices[e.v2_id]->data);
            func * f  = (func *)(e.data);

            if (f->c_func(NULL, 1, NULL, NULL, input, NULL) == false)
                PGOTO_ERROR(FAIL, "Error when running transformation, %s", f->type_func_name);
            else
                LOG_INFO("Transformation %s(%s) = %s ran successfully\n", f->type_func_name, v1->name,
                         v2->name);
        }
    }
    else {
        LOG_INFO("No path found\n");
        PGOTO_ERROR(FAIL, "No path to desired states");
    }

done:
    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_add_builtin_func(char *func_name, c_func_t c_func)
{
    FUNC_ENTER(NULL);

    int ret_value = SUCCEED;
    strcpy(pdc_tf_builtin_funcs_g[pdc_tf_builtin_cur_func_g].name, func_name);
    pdc_tf_builtin_funcs_g[pdc_tf_builtin_cur_func_g].c_func = c_func;

    pdc_tf_builtin_cur_func_g++;

    LOG_INFO("Successfully added builtin function %s\n", func_name);

    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_link_builtin_func(char *func_name, func *f)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;
    bool   found     = false;

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
    if (PDCtf_add_builtin_func("zfp_compress", pdc_tf_builtin_zfp_compress) != SUCCEED)
        PGOTO_ERROR(FAIL, "Failed to add builtin func zfp_compress");
    if (PDCtf_add_builtin_func("zfp_decompress", pdc_tf_builtin_zfp_decompress) != SUCCEED)
        PGOTO_ERROR(FAIL, "Failed to add builtin func zfp_decompress");

done:
    FUNC_LEAVE(ret_value);
}
