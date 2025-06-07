#ifndef PDC_TIMING_H
#define PDC_TIMING_H

#ifndef HOST_NAME_MAX
#if defined(__APPLE__)
#define HOST_NAME_MAX 255
#else
#define HOST_NAME_MAX 64
#endif /* __APPLE__ */
#endif /* HOST_NAME_MAX */

#include "pdc_config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <limits.h>

#ifdef PDC_TIMING
typedef struct pdc_timestamp {
    // array of start times
    double *start;
    // array of corresponding end times
    double *end;
    size_t  timestamp_max_size;
    size_t  timestamp_cur_size;
} pdc_timestamp;

#ifndef IS_PDC_SERVER
typedef enum {
    // Buffer object map/unmap
    PDC_CLIENT_BUF_OBJ_MAP_RPC_TIME = 0,
    PDC_CLIENT_BUF_OBJ_UNMAP_RPC_TIME,
    PDC_CLIENT_BUF_OBJ_MAP_RPC_WAIT_TIME,
    PDC_CLIENT_BUF_OBJ_UNMAP_RPC_WAIT_TIME,

    // Lock obtain/release (WRITE/READ)
    PDC_CLIENT_REG_OBTAIN_LOCK_WRITE_RPC_TIME,
    PDC_CLIENT_REG_RELEASE_LOCK_WRITE_RPC_TIME,
    PDC_CLIENT_REG_OBTAIN_LOCK_READ_RPC_TIME,
    PDC_CLIENT_REG_RELEASE_LOCK_READ_RPC_TIME,

    PDC_CLIENT_REG_OBTAIN_LOCK_WRITE_RPC_WAIT_TIME,
    PDC_CLIENT_REG_RELEASE_LOCK_WRITE_RPC_WAIT_TIME,
    PDC_CLIENT_REG_OBTAIN_LOCK_READ_RPC_WAIT_TIME,
    PDC_CLIENT_REG_RELEASE_LOCK_READ_RPC_WAIT_TIME,

    // Transfer request WRITE
    PDC_CLIENT_TRANSFER_REQUEST_START_WRITE_RPC_TIME,
    PDC_CLIENT_TRANSFER_REQUEST_WAIT_WRITE_RPC_TIME,
    PDC_CLIENT_TRANSFER_REQUEST_START_WRITE_RPC_WAIT_TIME,
    PDC_CLIENT_TRANSFER_REQUEST_WAIT_WRITE_RPC_WAIT_TIME,

    // Transfer request READ
    PDC_CLIENT_TRANSFER_REQUEST_START_READ_RPC_TIME,
    PDC_CLIENT_TRANSFER_REQUEST_WAIT_READ_RPC_TIME,
    PDC_CLIENT_TRANSFER_REQUEST_START_READ_RPC_WAIT_TIME,
    PDC_CLIENT_TRANSFER_REQUEST_WAIT_READ_RPC_WAIT_TIME,

    // Transfer request ALL
    PDC_CLIENT_TRANSFER_REQUEST_START_ALL_WRITE_RPC_TIME,
    PDC_CLIENT_TRANSFER_REQUEST_START_ALL_WRITE_RPC_WAIT_TIME,
    PDC_CLIENT_TRANSFER_REQUEST_START_ALL_READ_RPC_TIME,
    PDC_CLIENT_TRANSFER_REQUEST_START_ALL_READ_RPC_WAIT_TIME,
    PDC_CLIENT_TRANSFER_REQUEST_WAIT_ALL_RPC_TIME,
    PDC_CLIENT_TRANSFER_REQUEST_WAIT_ALL_RPC_WAIT_TIME,

    // Metadata and object/container creation
    PDC_CLIENT_TRANSFER_REQUEST_METADATA_QUERY_RPC_TIME,
    PDC_CLIENT_OBJ_CREATE_RPC_TIME,
    PDC_CLIENT_CONT_CREATE_RPC_TIME,

    // Enum size
    PDC_CLIENT_TIMES_SIZE,
} pdc_client_timestamps_t;

