#ifndef PDC_TF_H
#define PDC_TF_H

#include "pdc_tf_common.h"
#include "pdc_logger.h"
#include "pdc_timing.h"
#include "pdc_vector.h"

// FIXME: NOAH adhoc timers for now...
typedef enum TIMER_TARGETS {
    OPEN_TIME,
    CLOSE_TIME,
    READ_TIME,
    WRITE_TIME,
    TOTAL_GRAPH_EXEC_TIME,
    NUM_TIMER_TARGETS
} TIMER_TARGETS;

static const char *TIMER_TARGET_NAMES[NUM_TIMER_TARGETS] = {[OPEN_TIME]             = "open",
                                                            [CLOSE_TIME]            = "close",
                                                            [READ_TIME]             = "read",
                                                            [WRITE_TIME]            = "write",
                                                            [TOTAL_GRAPH_EXEC_TIME] = "total_graph_exec"};

extern double   __timer_totals[NUM_TIMER_TARGETS];
extern uint64_t __timer_totals_freq[NUM_TIMER_TARGETS];

extern double __timer_start;
extern double __graph_timer_start;

#define TIMER_START()                                                                                        \
    do {                                                                                                     \
        __timer_start = MPI_Wtime();                                                                         \
    } while (0)

#define TIMER_STOP(which)                                                                                    \
    do {                                                                                                     \
        double __tend = MPI_Wtime();                                                                         \
        __timer_totals[(which)] += (__tend - __timer_start);                                                 \
        __timer_totals_freq[(which)] += 1.0;                                                                 \
    } while (0)

#define GRAPH_TIMER_START()                                                                                  \
    do {                                                                                                     \
        __graph_timer_start = MPI_Wtime();                                                                   \
    } while (0)

#define GRAPH_TIMER_STOP(which)                                                                              \
    do {                                                                                                     \
        double __tend = MPI_Wtime();                                                                         \
        __timer_totals[(which)] += (__tend - __graph_timer_start);                                           \
        __timer_totals_freq[(which)] += 1.0;                                                                 \
    } while (0)

#define LOG_AVG_TOTAL_FREQ_TIMERS()                                                                          \
    do {                                                                                                     \
        for (int i = 0; i < NUM_TIMER_TARGETS; i++) {                                                        \
            if (__timer_totals_freq[i] > 0)                                                                  \
                LOG_WARNING("[TIMER] id=%d, name=%s, avg=%lf, total=%lf, count=%lu\n", i,                    \
                            TIMER_TARGET_NAMES[i], __timer_totals[i] / __timer_totals_freq[i],               \
                            __timer_totals[i], __timer_totals_freq[i]);                                      \
        }                                                                                                    \
    } while (0)

extern PDC_VECTOR *tf_obj_id_to_dg_vector_g;

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
