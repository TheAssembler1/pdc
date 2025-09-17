#ifndef PDC_SERVER_REGION_TRANSFER_H
#define PDC_SERVER_REGION_TRANSFER_H

#include "pdc_region.h"

typedef enum pdc_region_writeout_strategy {
    /**
     * Store data as multiple regions inside a single file.
     * Overlapping writes that are not fully contained append new regions
     * to the end of the file, with metadata tracking region locations.
     * Supports incremental updates without rewriting large parts of the file.
     */
    STORE_REGION_BY_REGION_SINGLE_FILE = 0,

    /**
     * Store the entire object as a single flat file.
     * Reads and writes operate by seeking directly within the file.
     * No region metadata bookkeeping; simpler but less flexible for partial updates.
     */
    STORE_FLATTENED_SINGLE_FILE,

    /**
     * Store each flattened region in its own separate file.
     * Enables independent file management per region.
     */
    STORE_FLATTENED_REGION_PER_FILE
} pdc_region_writeout_strategy;

typedef struct transfer_request_all_data {
    uint64_t **obj_dims;
    uint64_t **remote_offset;
    uint64_t **remote_length;
    pdcid_t *  obj_id;
    int *      obj_ndim;
    size_t *   unit;
    int *      remote_ndim;
    char **    data_buf;
    int        n_objs;
} transfer_request_all_data;

typedef struct pdc_transfer_request_status {
    hg_handle_t                         handle;
    uint64_t                            transfer_request_id;
    uint32_t                            status;
    int *                               handle_ref;
    int                                 out_type;
    struct pdc_transfer_request_status *next;
} pdc_transfer_request_status;

pdc_transfer_request_status *transfer_request_status_list;
pdc_transfer_request_status *transfer_request_status_list_end;
pthread_mutex_t              transfer_request_status_mutex;
pthread_mutex_t              transfer_request_id_mutex;
uint64_t                     transfer_request_id_g;

perr_t PDC_server_transfer_request_init();

perr_t PDC_server_transfer_request_finalize();

int try_reset_dims();

int get_server_rank();

/*
 * Create a new linked list node for a region transfer request and append it to the end of the linked list.
 * Thread-safe function, lock required ahead of time.
 */
perr_t PDC_commit_request(uint64_t transfer_request_id);

/*
 * Search a linked list for a transfer request.
 * Set the entry status to PDC_TRANSFER_STATUS_COMPLETE.
 * Thread-safe function, lock required ahead of time.
 */
perr_t PDC_finish_request(uint64_t transfer_request_id);

/*
 * Search a linked list for a region transfer request.
 * Remove the linked list node and free its memory.
 * Return the status of the region transfer request.
 * Thread-safe function, lock required ahead of time.
 */
pdc_transfer_status_t PDC_check_request(uint64_t transfer_request_id);

/*
 * Search a linked list for a region transfer request.
 * Bind an RPC handle to the transfer request, so the RPC can be returned when the PDC_finish_request function
 * is called. Thread-safe function, lock required ahead of time.
 */
pdc_transfer_status_t PDC_try_finish_request(uint64_t transfer_request_id, hg_handle_t handle,
                                             int *handle_ref, int out_type);

/*
 * Generate a remote transfer request ID in a very fast way.
 * What happen if we have one request pending and call the register 2^64 times? This could result a repetitive
 * transfer request ID generated.
 * TODO: Scan the entire transfer list and search for repetitive nodes.
 * Not a thread-safe function, need protection from pthread_mutex_lock(&transfer_request_id_mutex);
 */
pdcid_t PDC_transfer_request_id_register();

perr_t PDC_Server_transfer_request_io(uint64_t obj_id, int obj_ndim, const uint64_t *obj_dims,
                                      struct pdc_region_info *region_info, void *buf, size_t unit,
                                      int is_write);

int clean_write_bulk_data(transfer_request_all_data *request_data);

int parse_bulk_data(void *buf, transfer_request_all_data *request_data, pdc_access_t access_type);

#endif