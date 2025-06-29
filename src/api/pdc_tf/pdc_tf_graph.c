#include "pdc_tf_graph.h"

pdc_tf_dg_t *
PDCtf_graph_create(pdcid_t dg_id, const char *dg_name)
{
    FUNC_ENTER(NULL);

    pdc_tf_dg_t *pdc_tf_dg = (pdc_tf_dg_t *)PDC_calloc(1, sizeof(pdc_tf_dg_t));

    pdc_tf_dg->dg_id     = dg_id;
    pdc_tf_dg->dg_name   = strdup(dg_name);
    pdc_tf_dg->funcs     = NULL;
    pdc_tf_dg->num_funcs = 0;

    FUNC_LEAVE(pdc_tf_dg);
}

perr_t
PDCtf_graph_close(pdc_tf_dg_t *pdc_tf_dg)
{
    FUNC_ENTER(NULL);

    if (pdc_tf_dg) {
        if (pdc_tf_dg->dg_name)
            PDC_free(pdc_tf_dg->dg_name);
        if (pdc_tf_dg->funcs)
            PDC_free(pdc_tf_dg->funcs);
        PDC_free(pdc_tf_dg);
    }

    FUNC_LEAVE(SUCCEED);
}

// Adds a new state to the graph's known set of states
perr_t
PDCtf_graph_add_state(pdc_tf_dg_t *graph, pdc_tf_state_t *state)
{
    FUNC_ENTER(NULL);

    int ret_value = SUCCEED;

    if (!graph)
        PGOTO_ERROR(FAIL, "Invalid graph");
    if (!state)
        PGOTO_ERROR(FAIL, "Invalid state");

    // Allocate memory for the new state
    pdc_tf_state_t *new_states =
        (pdc_tf_state_t *)PDC_realloc(graph->func_ids, (graph->num_funcs + 1) * sizeof(pdc_tf_state_t));
    if (!new_states)
        PGOTO_ERROR(FAIL, "Memory allocation failed for new state");

    // Add the new state to the graph
    new_states[graph->num_funcs] = *state;
    graph->funcs                 = new_states;
    graph->num_funcs++;

done:
    FUNC_LEAVE(ret_value);
}

// Connects the input/output states to a function
perr_t
PDCtf_func_set_states(pdc_tf_func_t *func, pdc_tf_state_t *input, pdc_tf_state_t *output)
{
    FUNC_ENTER(NULL);

    int ret_value = SUCCEED;

    if (!func)
        PGOTO_ERROR(FAIL, "Invalid function");
    if (!input)
        PGOTO_ERROR(FAIL, "Invalid input state");
    if (!output)
        PGOTO_ERROR(FAIL, "Invalid output state);

    func->input_state  = input;
    func->output_state = output;

done:
    FUNC_LEAVE(ret_value);
}
