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
 *  10. Parse func name correctly when adding function
 *  11. PDC_TF_MAX_FUNC_NAME_LEN maybe should be dynamic?
 *  12. PDC_TF_MAX_BUILTIN_FUNCS should definitely by dynamic
 */
#include "pdc_public.h"
#include "pdc_dg.h"
#include "pdc_tf_common.h"

/**
 * - PDC_TF_OVERWRITE: the transformation modifies the existing object.
 * - PDC_TF_CREATE: the transformation creates a new object for the output.
 */
typedef enum pdc_tf_output_mode_t {
    PDC_TF_OVERWRITE,
    PDC_TF_CREATE,
} pdc_tf_output_mode_t;

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
                              pdc_var_type_t client_var_type, pdcid_t server_state_id);

// all region transfers for obj_id follow DG
perr_t PDCtf_attach_to_obj(pdcid_t dg_id, pdcid_t obj_id, pdcid_t client_state_id, pdcid_t server_state_id);

// all region transfers for specified obj_ids follow DG
perr_t PDCtf_attach_to_objs(pdcid_t dg_id, pdcid_t *obj_ids, int num_ids, pdcid_t client_state_id,
                            pdcid_t server_state_id);

// register data states, transformations, and directed graphs as types
perr_t PDCtf_init();

// print graph in readable format
void PDCtf_print_dg(pdcid_t dg_id, bool write_to_file);

// print execution path
void PDCtf_print_exec_path(pdcid_t dg_id, pdcid_t client_state_id, pdcid_t server_state_id);

// load JSON file describing directed graph
pdcid_t PDCtf_load_dg_json(char* file_path);

#endif /* PDC_TF_H */
