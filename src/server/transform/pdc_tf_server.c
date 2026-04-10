#include <assert.h>
#include <time.h>
#include <cuda_runtime.h>

#include "pdc_tf_server.h"
#include "pdc_malloc.h"
#include "pdc_client_server_common.h"
#include "pdc_vector.h"
#include "pdc_tf_user.h"
#include "pdc_tf_profiler.h"
#include "pdc_server_region_cache.h"

PDC_VECTOR *tf_obj_id_to_dg_vector_g = NULL;

#ifndef IS_PDC_SERVER
perr_t
PDCtf_store_json_mapping(pdcid_t obj_id, char *json_filepath, char *cur_state, char *client_state,
                         char *store_state, uint64_t *offset, uint64_t *size, uint8_t ndim,
                         pdc_var_type_t pdc_var_type)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(SUCCEED);
}
perr_t
PDCtf_exec_graph(pdc_dg_t *dg, uint64_t flat_conceptual_offset, char *cur_state, char *desired_state,
                 pdc_tf_region_t input_region, pdc_tf_region_t *output_region, void **input, int is_write)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(SUCCEED);
}
#else
static pdcid_t cur_graph = 1;

perr_t
PDCtf_store_json_mapping(pdcid_t obj_id, char *json_filepath, char *cur_state, char *client_state,
                         char *store_state, uint64_t *offset, uint64_t *size, uint8_t ndim,
                         pdc_var_type_t pdc_var_type)
{
    FUNC_ENTER(NULL);

    LOG_DEBUG("PDCtf_store_json_mapping was called\n");

    perr_t ret_value = SUCCEED;

    if (tf_obj_id_to_dg_vector_g == NULL)
        tf_obj_id_to_dg_vector_g = pdc_vector_create(8, 2.0);
    if (tf_obj_id_to_dg_vector_g == NULL)
        PGOTO_ERROR(FAIL, "tf_obj_id_to_dg_vector_g was NULL");

    // Find object in mapping if it exists
    pdc_tf_obj_id_to_dg_t *obj_id_to_dg                = NULL;
    PDC_VECTOR_ITERATOR *  tf_obj_id_to_dg_vector_iter = pdc_vector_iterator_new(tf_obj_id_to_dg_vector_g);
    while (pdc_vector_iterator_has_next(tf_obj_id_to_dg_vector_iter)) {
        pdc_tf_obj_id_to_dg_t *cur_obj_id_to_dg =
            (pdc_tf_obj_id_to_dg_t *)pdc_vector_iterator_next(tf_obj_id_to_dg_vector_iter);
        if (cur_obj_id_to_dg->obj_id == obj_id) {
            obj_id_to_dg = cur_obj_id_to_dg;
            break;
        }
    }
    pdc_vector_iterator_destroy(tf_obj_id_to_dg_vector_iter);

    // If object has attached graph make sure it is the same as the passed in graph
    if (obj_id_to_dg != NULL) {
        char *graph_json_filepath = (char *)(obj_id_to_dg->dg->data);
        if (strcmp(json_filepath, graph_json_filepath)) {
            PGOTO_ERROR(FAIL, "Passed graph filepath %s didn't match stored filepath %s", json_filepath,
                        graph_json_filepath);
        }
    }

    // Region mappings for passed in region and object
    pdc_tf_region_mapping_t *region_mapping = NULL;

    // If object doesn't have a directed graph create a new one
    if (obj_id_to_dg == NULL) {
        LOG_DEBUG("Creating directed graph for object\n");

        pdc_dg_t *dg = PDCtf_dg_json_create_common(json_filepath);
        if (dg == NULL)
            PGOTO_ERROR(FAIL, "Failed to load JSON\n");

        // Create new obj id to dg and append to vector
        obj_id_to_dg = PDC_malloc(sizeof(pdc_tf_obj_id_to_dg_t));
        pdc_vector_add(tf_obj_id_to_dg_vector_g, obj_id_to_dg);

        obj_id_to_dg->dg     = dg;
        obj_id_to_dg->obj_id = obj_id;

        // Create a new region mapping vector and mapping entry
        obj_id_to_dg->pdc_tf_obj.region_mappings_vector = pdc_vector_create(8, 2.0);
        region_mapping                                  = PDC_calloc(1, sizeof(pdc_tf_region_mapping_t));
        pdc_vector_add(obj_id_to_dg->pdc_tf_obj.region_mappings_vector, region_mapping);
    }

    // Check if this mapping already exists
    if (region_mapping == NULL) {
        PDC_VECTOR_ITERATOR *region_mapping_iter =
            pdc_vector_iterator_new(obj_id_to_dg->pdc_tf_obj.region_mappings_vector);
        while (pdc_vector_iterator_has_next(region_mapping_iter)) {
            pdc_tf_region_mapping_t *cur_region_mapping = pdc_vector_iterator_next(region_mapping_iter);
            if (cur_region_mapping == NULL)
                PGOTO_ERROR(FAIL, "cur_region_mapping was NULL");
            uint64_t *conceptual_offset = cur_region_mapping->conceptual_offset;
            if (memcmp(offset, conceptual_offset, ndim * sizeof(uint64_t)) == 0) {
                region_mapping = cur_region_mapping;
                break;
            }
        }
        pdc_vector_iterator_destroy(region_mapping_iter);
    }

    // If this is null we need to append this mapping
    if (region_mapping == NULL) {
        region_mapping = PDC_calloc(1, sizeof(pdc_tf_region_mapping_t));
        pdc_vector_add(obj_id_to_dg->pdc_tf_obj.region_mappings_vector, region_mapping);
    }

    pdc_tf_region_t *conceptual_region = &region_mapping->conceptual_region;
    uint64_t *       conceptual_offset = region_mapping->conceptual_offset;

    PDC_get_var_type_size(pdc_var_type);

    // copy region information into conceptual region
    conceptual_region->ndim         = ndim;
    conceptual_region->pdc_var_type = pdc_var_type;
    memcpy(conceptual_offset, offset, ndim * sizeof(uint64_t));
    memcpy(conceptual_region->size, size, ndim * sizeof(uint64_t));

    LOG_DEBUG("obj_id=%" PRIu64 " ndim=%u\n", obj_id, ndim);
    for (int i = 0; i < ndim; i++) {
        LOG_DEBUG("  offset[%d]=%" PRIu64 " size[%d]=%" PRIu64 "\n", i, conceptual_offset[i], i,
                  conceptual_region->size[i]);
    }

    // FIXME: need to free these strings later
    region_mapping->region_state.cur_state    = strdup(cur_state);
    region_mapping->region_state.client_state = strdup(client_state);
    region_mapping->region_state.store_state  = strdup(store_state);
    region_mapping->region_state.dg_id        = cur_graph;

done:
    FUNC_LEAVE(ret_value);
}

