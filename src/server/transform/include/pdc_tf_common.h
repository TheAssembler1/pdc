#ifndef PDC_TF_COMMON_H
#define PDC_TF_COMMON_H

#include "pdc.h"

typedef struct state {
    pdcid_t id;
    char *  name;
} state;

typedef struct func {
    pdc_tf_dev_t dev;
    char *       type_func_name;
    // could be NULL if a GPU function
    bool (*c_func)(void *input, void **output);
} func;

// FIXME: we could store this in a dynamically allocated buf
#define PDC_TF_MAX_FUNC_NAME_LEN 100
#define PDC_TF_MAX_BUILTIN_FUNCS 100
// FIXME: this could just happen on client/server init
extern bool pdc_tf_has_init_g;

// this structure used to store our builtin functions
typedef struct pdc_tf_builtin_func_t {
    char name[PDC_TF_MAX_FUNC_NAME_LEN];
    bool (*c_func)(void *input, void **output);
} pdc_tf_builtin_func_t;

// this is our global array of builtin functions
extern pdc_tf_builtin_func_t pdc_tf_builtin_funcs_g[PDC_TF_MAX_BUILTIN_FUNCS];
extern uint32_t pdc_tf_builtin_cur_func_g;

extern pdc_dg_t *graphs[200];
extern state *   states[200];

perr_t PDCtf_exec_graph(pdcid_t dg_id, pdcid_t current_state_id, pdcid_t desired_state_id);
perr_t PDCtf_init_builtin_funcs();
perr_t PDCtf_add_builtin_func(char* func_name, bool (*c_func)(void *input, void **output));
perr_t PDCtf_link_builtin_func(char* func_name, func* f);
#endif // PDC_TF_COMMON_H
