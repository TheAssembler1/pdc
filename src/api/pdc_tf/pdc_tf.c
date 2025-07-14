#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "pdc_tf.h"
#include "pdc_timing.h"
#include "pdc_interface.h"
#include "pdc_obj_pkg.h"
#include "pdc_dg.h"
#include "pdc_malloc.h"
#include "pdc_region.h"
#include "pdc_tf_common.h"
#include "pdc_client_server_common.h"

// FIXME: just a temp way of generating id's...
static pdcid_t tf_cur_graph_id = 100;
static pdcid_t tf_cur_state_id = 100;

bool
vertices_are_equal(void *v1, void *v2)
{
    state *s1 = (state *)v1;
    state *s2 = (state *)v2;

    if (s1 == NULL || s2 == NULL)
        return false;

    return s1->id == s2->id;
}

static void
edge_free(void *data)
{
    FUNC_ENTER(NULL);

    LOG_INFO("edge_free called\n");

    func *f           = (func *)data;
    f->type_func_name = PDC_free(f->type_func_name);
    f                 = PDC_free(f);

    FUNC_LEAVE_VOID();
}

static void
vertex_free(void *data)
{
    FUNC_ENTER(NULL);

    LOG_INFO("vertex_free called\n");

    state *s = (state *)data;
    s->name  = PDC_free(s->name);
    s        = PDC_free(s);

    FUNC_LEAVE_VOID();
}

pdcid_t
PDCtf_create_dg(char *dg_name)
{
    FUNC_ENTER(NULL);

    LOG_INFO("Creating directed graph\n");

    // FIXME: should do this in the init of the client/server
    if (!pdc_tf_has_init_g) {
        if (PDCtf_init_builtin_funcs() != SUCCEED)
            LOG_ERROR("PDCtf_init_builtin_funcs failed\n");
        pdc_tf_has_init_g = true;
    }

    pdcid_t dg_id = tf_cur_graph_id;
    pdc_tf_graphs[dg_id] = PDCdg_create(NULL, vertices_are_equal, NULL, edge_free, vertex_free);

    tf_cur_graph_id++;
    FUNC_LEAVE(dg_id);
}

/**
 * Extracts the 'type' substring from a string formatted as "type:func_name".
 *
 * This function allocates a buffer for the type part and copies characters
 * up to (but not including) the delimiter defined by PDC_TF_TYPE_FUNC_NAME_DELIM.
 *
 * \param[in] str The input string in the format "type:func_name".
 * \param[out] type Pointer to a char* where the extracted type string will be stored.
 *                  Memory is allocated inside the function and should be freed by the caller.
 *
 * \return Returns SUCCEED on success, FAIL if input is invalid or allocation fails.
 */
static perr_t
get_type_from_type_func_name(char *str, char **type)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    const char *has_delim = strchr(str, PDC_TF_TYPE_FUNC_NAME_DELIM);
    if (has_delim == NULL)
        PGOTO_ERROR(FAIL, "invalid type_func_name, must in the form \"type:func_name\"");

    uint32_t cur_pos = 0;
    *type            = PDC_calloc(1, PDC_TF_MAX_TYPE_LEN * sizeof(char));
    // extract type
    while (str[cur_pos] != PDC_TF_TYPE_FUNC_NAME_DELIM) {
        // make sure there is room for terminator
        if (cur_pos >= PDC_TF_MAX_TYPE_LEN - 1)
            PGOTO_ERROR(FAIL, "type part too long");
        (*type)[cur_pos] = str[cur_pos];
        cur_pos++;
    }
    (*type)[cur_pos] = '\0';

done:
    if (ret_value == FAIL)
        *type = PDC_free(*type);
    FUNC_LEAVE(ret_value);
}

/**
 * Extracts the 'func_name' substring from a string formatted as "type:func_name".
 *
 * This function allocates a buffer for the func_name part and copies characters
 * after the delimiter defined by PDC_TF_TYPE_FUNC_NAME_DELIM to the end of the string.
 *
 * \param[in] str The input string in the format "type:func_name".
 * \param[out] func_name Pointer to a char* where the extracted func_name string will be stored.
 *                      Memory is allocated inside the function and should be freed by the caller.
 *
 * \return Returns SUCCEED on success, FAIL if input is invalid or allocation fails.
 */
