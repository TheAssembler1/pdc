#ifndef PDC_TF_GRAPH_H
#define PDC_TF_GRAPH_H

#include "pdc.h"

typedef struct pdc_tf_state_t {
    pdcid_t state_id;   // data state ID
    char   *state_name; // name of the data state
};

typedef struct pdc_tf_func_t {
    pdcid_t         func_id;      // transformation function ID
    char           *func_name;    // name of the transformation function
    pdc_tf_dev_t    dev;          // device type for the function
    pdc_tf_state_t *input_state;  // input data state
    pdc_tf_state_t *output_state; // output data state
};

typedef struct pdc_tf_dg_t {
    pdcid_t        dg_id;     // directed graph ID
    char          *dg_name;   // name of the directed graph
    pdc_tf_func_t *funcs;     // array of tf_functions
    int            num_funcs; // number of transformation functions in the graph
};

pdc_tf_dg_t *PDCtf_graph_create(pdcid_t dg_id, const char *dg_name);
perr_t       PDCtf_graph_close(pdc_tf_dg_t *pdc_tf_dg);

// Adds a new state to the graph's known set of states
perr_t PDCtf_graph_add_state(pdc_tf_dg_t *graph, pdc_tf_state_t *state);

// Connects the input/output states to a function
perr_t PDCtf_func_set_states(pdc_tf_func_t *func, pdc_tf_state_t *input, pdc_tf_state_t *output);

#endif /* PDC_TF_GRAPH_H */