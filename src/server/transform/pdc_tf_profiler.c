#include "pdc_tf_profiler.h"

int                       pdc_tf_profiler_init              = 0;
int                       pdc_tf_profiler_nvml_init         = 0;
unsigned int              pdc_tf_profiler_nvml_device_count = 0;
pdc_tf_profiler_samples_t pdc_tf_profiler_samples;

static perr_t
pdc_tf_nvml_profiler_update()
{
    FUNC_ENTER(NULL);

    perr_t       ret_value = SUCCEED;
    nvmlReturn_t nvml_ret;

    // Initialize NVML profiler if not already done
    if (!pdc_tf_profiler_nvml_init) {
        LOG_INFO("Initializing PDC  profiler...\n");
        pdc_tf_profiler_nvml_init = 1;

        nvml_ret = nvmlInit();
        if (nvml_ret != NVML_SUCCESS) {
            LOG_ERROR("Failed to initialize NVML: %s\n", nvmlErrorString(nvml_ret));
            PGOTO_ERROR(FAIL, "Failed to initialize NVML");
        }
        else {
            LOG_INFO("NVML initialized successfully\n");
        }

        nvml_ret = nvmlDeviceGetCount(&pdc_tf_profiler_nvml_device_count);
        if (nvml_ret != NVML_SUCCESS) {
            LOG_ERROR("Failed to get NVML device count: %s\n", nvmlErrorString(nvml_ret));
            PGOTO_ERROR(FAIL, "Failed to get NVML device count");
        }
        else {
            LOG_INFO("NVML detected %d device(s)\n", pdc_tf_profiler_nvml_device_count);
        }
    }

    // Each void* in vector will point to array of pdc_tf_profiler_nvml_sample_t structs, one for each device
    // We should not have to malloc too much sense every time we update the profiler we can just overwrite the
    // existing samples in the array, but we need to malloc once to create the array of samples for each
    // device. But this is a FIXME
    pdc_tf_profiler_nvml_sample_t *nvml_samples_array = (pdc_tf_profiler_nvml_sample_t *)PDC_malloc(
        pdc_tf_profiler_nvml_device_count * sizeof(pdc_tf_profiler_nvml_sample_t));

    // Update NVML profiler state for each device
    for (int i = 0; i < pdc_tf_profiler_nvml_device_count; i++) {
        nvmlDevice_t device;
        nvml_ret = nvmlDeviceGetHandleByIndex(i, &device);
        if (nvml_ret != NVML_SUCCESS) {
            LOG_ERROR("Failed to get handle for NVML device %d: %s\n", i, nvmlErrorString(nvml_ret));
            PGOTO_ERROR(FAIL, "Failed to get handle for NVML device");
        }

        nvmlMemory_t mem_info;
        nvml_ret = nvmlDeviceGetMemoryInfo(device, &mem_info);
        if (nvml_ret != NVML_SUCCESS) {
            LOG_ERROR("Failed to get memory info for NVML device %d: %s\n", i, nvmlErrorString(nvml_ret));
            PGOTO_ERROR(FAIL, "Failed to get memory info for NVML device");
        }
        else {
            LOG_INFO("NVML Device %d Memory: Total: %lu MB, Used: %lu MB, Free: %lu MB\n", i,
                     mem_info.total / (1024 * 1024), mem_info.used / (1024 * 1024),
                     mem_info.free / (1024 * 1024));
        }

        nvmlUtilization_t util_info;
        nvml_ret = nvmlDeviceGetUtilizationRates(device, &util_info);
        if (nvml_ret != NVML_SUCCESS) {
            LOG_ERROR("Failed to get utilization info for NVML device %d: %s\n", i,
                      nvmlErrorString(nvml_ret));
            PGOTO_ERROR(FAIL, "Failed to get utilization info for NVML device");
        }
        else {
            LOG_INFO("NVML Device %d Utilization: GPU: %u%%, Memory: %u%%\n", i, util_info.gpu,
                     util_info.memory);
        }

        // Store samples in profiler state
        nvml_samples_array[i].gpu_utilization    = util_info.gpu;
        nvml_samples_array[i].memory_utilization = util_info.memory;
        nvml_samples_array[i].memory_total       = mem_info.total;
        nvml_samples_array[i].memory_used        = mem_info.used;
        nvml_samples_array[i].memory_free        = mem_info.free;
    }

    // Add sample to vector if we have any devices, otherwise just skip adding samples for this update
    if (pdc_tf_profiler_nvml_device_count > 0)
        pdc_vector_add(pdc_tf_profiler_samples.nvml_samples, nvml_samples_array);

done:
    FUNC_LEAVE(ret_value);
}

perr_t
pdc_tf_update_profiler()
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    if (!pdc_tf_profiler_init) {
        LOG_INFO("Initializing PDC profiler...\n");
        pdc_tf_profiler_init = 1;

        // Initialize vector to store profiler samples
        pdc_tf_profiler_samples.nvml_samples =
            pdc_vector_create(DEFAULT_PROFILER_SAMPLES_CAPACITY, DEFAULT_PROFILER_SAMPLES_EXPANSION_FACTOR);
    }

    // Update NVML profiler
    ret_value = pdc_tf_nvml_profiler_update();
    if (ret_value != SUCCEED)
        PGOTO_ERROR(FAIL, "Failed to update NVML profiler");

done:
    FUNC_LEAVE(ret_value);
}