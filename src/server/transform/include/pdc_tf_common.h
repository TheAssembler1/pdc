#ifndef PDC_TF_COMMON_H
#define PDC_TF_COMMON_H

#include "pdc.h"

/**
 * used as input and output region for transformations
 * ndim: number of dimensions
 * dims: array of dimensions
 * unit: bytes/element
 */
typedef struct pdc_tf_region_t {
    size_t   ndim;
    uint64_t dims[DIM_MAX];
    uint32_t unit;
} pdc_tf_region_t;


typedef struct state {
    pdcid_t id;
    char *  name;
    /**
     * store the latest shape of the data when at this state
     * useful for compression:
     *
     * Ex. If a previous PDC_WRITE ran a transformation to compress
     * the data to a region. The corresponding READ needs ot know
     * what the size of the compression was. The server_state will
     * have the region size which can be used.
     */
    //pdc_tf_region_t region;
} state;

/**
 * Prototype for region transformation functions
 *
 * Before the function is invoked, `output_reg` is set to `input_reg`, so if the
 * transformation does not change the region size, the user does not need to
 * modify `output_reg`.
 *
 * `region_data` is a double pointer to the input region's data buffer.
 * The function may either mutate the existing buffer in place or allocate a new
 * buffer and update `*region_data` to point to it.
 *
 * If a new data buffer is assigned to `*region_data`, it must be heap-allocated
 * so that PDC can free it. The original pointer should NOT be freed.
 */
typedef bool (*c_func_t)(void *params, void **region_data,
                         pdc_tf_region_t input_reg, pdc_tf_region_t* output_reg);

typedef struct func {
    pdc_tf_dev_t dev;
    char *       type_func_name;
    // could be NULL if a GPU function
    c_func_t c_func;
} func;

// FIXME: we could store this in a dynamically allocated buf
#define PDC_TF_MAX_FUNC_NAME_LEN 100
#define PDC_TF_MAX_BUILTIN_FUNCS 100
// FIXME: this could just happen on client/server init
extern bool pdc_tf_has_init_g;

// this structure used to store our builtin functions
typedef struct pdc_tf_builtin_func_t {
    char     name[PDC_TF_MAX_FUNC_NAME_LEN];
    c_func_t c_func;
} pdc_tf_builtin_func_t;

// this is our global array of builtin functions
extern pdc_tf_builtin_func_t pdc_tf_builtin_funcs_g[PDC_TF_MAX_BUILTIN_FUNCS];
extern uint32_t              pdc_tf_builtin_cur_func_g;

extern pdc_dg_t *graphs[200];
extern state *   states[200];

perr_t PDCtf_exec_graph(pdcid_t dg_id, pdcid_t current_state_id, pdcid_t desired_state_id,
                        pdc_tf_region_t input_region, pdc_tf_region_t* output_region, void **input);
perr_t PDCtf_init_builtin_funcs();
perr_t PDCtf_add_builtin_func(char *func_name, c_func_t c_func);
perr_t PDCtf_link_builtin_func(char *func_name, func *f);
#endif // PDC_TF_COMMON_H
