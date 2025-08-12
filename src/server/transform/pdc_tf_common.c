#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "common_io.h"
#include "pdc_malloc.h"
#include "pdc_tf_common.h"
#include "pdc_tf_builtin_common.h"
#include "pdc_tf.h"
#include "pdc_timing.h"
#include "json-c/json.h"

pdc_dg_t *            pdc_tf_graphs[200];
pdc_tf_builtin_func_t pdc_tf_builtin_funcs_g[PDC_TF_MAX_BUILTIN_FUNCS];
uint32_t              pdc_tf_builtin_cur_func_g = 0;
bool                  pdc_tf_has_init_g         = false;

perr_t
PDCtf_set_tf_region_t(pdc_tf_region_t *dest, uint8_t ndim, uint8_t unit, uint64_t *size)
{
    FUNC_ENTER(NULL);

    dest->ndim = ndim;
    dest->unit = unit;
    memcpy(dest->size, size, unit * sizeof(uint64_t));

    FUNC_LEAVE(SUCCEED);
}

perr_t
PDCtf_copy_tf_region_t(pdc_tf_region_t *src, pdc_tf_region_t *dest)
{
    FUNC_ENTER(NULL);

    dest->ndim = src->ndim;
    dest->unit = src->unit;
    memcpy(dest->size, src->size, src->unit * sizeof(uint64_t));

    FUNC_LEAVE(SUCCEED);
}

perr_t
PDCtf_exec_graph(pdcid_t dg_id, char *cur_state, char *desired_state, pdc_tf_region_t input_region,
                 pdc_tf_region_t *output_region, void **input)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    LOG_INFO("PDCtf_exec_graph was called\n");

    /**
     * Setup input and output states
     * NOTE: the vertices are check for equality based on the name alone
     */
    pdc_tf_state_t tf_input_state;
    pdc_tf_state_t tf_output_state;
    tf_input_state.name  = cur_state;
    tf_output_state.name = desired_state;
    void *input_state    = (void *)&tf_input_state;
    void *output_state   = (void *)&tf_output_state;

    pdc_dg_edge_t *edges_out;
    uint32_t       num_edges;

    memcpy(output_region, &input_region, sizeof(pdc_tf_region_t));

    if (PDCdg_shortest_path(pdc_tf_graphs[dg_id], input_state, output_state, &edges_out, &num_edges)) {
        LOG_INFO("Path was found:\n");
        for (uint32_t j = 0; j < num_edges; j++) {
            pdc_dg_edge_t   e  = edges_out[j];
            pdc_tf_state_t *v1 = (pdc_tf_state_t *)(pdc_tf_graphs[dg_id]->vertices[e.v1_id]->data);
            pdc_tf_state_t *v2 = (pdc_tf_state_t *)(pdc_tf_graphs[dg_id]->vertices[e.v2_id]->data);
            pdc_tf_func_t * f  = (pdc_tf_func_t *)(e.data);

            // setup input and output parameters
            pdc_tf_params_t tf_params;
            tf_params.params_str         = f->params_str;
            tf_params.input_params       = v1->params;
            tf_params.input_params_size  = v1->params_size;
            tf_params.output_params      = NULL;
            tf_params.output_params_size = 0;

            // run the transformation
            LOG_JUST_PRINT("--------------------------TRANSFORM_START--------------------------\n");
            if (f->c_func(&tf_params, input, input_region, output_region) == false)
                PGOTO_ERROR(FAIL, "Error when running transformation, %s", f->name);
            else
                LOG_INFO("Transformation %s(%s) = %s ran successfully\n", f->name, v1->name, v2->name);
            LOG_JUST_PRINT("--------------------------TRANSFORM_DONE--------------------------\n");

            // set output state params for next transformation
            v2->params      = tf_params.output_params;
            v2->params_size = tf_params.output_params_size;

            // set previous output region as input region for next transformation
            if (j + 1 != num_edges)
                memcpy(&input_region, output_region, sizeof(pdc_tf_region_t));
        }

        LOG_INFO("Done running transformations\n");
    }
    else
        PGOTO_ERROR(FAIL, "No path to desired states");

