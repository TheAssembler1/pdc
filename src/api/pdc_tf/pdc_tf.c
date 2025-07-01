#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "pdc_tf.h"
#include "pdc_timing.h"
#include "pdc_interface.h"
#include "pdc_dg.h"
#include "pdc_malloc.h"

// FIXME: just a temp way of generating id's...
static pdcid_t tf_cur_dg_id    = 1;
static pdcid_t tf_cur_state_id = 1;
static pdcid_t tf_cur_func_id  = 1;

// these types don't need to be exposed to the client
typedef struct data_state {
    pdcid_t *state_id;
    char    *state_name;
} data_state;

typedef struct func {
    pdcid_t     *func_id;
    char        *path_colon_name;
    pdc_tf_dev_t dev;
    pdcid_t      input_data_state_id;
    pdcid_t      output_data_state_id;
} func;

pdc_dg_t  *graphs[100];
func       funcs[100];
data_state states[100];

static void
edge_free(void *data)
{
    FUNC_ENTER(NULL);

    LOG_INFO("edge_free called\n");
    data = PDC_free(data);

    FUNC_LEAVE_VOID();
}

static void
vertex_free(void *data)
{
    FUNC_ENTER(NULL);

    LOG_INFO("vertex_free called\n");
    data = PDC_free(data);

    FUNC_LEAVE_VOID();
}

pdcid_t
PDCtf_create_dg(char *dg_name)
{
    FUNC_ENTER(NULL);

    LOG_INFO("Creating directed graph\n");

    pdcid_t   dg_id = tf_cur_dg_id;
    pdc_dg_t *dg    = PDCdg_create(&dg_id, edge_free, vertex_free);
    graphs[dg_id]   = dg;

    tf_cur_dg_id++;
    FUNC_LEAVE(dg_id);
}

pdcid_t
PDCtf_create_func(char *path_colon_name, pdc_tf_dev_t dev, pdcid_t input_data_state,
                  pdcid_t output_data_state)
{
    FUNC_ENTER(NULL);

    LOG_INFO("Creating %s transformation\n", path_colon_name);

    pdcid_t func_id = tf_cur_func_id;

    funcs[func_id].func_id              = (pdcid_t *)PDC_calloc(1, sizeof(pdcid_t));
    *funcs[func_id].func_id             = func_id;
    funcs[func_id].path_colon_name      = path_colon_name;
    funcs[func_id].dev                  = dev;
    funcs[func_id].input_data_state_id  = input_data_state;
    funcs[func_id].output_data_state_id = output_data_state;

    tf_cur_func_id++;
    FUNC_LEAVE(func_id);
}

bool
is_vertex(void *data, void *input)
{
    pdcid_t *vertex_id = (pdcid_t *)data;
    pdcid_t *cur_id    = (pdcid_t *)input;

    return *vertex_id == *cur_id;
}

void
PDCtf_add_func(pdcid_t dg_id, pdcid_t func_id)
{
    FUNC_ENTER(NULL);

    // vertices to be added
    pdc_dg_vertex_id_t v1, v2;

    // first check that vertex does not exist from previous function insertion
    if ((v1 = PDCdg_has_vertex_data(graphs[dg_id], is_vertex, &(funcs[func_id].input_data_state_id))) ==
        PDC_DG_INVALID_VERTEX) {
        LOG_INFO("Adding %s vertex to graph\n", states[funcs[func_id].input_data_state_id].state_name);
        v1 = PDCdg_add_vertex(graphs[dg_id], states[funcs[func_id].input_data_state_id].state_id);
    }
    // first check that vertex does not exist from previous function insertion
    if ((v2 = PDCdg_has_vertex_data(graphs[dg_id], is_vertex, &(funcs[func_id].output_data_state_id))) ==
        PDC_DG_INVALID_VERTEX) {
        LOG_INFO("Adding %s vertex to graph\n", states[funcs[func_id].output_data_state_id].state_name);
        v2 = PDCdg_add_vertex(graphs[dg_id], states[funcs[func_id].output_data_state_id].state_id);
    }

    LOG_INFO("Adding %s function to graph\n", funcs[func_id].path_colon_name);
    PDCdg_add_edge(graphs[dg_id], v1, v2, funcs[func_id].func_id);

    FUNC_LEAVE_VOID();
}

pdcid_t
PDCtf_create_state(char *state_name)
{
    FUNC_ENTER(NULL);

    LOG_INFO("Creating %s state\n", state_name);

    pdcid_t state_id = tf_cur_state_id;

    states[state_id].state_id   = (pdcid_t *)PDC_calloc(1, sizeof(pdcid_t));
    *states[state_id].state_id  = state_id;
    states[state_id].state_name = state_name;

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
    PDCdg_destroy(graphs[dg_id]);

    FUNC_LEAVE(ret_value);
}

// region transfer to/from the specified obj_id, global_reg_id follow DG
perr_t
PDCtf_attach_to_region(pdcid_t dg_id, pdcid_t obj_id, pdcid_t global_reg_id, pdcid_t client_state_id,
                       pdcid_t server_state_id)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

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
PDCtf_dg_free(pdcid_t dg_id)
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
    if (PDC_register_type(PDC_TF_FUNCTION, (PDC_free_t)PDCtf_function_free) < 0)
        PGOTO_ERROR(FAIL, "Failed to register PDC_TF_FUNCTION type");
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

    pdc_dg_t *dg = graphs[dg_id];
    for (int i = 0; i < dg->edge_count; i++) {
        pdc_dg_edge_t *edge = dg->edges[i];

        pdc_dg_vertex_id_t input_vertex_id  = edge->from_vertex_id;
        pdc_dg_vertex_id_t output_vertex_id = edge->to_vertex_id;

        pdcid_t input_state_id  = *(pdcid_t *)dg->vertices[input_vertex_id]->data;
        pdcid_t output_state_id = *(pdcid_t *)dg->vertices[output_vertex_id]->data;

        pdcid_t func_id = *(pdcid_t *)edge->data;

        const char *color = (funcs[func_id].dev == PDC_TF_CPU_DEVICE) ? "blue" : "red";
        LOG_JUST_PRINT("\t%s -> %s [label=\"%s\", color=%s];\n", states[input_state_id].state_name,
                       states[output_state_id].state_name, funcs[func_id].path_colon_name, color);
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