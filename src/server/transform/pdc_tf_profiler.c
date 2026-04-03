#include "pdc_tf_profiler.h"
#include <pthread.h>

int                       pdc_tf_profiler_init              = 0;
int                       pdc_tf_profiler_nvml_init         = 0;
int                       pdc_tf_profiler_cpu_init          = 0;
unsigned int              pdc_tf_profiler_nvml_device_count = 0;
pdc_tf_profiler_samples_t pdc_tf_profiler_samples           = {0};
static pthread_mutex_t    profiler_lock                     = PTHREAD_MUTEX_INITIALIZER;

/**
 * Get the average GPU utilization for a specific device over the last MAX_VECTOR_SIZE samples.
 * @param device_index Index of the GPU device (0-based)
 * @return Average GPU utilization (%) for that device, or -1.0 if invalid
 */
double
pdc_tf_avg_gpu_utilization(unsigned int device_index)
{
    // pthread_mutex_lock(&profiler_lock);
    if (device_index >= pdc_tf_profiler_samples.nvml_device_count) {
        // pthread_mutex_unlock(&profiler_lock);
        return -1.0; // invalid device index
    }

    int    count = 0;
    double sum   = 0.0;

    for (int i = 0; i < MAX_VECTOR_SIZE; i++) {
        pdc_tf_profiler_nvml_sample_t *samples = pdc_tf_profiler_samples.nvml_samples[i];
        if (samples != NULL) {
            sum += samples[device_index].gpu_utilization;
            count++;
        }
    }

    // pthread_mutex_unlock(&profiler_lock);
    return (count > 0) ? (sum / count) : 0.0;
}

/**
 * Get the average CPU utilization over the last MAX_VECTOR_SIZE samples.
 */
double
pdc_tf_avg_cpu_utilization()
{
    // pthread_mutex_lock(&profiler_lock);

    int    count = 0;
    double sum   = 0.0;

    for (int i = 0; i < MAX_VECTOR_SIZE; i++) {
        pdc_tf_profiler_cpu_sample_t *sample = pdc_tf_profiler_samples.cpu_samples[i];
        if (sample != NULL) {
            sum += sample->cpu_utilization;
            count++;
        }
    }

    // pthread_mutex_unlock(&profiler_lock);
    return (count > 0) ? (sum / count) : 0.0;
}

// ====================== NVML (GPU) Profiler ======================
static perr_t
pdc_tf_nvml_profiler_update()
{
    FUNC_ENTER(NULL);
    perr_t       ret_value = SUCCEED;
    nvmlReturn_t nvml_ret;

    // Initialize NVML profiler if not already done
    // pthread_mutex_lock(&profiler_lock);
    if (!pdc_tf_profiler_nvml_init) {
        LOG_INFO("Initializing PDC profiler NVML...\n");
        pdc_tf_profiler_nvml_init = 1;
        // pthread_mutex_unlock(&profiler_lock);

        nvml_ret = nvmlInit();
        if (nvml_ret != NVML_SUCCESS) {
            LOG_ERROR("Failed to initialize NVML: %s\n", nvmlErrorString(nvml_ret));
            PGOTO_ERROR(FAIL, "Failed to initialize NVML");
        }

        nvml_ret = nvmlDeviceGetCount(&pdc_tf_profiler_nvml_device_count);
        if (nvml_ret != NVML_SUCCESS) {
            LOG_ERROR("Failed to get NVML device count: %s\n", nvmlErrorString(nvml_ret));
            PGOTO_ERROR(FAIL, "Failed to get NVML device count");
        }

        // pthread_mutex_lock(&profiler_lock);
        pdc_tf_profiler_samples.nvml_device_count = pdc_tf_profiler_nvml_device_count;
        for (int i = 0; i < MAX_VECTOR_SIZE; i++)
            pdc_tf_profiler_samples.nvml_samples[i] = NULL;
    }
    else {
        // pthread_mutex_unlock(&profiler_lock);
    }

    if (pdc_tf_profiler_nvml_device_count == 0)
        goto done;

    // Allocate and populate NVML sample outside lock
    pdc_tf_profiler_nvml_sample_t *nvml_sample = (pdc_tf_profiler_nvml_sample_t *)PDC_malloc(
        sizeof(pdc_tf_profiler_nvml_sample_t) * pdc_tf_profiler_nvml_device_count);

    for (int i = 0; i < pdc_tf_profiler_nvml_device_count; i++) {
        nvmlDevice_t device;
        nvml_ret = nvmlDeviceGetHandleByIndex(i, &device);
        if (nvml_ret != NVML_SUCCESS)
            continue;

        nvmlMemory_t mem_info;
        nvml_ret = nvmlDeviceGetMemoryInfo(device, &mem_info);
        if (nvml_ret != NVML_SUCCESS)
            continue;

        nvmlUtilization_t util_info;
        nvml_ret = nvmlDeviceGetUtilizationRates(device, &util_info);
        if (nvml_ret != NVML_SUCCESS)
            continue;

        nvml_sample[i].gpu_utilization    = (double)util_info.gpu / 100.0; // Convert to fraction
        nvml_sample[i].memory_utilization = util_info.memory;
        nvml_sample[i].memory_total       = mem_info.total;
        nvml_sample[i].memory_used        = mem_info.used;
        nvml_sample[i].memory_free        = mem_info.free;

        // Log utilization
        LOG_INFO("Device %d - GPU Util: %f, Mem Util: %u%%, Mem Total: %lu MB, Mem Used: %lu MB, Mem Free: "
                 "%lu MB\n",
                 i, nvml_sample[i].gpu_utilization, nvml_sample[i].memory_utilization,
                 nvml_sample[i].memory_total / (1024 * 1024), nvml_sample[i].memory_used / (1024 * 1024),
                 nvml_sample[i].memory_free / (1024 * 1024));
    }

    int idx = pdc_tf_profiler_samples.nvml_head % MAX_VECTOR_SIZE;
    if (pdc_tf_profiler_samples.nvml_samples[idx] != NULL)
        PDC_free(pdc_tf_profiler_samples.nvml_samples[idx]);
    pdc_tf_profiler_samples.nvml_samples[idx] = nvml_sample;
    pdc_tf_profiler_samples.nvml_head++;

    // pthread_mutex_unlock(&profiler_lock);

done:
    FUNC_LEAVE(ret_value);
}

