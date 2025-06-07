#include "pdc_timing.h"
#include "pdc_malloc.h"
#include "pdc_logger.h"
#include "assert.h"
#include "mpi.h"

#ifdef PDC_TIMING
static double pdc_base_time;

void
pdc_timestamp_register(pdc_timestamp **_timestamp, double start, double end)
{
    double *temp;

    if (*_timestamp == NULL)
        *_timestamp = (pdc_timestamp *)PDC_calloc(1, sizeof(pdc_timestamp));
    pdc_timestamp *timestamp = *_timestamp;
    if (timestamp->timestamp_max_size == 0) {
        timestamp->timestamp_max_size = 256;
        timestamp->start = (double *)PDC_malloc(sizeof(double) * timestamp->timestamp_max_size * 2);
        timestamp->end   = timestamp->start + timestamp->timestamp_max_size;
        timestamp->timestamp_cur_size = 0;
    }
    else if (timestamp->timestamp_cur_size == timestamp->timestamp_max_size) {
        temp = (double *)PDC_malloc(sizeof(double) * timestamp->timestamp_max_size * 4);
        memcpy(temp, timestamp->start, sizeof(double) * timestamp->timestamp_max_size);
        memcpy(temp + timestamp->timestamp_max_size * 2, timestamp->end,
               sizeof(double) * timestamp->timestamp_max_size);
        PDC_free(timestamp->start);
        timestamp->start = temp;
        timestamp->end   = temp + timestamp->timestamp_max_size * 2;
        timestamp->timestamp_max_size *= 2;
    }
    timestamp->start[timestamp->timestamp_cur_size] = start;
    timestamp->end[timestamp->timestamp_cur_size]   = end;
    timestamp->timestamp_cur_size++;
}

static void
pdc_timestamp_clean(pdc_timestamp *timestamp)
{
    if (timestamp != NULL && timestamp->start != NULL) {
        PDC_free(timestamp->start);
        timestamp->start              = NULL;
        timestamp->end                = NULL;
        timestamp->timestamp_cur_size = 0;
        timestamp->timestamp_max_size = 0;
    }
}

static void
timestamp_log(pdc_timestamp *timestamp)
{
    size_t i;
    double total = 0.0;

    if (timestamp == NULL)
        return;

    for (i = 0; i < timestamp->timestamp_cur_size; ++i) {
        if (i == 0)
            LOG_JUST_PRINT("%0.6f-%0.6f", timestamp->start[i], timestamp->end[i]);
        else
            LOG_JUST_PRINT(",%0.6f-%0.6f", timestamp->start[i], timestamp->end[i]);
        total += timestamp->end[i] - timestamp->start[i];
    }
    LOG_JUST_PRINT("\n");

    if (i > 0)
        LOG_JUST_PRINT("total, %f\n", total);
}

void
pdc_timing_init(double *timings, pdc_timestamp **timestamps, int size)
{
    memset(timings, 0, sizeof(double) * size);
    for (int i = 0; i < size; i++)
        pdc_timestamp_clean(timestamps[i]);
}

void
pdc_timing_finalize(double *timings, pdc_timestamp **timestamps, int size)
{
    memset(timings, 0, sizeof(double) * size);
    for (int i = 0; i < size; i++) {
        pdc_timestamp_clean(timestamps[i]);
        timestamps[i] = NULL;
    }
}

void
pdc_timing_report(double *timings, pdc_timestamp **timestamps, int size, const char **timestamp_strs,
                  const char *prefix)
{
    double max_timings[size];
    int    rank;
    char   hostname[HOST_NAME_MAX];

    gethostname(hostname, HOST_NAME_MAX);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Reduce(timings, max_timings, size, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        LOG_JUST_PRINT("Total Times:\n");
        for (int i = 0; i < size; i++)
            LOG_JUST_PRINT("[%s, %s] %s = %f\n", hostname, prefix, timestamp_strs[i], max_timings[i]);

        LOG_JUST_PRINT("Timestamps:\n");
        for (int i = 0; i < size; i++) {
            if (timestamps[i] != NULL && timestamps[i]->timestamp_cur_size > 0) {
                LOG_JUST_PRINT("[%s, %s] %s:\n", hostname, prefix, timestamp_strs[i]);
                timestamp_log(timestamps[i]);
            }
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);
}

#ifndef IS_PDC_SERVER
double         pdc_client_timings_g[PDC_CLIENT_TIMES_SIZE];
pdc_timestamp *pdc_client_timestamps_g[PDC_CLIENT_TIMES_SIZE];

void
PDC_client_timing_init()
{
    pdc_timing_init(pdc_client_timings_g, pdc_client_timestamps_g, PDC_CLIENT_TIMES_SIZE);
}

void
PDC_client_timing_finalize()
{
    pdc_timing_finalize(pdc_client_timings_g, pdc_client_timestamps_g, PDC_CLIENT_TIMES_SIZE);
}

void
PDC_client_timing_report(const char *prefix)
{
    pdc_timing_report(pdc_client_timings_g, pdc_client_timestamps_g, PDC_CLIENT_TIMES_SIZE,
                      pdc_client_timestamps_str, prefix);
}

#else  // IS_PDC_SERVER
double         pdc_server_timings_g[PDC_SERVER_TIMES_SIZE];
pdc_timestamp *pdc_server_timestamps_g[PDC_SERVER_TIMES_SIZE];

void
PDC_server_timing_init()
{
    pdc_timing_init(pdc_server_timings_g, pdc_server_timestamps_g, PDC_SERVER_TIMES_SIZE);
}

void
PDC_server_timing_finalize()
{
    pdc_timing_finalize(pdc_server_timings_g, pdc_server_timestamps_g, PDC_SERVER_TIMES_SIZE);
}

void
PDC_server_timing_report(const char *prefix)
{
    pdc_timing_report(pdc_server_timings_g, pdc_server_timestamps_g, PDC_SERVER_TIMES_SIZE,
                      pdc_server_timestamps_str, prefix);
}
#endif // IS_PDC_SERVER
int
PDC_timing_report(const char *prefix __attribute__((unused)))
{
    return 0;
}
#endif // PDC_TIMING

int pdc_timing_rank_g = -1;

inline int
PDC_get_rank()
{
#ifdef ENABLE_MPI
    if (pdc_timing_rank_g == -1)
        MPI_Comm_rank(MPI_COMM_WORLD, &pdc_timing_rank_g);
    return pdc_timing_rank_g;
#else
    return 0;
#endif
}

inline void
PDC_get_time_str(char *cur_time)
{
    struct timespec ts;

    assert(cur_time);

    clock_gettime(CLOCK_REALTIME, &ts);
    sprintf(cur_time, "%04d-%02d-%02d %02d:%02d:%02d.%06ld", 1900 + localtime(&ts.tv_sec)->tm_year,
            localtime(&ts.tv_sec)->tm_mon + 1, localtime(&ts.tv_sec)->tm_mday, localtime(&ts.tv_sec)->tm_hour,
            localtime(&ts.tv_sec)->tm_min, localtime(&ts.tv_sec)->tm_sec, ts.tv_nsec / 1000);

    return;
}
