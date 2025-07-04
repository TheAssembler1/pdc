#ifndef PDC_TF_H
#define PDC_TF_H

/**
 * List of FIXME:'s
 *  1. Use actual pdcid_t instead of ad hoc generating them
 *  2. Keep track of region history
 *  3. Create dynamic arrays for MAX_REGIONS
 *  4. When a graph is attached to region ensure no collisions with existing regions
 *  5. On client side graph execution happens synchronously on region start
 *  6. Check for existing structs to represent pdc_tf_remote_reg
 *  7. Rename global region remote region... that could be confusing
 *  8. Figure out of total data size of region is important for identifying during transfer
 *  9. Remove state and func structs also remove the extern
 */

#include "pdc_public.h"
#include "pdc_dg.h"

#define MAX_REGIONS 10

/**
 * since the transfer request doesn't store the
 * region id we have to identify the region
 * by its dimensions using the following attributes
 * there may be another struct already in the codebase
 * we could copy here.
 */
typedef struct pdc_tf_remote_reg {
    size_t       remote_region_ndim;
    uint64_t *remote_region_offset;
    uint64_t *remote_region_size;
    uint64_t  total_data_size;
} pdc_tf_remote_reg;

/**
 * Keeps track of the state of a region as it
 * progresses to the server_state_id or
 * the client_state_id
 */
typedef struct pdc_tf_region_info {
    pdcid_t dg_id;
    pdcid_t current_state_id;
    pdcid_t client_state_id;
    pdcid_t server_state_id;
} pdc_tf_region_info;

/**
 * Used as a  field in _pdc_obj_info see pdc_obj_pkh.h
 * When a user attaches a graph to an object(s)/region
 * it is appended the array of regions here.
 *
 * Each appended region has an associated tf_region_info
 * in the tf_regions_info array. This has information
 * such as the current state of the region and the associated
 * graph.
 *
 * If the graph is attached to an object we simply
 * create a region that spans the entire object and
 * tie the graph to that.
 */
typedef struct pdc_tf_obj_t {
    pdc_tf_remote_reg remote_regions[MAX_REGIONS];
    pdc_tf_region_info tf_regions_info[MAX_REGIONS];
    uint32_t           num_remote_regions;
} pdc_obj_tf_t;

/**
 * what device the function can run on
 */
typedef enum { PDC_TF_GPU_DEVICE, PDC_TF_CPU_DEVICE } pdc_tf_dev_t;

/**
 * - PDC_TF_OVERWRITE: the transformation modifies the existing object.
 * - PDC_TF_CREATE: the transformation creates a new object for the output.
 */
typedef enum {
    PDC_TF_OVERWRITE,
    PDC_TF_CREATE,
} pdc_tf_output_mode_t;

// creates a new directed graph and returns ID
pdcid_t PDCtf_create_dg(char *dg_name);

/**
 * creates a new function and returns ID
 * the lib:func is the path to the library and function to execute
 */
perr_t PDCtf_add_func(pdcid_t dg_id, char *path_colon_name, pdc_tf_dev_t dev, pdcid_t input_state_id,
                      pdcid_t output_state_id);

/**
 * creates a new data state and returns ID
 * the state_name is used when attaching the directed graph to a PDC resources
 * to specify the source state and the destination state.
 */
pdcid_t PDCtf_create_state(char *state_name);

/**
 * specifies how the output of a transformation function should be handled
 * the default is PDC_TF_OVERWRITE if this function is not called
 */
perr_t PDCtf_set_output_mode(pdcid_t dg_id, pdc_tf_output_mode_t mode, pdcid_t *obj_ids, char **result_names,
                             int num_ids);

/**
 * free resources used by directed graph
 * this includes resources managed by functions/data states
 */
perr_t PDCtf_close_dg(pdcid_t dg_id);

// region transfer to/from the specified obj_id, global_reg_id follow DG
perr_t PDCtf_attach_to_region(pdcid_t dg_id, pdcid_t obj_id, pdcid_t global_reg_id, pdcid_t client_state_id,
                              pdcid_t server_state_id);

// all region transfers for obj_id follow DG
perr_t PDCtf_attach_to_obj(pdcid_t dg_id, pdcid_t obj_id, pdcid_t client_state_id, pdcid_t server_state_id);

// all region transfers for specified obj_ids follow DG
perr_t PDCtf_attach_to_objs(pdcid_t dg_id, pdcid_t *obj_ids, int num_ids, pdcid_t client_state_id,
                            pdcid_t server_state_id);

// register data states, transformations, and directed graphs as types
perr_t PDCtf_init();

// print graph in readable format
void PDCtf_print_dg(pdcid_t dg_id);

// print execution path
void PDCtf_print_exec_path(pdcid_t dg_id, pdcid_t client_state_id, pdcid_t server_state_id);

#endif /* PDC_TF_H */