done:
    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_add_builtin_func(char *func_name, c_func_t c_func)
{
    FUNC_ENTER(NULL);

    int ret_value = SUCCEED;

    if (func_name == NULL)
        PGOTO_ERROR(FAIL, "func_name was NULL");

    strcpy(pdc_tf_builtin_funcs_g[pdc_tf_builtin_cur_func_g].name, func_name);
    pdc_tf_builtin_funcs_g[pdc_tf_builtin_cur_func_g].c_func = c_func;

    pdc_tf_builtin_cur_func_g++;

    LOG_INFO("Successfully added builtin function %s\n", func_name);

done:
    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_link_builtin_func(char *func_name, pdc_tf_func_t *f)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;
    bool   found     = false;

    if (func_name == NULL)
        PGOTO_ERROR(FAIL, "func_name was NULL");
    if (f == NULL)
        PGOTO_ERROR(FAIL, "f was NULL");

    for (int i = 0; i < pdc_tf_builtin_cur_func_g; i++) {
        if (strcmp(pdc_tf_builtin_funcs_g[i].name, func_name) == 0) {
            found     = true;
            f->c_func = pdc_tf_builtin_funcs_g[i].c_func;
        }
    }

    if (!found)
        PGOTO_ERROR(FAIL, "Builtin function not found");

done:
    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_init_builtin_funcs()
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    if (PDCtf_add_builtin_func("double_to_float", pdc_tf_builtin_double_to_float) != SUCCEED)
        PGOTO_ERROR(FAIL, "Failed to add builtin func double_to_float");
    if (PDCtf_add_builtin_func("float_to_double", pdc_tf_builtin_float_to_double) != SUCCEED)
        PGOTO_ERROR(FAIL, "Failed to add builtin func float_to_double");
#ifdef ENABLE_TF_ZFP_COMPRESSION
    if (PDCtf_add_builtin_func("zfp_compress", pdc_tf_builtin_zfp_compress) != SUCCEED)
        PGOTO_ERROR(FAIL, "Failed to add builtin func zfp_compress");
    if (PDCtf_add_builtin_func("zfp_decompress", pdc_tf_builtin_zfp_decompress) != SUCCEED)
        PGOTO_ERROR(FAIL, "Failed to add builtin func zfp_decompress");
#endif

done:
    FUNC_LEAVE(ret_value);
}

bool
PDCtf_region_has_attached_graph(struct pdc_tf_obj_t *tf_obj, int ndim, uint8_t unit, uint64_t *offset,
                                uint64_t *size, pdc_tf_region_mapping_t **region_mapping)
{
    FUNC_ENTER(NULL);

    bool ret_value = false;

    LOG_INFO("num_region_mappings: %d\n", tf_obj->num_region_mappings);

    for (int i = 0; i < tf_obj->num_region_mappings; i++) {
        *region_mapping                    = &tf_obj->region_mappings[i];
        pdc_tf_region_t *coneptual_region  = &((*region_mapping)->conceptual_region);
        uint64_t *       conceptual_offset = (*region_mapping)->conceptual_offset;

        // check if client ndim, offset, dims, unit match
        bool ndim_matches = coneptual_region->ndim == ndim;
        bool unit_matches = coneptual_region->unit == unit;
        // note these return 0 on match so ! is needed
        bool offset_matches = !memcmp(conceptual_offset, offset, ndim * sizeof(uint64_t));
        bool size_matches   = !memcmp(coneptual_region->size, size, ndim * sizeof(uint64_t));

        LOG_INFO("ndim_matches: %d, unit_matches: %d, offset_matches: %d, size_matches: %d\n", ndim_matches,
                 unit_matches, offset_matches, size_matches);

        if (ndim_matches && offset_matches && size_matches && unit_matches) {
            PGOTO_DONE(true);
        }
    }

done:
    FUNC_LEAVE(ret_value);
}

