#ifndef PDC_TF_COMMON_H
#define PDC_TF_COMMON_H

#include "mercury_proc_string.h"

#include "pdc_public.h"
#include "pdc_dg.h"
#include "pdc_region.h"
#include "pdc_obj_pkg.h"
#include "pdc_vector.h"
#include "pdc_tf_user.h"

typedef struct pdc_tf_region_state_t {
    pdcid_t dg_id;
    char *  cur_state;
    char *  client_state;
    char *  store_state;
} pdc_tf_region_state_t;

typedef struct pdc_tf_region_mapping_t {
    pdc_tf_region_state_t region_state;
    pdc_tf_region_t       conceptual_region;
    uint64_t              conceptual_offset[DIM_MAX];

    // This is the region information for storing on disk
    pdc_tf_region_t actual_region;
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

    // This field is used to attach graphs to individual regions
    PDC_VECTOR *region_mappings_vector;
} pdc_obj_tf_t;

extern char *pdc_tf_dev_strs[];
extern char *pdc_tf_location_strs[];

/**
 * Strings needed by server to run transformation
 */
typedef struct pdc_tf_pkg_t {
    uint32_t    pdc_var_type;
    hg_string_t json_filepath;
    hg_string_t cur_state;
    hg_string_t client_state;
    hg_string_t store_state;
} pdc_tf_pkg_t;

/**
 * This structure used to store our builtin functions
 * Functions are unique according to name and device
 * Allows for identical functions to be differentiated by device
 * Such as zfp compression on the CPU and zfp compression on the GPU
 */
typedef struct pdc_tf_builtin_func_t {
    char *       name;
    pdc_tf_dev_t dev;
    c_func_t     c_func;
} pdc_tf_builtin_func_t;

// this is our global array of builtin functions
extern PDC_VECTOR *pdc_tf_builtin_funcs_vector_g;

perr_t PDCtf_set_tf_region_t(pdc_tf_region_t *dest, uint8_t ndim, pdc_var_type_t pdc_var_type,
                             uint64_t *size);
perr_t PDCtf_copy_tf_region_t(pdc_tf_region_t *src, pdc_tf_region_t *dest);
pdc_dg_t *PDCtf_dg_json_create_common(char *filepath);
perr_t    PDCtf_init_builtin_funcs();
perr_t    PDCtf_add_builtin_func(char *func_name, c_func_t c_func, pdc_tf_dev_t dev);
perr_t    PDCtf_link_builtin_func(char *func_name, pdc_tf_dev_t dev, pdc_tf_func_t *f);
bool   PDCtf_region_has_attached_graph(struct pdc_tf_obj_t *tf_obj, int ndim, size_t unit, uint64_t *offset,
                                       uint64_t *size, pdc_tf_region_mapping_t **region_mapping);
size_t PDCtf_get_pdc_region_t_elements(pdc_tf_region_t reg);
size_t PDCtf_get_flat_conceptual_offset(int ndim, uint64_t offset[4], const uint64_t *dims);
size_t PDCtf_get_pdc_region_t_bytes(pdc_tf_region_t reg);
void   PDCtf_log_pdc_region_t(pdc_tf_region_t reg);
void   PDCtf_print_exec_path_common(pdc_dg_t *dg, char *cur_state, char *desired_state);
void   PDCtf_print_dg_common(pdc_dg_t *dg, bool write_to_file);
#endif // PDC_TF_COMMON_H
