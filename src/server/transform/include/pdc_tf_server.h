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
typedef struct pdc_tf_server_obj_info_t {
    pdcid_t                 obj_id;
    pdc_tf_region_mapping_t region_mappings[MAX_REGIONS];
    uint32_t                num_region_mappings;
} pdc_tf_server_obj_info_t;
static uint32_t num_tf_server_obj_infos = 0;

#define MAX_NUM_TF_SERVER_OBJ_INFOS 100

pdc_tf_server_obj_info_t pdc_tf_server_obj_info[MAX_NUM_TF_SERVER_OBJ_INFOS];

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
perr_t PDCtf_store_json_mapping(char *json_filepath, char *cur_state, char *store_state, uint64_t *offset,
                                uint64_t *size, uint8_t ndim, uint8_t unit);

#endif