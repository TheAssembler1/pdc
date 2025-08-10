#include "pdc_client_server_common.h"
#include "pdc_server_data.h"
#include "pdc_timing.h"
#include "pdc_logger.h"
#include "pdc_malloc.h"

static pdc_region_writeout_strategy storage_strategy_g = STORE_FLATTENED_REGION_PER_FILE;

int
can_reset_dims()
{
    return storage_strategy_g == STORE_REGION_BY_REGION_SINGLE_FILE;
}

perr_t
PDC_server_transfer_request_init()
{
    FUNC_ENTER(NULL);

    transfer_request_status_list = NULL;
    pthread_mutex_init(&transfer_request_status_mutex, NULL);
    pthread_mutex_init(&transfer_request_id_mutex, NULL);
    transfer_request_id_g = 1;

    FUNC_LEAVE(SUCCEED);
}

perr_t
PDC_server_transfer_request_finalize()
{
    FUNC_ENTER(NULL);

    pthread_mutex_destroy(&transfer_request_status_mutex);
    pthread_mutex_destroy(&transfer_request_id_mutex);

    FUNC_LEAVE(SUCCEED);
}

/*
 * Create a new linked list node for a region transfer request and append it to the end of the linked list.
 * Thread-safe function, lock required ahead of time.
 */
perr_t
PDC_commit_request(uint64_t transfer_request_id)
{
    FUNC_ENTER(NULL);

    pdc_transfer_request_status *ptr;
    perr_t                       ret_value = SUCCEED;

    if (transfer_request_status_list == NULL) {
        transfer_request_status_list =
            (pdc_transfer_request_status *)PDC_malloc(sizeof(pdc_transfer_request_status));
        transfer_request_status_list->status              = PDC_TRANSFER_STATUS_PENDING;
        transfer_request_status_list->handle_ref          = NULL;
        transfer_request_status_list->out_type            = -1;
        transfer_request_status_list->transfer_request_id = transfer_request_id;
        transfer_request_status_list->next                = NULL;
        transfer_request_status_list_end                  = transfer_request_status_list;
    }
    else {
        ptr               = transfer_request_status_list_end;
        ptr->next         = (pdc_transfer_request_status *)PDC_malloc(sizeof(pdc_transfer_request_status));
        ptr->next->status = PDC_TRANSFER_STATUS_PENDING;
        ptr->next->handle_ref            = NULL;
        ptr->next->out_type              = -1;
        ptr->next->transfer_request_id   = transfer_request_id;
        ptr->next->next                  = NULL;
        transfer_request_status_list_end = ptr->next;
    }

    FUNC_LEAVE(ret_value);
}

/*
 * Search a linked list for a transfer request.
 * Set the entry status to PDC_TRANSFER_STATUS_COMPLETE.
 * Thread-safe function, lock required ahead of time.
 */
