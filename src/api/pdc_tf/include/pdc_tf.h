#ifndef PDC_TF_H
#define PDC_TF_H

#include "pdc_public.h"

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

// attach transformation to DG
void PDCtf_add_func(pdcid_t dg_id, pdcid_t func_id);

/**
 * creates a new data state and returns ID
 * the state_name is used when attaching the directed graph to a PDC resources
 * to specify the source state and the destination state.
 */
pdcid_t PDCtf_create_state(char *state_name);

/**
 * creates a new function and returns ID
 * the lib:func is the path to the library and function to execute
 */
pdcid_t PDCtf_create_func(char *path_colon_name, pdc_tf_dev_t dev, pdcid_t input_data_state,
                          pdcid_t output_data_state);

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

#endif
