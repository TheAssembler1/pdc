#ifndef PDC_TF_PROFILER_H
#define PDC_TF_PROFILER_H

// FIXME: guards to handle CUDA and NVML not being available

#include <nvml.h>
#include "pdc_vector.h"
#include "pdc_logger.h"
#include "pdc_timing.h"
#include "pdc_malloc.h"

// Flag to indiciate if profiler has been initialized
extern int pdc_tf_profiler_init;
// Flag to indicate if NVML profiler has been initialized
extern int pdc_tf_profiler_nvml_init;
// Number of NVIDIA devices detected by NVML
extern unsigned int pdc_tf_profiler_nvml_device_count;

// Vector configuration to store profiler samples for each device, with dynamic resizing
#define DEFAULT_PROFILER_SAMPLES_CAPACITY         100
#define DEFAULT_PROFILER_SAMPLES_EXPANSION_FACTOR 2.0

// Struct to keep track samples
typedef struct {
    // NVML profiler fields
    uint64_t gpu_utilization;    // GPU utilization percentage
    uint64_t memory_utilization; // Memory utilization percentage
    uint64_t memory_total;       // Total memory in bytes
    uint64_t memory_used;        // Used memory in bytes
    uint64_t memory_free;        // Free memory in bytes
} pdc_tf_profiler_nvml_sample_t;

typedef struct {
    PDC_VECTOR *nvml_samples;
} pdc_tf_profiler_samples_t;

// TODO: add fields for CUDA profiler and other profilers as needed
extern pdc_tf_profiler_samples_t pdc_tf_profiler_samples;

// Called in server loop to update profiler state and log any relevant information
perr_t pdc_tf_update_profiler();

#endif /* PDC_TF_PROFILER_H */