perr_t
PDC_finish_request(uint64_t transfer_request_id)
{
    FUNC_ENTER(NULL);

    pdc_transfer_request_status *   ptr, *tmp = NULL;
    perr_t                          ret_value = SUCCEED;
    transfer_request_wait_out_t     out;
    transfer_request_wait_all_out_t out_all;
    char                            cur_time[64];

    ptr = transfer_request_status_list;
    while (ptr != NULL) {
        if (ptr->transfer_request_id == transfer_request_id) {
            ptr->status = PDC_TRANSFER_STATUS_COMPLETE;
            if (ptr->handle_ref != NULL) {
                /* Wait request is going to be returned, so we are not expecting any further checks for the
                 * current request. Immediately eject the current transfer request out of the list.*/
                ptr->handle_ref[0]--;
                if (!ptr->handle_ref[0]) {
                    if (ptr->out_type == -1) {
                        LOG_ERROR("PDC SERVER PDC_finish_request out type unset error\n");
                    }
                    if (ptr->out_type) {
                        out_all.ret = 1;
                        ret_value   = HG_Respond(ptr->handle, NULL, NULL, &out_all);
                    }
                    else {
                        out.ret   = 1;
                        ret_value = HG_Respond(ptr->handle, NULL, NULL, &out);
                    }
                    HG_Destroy(ptr->handle);
                    ptr->handle_ref = (int *)PDC_free(ptr->handle_ref);
                }
                if (tmp != NULL) {
                    /* Case for removing the any nodes but the first one. */
                    tmp->next = ptr->next;
                    /* Free pointer is the last list node, so we set the end to the previous one. */
                    if (ptr->next == NULL) {
                        transfer_request_status_list_end = tmp;
                    }
                    ptr = (pdc_transfer_request_status *)PDC_free(ptr);
                }
                else {
                    /* Case for removing the first node, i.e ptr == transfer_request_status_list*/
                    tmp                          = transfer_request_status_list;
                    transfer_request_status_list = transfer_request_status_list->next;
                    tmp                          = (pdc_transfer_request_status *)PDC_free(tmp);
                    /* Free pointer is the last list node, so nothing is left in the list. */
                    if (transfer_request_status_list == NULL) {
                        transfer_request_status_list_end = NULL;
                    }
                }
                break;
            }
        }
        tmp = ptr;
        ptr = ptr->next;
    }

    FUNC_LEAVE(ret_value);
}

/*
 * Search a linked list for a region transfer request.
 * Remove the linked list node and free its memory.
 * Return the status of the region transfer request.
 * Thread-safe function, lock required ahead of time.
 */
pdc_transfer_status_t
PDC_check_request(uint64_t transfer_request_id)
{
    FUNC_ENTER(NULL);

    pdc_transfer_request_status *ptr, *tmp = NULL;
    pdc_transfer_status_t        ret_value = PDC_TRANSFER_STATUS_NOT_FOUND;

    ptr = transfer_request_status_list;
    while (ptr != NULL) {
        if (ptr->transfer_request_id == transfer_request_id) {
            ret_value = ptr->status;
            if (ptr->handle_ref != NULL) {
                ret_value = PDC_TRANSFER_STATUS_COMPLETE;
                return ret_value;
            }
            if (ret_value == PDC_TRANSFER_STATUS_COMPLETE) {
                if (tmp != NULL) {
                    /* Case for removing the any nodes but the first one. */
                    tmp->next = ptr->next;
                    /* Free pointer is the last list node, so we set the end to the previous one. */
                    if (ptr->next == NULL) {
                        transfer_request_status_list_end = tmp;
                    }
                    ptr = (pdc_transfer_request_status *)PDC_free(ptr);
                }
                else {
                    /* Case for removing the first node, i.e ptr == transfer_request_status_list*/
                    tmp                          = transfer_request_status_list;
                    transfer_request_status_list = transfer_request_status_list->next;
                    tmp                          = (pdc_transfer_request_status *)PDC_free(tmp);
                    /* Free pointer is the last list node, so nothing is left in the list. */
                    if (transfer_request_status_list == NULL) {
                        transfer_request_status_list_end = NULL;
                    }
                }
            }
            break;
        }
        tmp = ptr;
        ptr = ptr->next;
    }

    FUNC_LEAVE(ret_value);
}

/*
 * Search a linked list for a region transfer request.
 * Bind an RPC handle to the transfer request, so the RPC can be returned when the PDC_finish_request function
 * is called. Thread-safe function, lock required ahead of time.
 */
pdc_transfer_status_t
PDC_try_finish_request(uint64_t transfer_request_id, hg_handle_t handle, int *handle_ref, int out_type)
{
    FUNC_ENTER(NULL);

    pdc_transfer_request_status *ptr;
    pdc_transfer_status_t        ret_value = PDC_TRANSFER_STATUS_NOT_FOUND;

    ptr = transfer_request_status_list;
    while (ptr != NULL) {
        if (ptr->transfer_request_id == transfer_request_id) {
            ptr->handle     = handle;
            ptr->out_type   = out_type;
            ptr->handle_ref = handle_ref;
            handle_ref[0]++;
            break;
        }
        ptr = ptr->next;
    }

    FUNC_LEAVE(ret_value);
}

