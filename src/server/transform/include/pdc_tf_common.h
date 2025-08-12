#ifndef PDC_TF_COMMON_H
#define PDC_TF_COMMON_H

#include "pdc_public.h"
#include "pdc_dg.h"
#include "pdc_region.h"
#include "pdc_obj_pkg.h"

#define MAX_REGIONS 10

typedef struct pdc_tf_region_t {
    size_t   ndim;
    uint8_t  unit;
    uint64_t size[DIM_MAX];
} pdc_tf_region_t;

typedef struct pdc_tf_region_state_t {
    pdcid_t dg_id;
    char   *cur_state;
    char   *client_state;
    char   *store_state;
} pdc_tf_region_state_t;

typedef struct pdc_tf_region_mapping_t {
    pdc_tf_region_state_t region_state;
    pdc_tf_region_t       conceptual_region;
    uint64_t              conceptual_offset[DIM_MAX];
    pdc_tf_region_t       actual_region;
} pdc_tf_region_mapping_t;

/**
 * This is a field in _pdc_obj_info that enables transformations
 * This is a mapping between conceptual and actual regions
 * Example: Conceptual region is the region before compression,
 *          actual region is the region after compression.
 *          The conceptual region is used to define the transformation,
 *          while the actual region is used to store the data.
 * The num_region is the number of regions with attached graphs.
 */
typedef struct pdc_tf_obj_t {
    pdc_tf_region_mapping_t region_mappings[MAX_REGIONS];
    uint32_t                num_region_mappings;
} pdc_obj_tf_t;

typedef enum pdc_tf_granularities_t {
    PDC_TF_ELEMENT_GRANULARITY,
    PDC_TF_REGION_GRANULARITY,
    PDC_TF_NUM_GRANULARITIES
} pdc_tf_granularities_t;
extern char *pdc_tf_granularity_strs[];

typedef struct pdc_tf_state_t {
    char                  *name;
    pdc_tf_granularities_t granularity;
} pdc_tf_state_t;

/**
 * Prototype for region transformation functions
 *
 * Before the function is invoked, `output_state.tf_region` is set to `input_state.tf_region`, so if the
 * transformation does not change the region size, the user does not need to
 * modify `output_state.tf_region`.
 *
 * `region_data` is a double pointer to the input region's data buffer.
 * The function may either mutate the existing buffer in place or allocate a new
 * buffer and update `*region_data` to point to it.
 *
 * If a new data buffer is assigned to `*region_data`, it must be heap-allocated
 * so that PDC can free it. The original pointer should NOT be freed.
 */
typedef bool (*c_func_t)(void *params, void **region_data, pdc_tf_region_t input_state,
                         pdc_tf_region_t *output_state);

/**
 * what device the function can run on
 */
typedef enum pdc_tf_dev_t { PDC_TF_CPU_DEVICE, PDC_TF_GPU_DEVICE, PDC_TF_NUM_DEVICES } pdc_tf_dev_t;
extern char *pdc_tf_dev_strs[];

/**
 * Is this an internal or external function.
 */
typedef enum pdc_tf_location_t { PDC_TF_BUILTIN, PDC_TF_EXTERNAL, PDC_TF_NUM_LOCATIONS } pdc_tf_location_t;
extern char *pdc_tf_location_strs[];

typedef struct pdc_tf_func_t {
    pdc_tf_dev_t      dev;
    pdc_tf_location_t location;
    char             *name;
    c_func_t          c_func;
} pdc_tf_func_t;

// FIXME: we could store this in a dynamically allocated buf
#define PDC_TF_MAX_FUNC_NAME_LEN 100
#define PDC_TF_MAX_BUILTIN_FUNCS 100
#define PDC_TF_MAPPINGS          100

/**
 * This could just happen on client/server init
 * Currently happens in PDCtf_open_dg_json_common
 */
extern bool pdc_tf_has_init_g;

// this structure used to store our builtin functions
typedef struct pdc_tf_builtin_func_t {
    char     name[PDC_TF_MAX_FUNC_NAME_LEN];
    c_func_t c_func;
} pdc_tf_builtin_func_t;

// this is our global array of builtin functions
extern pdc_tf_builtin_func_t pdc_tf_builtin_funcs_g[PDC_TF_MAX_BUILTIN_FUNCS];
extern uint32_t              pdc_tf_builtin_cur_func_g;

extern pdc_dg_t *pdc_tf_graphs[200];

pdcid_t PDCtf_open_dg_json_common(char *filepath);
perr_t  PDCtf_exec_graph(pdcid_t dg_id, char *cur_state, char *desired_state, pdc_tf_region_t input_region,
                         pdc_tf_region_t *output_region, void **input);
perr_t  PDCtf_init_builtin_funcs();
perr_t  PDCtf_add_builtin_func(char *func_name, c_func_t c_func);
perr_t  PDCtf_link_builtin_func(char *func_name, pdc_tf_func_t *f);
bool    PDCtf_region_has_attached_graph(struct pdc_tf_obj_t *tf_obj, int ndim, uint8_t unit, uint64_t *offset,
                                        uint64_t *size, pdc_tf_region_mapping_t **region_mapping);
size_t  PDCtf_get_pdc_region_t_elements(pdc_tf_region_t reg);
size_t  PDCtf_get_pdc_region_t_bytes(pdc_tf_region_t reg);
void    PDCtf_log_pdc_region_t(pdc_tf_region_t reg);
#endif // PDC_TF_COMMON_H
