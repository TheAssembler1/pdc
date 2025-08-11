#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "pdc_tf.h"
#include "pdc_timing.h"
#include "pdc_interface.h"
#include "pdc_prop.h"
#include "pdc_obj_pkg.h"
#include "pdc_dg.h"
#include "pdc_malloc.h"
#include "pdc_region.h"
#include "pdc_tf_common.h"
#include "pdc_client_server_common.h"

// FIXME: just a temp way of generating id's...
static pdcid_t tf_cur_graph_id = 100;

pdcid_t
PDCtf_load_dg_json(char *file_path)
{
    FUNC_ENTER(NULL);

    pdcid_t   ret_value = tf_cur_graph_id;
    pdc_dg_t *dg;

    if (PDCtf_load_dg_json_common(file_path, &dg) != SUCCEED)
        PGOTO_ERROR(0, "Failed to load JSON at path %s\n", file_path);

    pdc_tf_graphs[ret_value] = dg;
    tf_cur_graph_id++;

done:
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

perr_t
PDCtf_set_output_mode(pdcid_t dg_id, pdc_tf_output_mode_t mode, pdcid_t *obj_ids, char **result_names,
                      int num_ids)
{
    FUNC_ENTER(NULL);

    perr_t ret_value = SUCCEED;

    FUNC_LEAVE(ret_value);
}

// region transfer to/from the specified obj_id, remote_reg_id follow DG
perr_t
PDCtf_attach_to_region(pdcid_t dg_id, pdcid_t obj_id, pdcid_t remote_reg, char *client_state,
                       char *stored_state)
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
    struct pdc_tf_obj_t *pdc_tf_obj     = obj_info->pdc_tf_obj;
    const uint32_t       cur_region_map = pdc_tf_obj->num_region_mappings;

    // get region information
    struct _pdc_id_info *region_id_info = PDC_find_id(remote_reg);
    if (region_id_info == NULL)
        PGOTO_ERROR(FAIL, "Cannot locate remote region ID");
    struct pdc_region_info *region_info = region_id_info->obj_ptr;

    // get region mapping fields from object
    pdc_tf_region_mapping_t *region_mapping    = &pdc_tf_obj->region_mappings[cur_region_map];
    pdc_tf_region_t         *conceptual_region = &region_mapping->conceptual_region;
    uint64_t                *coneptual_offset  = region_mapping->conceptual_offset;

    // copy region information into conceptual region
    conceptual_region->ndim = region_info->ndim;
    conceptual_region->unit = PDC_get_var_type_size(obj_info->obj_pt->obj_prop_pub->type);
    memcpy(coneptual_offset, region_info->offset, region_info->ndim * sizeof(uint64_t));
    memcpy(conceptual_region->size, region_info->size, region_info->ndim * sizeof(uint64_t));

    // FIXME: need to free these strings later
    pdc_tf_obj->region_mappings[cur_region_map].region_state.cur_state     = strdup(client_state);
    pdc_tf_obj->region_mappings[cur_region_map].region_state.desired_state = strdup(stored_state);
    region_mapping->region_state.dg_id                                     = dg_id;

    // increase the current region mapping
    pdc_tf_obj->num_region_mappings++;
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

    if (PDC_register_type(PDC_TF_DG, (PDC_free_t)PDCtf_dg_free) < 0)
        PGOTO_ERROR(FAIL, "Failed to register PDC_TF_DG type");

done:
    FUNC_LEAVE(ret_value);
}

void
PDCtf_print_dg(pdcid_t dg_id, bool write_to_file)
{
    int stdout_fd;
    int file_fd;

    if (write_to_file) {
        stdout_fd = dup(STDOUT_FILENO);
        if (stdout_fd == -1) {
            perror("dup");
            return;
        }

        file_fd = open("graph.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
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
    }

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
        pdc_tf_state_t *input_state  = (pdc_tf_state_t *)dg->vertices[input_vertex_id]->data;
        pdc_tf_state_t *output_state = (pdc_tf_state_t *)dg->vertices[output_vertex_id]->data;

        // Cast edge data to func*
        pdc_tf_func_t *edge_func = (pdc_tf_func_t *)edge->data;

        const char *color = (edge_func->dev == PDC_TF_CPU_DEVICE) ? "blue" : "red";

        LOG_JUST_PRINT("\t\"%s\" -> \"%s\" [label=\"%s\", color=%s];\n", input_state->name,
                       output_state->name, edge_func->name, color);
    }

    LOG_JUST_PRINT("}\n");

    if (write_to_file) {
        // --- End printing graph ---
        fflush(stdout);

        // restore original stdout
        if (dup2(stdout_fd, STDOUT_FILENO) == -1) {
            perror("dup2 restore");
        }
        close(stdout_fd);
    }
}

// print execution path
void
PDCtf_print_exec_path(pdcid_t dg_id, char *cur_state, char *desired_state)
{
    pdc_tf_state_t tf_cur_state;
    pdc_tf_state_t tf_desired_state;
    pdc_dg_edge_t *edges_out;
    uint32_t       num_edges;

    tf_cur_state.name     = cur_state;
    tf_desired_state.name = desired_state;

    if (PDCdg_shortest_path(pdc_tf_graphs[dg_id], &tf_cur_state, &tf_desired_state, &edges_out, &num_edges)) {
        LOG_INFO("Path was found:\n");
        for (uint32_t j = 0; j < num_edges; j++) {
            pdc_dg_edge_t e = edges_out[j];

            pdc_tf_state_t *v1 = (pdc_tf_state_t *)(pdc_tf_graphs[dg_id]->vertices[e.v1_id]->data);
            pdc_tf_state_t *v2 = (pdc_tf_state_t *)(pdc_tf_graphs[dg_id]->vertices[e.v2_id]->data);

            LOG_INFO("%d: %s(%s) = %s\n", j + 1, ((pdc_tf_func_t *)(e.data))->name, v1->name, v2->name);
        }
    }
    else
        LOG_INFO("No path found\n");
}