/*
 * Generate a remote transfer request ID in a very fast way.
 * What happen if we have one request pending and call the register 2^64 times? This could result a repetitive
 * transfer request ID generated.
 * TODO: Scan the entire transfer list and search for repetitive nodes.
 * Not a thread-safe function, need protection from pthread_mutex_lock(&transfer_request_id_mutex);
 */

pdcid_t
PDC_transfer_request_id_register()
{
    FUNC_ENTER(NULL);

    pdcid_t ret_value;

    ret_value = transfer_request_id_g;
    transfer_request_id_g++;

    FUNC_LEAVE(ret_value);
}

/*
 * Core I/O functions for region transfer request.
 * Nonzero io_by_region_g will trigger region by region storage. Otherwise file flatten strategy is used
 */
#define PDC_POSIX_IO(fd, buf, io_size, is_write)                                                             \
    if (is_write) {                                                                                          \
        if (write(fd, buf, io_size) != io_size) {                                                            \
            LOG_ERROR("server POSIX write failed\n");                                                        \
        }                                                                                                    \
    }                                                                                                        \
    else {                                                                                                   \
        if (read(fd, buf, io_size) != io_size) {                                                             \
            LOG_ERROR("server POSIX read failed\n");                                                         \
        }                                                                                                    \
    }

static inline char* get_data_path() {
    FUNC_ENTER(NULL);

    char* user_specified_data_path;
    char* data_path;

    user_specified_data_path = getenv("PDC_DATA_LOC");
    if (user_specified_data_path != NULL) {
        data_path = user_specified_data_path;
    }
    else {
        data_path = getenv("SCRATCH");
        if (data_path == NULL)
            data_path = ".";
    }

done:
    FUNC_LEAVE(data_path);
}

static perr_t PDC_Server_data_io_flattened(
    uint64_t obj_id, 
    int obj_ndim, 
    const uint64_t *obj_dims,
    struct pdc_region_info *region_info, 
    void *buf,
    size_t unit, 
    int is_write) 
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;
    int      fd;
    char *   data_path                = NULL;
    char     storage_location[ADDR_MAX];
    ssize_t  io_size;
    uint64_t i, j;

    if (obj_ndim != (int)region_info->ndim)
        PGOTO_ERROR(FAIL, "Obj dim does not match region dim\n");

    data_path = get_data_path();

    // Data path prefix will be $SCRATCH/pdc_data/$obj_id/server$rank/s$rank.bin
    snprintf(storage_location, ADDR_MAX, "%.200s/pdc_data/%" PRIu64 "/server%d/s%04d.bin", data_path, obj_id,
             PDC_get_rank(), PDC_get_rank());
    PDC_mkdir(storage_location);

    fd = open(storage_location, O_RDWR | O_CREAT, 0666);
    if (region_info->ndim == 1) {
        lseek(fd, region_info->offset[0] * unit, SEEK_SET);
        io_size = region_info->size[0] * unit;
        PDC_POSIX_IO(fd, buf, io_size, is_write);
    }
    else if (region_info->ndim == 2) {
        // Check we can directly write the contiguous chunk to the file
        if (region_info->offset[1] == 0 && region_info->size[1] == obj_dims[1]) {
            lseek(fd, region_info->offset[0] * obj_dims[1] * unit, SEEK_SET);
            io_size = region_info->size[0] * obj_dims[1] * unit;
            PDC_POSIX_IO(fd, buf, io_size, is_write);
        }
        else {
            // We have to write line by line
            for (i = 0; i < region_info->size[0]; ++i) {
                lseek(fd, ((i + region_info->offset[0]) * obj_dims[1] + region_info->offset[1]) * unit,
                      SEEK_SET);
                io_size = region_info->size[1] * unit;
                PDC_POSIX_IO(fd, buf, io_size, is_write);
                buf += io_size;
            }
        }
    }
    else if (region_info->ndim == 3) {
        // Check we can directly write the contiguous chunk to the file
        if (region_info->offset[1] == 0 && region_info->size[1] == obj_dims[1] &&
            region_info->offset[2] == 0 && region_info->size[2] == obj_dims[2]) {
            lseek(fd, region_info->offset[0] * region_info->size[1] * region_info->size[2] * unit, SEEK_SET);
            io_size = region_info->size[0] * region_info->size[1] * region_info->size[2] * unit;
            PDC_POSIX_IO(fd, buf, io_size, is_write);
        }
        else if (region_info->offset[2] == 0 && region_info->size[2] == obj_dims[2]) {
            // We have to write plane by plane
            for (i = 0; i < region_info->size[0]; ++i) {
                lseek(fd,
                      ((i + region_info->offset[0]) * obj_dims[1] * obj_dims[2] +
                       region_info->offset[1] * obj_dims[2]) *
                          unit,
                      SEEK_SET);
                io_size = region_info->size[1] * obj_dims[2] * unit;
                PDC_POSIX_IO(fd, buf, io_size, is_write);
                buf += io_size;
            }
        }
        else {
            // We have to write line by line
            for (i = 0; i < region_info->size[0]; ++i) {
                for (j = 0; j < region_info->size[1]; ++j) {
                    lseek(fd,
                          ((region_info->offset[0] + i) * obj_dims[1] * obj_dims[2] +
                           (region_info->offset[1] + j) * obj_dims[2] + region_info->offset[2]) *
                              unit,
                          SEEK_SET);
                    io_size = region_info->size[2] * unit;
                    PDC_POSIX_IO(fd, buf, io_size, is_write);
                    buf += io_size;
                }
            }
        }
    }
    close(fd);