static double prev_gpu_times[NUM_TF_FUNC_TIMES] = {[0 ... NUM_TF_FUNC_TIMES-1] = 0.001};
static double prev_cpu_times[NUM_TF_FUNC_TIMES] = {[0 ... NUM_TF_FUNC_TIMES-1] = 1.0};
static int    gpu_time_index = 0;
static int    cpu_time_index = 0;

perr_t PDCtf_exec_graph(pdc_dg_t *dg, uint64_t flat_conceptual_offset, char *cur_state,
                        char *desired_state, pdc_tf_region_t input_region,
                        pdc_tf_region_t *output_region, void **input, int is_write)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    PDC_get_var_type_size(input_region.pdc_var_type);

    pdc_tf_state_t tf_input_state  = {.name = cur_state};
    pdc_tf_state_t tf_output_state = {.name = desired_state};
    void *input_state  = (void *)&tf_input_state;
    void *output_state = (void *)&tf_output_state;

    pdc_dg_edge_t *edges_out = NULL;
    uint32_t       num_edges;

    const char *value = getenv("USE_GPU");
    bool always_use_gpu = (value != NULL);

    if (PDCdg_shortest_path(dg, input_state, output_state, &edges_out, &num_edges)) {
        memcpy(output_region, &input_region, sizeof(pdc_tf_region_t));

        for (uint32_t j = 0; j < num_edges;) {
            pdc_dg_vertex_id_t v1_id = edges_out[j].v1_id;
            pdc_dg_vertex_id_t v2_id = edges_out[j].v2_id;

            uint32_t cur_edges_between_vertices = 0;
            uint32_t k = j;
            while (k < num_edges && edges_out[k].v1_id == v1_id && edges_out[k].v2_id == v2_id) {
                cur_edges_between_vertices++;
                k++;
            }

            // Compute current device utilization
            double min_avg_gpu_utilization = 1.1;
            int    min_gpu_utilization_device_index = -1;
            for (int i = 0; i < pdc_tf_profiler_nvml_device_count; i++) {
                double util = pdc_tf_avg_gpu_utilization(i);
                if (util < min_avg_gpu_utilization) {
                    min_avg_gpu_utilization          = util;
                    min_gpu_utilization_device_index = i;
                }
            }
            double avg_cpu_utilization = pdc_tf_avg_cpu_utilization();

            if (pdc_server_rank_g == 0) {
                LOG_WARNING("SCHED: avg_cpu_util=%.4f min_avg_gpu_util=%.4f gpu_device=%d nvml_count=%d always_use_gpu=%d close_time_g=%d\n",
                            avg_cpu_utilization, min_avg_gpu_utilization,
                            min_gpu_utilization_device_index, pdc_tf_profiler_nvml_device_count,
                            always_use_gpu, close_time_g);
                for (int i = 0; i < pdc_tf_profiler_nvml_device_count; i++) {
                    LOG_WARNING("SCHED: GPU device %d utilization: %.4f\n", i, pdc_tf_avg_gpu_utilization(i));
                }
            }

            // Select the best edge
            uint32_t best_edge_idx   = j;
            double   best_expected_time = 1e9;
            bool     force_selected  = false;

            // Static schedule: always pick GPU edge if available
            if (always_use_gpu || close_time_g) {
                for (uint32_t idx = j; idx < j + cur_edges_between_vertices; idx++) {
                    pdc_tf_func_t *f = (pdc_tf_func_t *)edges_out[idx].data;
                    if (f->dev == PDC_TF_GPU_DEVICE) {
                        best_edge_idx = idx;
                        min_gpu_utilization_device_index = 0;
                        force_selected = true;
                        if (pdc_server_rank_g == 0 && always_use_gpu)
                            LOG_WARNING("ALWAYS_USE_GPU: selecting edge %u on GPU 0\n", idx);
                        if (close_time_g && pdc_server_rank_g == 0)
                            LOG_WARNING("CLOSE_TIME: selecting edge %u on GPU 0\n", idx);
                        break;
                    }
                }
            }

            // Dynamic schedule: select based on expected time and utilization
            if (!force_selected) {
                for (uint32_t idx = j; idx < j + cur_edges_between_vertices; idx++) {
                    pdc_dg_edge_t *e = &edges_out[idx];
                    pdc_tf_func_t *f = (pdc_tf_func_t *)(e->data);

                    // Skip GPU edges if utilization too high
                    if (f->dev == PDC_TF_GPU_DEVICE && min_avg_gpu_utilization > 0.5) {
                        if (pdc_server_rank_g == 0)
                            LOG_WARNING("SCHED: skipping GPU edge %u due to high utilization %.4f\n",
                                        idx, min_avg_gpu_utilization);
                        continue;
                    }

                    // Compute average past time
                    double last_avg_time = 0.0;
                    for (int i = 0; i < NUM_TF_FUNC_TIMES; i++) {
                        if (f->dev == PDC_TF_CPU_DEVICE)
                            last_avg_time += prev_cpu_times[i];
                        else if (f->dev == PDC_TF_GPU_DEVICE)
                            last_avg_time += prev_gpu_times[i];
                    }
                    last_avg_time /= NUM_TF_FUNC_TIMES;

                    // Compute expected time
                    double expected_time = (f->dev == PDC_TF_CPU_DEVICE)
                        ? last_avg_time / fmax(0.1, 1.0 - avg_cpu_utilization)
                        : last_avg_time / fmax(0.1, 1.0 - min_avg_gpu_utilization);

                    if (pdc_server_rank_g == 0) {
                        LOG_WARNING("SCHED: edge %u func=%s dev=%s last_avg_time=%.4f expected_time=%.4f best_so_far=%.4f\n",
                                    idx, f->name,
                                    f->dev == PDC_TF_CPU_DEVICE ? "CPU" : "GPU",
                                    last_avg_time, expected_time, best_expected_time);
                    }

                    if (expected_time < best_expected_time) {
                        best_expected_time = expected_time;
                        best_edge_idx = idx;
                    }
                }

                if (pdc_server_rank_g == 0) {
                    pdc_tf_func_t *chosen = (pdc_tf_func_t *)edges_out[best_edge_idx].data;
                    LOG_WARNING("SCHED: chose edge %u func=%s dev=%s expected_time=%.4f\n",
                                best_edge_idx, chosen->name,
                                chosen->dev == PDC_TF_CPU_DEVICE ? "CPU" : "GPU",
                                best_expected_time);
                }
            }

            // Execute the selected edge
            pdc_dg_edge_t  e = edges_out[best_edge_idx];
            pdc_tf_func_t *f = (pdc_tf_func_t *)(e.data);

            pdc_tf_internal_param internal_params = {0};
            internal_params.dg = dg;
            internal_params.flat_conceptual_offset = flat_conceptual_offset;

            if (f->dev == PDC_TF_GPU_DEVICE) {
                LOG_WARNING("SCHED: setting CUDA device to %d for edge %u func=%s\n",
                            always_use_gpu ? 0 : min_gpu_utilization_device_index,
                            best_edge_idx, f->name);
                cudaError_t err = cudaSetDevice(always_use_gpu ? 0 : min_gpu_utilization_device_index);
                if (err != cudaSuccess) {
                    PGOTO_ERROR(FAIL, "Failed to set device %d\n",
                                always_use_gpu ? 0 : min_gpu_utilization_device_index);
                }
            }

            LOG_WARNING("SCHED: executing edge %u func=%s dev=%s\n",
                        best_edge_idx, f->name,
                        f->dev == PDC_TF_CPU_DEVICE ? "CPU" : "GPU");

            void *prev_input = *input;
            struct timespec start_time, end_time;
            clock_gettime(CLOCK_MONOTONIC, &start_time);

            memcpy(&input_region, output_region, sizeof(pdc_tf_region_t));
            if (!f->c_func(internal_params, f->params_str, input, input_region, output_region))
                PGOTO_ERROR(FAIL, "Error running transformation %s", f->name);

            clock_gettime(CLOCK_MONOTONIC, &end_time);
            double transform_time =
                (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

            double projected_time =
                (f->dev == PDC_TF_CPU_DEVICE)
                    ? transform_time * (1.0 - fmax(avg_cpu_utilization, 0.1))
                    : transform_time * (1.0 - fmax(min_avg_gpu_utilization, 0.1));

            if (pdc_server_rank_g == 0) {
                LOG_WARNING("Time for (%s) transformation %s: %.4f s, projected: %.4f s\n",
                            f->dev == PDC_TF_CPU_DEVICE ? "CPU" : "GPU",
                            f->name, transform_time, projected_time);
            }

            // Update history
            double last_avg_time = 0.0;
            if (f->dev == PDC_TF_CPU_DEVICE) {
                prev_cpu_times[cpu_time_index] = projected_time;
                cpu_time_index = (cpu_time_index + 1) % NUM_TF_FUNC_TIMES;
                for (int i = 0; i < NUM_TF_FUNC_TIMES; i++) last_avg_time += prev_cpu_times[i];
                last_avg_time /= NUM_TF_FUNC_TIMES;
                if (pdc_server_rank_g == 0)
                    LOG_WARNING("SCHED: updated cpu_times[%d]=%.4f new_avg=%.4f\n",
                                (cpu_time_index - 1 + NUM_TF_FUNC_TIMES) % NUM_TF_FUNC_TIMES,
                                projected_time, last_avg_time);
            } else {
                prev_gpu_times[gpu_time_index] = projected_time;
                gpu_time_index = (gpu_time_index + 1) % NUM_TF_FUNC_TIMES;
                for (int i = 0; i < NUM_TF_FUNC_TIMES; i++) last_avg_time += prev_gpu_times[i];
                last_avg_time /= NUM_TF_FUNC_TIMES;
                if (pdc_server_rank_g == 0)
                    LOG_WARNING("SCHED: updated gpu_times[%d]=%.4f new_avg=%.4f\n",
                                (gpu_time_index - 1 + NUM_TF_FUNC_TIMES) % NUM_TF_FUNC_TIMES,
                                projected_time, last_avg_time);
            }

            if (!(is_write && j == 0) && prev_input != *input)
                prev_input = PDC_free(prev_input);

            if (j + cur_edges_between_vertices != num_edges)
                memcpy(&input_region, output_region, sizeof(pdc_tf_region_t));

            j += cur_edges_between_vertices;
        }

        LOG_WARNING("SCHED: done running transformations\n");
    } else {
        LOG_ERROR("JSON filepath %s\n", (char *)dg->data);
        LOG_ERROR("Current state %s, desired state %s\n", cur_state, desired_state);
        PGOTO_ERROR(FAIL, "No path to desired state");
    }

done:
    if (edges_out != NULL)
        edges_out = PDC_free(edges_out);

    FUNC_LEAVE(ret_value);
}