static struct array_list *
get_json_array(struct json_object *json_obj, char *arr_name)
{
    FUNC_ENTER(NULL);

    struct json_object *ret_value = NULL;

    if (json_object_object_get_ex(json_obj, arr_name, &ret_value) == 0)
        PGOTO_ERROR(NULL, "%s was not found", arr_name);
    if (json_object_get_type(ret_value) != json_type_array)
        PGOTO_ERROR(NULL, "%s was not an array", arr_name);

done:
    if (ret_value == NULL)
        FUNC_LEAVE(NULL);
    FUNC_LEAVE(json_object_get_array(ret_value));
}

static const char *
get_json_string(struct json_object *json_obj, char *str_name, bool expect_string)
{
    struct json_object *str_json_obj = NULL;
    const char *        ret_value    = NULL;

    if (!json_object_object_get_ex(json_obj, str_name, &str_json_obj)) {
        if (expect_string)
            PGOTO_ERROR(NULL, "%s was not found", str_name);
        else
            PGOTO_DONE(NULL);
    }
    if (json_object_get_type(str_json_obj) != json_type_string)
        PGOTO_ERROR(NULL, "%s was not a string\n", str_json_obj);
    ret_value = json_object_get_string(str_json_obj);

done:
    if (ret_value == NULL)
        FUNC_LEAVE(ret_value);
    FUNC_LEAVE(json_object_get_string(str_json_obj));
}

/**
 * {
 *   "states": [
 *     {
 *       "name": "string",
 *       "granularity": "element | region"
 *     },
 *     ...
 *   ],
 *   "functions": [
 *     {
 *       "device": "CPU | GPU",
 *       "input_state": "states[i].name",
 *       "output_state": "states[j].name",
 *       "location": "built-in | external",
 *       "name": "string"
 *     },
 *     ...
 *   ],
 *   "name": "string"
 * }
 */

// NOTE: These must match the order of the enum in the header
char *pdc_tf_dev_strs[]         = {"CPU", "GPU"};
char *pdc_tf_granularity_strs[] = {"element", "region"};
char *pdc_tf_location_strs[]    = {"builtin", "external"};

bool
vertices_are_equal(void *v1, void *v2)
{
    pdc_tf_state_t *s1 = (pdc_tf_state_t *)v1;
    pdc_tf_state_t *s2 = (pdc_tf_state_t *)v2;

    if (s1 == NULL || s2 == NULL)
        return false;

    return !strcmp(s1->name, s2->name);
}

static void
graph_free(void *data)
{
    FUNC_ENTER(NULL);

    LOG_INFO("graph_free called\n");

    char *json_filepath = (char *)data;
    json_filepath       = PDC_free(data);

    FUNC_LEAVE_VOID();
}

static void
edge_free(void *data)
{
    FUNC_ENTER(NULL);

    LOG_INFO("edge_free called\n");

    pdc_tf_func_t *f = (pdc_tf_func_t *)data;
    f->name          = PDC_free(f->name);
    f                = PDC_free(f);

    FUNC_LEAVE_VOID();
}

static void
vertex_free(void *data)
{
    FUNC_ENTER(NULL);

    LOG_INFO("vertex_free called\n");

    pdc_tf_state_t *s = (pdc_tf_state_t *)data;
    s->name           = PDC_free(s->name);
    s                 = PDC_free(s);

    FUNC_LEAVE_VOID();
}

// FIXME: just a temp way of generating id's...
/**
 * Need to start at 1 since pdcid_t is used as a failure indicator
 */
static pdcid_t tf_cur_graph_id = 1;