done:
    FUNC_LEAVE(ret_value);
}

/**
 * Constructs file name for STORE_FLATTENED_REGION_PER_FILE storage strategy
 * The returned pointer is heap allocated caller must free
 */
static inline char* get_storage_location_region_per_file(
    int obj_id, 
    int obj_ndim, 
    const uint64_t* offset, 
    const uint64_t* file_dims) 
{
    FUNC_ENTER(NULL);

    char* storage_location = PDC_calloc(1, sizeof(char) * ADDR_MAX);
    /**
     * This is the filename suffix 
     * Each dimension can be a max of 20 character hence the DIM_MAX * 20
     * Also there is an '_' between each number and NULL terminator hence the DIM_MAX - 1 + 1
     */
    uint32_t storage_location_suffix_max_size = (DIM_MAX * 20) + DIM_MAX - 1 + 1;
    // this is largest 64 bit number represented in base 10 assci and a NULL terminator hence + 1
    uint32_t uint64_t_max_assci_size = 20 + 1;

    // create temp strings and 0 out the data
    char storage_location_suffix[storage_location_suffix_max_size];
    memset(storage_location_suffix, 0, storage_location_suffix_max_size);
    char num_str[uint64_t_max_assci_size];
    memset(num_str, 0, uint64_t_max_assci_size);

    for(int i = 0; i < obj_ndim; i++) {
        // NOTE: we validated earlier that file_dims[i] != 0
        snprintf(num_str, sizeof(num_str), "%" PRIu64, offset[i] / file_dims[i]);
        strcat(storage_location_suffix, num_str);
        // dont' add '_' unless there is another character after
        if(i + 1  != obj_ndim)
            strcat(storage_location_suffix, "_");
    }

    LOG_INFO("Storage location suffix: %s\n", storage_location_suffix);

    // Data path prefix will be $SCRATCH/pdc_data/$obj_id/server$rank/s$rank_$suffix.bin
    snprintf(storage_location, ADDR_MAX, "%.200s/pdc_data/%" PRIu64 "/server%d/s%04d_%s.bin", get_data_path(), obj_id,
             PDC_get_rank(), PDC_get_rank(), storage_location_suffix);

    LOG_INFO("Final region location storage path: %s\n", storage_location);

done:
    FUNC_LEAVE(storage_location);
}