// ====================== CPU Profiler ======================
static perr_t
pdc_tf_cpu_profiler_update(double elapsed_total_time_sec, double elapsed_progress_time_sec)
{
    FUNC_ENTER(NULL);
    perr_t ret_value = SUCCEED;

    if (!pdc_tf_profiler_cpu_init) {
        // pthread_mutex_lock(&profiler_lock);
        if (!pdc_tf_profiler_cpu_init) {
            LOG_INFO("Initializing CPU profiler...\n");
            pdc_tf_profiler_cpu_init = 1;
            for (int i = 0; i < MAX_VECTOR_SIZE; i++)
                pdc_tf_profiler_samples.cpu_samples[i] = NULL;
        }
        // pthread_mutex_unlock(&profiler_lock);
    }

    // Compute CPU utilization outside lock
    double cpu_util = 1 - (elapsed_progress_time_sec / elapsed_total_time_sec);

    pdc_tf_profiler_cpu_sample_t *cpu_sample =
        (pdc_tf_profiler_cpu_sample_t *)PDC_malloc(sizeof(pdc_tf_profiler_cpu_sample_t));
    cpu_sample->cpu_utilization = cpu_util;

    LOG_INFO("Elapsed total time: %.2f seconds\n", elapsed_total_time_sec);
    LOG_INFO("Elapsed progress time: %.2f seconds\n", elapsed_progress_time_sec);
    LOG_INFO("CPU Utilization: %f\n", cpu_sample->cpu_utilization);

    // Update rolling buffer under lock
    // pthread_mutex_lock(&profiler_lock);
    int idx = pdc_tf_profiler_samples.cpu_head % MAX_VECTOR_SIZE;
    if (pdc_tf_profiler_samples.cpu_samples[idx] != NULL)
        PDC_free(pdc_tf_profiler_samples.cpu_samples[idx]);
    pdc_tf_profiler_samples.cpu_samples[idx] = cpu_sample;
    pdc_tf_profiler_samples.cpu_head++;
    // pthread_mutex_unlock(&profiler_lock);

    FUNC_LEAVE(ret_value);
}

// ====================== Update Profiler ======================
perr_t
pdc_tf_update_profiler(double elapsed_total_time_sec, double elapsed_progress_time_sec)
{
    FUNC_ENTER(NULL);
    perr_t ret_value = SUCCEED;

    // pthread_mutex_lock(&profiler_lock);
    if (!pdc_tf_profiler_init) {
        LOG_INFO("Initializing PDC profiler...\n");
        pdc_tf_profiler_init              = 1;
        pdc_tf_profiler_samples.cpu_head  = 0;
        pdc_tf_profiler_samples.nvml_head = 0;
    }
    // pthread_mutex_unlock(&profiler_lock);

    ret_value = pdc_tf_nvml_profiler_update();
    if (ret_value != SUCCEED)
        PGOTO_ERROR(FAIL, "Failed to update NVML profiler");

    ret_value = pdc_tf_cpu_profiler_update(elapsed_total_time_sec, elapsed_progress_time_sec);
    if (ret_value != SUCCEED)
        PGOTO_ERROR(FAIL, "Failed to update CPU profiler");

done:
    FUNC_LEAVE(ret_value);
}