pdcid_t
PDCtf_open_dg_json_common(char *filepath)
{
    FUNC_ENTER(NULL);

    pdcid_t             ret_value = tf_cur_graph_id;
    FILE *              fp        = NULL;
    struct json_object *json_obj  = NULL;
    io_buffer_t         io_buffer;
    memset(&io_buffer, 0, sizeof(io_buffer_t));
    pdc_dg_t *dg = NULL;

    if (!pdc_tf_has_init_g) {
        if (PDCtf_init_builtin_funcs() == FAIL)
            PGOTO_ERROR(0, "Failed to initialialize builtin functions");
        pdc_tf_has_init_g = true;
    }

    // Open and read JSON file into buffer
    if ((fp = open_file(filepath, IO_MODE_READ)) == NULL)
        PGOTO_ERROR(0, "Failed to open_file: %s\n", filepath);
    if (read_file(fp, &io_buffer) != 0)
        PGOTO_ERROR(0, "Failed to read_file");

    LOG_INFO("File size was %ld bytes\n", io_buffer.size);

    // Parse and pretty print JSON
    if ((json_obj = json_tokener_parse(io_buffer.buffer)) == NULL)
        PGOTO_ERROR(0, "Failed to parse JSON");
    printf("%s\n", json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PRETTY));

    LOG_INFO("================START loading JSON into PDC===================\n");

    // Get directed graph name
    const char *dg_name = get_json_string(json_obj, "name", true);
    if (dg_name == NULL)
        PGOTO_ERROR(0, "Failed to find graph name");
    LOG_INFO("Directed graph name: %s\n", dg_name);

    // Actually create directed graph data structure
    dg         = PDCdg_create(graph_free, vertices_are_equal, NULL, edge_free, vertex_free);
    (dg)->data = strdup(filepath);

    // Parse and pretty print JSON
    struct array_list *states    = get_json_array(json_obj, "states");
    struct array_list *functions = get_json_array(json_obj, "functions");
    if (states == NULL || functions == NULL)
        PGOTO_DONE(0);

    /**
     * Extract states
     * FIXME: Need to validate all this
     */
    int states_length = array_list_length(states);
    for (int i = 0; i < states_length; i++) {
        struct json_object *s = array_list_get_idx(states, i);

        const char *s_name        = get_json_string(s, "name", true);
        const char *s_granularity = get_json_string(s, "granularity", true);

        if (s_name == NULL || s_granularity == NULL)
            PGOTO_DONE(0);

        LOG_INFO("Found state: %s\n", s_name);
        LOG_INFO("\tGranularity: %s\n", s_granularity);

        // Validate and set granularity
        pdc_tf_granularities_t granularity;
        bool                   found_granularity = false;
        for (int j = 0; j < PDC_TF_NUM_GRANULARITIES; j++) {
            if (!strcmp(s_granularity, pdc_tf_granularity_strs[j])) {
                found_granularity = true;
                granularity       = j;
                break;
            }
        }
        if (!found_granularity)
            PGOTO_ERROR(0, "Invalid granularity %s\n", s_granularity);

        // Add vertex to the directed graph data structure
        pdc_tf_state_t *dg_state = PDC_malloc(sizeof(pdc_tf_state_t));

        dg_state->name        = strdup(s_name);
        dg_state->granularity = granularity;
        dg_state->params      = NULL;
        dg_state->params_size = 0;

        if (PDCdg_add_vertex(dg, dg_state) == PDC_DG_INVALID_VERTEX)
            PGOTO_ERROR(0, "Failed to add vertex to directed graph\n");
    }

    /**
     * Extract functions
     * FIXME: Need to validate all this
     */
    int functions_length = array_list_length(functions);
    for (int i = 0; i < functions_length; i++) {
        struct json_object *f = array_list_get_idx(functions, i);

        const char *f_name         = get_json_string(f, "name", true);
        const char *f_device       = get_json_string(f, "device", true);
        const char *f_input_state  = get_json_string(f, "input_state", true);
        const char *f_output_state = get_json_string(f, "output_state", true);
        const char *f_location     = get_json_string(f, "location", true);
        const char *f_params_str   = get_json_string(f, "params", false);

        if (f_name == NULL || f_input_state == NULL || f_output_state == NULL || f_location == NULL)
            PGOTO_DONE(0);

        LOG_INFO("Found function: %s\n", f_name);
        LOG_INFO("\tDevice: %s\n", f_device);
        LOG_INFO("\tInput state: %s\n", f_input_state);
        LOG_INFO("\tOutput state: %s\n", f_output_state);
        LOG_INFO("\tLocation: %s\n", f_location);
        LOG_INFO("\tParams string: %s\n", (f_params_str) ? f_params_str : "None");

        pdc_tf_func_t *dg_func = PDC_calloc(1, sizeof(pdc_tf_func_t));

        // Validate device
        pdc_tf_dev_t dev;
        bool         found_device = false;
        for (int j = 0; j < PDC_TF_NUM_DEVICES; j++) {
            if (!strcmp(f_device, pdc_tf_dev_strs[j])) {
                found_device = true;
                dev          = j;
                break;
            }
        }
        if (!found_device)
            PGOTO_ERROR(0, "Invalid device %s\n", f_device);

        // Validate location
        pdc_tf_location_t location;
        bool              found_location = false;
        for (int j = 0; j < PDC_TF_NUM_LOCATIONS; j++) {
            if (!strcmp(f_location, pdc_tf_location_strs[j])) {
                found_location = true;
                location       = j;
                break;
            }
        }
        if (!found_location)
            PGOTO_ERROR(0, "Invalid location %s\n", f_location);

        // FIXME: Currently on support CPU devices and built-in functions
        if (dev != PDC_TF_CPU_DEVICE)
            PGOTO_ERROR(0, "Currently, only support CPU functions");
        if (location != PDC_TF_BUILTIN)
            PGOTO_ERROR(0, "Currently, only support builtin functions");

        dg_func->dev      = dev;
        dg_func->location = location;
        if (PDCtf_link_builtin_func(f_name, dg_func) != SUCCEED)
            PGOTO_ERROR(0, "Failed to link to builtin function");
        dg_func->name       = strdup(f_name);
        dg_func->params_str = (f_params_str) ? strdup(f_params_str) : NULL;

        /**
         * Construct input/output dg states
         * NOTE: the compare function is based only on the name string
         */
        pdc_tf_state_t i_state;
        pdc_tf_state_t o_state;
        i_state.name = (char *)f_input_state;
        o_state.name = (char *)f_output_state;

        if (PDCdg_add_edge(dg, &i_state, &o_state, dg_func) == PDC_DG_INVALID_EDGE)
            PGOTO_ERROR(FAIL, "Failed to add edge to directed graph\n");
    }

    LOG_INFO("================DONE loading JSON into PDC===================\n");