#define FILE_CHANGED(curr, next, file_dims, obj_ndim) ({ \
    int _changed = 0;                                     \
    for (int _i = 0; _i < (obj_ndim); _i++) {            \
        if (((curr)[_i] / (file_dims)[_i]) != ((next)[_i] / (file_dims)[_i])) { \
            _changed = 1;                                 \
            break;                                        \
        }                                                \
    }                                                    \
    _changed;                                            \
})

static perr_t PDC_Server_data_io_region_per_file(
    uint64_t obj_id, 
    int obj_ndim, 
    const uint64_t *obj_dims,
    const uint64_t *file_dims,
    struct pdc_region_info *region_info, 
    void *buf,
    size_t unit, 
    int is_write) 
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;
    char* storage_location = NULL;

    int d;
    uint64_t i, rem;
    uint64_t total_elements = 0;
    uint64_t blocks_per_dim[DIM_MAX];  // assuming DIM_MAX defined >= obj_ndim
    uint64_t start_blocks[DIM_MAX];
    uint64_t total_blocks = 1;
    size_t file_block_elems = 1;
    size_t file_block_size_bytes = 0;

    char **buffers = NULL;

    uint64_t coords[DIM_MAX];
    uint64_t file_block_coords[DIM_MAX];
    uint64_t block_coords_relative[DIM_MAX];
    uint64_t block_coords[DIM_MAX];
    uint64_t local_coords[DIM_MAX];

    uint64_t multiplier;
    uint64_t local_linear_idx;
    uint64_t buf_idx;

    // Validate dimensions
    if (obj_ndim != (int)region_info->ndim)
        PGOTO_ERROR(FAIL, "Obj dim does not match region dim\n");

    for (d = 0; d < obj_ndim; d++) {
        if (file_dims[d] == 0)
            PGOTO_ERROR(FAIL, "file_dims[%d] must be > 0\n", d);
    }

    // Compute total elements in region
    total_elements = PDC_get_region_desc_size(region_info->size, obj_ndim);
    LOG_INFO("Region total elements %lu\n", total_elements);

    // Calculate blocks touched per dimension
    for (d = 0; d < obj_ndim; d++) {
        start_blocks[d] = region_info->offset[d] / file_dims[d];
        uint64_t region_end = region_info->offset[d] + region_info->size[d] - 1;
        uint64_t end_block = region_end / file_dims[d];
        blocks_per_dim[d] = end_block - start_blocks[d] + 1;
        total_blocks *= blocks_per_dim[d];
    }

    // Compute file block buffer size (in bytes)
    for (d = 0; d < obj_ndim; d++)
        file_block_elems *= file_dims[d];
    file_block_size_bytes = file_block_elems * unit;

    // Allocate array of buffers (one per file block)
    buffers = (char **)malloc(total_blocks * sizeof(char *));
    if (!buffers)
        PGOTO_ERROR(FAIL, "Failed to allocate buffers array\n");

    for (i = 0; i < total_blocks; i++)
        buffers[i] = NULL;

    // Helper to flatten multi-dim index to 1D index
    uint64_t flatten_idx(const uint64_t *idx) {
        uint64_t flat = 0;
        uint64_t mul = 1;
        for (d = obj_ndim - 1; d >= 0; d--) {
            flat += idx[d] * mul;
            mul *= blocks_per_dim[d];
        }
        return flat;
    }

    // Allocate and pre-read each buffer if writing
    for (i = 0; i < total_blocks; i++) {
        rem = i;
        for (d = obj_ndim - 1; d >= 0; d--) {
            block_coords[d] = rem % blocks_per_dim[d];
            rem /= blocks_per_dim[d];
        }

        // Absolute file block coords
        for (d = 0; d < obj_ndim; d++) {
            file_block_coords[d] = start_blocks[d] + block_coords[d];
        }

        buffers[i] = malloc(file_block_size_bytes);
        if (!buffers[i])
            PGOTO_ERROR(FAIL, "Failed to allocate buffer %lu\n", i);
        memset(buffers[i], 0, file_block_size_bytes);

        if (is_write) {
            storage_location = get_storage_location_region_per_file(obj_id, obj_ndim, file_block_coords, file_dims);
            int fd = open(storage_location, O_RDONLY);
            if (fd >= 0) {
                ssize_t read_bytes = read(fd, buffers[i], file_block_size_bytes);
                if (read_bytes < 0)
                    LOG_WARN("Failed to read full buffer from %s\n", storage_location);
                close(fd);
            }
            storage_location = PDC_free(storage_location);
        }
    }

    // Copy data from buf to appropriate buffer & offset
    for (i = 0; i < total_elements; i++) {
        uint64_t remainder = i;
        for (d = obj_ndim - 1; d >= 0; d--) {
            coords[d] = remainder % region_info->size[d];
            remainder /= region_info->size[d];
            coords[d] += region_info->offset[d];
        }

        // Compute file block coords relative to start_blocks
        for (d = 0; d < obj_ndim; d++) {
            file_block_coords[d] = coords[d] / file_dims[d];
            block_coords_relative[d] = file_block_coords[d] - start_blocks[d];
        }

        buf_idx = flatten_idx(block_coords_relative);

        // Local coords inside this file block
        for (d = 0; d < obj_ndim; d++) {
            local_coords[d] = coords[d] % file_dims[d];
        }

        // Compute linear local offset in buffer
        local_linear_idx = 0;
        multiplier = 1;
        for (d = obj_ndim - 1; d >= 0; d--) {
            local_linear_idx += local_coords[d] * multiplier;
            multiplier *= file_dims[d];
        }

        memcpy(buffers[buf_idx] + local_linear_idx * unit, (char*)buf + i * unit, unit);
    }

    // Flush buffers to disk
    for (i = 0; i < total_blocks; i++) {
        rem = i;
        for (d = obj_ndim - 1; d >= 0; d--) {
            block_coords[d] = rem % blocks_per_dim[d];
            rem /= blocks_per_dim[d];
        }

        for (d = 0; d < obj_ndim; d++) {
            file_block_coords[d] = start_blocks[d] + block_coords[d];
        }

        storage_location = get_storage_location_region_per_file(obj_id, obj_ndim, file_block_coords, file_dims);
        PDC_mkdir(storage_location);

        int fd = open(storage_location, O_WRONLY | O_CREAT, 0644);
        if (fd < 0) {
            PGOTO_ERROR(FAIL, "Failed to open file %s\n", storage_location);
        }

        ssize_t written = write(fd, buffers[i], file_block_size_bytes);
        if ((size_t)written != file_block_size_bytes) {
            close(fd);
            PGOTO_ERROR(FAIL, "Failed to write full buffer to %s\n", storage_location);
        }
        close(fd);

        storage_location = PDC_free(storage_location);
        free(buffers[i]);
    }

    free(buffers);