static const char *pdc_client_timestamps_str[] = {
    // Buffer object map/unmap
    "Client buf obj map rpc time", "Client buf obj unmap rpc time", "Client buf obj map rpc wait time",
    "Client buf obj unmap rpc wait time",

    // Lock obtain/release (write/read)
    "Client reg obtain lock write rpc time", "Client reg release lock write rpc time",
    "Client reg obtain lock read rpc time", "Client reg release lock read rpc time",

    "Client reg obtain lock write rpc wait time", "Client reg release lock write rpc wait time",
    "Client reg obtain lock read rpc wait time", "Client reg release lock read rpc wait time",

    // Transfer request write
    "Client transfer request start write rpc time", "Client transfer request wait write rpc time",
    "Client transfer request start write rpc wait time", "Client transfer request wait write rpc wait time",

    // Transfer request read
    "Client transfer request start read rpc time", "Client transfer request wait read rpc time",
    "Client transfer request start read rpc wait time", "Client transfer request wait read rpc wait time",

    // Transfer request all
    "Client transfer request start all write rpc time",
    "Client transfer request start all write rpc wait time",
    "Client transfer request start all read rpc time", "Client transfer request start all read rpc wait time",
    "Client transfer request wait all rpc time", "Client transfer request wait all rpc wait time",

    // Metadata and object/container creation
    "Client transfer request metadata query rpc time", "Client obj create rpc time",
    "Client cont create rpc time"};

extern pdc_timestamp *pdc_client_timestamps_g[PDC_CLIENT_TIMES_SIZE];
extern double         pdc_client_timings_g[PDC_CLIENT_TIMES_SIZE];
#else  // IS_PDC_SERVER
typedef enum {
    // Buffer object map/unmap
    PDC_SERVER_BUF_OBJ_MAP_RPC_TIME = 0,
    PDC_SERVER_BUF_OBJ_UNMAP_RPC_TIME,

    // Lock obtain/release
    PDC_SERVER_OBTAIN_LOCK_WRITE_TIME,
    PDC_SERVER_OBTAIN_LOCK_READ_TIME,
    PDC_SERVER_RELEASE_LOCK_WRITE_TIME,
    PDC_SERVER_RELEASE_LOCK_READ_TIME,
    PDC_SERVER_RELEASE_LOCK_BULK_TRANSFER_WRITE_TIME,
    PDC_SERVER_RELEASE_LOCK_BULK_TRANSFER_INNER_WRITE_TIME,
    PDC_SERVER_RELEASE_LOCK_BULK_TRANSFER_READ_TIME,
    PDC_SERVER_RELEASE_LOCK_BULK_TRANSFER_INNER_READ_TIME,

    // Transfer request WRITE
    PDC_SERVER_TRANSFER_REQUEST_START_WRITE_TIME,
    PDC_SERVER_TRANSFER_REQUEST_START_READ_TIME,
    PDC_SERVER_TRANSFER_REQUEST_WAIT_WRITE_TIME,
    PDC_SERVER_TRANSFER_REQUEST_WAIT_READ_TIME,
    PDC_SERVER_TRANSFER_REQUEST_START_WRITE_BULK_TIME,
    PDC_SERVER_TRANSFER_REQUEST_INNER_WRITE_BULK_TIME,
    PDC_SERVER_TRANSFER_REQUEST_START_READ_BULK_TIME,
    PDC_SERVER_TRANSFER_REQUEST_INNER_READ_BULK_TIME,

    // Transfer request ALL
    PDC_SERVER_TRANSFER_REQUEST_START_ALL_WRITE_TIME,
    PDC_SERVER_TRANSFER_REQUEST_START_ALL_READ_TIME,
    PDC_SERVER_TRANSFER_REQUEST_START_ALL_WRITE_BULK_TIME,
    PDC_SERVER_TRANSFER_REQUEST_START_ALL_READ_BULK_TIME,
    PDC_SERVER_TRANSFER_REQUEST_WAIT_ALL_TIME,
    PDC_SERVER_TRANSFER_REQUEST_INNER_WRITE_ALL_BULK_TIME,
    PDC_SERVER_TRANSFER_REQUEST_INNER_READ_ALL_BULK_TIME,

    // Enum size
    PDC_SERVER_TIMES_SIZE
} pdc_server_timestamps_t;