static perr_t
get_func_from_type_func_name(char *str, char **func_name)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    const char *has_delim = strchr(str, PDC_TF_TYPE_FUNC_NAME_DELIM);
    if (has_delim == NULL)
        PGOTO_ERROR(FAIL, "invalid type_func_name, must in the form \"type:func_name\"");

    uint32_t cur_pos = 0;
    *func_name       = PDC_calloc(1, PDC_TF_MAX_FUNC_NAME_LEN * sizeof(char));
    // move to delimiter
    while (str[cur_pos] != PDC_TF_TYPE_FUNC_NAME_DELIM) {
        cur_pos++;
    }

    // move beyond the delimiter
    cur_pos++;
    uint32_t start = cur_pos;

    // extract func_name
    while (str[cur_pos] != '\0') {
        // make sure there is room for terminator
        if (cur_pos - start >= PDC_TF_MAX_FUNC_NAME_LEN - 1)
            PGOTO_ERROR(FAIL, "func_name part too long");
        (*func_name)[cur_pos - start] = str[cur_pos];
        cur_pos++;
    }
    (*func_name)[cur_pos - start] = '\0';

done:
    if (ret_value == FAIL)
        *func_name = PDC_free(*func_name);
    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_add_func(pdcid_t dg_id, char *type_func_name, pdc_tf_dev_t dev, pdcid_t input_data_state,
               pdcid_t output_data_state)
{
    FUNC_ENTER(NULL);

    int ret_value = SUCCEED;

    char *type = NULL, *func_name = NULL;
    if (get_type_from_type_func_name(type_func_name, &type) != SUCCEED)
        PGOTO_ERROR(FAIL, "Error with get_type_from_type_func_name");
    if (get_func_from_type_func_name(type_func_name, &func_name) != SUCCEED)
        PGOTO_ERROR(FAIL, "Error with get_func_from_type_func_name");

    LOG_INFO("Attempting to load function with type:name %s:%s\n", type, func_name);

    if (strcmp(type, "builtin") == 0) {
        LOG_INFO("Looking up builtin function");
    }
    else
        PGOTO_ERROR(FAIL, "Invalid type_func_name type");

    func *f = (func *)PDC_calloc(1, sizeof(func));

    // try to link function
    if (PDCtf_link_builtin_func(func_name, f) != SUCCEED)
        PGOTO_ERROR(FAIL, "Failed to PDCtf_link_builtin_func");
    f->type_func_name = strdup(type_func_name);
    f->dev            = dev;

    if (PDCdg_add_edge(pdc_tf_graphs[dg_id], pdc_tf_states[input_data_state],
                       pdc_tf_states[output_data_state], f) ==
        PDC_DG_INVALID_EDGE) {
        PGOTO_ERROR(FAIL, "Failed to add edge to dg");
    }

done:
    if (type)
        type = PDC_free(type);
    if (func_name)
        func_name = PDC_free(func_name);

    FUNC_LEAVE(ret_value);
}

pdcid_t
PDCtf_create_state(char *state_name)
{
    FUNC_ENTER(NULL);

    LOG_INFO("Creating %s state\n", state_name);

    pdcid_t state_id       = tf_cur_state_id;
    pdc_tf_states[state_id]       = (state *)PDC_calloc(1, sizeof(state));
    pdc_tf_states[state_id]->id   = state_id;
    pdc_tf_states[state_id]->name = strdup(state_name);

    tf_cur_state_id++;
    FUNC_LEAVE(state_id);
}

perr_t
PDCtf_set_output_mode(pdcid_t dg_id, pdc_tf_output_mode_t mode, pdcid_t *obj_ids, char **result_names,
                      int num_ids)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_close_dg(pdcid_t dg_id)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;
    PDCdg_destroy(pdc_tf_graphs[dg_id]);

    FUNC_LEAVE(ret_value);
}

