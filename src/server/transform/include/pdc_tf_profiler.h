#ifndef PDC_TF_PROFILER_H
#define PDC_TF_PROFILER_H

// FIXME: guards to handle CUDA and NVML not being available

#include <nvml.h>
#include "pdc_logger.h"
#include "pdc_timing.h"
#include "pdc_malloc.h"

#define MAX_VECTOR_SIZE 10 // Rolling buffer size

// Flag to indicate if profiler has been initialized
extern int pdc_tf_profiler_init;
// Flag to indicate if NVML profiler has been initialized
extern int pdc_tf_profiler_nvml_init;
// Flag to indicate if CPU profiler has been initialized
extern int pdc_tf_profiler_cpu_init;
// Number of NVIDIA devices detected by NVML
extern unsigned int pdc_tf_profiler_nvml_device_count;

// Struct to keep track of NVML (GPU) samples
typedef struct {
    double        gpu_utilization;    // GPU utilization percentage
    int           memory_utilization; // Memory utilization percentage
    unsigned long memory_total;       // Total memory in bytes
    unsigned long memory_used;        // Used memory in bytes
    unsigned long memory_free;        // Free memory in bytes
} pdc_tf_profiler_nvml_sample_t;

// Struct to keep track of CPU samples
typedef struct {
    double cpu_utilization; // CPU utilization percentage
} pdc_tf_profiler_cpu_sample_t;

// Struct to keep track of all profiler samples (rolling buffers)
typedef struct {
    // Rolling buffers
    pdc_tf_profiler_nvml_sample_t *nvml_samples[MAX_VECTOR_SIZE]; // Each element is an array for all devices
    int                            nvml_head;                     // Next index to write

    pdc_tf_profiler_cpu_sample_t *cpu_samples[MAX_VECTOR_SIZE]; // CPU samples
    int                           cpu_head;                     // Next index to write

    unsigned int nvml_device_count; // Number of devices detected
} pdc_tf_profiler_samples_t;

// Global profiler state
extern pdc_tf_profiler_samples_t pdc_tf_profiler_samples;

// Update profiler (CPU and GPU) in server loop
perr_t pdc_tf_update_profiler(double elapsed_total_time_sec, double elapsed_progress_time_sec);

// Get the average GPU utilization for a specific device over the last MAX_VECTOR_SIZE samples
double pdc_tf_avg_gpu_utilization(unsigned int device_index);

// Get the average CPU utilization over the last MAX_VECTOR_SIZE samples
double pdc_tf_avg_cpu_utilization();

#endif /* PDC_TF_PROFILER_H */