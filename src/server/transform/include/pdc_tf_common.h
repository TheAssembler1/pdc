#ifndef PDC_TF_COMMON_H
#define PDC_TF_COMMON_H

#include "mercury_proc_string.h"

#include "pdc_public.h"
#include "pdc_dg.h"
#include "pdc_region.h"
#include "pdc_obj_pkg.h"

#define MAX_REGIONS 10

typedef struct pdc_tf_region_t {
    size_t         ndim;
    pdc_var_type_t pdc_var_type;
    uint64_t       size[DIM_MAX];
} pdc_tf_region_t;

typedef struct pdc_tf_region_state_t {
    pdcid_t dg_id;
    char   *cur_state;
    char   *client_state;
    char   *store_state;
} pdc_tf_region_state_t;

typedef struct pdc_tf_region_mapping_t {
    pdc_tf_region_state_t region_state;
    pdc_var_type_t        pdc_var_type;
    pdc_tf_region_t       conceptual_region;
    uint64_t              conceptual_offset[DIM_MAX];

    // This is the region information for storing on disk
    pdc_tf_region_t actual_region;

    /**
     * In the directed graph, both edges and nodes can have associated void* states.
     * These states are specific to each region mapping, so to locate the correct
     * void* for a given state/edge you need three pieces of information:
     *   1. the object ID
     *   2. the region mapping ID
     *   3. the state/edge identifier
     *
     * In other words, each conceptual region can have its own distinct void*
     * associated with each state/edge in the graph.
     */
    uint32_t cur_region_mapping_id;
} pdc_tf_region_mapping_t;

/**
 * This is a field in _pdc_obj_info that enables transformations
 * This is a mapping between conceptual and actual regions
 * Example: Conceptual region is the region before compression,
 *          actual region is the region after compression.
 *          The conceptual region is used to define the transformation,
 *          while the actual region is used to store the data.
 * The num_region is the number of regions with attached graphs.
 * The attach_to_all_regions indicates whether all region transfers
 * should go through the directed graph.
 */
typedef struct pdc_tf_obj_t {
    /**
     * These fields are for attaching graphs to region transfers
     * after the graph has been attached to the entire object
     */
    bool                  attach_to_all_regions;
    pdc_tf_region_state_t all_regions_state;

    /**
     * These fields are used to attach graphs to individual regions
     */
    pdc_tf_region_mapping_t region_mappings[MAX_REGIONS];
    uint32_t                num_region_mappings;
} pdc_obj_tf_t;

typedef enum pdc_tf_granularities_t {
    PDC_TF_ELEMENT_GRANULARITY,
    PDC_TF_REGION_GRANULARITY,
    PDC_TF_NUM_GRANULARITIES
} pdc_tf_granularities_t;
extern char *pdc_tf_granularity_strs[];

// FIXME: this should be a dynamic array
#define MAX_PDC_DG_PARAMS 10

/**
 * Used to store parameters for states and edges
 * within the directed graph.
 *
 * The conceptual ID (which is the flat offset)
 * is used to identify the parameters
 * for a specific region.
 */
typedef struct pdc_tf_dg_params_t {
    uint64_t flat_conceptual_offset;
    void    *params;
    uint64_t params_size;
} pdc_tf_dg_params_t;

typedef struct pdc_tf_state_t {
    char                  *name;
    pdc_tf_dg_params_t     pdc_tf_dg_params_list[MAX_PDC_DG_PARAMS];
    uint32_t               cur_num_params;
    pdc_tf_granularities_t granularity;
} pdc_tf_state_t;

typedef struct pdc_tf_internal_param {
    pdc_dg_t *dg;
    uint64_t  flat_conceptual_offset;
} pdc_tf_internal_param;

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
typedef bool (*c_func_t)(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                         pdc_tf_region_t input_region, pdc_tf_region_t *output_region);

// Specifies what device the function can run on
typedef enum pdc_tf_dev_t { PDC_TF_CPU_DEVICE, PDC_TF_GPU_DEVICE, PDC_TF_NUM_DEVICES } pdc_tf_dev_t;
extern char *pdc_tf_dev_strs[];