done:
    if (storage_location != NULL)
        storage_location = PDC_free(storage_location);

    FUNC_LEAVE(ret_value);
}

perr_t
PDC_Server_transfer_request_io(uint64_t obj_id, int obj_ndim, const uint64_t *obj_dims,
                               struct pdc_region_info *region_info, void *buf, size_t unit, int is_write)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    const uint64_t temp_file_dims[3] = {4096, 4096, 4096};

    /**
     * Switch between storage strategies and hand off to correct handler
     */
    if (storage_strategy_g == STORE_REGION_BY_REGION_SINGLE_FILE || obj_ndim == 0) {
        if (is_write)
            PGOTO_DONE(PDC_Server_data_write_out(obj_id, region_info, buf, unit));
        else
            PGOTO_DONE(PDC_Server_data_read_from(obj_id, region_info, buf, unit));
    } else if (storage_strategy_g == STORE_FLATTENED_SINGLE_FILE) {
        PGOTO_DONE(PDC_Server_data_io_flattened(obj_id, obj_ndim, obj_dims, region_info, buf, unit, is_write));
    } else if (storage_strategy_g == STORE_FLATTENED_REGION_PER_FILE) {
        PGOTO_DONE(PDC_Server_data_io_region_per_file(obj_id, obj_ndim, obj_dims, temp_file_dims, 
                                           region_info, buf, unit, is_write));
    } else {
        PGOTO_ERROR(FAIL, "Invalid storage strategy");
    }

