#include "pdc_tf_common.h"
#include "pdc_tf.h"
#include "pdc_timing.h"

pdc_dg_t *graphs[200];
state *   states[200];

bool c_func_dummy(void* input, void** output) {
    LOG_INFO("c_func_dummy was called\n");

    return true;
}

perr_t PDCtf_exec_graph(pdcid_t dg_id, pdcid_t current_state_id, pdcid_t desired_state_id)
{
    FUNC_ENTER(NULL);

    int ret_value = SUCCEED;

    LOG_INFO("PDCtf_exec_graph was called\n");
    pdc_dg_t* dg = graphs[dg_id];

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
            func* f = (func*)(e.data);

            LOG_INFO("running %d: %s(%s) = %s\n", j + 1, ((func *)(e.data))->path_colon_name, v1->name, v2->name);
            f->c_func(NULL, NULL);
        }
    }
    else {
        LOG_INFO("No path found\n");
        PGOTO_ERROR(FAIL, "No path to desired states");
    }

done:
    FUNC_LEAVE(ret_value);
}