done:
    if (fp != NULL)
        close_file(fp);
    if (io_buffer.buffer != NULL)
        PDC_free(io_buffer.buffer);
    if (json_obj != NULL)
        json_object_put(json_obj);
    if (ret_value == 0)
        PDCdg_destroy(dg);
    else {
        pdc_tf_graphs[ret_value] = dg;
        tf_cur_graph_id++;
    }

    FUNC_LEAVE(ret_value);
}

size_t
PDCtf_get_pdc_region_t_elements(pdc_tf_region_t reg)
{
    size_t num_elements = 1;
    for (int i = 0; i < reg.ndim; ++i) {
        num_elements *= reg.size[i];
    }
    return num_elements;
}

size_t
PDCtf_get_pdc_region_t_bytes(pdc_tf_region_t reg)
{
    return PDCtf_get_pdc_region_t_elements(reg) * reg.unit;
}

void
PDCtf_log_pdc_region_t(pdc_tf_region_t reg)
{
    LOG_INFO("region ndim: %lu\n", reg.ndim);
    LOG_INFO("region unit: %lu\n", reg.unit);
    for (int i = 0; i < reg.ndim; i++)
        LOG_INFO("\tsize[%d] = %lu\n", i + 1, reg.size[0]);
    LOG_INFO("region bytes: %zu\n", PDCtf_get_pdc_region_t_bytes(reg));
}