perr_t PDCtf_exec_graph_backup(pdc_dg_t *dg, uint64_t flat_conceptual_offset, char *cur_state,
                        char *desired_state, pdc_tf_region_t input_region,
                        pdc_tf_region_t *output_region, void **input, int is_write)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    PDC_get_var_type_size(input_region.pdc_var_type);

    // Setup input and output states
    pdc_tf_state_t tf_input_state  = {.name = cur_state};
    pdc_tf_state_t tf_output_state = {.name = desired_state};
    void *input_state  = (void *)&tf_input_state;
    void *output_state = (void *)&tf_output_state;

    pdc_dg_edge_t *edges_out = NULL;
    uint32_t       num_edges;

    const char *value = getenv("USE_GPU");
    bool always_use_gpu = (value != NULL);

    if (PDCdg_shortest_path(dg, input_state, output_state, &edges_out, &num_edges)) {
        memcpy(output_region, &input_region, sizeof(pdc_tf_region_t));

        // Loop over edges by vertex pairs
        for (uint32_t j = 0; j < num_edges;) {
            pdc_dg_vertex_id_t v1_id = edges_out[j].v1_id;
            pdc_dg_vertex_id_t v2_id = edges_out[j].v2_id;

            // Count edges between this vertex pair
            uint32_t cur_edges_between_vertices = 0;
            uint32_t k = j;
            while (k < num_edges && edges_out[k].v1_id == v1_id && edges_out[k].v2_id == v2_id) {
                cur_edges_between_vertices++;
                k++;
            }

            // Compute current device utilization
            double min_avg_gpu_utilization = 1.1;
            int    min_gpu_utilization_device_index = -1;
            for (int i = 0; i < pdc_tf_profiler_nvml_device_count; i++) {
                double util = pdc_tf_avg_gpu_utilization(i);
                if (util < min_avg_gpu_utilization) {
                    min_avg_gpu_utilization          = util;
                    min_gpu_utilization_device_index = i;
                }
            }
            double avg_cpu_utilization = pdc_tf_avg_cpu_utilization();

            // Log current device utilizations
            if (pdc_server_rank_g == 0) {
                LOG_DEBUG("Avg CPU utilization: %f\n", avg_cpu_utilization);
                for (int i = 0; i < pdc_tf_profiler_nvml_device_count; i++) {
                    LOG_DEBUG("Avg GPU utilization device %d: %f\n", i, pdc_tf_avg_gpu_utilization(i));
                }
            }

            // Select the best edge
            uint32_t best_edge_idx = j;
            double best_expected_time = 1e9;

            if(always_use_gpu || close_time_g) {
                best_edge_idx = j;  // default
                for (uint32_t idx = j; idx < j + cur_edges_between_vertices; idx++) {
                    pdc_tf_func_t *f = (pdc_tf_func_t *)edges_out[idx].data;
                    if (f->dev == PDC_TF_GPU_DEVICE) {
                        best_edge_idx = idx;
                        min_gpu_utilization_device_index = 0; // force GPU 0
                        if (pdc_server_rank_g == 0 && always_use_gpu)
                            LOG_WARNING("ALWAYS_USE_GPU: selecting edge %u on GPU 0\n", idx);
                        if(close_time_g && pdc_server_rank_g == 0)
                            LOG_WARNING("CLOSE_TIME: selecting edge %u on GPU 0\n", idx);
                        break;
                    }
                }
            }

            // Normal selection based on expected time and utilization
            for (uint32_t idx = j; idx < j + cur_edges_between_vertices; idx++) {
                pdc_dg_edge_t *e = &edges_out[idx];
                pdc_tf_func_t *f = (pdc_tf_func_t *)(e->data);

                // Skip GPU edges if utilization too high
                if (f->dev == PDC_TF_GPU_DEVICE && min_avg_gpu_utilization > 0.5 && !always_use_gpu && !close_time_g)
                    continue;

                // Compute average past time
                double last_avg_time = 0.0;
                for (int i = 0; i < NUM_TF_FUNC_TIMES; i++) {
                    if (f->dev == PDC_TF_CPU_DEVICE)
                        last_avg_time += prev_cpu_times[i];
                    else if (f->dev == PDC_TF_GPU_DEVICE)
                        last_avg_time += prev_gpu_times[i];
                }
                last_avg_time /= NUM_TF_FUNC_TIMES;

                // Compute expected time
                double expected_time = (f->dev == PDC_TF_CPU_DEVICE)
                    ? last_avg_time / fmax(0.1, 1.0 - avg_cpu_utilization)
                    : last_avg_time / fmax(0.1, 1.0 - min_avg_gpu_utilization);

                if (expected_time < best_expected_time) {
                    best_expected_time = expected_time;
                    best_edge_idx = idx;
                }

                // Debug logging
                if (pdc_server_rank_g == 0) {
                    LOG_DEBUG("Edge %u: func=%s, dev=%s, last_avg_time=%.4f, expected_time=%.4f\n",
                                idx, f->name, f->dev == PDC_TF_CPU_DEVICE ? "CPU" : "GPU",
                                last_avg_time, expected_time);
                }
            }

            // Execute the selected edge
            pdc_dg_edge_t   e  = edges_out[best_edge_idx];
            pdc_tf_func_t * f  = (pdc_tf_func_t *)(e.data);

            // Setup internal params
            pdc_tf_internal_param internal_params = {0};
            internal_params.dg = dg;
            internal_params.flat_conceptual_offset = flat_conceptual_offset;

            // Set GPU device if needed
            if (f->dev == PDC_TF_GPU_DEVICE) {
                LOG_DEBUG("Setting device to %d for edge %u with func %s\n", always_use_gpu ? 0 : min_gpu_utilization_device_index,
                            best_edge_idx, f->name);
                cudaError_t err = cudaSetDevice(always_use_gpu ? 0 : min_gpu_utilization_device_index);
                if (err != cudaSuccess) {
                    PGOTO_ERROR(FAIL, "Failed to set device %d\n",
                                always_use_gpu ? 0 : min_gpu_utilization_device_index);
                }
            }

            // Run transformation
            LOG_DEBUG("------------------ TRANSFORM_START ------------------\n");
            void *prev_input = *input;
            struct timespec start_time, end_time;
            clock_gettime(CLOCK_MONOTONIC, &start_time);

            memcpy(&input_region, output_region, sizeof(pdc_tf_region_t));
            if (!f->c_func(internal_params, f->params_str, input, input_region, output_region))
                PGOTO_ERROR(FAIL, "Error running transformation %s", f->name);

            clock_gettime(CLOCK_MONOTONIC, &end_time);
            double transform_time =
                (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

            double projected_time =
                (f->dev == PDC_TF_CPU_DEVICE)
                    ? transform_time * (1 - fmax(avg_cpu_utilization, 0.1))
                    : transform_time * (1 - fmax(min_avg_gpu_utilization, 0.1));

            if (pdc_server_rank_g == 0) {
                LOG_WARNING("Time for (%s) transformation %s: %.4f s, projected: %.4f s\n", f->dev == PDC_TF_CPU_DEVICE ? "CPU" : "GPU",
                            f->name, transform_time, projected_time);
            }

            // Update history
            double last_avg_time = 0.0;
            if (f->dev == PDC_TF_CPU_DEVICE) {
                prev_cpu_times[cpu_time_index] = projected_time;
                cpu_time_index = (cpu_time_index + 1) % NUM_TF_FUNC_TIMES;
                for (int i = 0; i < NUM_TF_FUNC_TIMES; i++) last_avg_time += prev_cpu_times[i];
                last_avg_time /= NUM_TF_FUNC_TIMES;
            } else {
                prev_gpu_times[gpu_time_index] = projected_time;
                gpu_time_index = (gpu_time_index + 1) % NUM_TF_FUNC_TIMES;
                for (int i = 0; i < NUM_TF_FUNC_TIMES; i++) last_avg_time += prev_gpu_times[i];
                last_avg_time /= NUM_TF_FUNC_TIMES;
            }

            if (!(is_write && j == 0) && prev_input != *input)
                prev_input = PDC_free(prev_input);

            LOG_DEBUG("------------------ TRANSFORM_DONE ------------------\n");

            if (j + cur_edges_between_vertices != num_edges)
                memcpy(&input_region, output_region, sizeof(pdc_tf_region_t));

            j += cur_edges_between_vertices;
        }

        LOG_DEBUG("Done running transformations\n");
    } else {
        LOG_ERROR("JSON filepath %s\n", (char *)dg->data);
        LOG_ERROR("Current state %s, desired state %s\n", cur_state, desired_state);
        PGOTO_ERROR(FAIL, "No path to desired state");
    }

done:
    if (edges_out != NULL)
        edges_out = PDC_free(edges_out);

    FUNC_LEAVE(ret_value);
}

#endif // IS_PDC_SERVER