static const char *pdc_server_timestamps_str[] = {
    // Buffer object map/unmap
    "Server buf obj map rpc time", "Server buf obj unmap rpc time",

    // Lock obtain/release
    "Server obtain lock write time", "Server obtain lock read time", "Server release lock write time",
    "Server release lock read time", "Server release lock bulk transfer write time",
    "Server release lock bulk transfer inner write time", "Server release lock bulk transfer read time",
    "Server release lock bulk transfer inner read time",

    // Transfer request WRITE
    "Server transfer request start write time", "Server transfer request start read time",
    "Server transfer request wait write time", "Server transfer request wait read time",
    "Server transfer request start write bulk time", "Server transfer request inner write bulk time",
    "Server transfer request start read bulk time", "Server transfer request inner read bulk time",

    // Transfer request ALL
    "Server transfer request start all write time", "Server transfer request start all read time",
    "Server transfer request start all write bulk time", "Server transfer request start all read bulk time",
    "Server transfer request wait all time", "Server transfer request inner write all bulk time",
    "Server transfer request inner read all bulk time"};

extern double         pdc_server_timings_g[PDC_SERVER_TIMES_SIZE];
extern pdc_timestamp *pdc_server_timestamps_g[PDC_SERVER_TIMES_SIZE];
#endif // IS_PDC_SERVER

void pdc_timestamp_register(pdc_timestamp **_timestamp, double start, double end);

#ifndef IS_PDC_SERVER
void PDC_client_timing_init();
void PDC_client_timing_finalize();
void PDC_client_timing_report(const char *prefix);
#else
void PDC_server_timing_init();
void PDC_server_timing_finalize();
void PDC_server_timing_report(const char *prefix);
#endif

/**
 * The enum_label is used to create unique variable
 * names, allowing the macros to be used multiple times
 * in the same function scope without collisions.
 *
 * It is also used to identify which timestamp linked list
 * to append to for recording timing information.
 *
 * Note: The PDC_TIMING_DECLARE cannot be used
 * multiple times within the same function scope with
 * the same enum_label.
 *
 * PDC_TIMING_DECLARE creates the timing variables
 * used by the corresponding calls to PDC_TIMING_START
 * and PDC_TIMING_END. This separation is necessary
 * because some timing measurements depend on conditions
 * (e.g., PDC_READ vs. PDC_WRITE), and declaring the variables
 * inside PDC_TIMING_START would limit their scope
 * to that conditional block, making them inaccessible outside.
 */
#define PDC_TIMING_DECLARE_GENERAL(enum_label)                                                               \
    double __start_time_##enum_label          = 0.0;                                                         \
    double __function_start_time_##enum_label = 0.0;
#define PDC_TIMING_START_GENERAL(enum_label)                                                                 \
    do {                                                                                                     \
        __start_time_##enum_label          = MPI_Wtime();                                                    \
        __function_start_time_##enum_label = __start_time_##enum_label;                                      \
    } while (0)