// region transfer to/from the specified obj_id, global_reg_id follow DG
perr_t
PDCtf_attach_to_region(pdcid_t dg_id, pdcid_t obj_id, pdcid_t remote_reg_id, pdcid_t client_state_id, pdc_var_type_t client_var_type,
                       pdcid_t server_state_id)
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCtf_attach_to_region was called\n");

    perr_t ret_value = SUCCEED;

    // first locate object
    const struct _pdc_id_info *obj_id_info = PDC_find_id(obj_id);
    if (obj_id_info == NULL)
        PGOTO_ERROR(FAIL, "Failed to find object using pdcid");
    const struct _pdc_obj_info *obj_info = obj_id_info->obj_ptr;

    // pull out pdc obj transform information
    struct pdc_tf_obj_t *pdc_tf_obj        = obj_info->pdc_tf_obj;
    const uint32_t       cur_remote_region = pdc_tf_obj->num_regions;

    // add remote region information
    struct _pdc_id_info *region_id_info = PDC_find_id(remote_reg_id);
    if (region_id_info == NULL)
        PGOTO_ERROR(FAIL, "Cannot locate remote region ID");
    struct pdc_region_info *region_info                  = region_id_info->obj_ptr;
    pdc_tf_obj->client_regions[cur_remote_region].ndim   = region_info->ndim;
    memcpy(pdc_tf_obj->client_regions[cur_remote_region].offset, region_info->offset, region_info->ndim * sizeof(uint64_t));
    memcpy(pdc_tf_obj->client_regions[cur_remote_region].dims, region_info->size, region_info->ndim * sizeof(uint64_t));
    pdc_tf_obj->client_regions[cur_remote_region].unit   = PDC_get_var_type_size(client_var_type);

    // since this in on the client side the current state is the client_state_id
    pdc_tf_obj->tf_regions_info[cur_remote_region].current_state_id = client_state_id;
    pdc_tf_obj->tf_regions_info[cur_remote_region].client_state_id  = client_state_id;
    pdc_tf_obj->tf_regions_info[cur_remote_region].server_state_id  = server_state_id;
    // finally attach the graph to the region
    pdc_tf_obj->tf_regions_info[cur_remote_region].dg_id = dg_id;

    // increase the current region
    pdc_tf_obj->num_regions++;
done:
    FUNC_LEAVE(ret_value);
}

// all region transfers for obj_id follow DG
perr_t
PDCtf_attach_to_obj(pdcid_t dg_id, pdcid_t obj_id, pdcid_t client_state_id, pdcid_t server_state_id)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}

// all region transfers for specified obj_ids follow DG
perr_t
PDCtf_attach_to_objs(pdcid_t dg_id, pdcid_t *obj_ids, int num_ids, pdcid_t client_state_id,
                     pdcid_t server_state_id)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}

static perr_t
PDCtf_state_free(void *)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;
    LOG_INFO("PDCtf_state_free called\n");

    FUNC_LEAVE(ret_value);
}

static perr_t
PDCtf_function_free(void *)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;
    LOG_INFO("PDCtf_function_free called\n");

    FUNC_LEAVE(ret_value);
}

static perr_t
PDCtf_dg_free(void *dg_id)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;
    LOG_INFO("PDCtf_dg_free called\n");

    FUNC_LEAVE(ret_value);
}

perr_t
PDCtf_init()
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCtf_init called\n");

    perr_t ret_value = SUCCEED;

    if (PDC_register_type(PDC_TF_STATE, (PDC_free_t)PDCtf_state_free) < 0)
        PGOTO_ERROR(FAIL, "Failed to register PDC_TF_STATE type");
    if (PDC_register_type(PDC_TF_DG, (PDC_free_t)PDCtf_dg_free) < 0)
        PGOTO_ERROR(FAIL, "Failed to register PDC_TF_DG type");

done:
    FUNC_LEAVE(ret_value);
}

