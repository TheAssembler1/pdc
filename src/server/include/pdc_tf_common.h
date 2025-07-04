#ifndef PDC_TF_COMMON_H
#define PDC_TF_COMMON_H

#include "pdc.h"

typedef struct state {
    pdcid_t id;
    char *  name;
} state;

typedef struct func {
    pdc_tf_dev_t dev;
    char *       path_colon_name;
    bool (*c_func)(void *input, void **output);
} func;

extern pdc_dg_t *graphs[200];
extern state *   states[200];

perr_t PDCtf_exec_graph(pdcid_t dg_id, pdcid_t current_state_id, pdcid_t desired_state_id);

// FIXME: dummy transformation function
bool c_func_dummy(void *input, void **output);

#endif // PDC_TF_COMMON_H