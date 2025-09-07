#ifndef PDC_TF_H
#define PDC_TF_H

#include "pdc_tf_common.h"
#include "pdc_logger.h"
#include "pdc_timing.h"

/**
 * This is similar to the client side pdc_tf_obj_t
 * The mean difference being that in the former
 * the structure is a field on _pdc_obj_info which has the
 * object id. We need to keep track of it on the server side.
 * Hence the extra obj_id field here.
 */

typedef struct pdc_tf_obj_id_to_dg_t {
    pdcid_t             obj_id;
    struct pdc_tf_obj_t pdc_tf_obj;
    pdc_dg_t *          dg;
} pdc_tf_obj_id_to_dg_t;

// FIXME: this should be dynamic
#define MAX_TF_OBJECT_ID_TO_DG_MAPPINGS 400

extern pdc_tf_obj_id_to_dg_t pdc_tf_obj_id_to_dg_list[MAX_TF_OBJECT_ID_TO_DG_MAPPINGS];
extern uint32_t              num_objs_with_dg;

/**
 * These functions should only be used on the
 * data server. There are similar functions between
 * this and the client API, however, the method in
 * which directed graphs and their relation to regions
 * is stored does not use the pdcid_t on the server side.
 */

/**
 * Load the JSON filepath into a directed graph and
 * creates a mapping for the conceptual to actual region representation
 * NOTE: This does not set the actual reigon size because that should be
 *       set after the transformations have been run when writing to the data server
 *       and before transformations have been run when reading from the data server
 */
perr_t PDCtf_store_json_mapping(pdcid_t obj_id, char *json_filepath, char *cur_state, char *client_state,
                                char *store_state, uint64_t *offset, uint64_t *size, uint8_t ndim,
                                pdc_var_type_t pdc_var_type);
perr_t PDCtf_exec_graph(pdc_dg_t *dg, uint64_t flat_conceptual_offset, char *cur_state, char *desired_state,
                        pdc_tf_region_t input_region, pdc_tf_region_t *output_region, void **input,
                        int is_write);
#endif