// Specifies whether a function is internal or external.
typedef enum pdc_tf_location_t { PDC_TF_BUILTIN, PDC_TF_EXTERNAL, PDC_TF_NUM_LOCATIONS } pdc_tf_location_t;
extern char *pdc_tf_location_strs[];

typedef struct pdc_tf_func_t {
    pdc_tf_dev_t       dev;
    pdc_tf_location_t  location;
    char              *name;
    pdc_tf_dg_params_t pdc_tf_dg_params_list[MAX_PDC_DG_PARAMS];
    uint32_t           cur_num_params;
    char              *params_str;
    c_func_t           c_func;
} pdc_tf_func_t;

/**
 * Strings need by server to run transformation
 */
typedef struct pdc_tf_pkg_t {
    uint32_t    pdc_var_type;
    hg_string_t json_filepath;
    hg_string_t cur_state;
    hg_string_t client_state;
    hg_string_t store_state;
} pdc_tf_pkg_t;

// FIXME: we could store this in a dynamically allocated buf
#define PDC_TF_MAX_FUNC_NAME_LEN 100
#define PDC_TF_MAX_BUILTIN_FUNCS 100
#define PDC_TF_MAPPINGS          100

// this structure used to store our builtin functions
typedef struct pdc_tf_builtin_func_t {
    char     name[PDC_TF_MAX_FUNC_NAME_LEN];
    c_func_t c_func;
} pdc_tf_builtin_func_t;

// this is our global array of builtin functions
extern pdc_tf_builtin_func_t pdc_tf_builtin_funcs_g[PDC_TF_MAX_BUILTIN_FUNCS];
extern uint32_t              pdc_tf_builtin_cur_func_g;

perr_t PDCtf_set_tf_region_t(pdc_tf_region_t *dest, uint8_t ndim, pdc_var_type_t pdc_var_type,
                             uint64_t *size);
perr_t PDCtf_copy_tf_region_t(pdc_tf_region_t *src, pdc_tf_region_t *dest);
perr_t PDCtf_set_state_param(pdc_dg_t *dg, char *state_name, uint64_t flat_conceptual_offset, void *params,
                             uint64_t params_size);
perr_t PDCtf_get_state_param(pdc_dg_t *dg, char *state_name, uint64_t flat_conceptual_offset, void **params,
                             uint64_t *params_size);
perr_t PDCtf_set_func_param(pdc_dg_t *dg, char *func_name, uint64_t flat_conceptual_offset, void *params,
                            uint64_t params_size);
perr_t PDCtf_get_func_param(pdc_dg_t *dg, char *func_name, uint64_t flat_conceptual_offset, void **params,
                            uint64_t *params_size);
pdc_dg_t *PDCtf_dg_json_create_common(char *filepath);
perr_t    PDCtf_init_builtin_funcs();
perr_t    PDCtf_add_builtin_func(char *func_name, c_func_t c_func);
perr_t    PDCtf_link_builtin_func(char *func_name, pdc_tf_func_t *f);
bool   PDCtf_region_has_attached_graph(struct pdc_tf_obj_t *tf_obj, int ndim, size_t unit, uint64_t *offset,
                                       uint64_t *size, pdc_tf_region_mapping_t **region_mapping);
size_t PDCtf_get_pdc_region_t_elements(pdc_tf_region_t reg);
size_t PDCtf_get_flat_conceptual_offset(int ndim, uint64_t offset[4], const uint64_t *dims);
size_t PDCtf_get_pdc_region_t_bytes(pdc_tf_region_t reg);
void   PDCtf_log_pdc_region_t(pdc_tf_region_t reg);
void   PDCtf_print_exec_path_common(pdc_dg_t *dg, char *cur_state, char *desired_state);
void   PDCtf_print_dg_common(pdc_dg_t *dg, bool write_to_file);
#endif // PDC_TF_COMMON_H