done:
    FUNC_LEAVE(ret_value);
}

int
clean_write_bulk_data(transfer_request_all_data *request_data)
{
    FUNC_ENTER(NULL);

    request_data->obj_id        = (pdcid_t *)PDC_free(request_data->obj_id);
    request_data->obj_ndim      = (int *)PDC_free(request_data->obj_ndim);
    request_data->remote_ndim   = (int *)PDC_free(request_data->remote_ndim);
    request_data->remote_offset = (uint64_t **)PDC_free(request_data->remote_offset);
    request_data->unit          = (size_t *)PDC_free(request_data->unit);
    request_data->data_buf      = (char **)PDC_free(request_data->data_buf);

    FUNC_LEAVE(0);
}

int
parse_bulk_data(void *buf, transfer_request_all_data *request_data, pdc_access_t access_type)
{
    FUNC_ENTER(NULL);

    char *   ptr = (char *)buf;
    int      i, j;
    uint64_t data_size;

    // preallocate arrays of size number of objects
    request_data->obj_id        = (pdcid_t *)PDC_malloc(sizeof(pdcid_t) * request_data->n_objs);
    request_data->obj_ndim      = (int *)PDC_malloc(sizeof(int) * request_data->n_objs);
    request_data->remote_ndim   = (int *)PDC_malloc(sizeof(int) * request_data->n_objs);
    request_data->remote_offset = (uint64_t **)PDC_malloc(sizeof(uint64_t *) * request_data->n_objs * 3);
    request_data->remote_length = request_data->remote_offset + request_data->n_objs;
    request_data->obj_dims      = request_data->remote_length + request_data->n_objs;
    request_data->unit          = (size_t *)PDC_malloc(sizeof(size_t) * request_data->n_objs);
    request_data->data_buf      = (char **)PDC_malloc(sizeof(char *) * request_data->n_objs);

    /*
     * The following times n_objs (one set per object).
     *     obj_id: sizeof(pdcid_t)
     *     obj_ndim: sizeof(int)
     *     remote remote_ndim: sizeof(int)
     *     unit: sizeof(size_t)
     */
    for (i = 0; i < request_data->n_objs; ++i) {
        request_data->obj_id[i] = *((pdcid_t *)ptr);
        ptr += sizeof(pdcid_t);
        request_data->obj_ndim[i] = *((int *)ptr);
        ptr += sizeof(int);
        request_data->remote_ndim[i] = *((int *)ptr);
        ptr += sizeof(int);
        request_data->unit[i] = *((pdcid_t *)ptr);
        ptr += sizeof(size_t);
    }
    /*
     * For each of objects
     *     remote region offset: size(uint64_t) * region_ndim
     *     remote region length: size(uint64_t) * region_ndim
     *     obj_dims: size(uint64_t) * remote_ndim
     *     buf: computed from region length (summed up)
     */
    for (i = 0; i < request_data->n_objs; ++i) {
        request_data->remote_offset[i] = (uint64_t *)ptr;
        ptr += request_data->remote_ndim[i] * sizeof(uint64_t);
        request_data->remote_length[i] = (uint64_t *)ptr;
        ptr += request_data->remote_ndim[i] * sizeof(uint64_t);
        request_data->obj_dims[i] = (uint64_t *)ptr;
        ptr += request_data->obj_ndim[i] * sizeof(uint64_t);
        if (access_type == PDC_WRITE) {
            data_size = request_data->remote_length[i][0] * request_data->unit[i];
            for (j = 1; j < request_data->remote_ndim[i]; ++j) {
                data_size *= request_data->remote_length[i][j];
            }
            request_data->data_buf[i] = (char *)ptr;
            ptr += data_size;
        }
    }

    FUNC_LEAVE(0);
}