#define PDC_TIMING_END_GENERAL(enum_label, client_or_server_timings_g, client_or_server_timestamps_g)        \
    do {                                                                                                     \
        double __end_time = MPI_Wtime();                                                                     \
        client_or_server_timings_g[enum_label] += __end_time - __start_time_##enum_label;                    \
        pdc_timestamp_register(&(client_or_server_timestamps_g[enum_label]),                                 \
                               __function_start_time_##enum_label, __end_time);                              \
    } while (0)
/**
 * Starts the appropriate timing measurement based on access_type.
 *
 * Since some timing regions differ depending on whether the access
 * type is PDC_READ or PDC_WRITE, this macro selects the correct
 * enum_label accordingly, avoiding conditional scoping issues
 * and keeping the timing calls consistent and clear.
 */
#define PDC_TIMING_START_ACCESS_GENERAL(access_type, read_label, write_label)                                \
    do {                                                                                                     \
        if ((access_type) == PDC_READ)                                                                       \
            PDC_TIMING_START(read_label);                                                                    \
        else                                                                                                 \
            PDC_TIMING_START(write_label);                                                                   \
    } while (0)
/**
 * Ends the appropriate timing measurement based on access_type.
 *
 * Matches the timing started by PDC_TIMING_START_ACCESS,
 * ending the correct timing block depending on whether the access
 * type is PDC_READ or PDC_WRITE.
 */
#define PDC_TIMING_END_ACCESS_GENERAL(access_type, read_label, write_label, client_or_server_timings_g,      \
                                      client_or_server_timestamps_g)                                         \
    do {                                                                                                     \
        if ((access_type) == PDC_READ)                                                                       \
            PDC_TIMING_END(read_label, client_or_server_timings_g, client_or_server_timestamps_g);           \
        else                                                                                                 \
            PDC_TIMING_END(write_label, client_or_server_timings_g, client_or_server_timestamps_g);          \
    } while (0)

#ifndef IS_PDC_SERVER
#define PDC_TIMING_DECLARE(enum_label) PDC_TIMING_DECLARE_GENERAL(enum_label)
#define PDC_TIMING_START(enum_label)   PDC_TIMING_START_GENERAL(enum_label)
#define PDC_TIMING_END(enum_label)                                                                           \
    PDC_TIMING_END_GENERAL(enum_label, pdc_client_timings_g, pdc_client_timestamps_g)
#define PDC_TIMING_START_ACCESS(access_type, read_label, write_label)                                        \
    PDC_TIMING_START_ACCESS_GENERAL(access_type, read_label, write_label)
#define PDC_TIMING_END_ACCESS(access_type, read_label, write_label)                                          \
    PDC_TIMING_END_ACCESS_GENERAL(access_type, read_label, write_label, pdc_client_timings_g,                \
                                  pdc_client_timestamps_g)
#else
#define PDC_TIMING_DECLARE(enum_label) PDC_TIMING_DECLARE_GENERAL(enum_label)
#define PDC_TIMING_START(enum_label)   PDC_TIMING_START_GENERAL(enum_label)
#define PDC_TIMING_END(enum_label)                                                                           \
    PDC_TIMING_END_GENERAL(enum_label, pdc_server_timings_g, pdc_server_timestamps_g)
#define PDC_TIMING_START_ACCESS(access_type, read_label, write_label)                                        \
    PDC_TIMING_START_ACCESS_GENERAL(access_type, read_label, write_label)
#define PDC_TIMING_END_ACCESS(access_type, read_label, write_label)                                          \
    PDC_TIMING_END_ACCESS_GENERAL(access_type, read_label, write_label, pdc_server_timings_g,                \
                                  pdc_server_timestamps_g)
#endif
#else
void PDC_timing_report(const char *prefix);
#define PDC_CLIENT_TIMING_DECLARE(enum_label)
#define PDC_CLIENT_TIMING_START(enum_label)
#define PDC_CLIENT_TIMING_END(enum_label)
#define PDC_CLIENT_TIMING_START_ACCESS(access_type, read_label, write_label)
#define PDC_CLIENT_TIMING_END_ACCESS(access_type, read_label, write_label)

#define PDC_SERVER_TIMING_DECLARE(enum_label)
#define PDC_SERVER_TIMING_START(enum_label)
#define PDC_SERVER_TIMING_END(enum_label)
#define PDC_SERVER_TIMING_START_ACCESS(access_type, read_label, write_label)
#define PDC_SERVER_TIMING_END_ACCESS(access_type, read_label, write_label)
#endif

extern int pdc_timing_rank_g;
void       PDC_get_time_str(char *cur_time);
int        PDC_get_rank();
#endif