void
PDCtf_print_dg(pdcid_t dg_id)
{
    int stdout_fd = dup(STDOUT_FILENO);
    if (stdout_fd == -1) {
        perror("dup");
        return;
    }

    int file_fd = open("graph.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1) {
        perror("open");
        close(stdout_fd);
        return;
    }

    if (dup2(file_fd, STDOUT_FILENO) == -1) {
        perror("dup2");
        close(stdout_fd);
        close(file_fd);
        return;
    }

    close(file_fd);

    // --- Begin printing graph ---
    LOG_JUST_PRINT("\tdigraph G {\n");
    LOG_JUST_PRINT("legend [shape=none, margin=0, label=<\n");
    LOG_JUST_PRINT("  <TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"0\">\n");
    LOG_JUST_PRINT("    <TR><TD><FONT COLOR=\"blue\">&#8594;</FONT></TD><TD>CPU</TD></TR>\n");
    LOG_JUST_PRINT("    <TR><TD><FONT COLOR=\"red\">&#8594;</FONT></TD><TD>GPU</TD></TR>\n");
    LOG_JUST_PRINT("  </TABLE>\n");
    LOG_JUST_PRINT(">];\n");
    LOG_JUST_PRINT("\tlabel=\"Transformation Graph\";\n");
    LOG_JUST_PRINT("\tlabelloc=top;\n");
    LOG_JUST_PRINT("\tfontsize=8;\n");
    LOG_JUST_PRINT("\tfontname=\"Helvetica\";\n");
    LOG_JUST_PRINT("\tnodesep=1.0;\n");
    LOG_JUST_PRINT("\tranksep=1.0;\n");
    LOG_JUST_PRINT("\tsplines=true;\n");
    LOG_JUST_PRINT("\toverlap=false;\n");
    LOG_JUST_PRINT("\trankdir=LR;\n");
    LOG_JUST_PRINT(
        "\tnode [fontsize=10, shape=box, style=filled, fillcolor=lightgray, fontname=\"Helvetica\"];\n");
    LOG_JUST_PRINT("\tedge [fontsize=10];\n");

    pdc_dg_t *dg = pdc_tf_graphs[dg_id];
    for (int i = 0; i < dg->edge_count; i++) {
        pdc_dg_edge_t *edge = dg->edges[i];

        pdcid_t input_vertex_id  = edge->v1_id;
        pdcid_t output_vertex_id = edge->v2_id;

        // Correctly cast vertex data to state*
        state *input_state  = (state *)dg->vertices[input_vertex_id]->data;
        state *output_state = (state *)dg->vertices[output_vertex_id]->data;

        // Cast edge data to func*
        func *edge_func = (func *)edge->data;

        const char *color = (edge_func->dev == PDC_TF_CPU_DEVICE) ? "blue" : "red";

        LOG_JUST_PRINT("\t\"[%d] %s\" -> \"[%d] %s\" [label=\"%s\", color=%s];\n", input_state->id,
                       input_state->name, output_state->id, output_state->name, edge_func->type_func_name,
                       color);
    }

    LOG_JUST_PRINT("}\n");

    // --- End printing graph ---
    fflush(stdout);

    // restore original stdout
    if (dup2(stdout_fd, STDOUT_FILENO) == -1) {
        perror("dup2 restore");
    }
    close(stdout_fd);
}

// print execution path
void
PDCtf_print_exec_path(pdcid_t dg_id, pdcid_t client_state_id, pdcid_t server_state_id)
{
    void *         input_state  = pdc_tf_states[client_state_id];
    void *         output_state = pdc_tf_states[server_state_id];
    pdc_dg_edge_t *edges_out;
    uint32_t       num_edges;

    if (PDCdg_shortest_path(pdc_tf_graphs[dg_id], input_state, output_state, &edges_out, &num_edges)) {
        LOG_INFO("Path was found:\n");
        for (uint32_t j = 0; j < num_edges; j++) {
            pdc_dg_edge_t e = edges_out[j];

            state *v1 = (state *)(pdc_tf_graphs[dg_id]->vertices[e.v1_id]->data);
            state *v2 = (state *)(pdc_tf_graphs[dg_id]->vertices[e.v2_id]->data);

            LOG_INFO("%d: %s(%s) = %s\n", j + 1, ((func *)(e.data))->type_func_name, v1->name, v2->name);
        }
    }
    else {
        LOG_INFO("No path found\n");
    }
}
