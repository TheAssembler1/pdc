#include "pdc_dg.h"
#include "pdc_logger.h"
#include "pdc_timing.h"
#include "pdc_malloc.h"

static perr_t
resize_dg(pdc_dg_t *dg, uint32_t new_vertex_count, uint32_t new_edge_count)
{
    FUNC_ENTER(NULL);

    LOG_INFO("resize_dg was called\n");

    perr_t ret_value = SUCCEED;

    if (dg == NULL)
        PGOTO_ERROR(FAIL, "Cannot resize a NULL directed graph\n");

    // Resize vertices if needed
    if (dg->vertex_capacity == 0)
        dg->vertex_capacity = 1;
    if (dg->vertex_capacity < new_vertex_count) {
        while (dg->vertex_capacity < new_vertex_count)
            dg->vertex_capacity *= 2;

        dg->vertices =
            (pdc_dg_vertex_t **)PDC_realloc(dg->vertices, sizeof(pdc_dg_vertex_t *) * dg->vertex_capacity);
    }
    // Resize edges if needed
    if (dg->edge_capacity == 0)
        dg->edge_capacity = 1;
    if (dg->edge_capacity < new_edge_count) {
        while (dg->edge_capacity < new_edge_count)
            dg->edge_capacity *= 2;

        dg->edges = (pdc_dg_edge_t **)PDC_realloc(dg->edges, sizeof(pdc_dg_edge_t *) * dg->edge_capacity);
    }

done:
    FUNC_LEAVE(ret_value);
}

pdc_dg_t *
PDCdg_create(void *data, void (*dg_data_free)(void *data), void (*edge_data_free)(void *data),
             void (*vertex_data_free)(void *data))
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCdg_create was called\n");

    pdc_dg_t *dg = (pdc_dg_t *)PDC_calloc(1, sizeof(pdc_dg_t));

    dg->vertex_capacity = PDC_DG_INIT_VERTEX_CAPACITY;
    dg->edge_capacity   = PDC_DG_INIT_EDGE_CAPACITY;
    dg->vertices        = (pdc_dg_vertex_t **)PDC_calloc(dg->vertex_capacity, sizeof(pdc_dg_vertex_t *));
    dg->edges           = (pdc_dg_edge_t **)PDC_calloc(dg->edge_capacity, sizeof(pdc_dg_edge_t *));

    dg->vertex_count = 0;
    dg->edge_count   = 0;
    dg->data         = data;

    dg->dg_data_free     = dg_data_free;
    dg->edge_data_free   = edge_data_free;
    dg->vertex_data_free = vertex_data_free;

    FUNC_LEAVE(dg);
}

void
PDCdg_destroy(pdc_dg_t *dg)
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCdg_destroy was called\n");

    if (dg == NULL) {
        LOG_ERROR("dg was NULL\n");
        FUNC_LEAVE_VOID();
    }

    LOG_INFO("Destroying graph with %d vertices, %d edges\n", dg->vertex_count, dg->edge_count);

    // first check that there are edges
    if (dg->edges) {
        for (int i = 0; i < dg->edge_count; i++) {
            /**
             * if user has defined a edge_data_free function
             * call it on each edge's data
             */
            if (dg->edges[i] && dg->edges[i]->data && dg->edge_data_free)
                dg->edge_data_free(dg->edges[i]->data);
            // free the edge
            if (dg->edges[i])
                dg->edges[i] = PDC_free(dg->edges[i]);
        }
        dg->edges = (pdc_dg_edge_t **)PDC_free(dg->edges);
    }
    // first check that there are vertices
    if (dg->vertices) {
        for (int i = 0; i < dg->vertex_count; i++) {
            /**
             * if user has defined a vertex_data_free function
             * call it on each vertex's data
             */
            if (dg->vertices[i] && dg->vertices[i]->data && dg->vertex_data_free)
                dg->vertex_data_free(dg->vertices[i]->data);
            // free the vertex
            if (dg->vertices[i])
                dg->vertices[i] = PDC_free(dg->vertices[i]);
        }
        dg->vertices = (pdc_dg_vertex_t **)PDC_free(dg->vertices);
    }

    /**
     * if user has defined a dg_data_free function
     * call it on dg's data
     */
    dg->dg_data_free(dg->data);

    dg = (pdc_dg_t *)PDC_free(dg);

    FUNC_LEAVE_VOID();
}

pdc_dg_vertex_id_t
PDCdg_add_vertex(pdc_dg_t *dg, void *data)
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCdg_add_vertex was called\n");

    if (resize_dg(dg, dg->vertex_count + 1, dg->edge_count) != SUCCEED) {
        LOG_ERROR("Failed to resize dg\n");
        FUNC_LEAVE(PDC_DG_INVALID_VERTEX);
    }

    pdc_dg_vertex_id_t cur_vertex       = dg->vertex_count;
    dg->vertices[cur_vertex]            = (pdc_dg_vertex_t *)PDC_calloc(1, sizeof(pdc_dg_vertex_t));
    dg->vertices[cur_vertex]->data      = data;
    dg->vertices[cur_vertex]->vertex_id = cur_vertex;

    dg->vertex_count++;

    FUNC_LEAVE(cur_vertex);
}

pdc_dg_edge_id_t
PDCdg_add_edge(pdc_dg_t *dg, pdc_dg_vertex_id_t from_vertex_id, pdc_dg_vertex_id_t to_vertex_id, void *data)
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCdg_add_edge was called\n");

    if (!PDCdg_has_vertex(dg, from_vertex_id)) {
        LOG_ERROR("from_vertex_id did not exist\n");
        FUNC_LEAVE(PDC_DG_INVALID_EDGE);
    }
    if (!PDCdg_has_vertex(dg, to_vertex_id)) {
        LOG_ERROR("to_vertex_id did not exist\n");
        FUNC_LEAVE(PDC_DG_INVALID_EDGE);
    }

    if (resize_dg(dg, dg->vertex_count, dg->edge_count + 1) != SUCCEED) {
        LOG_ERROR("Failed to resize dg\n");
        FUNC_LEAVE(PDC_DG_INVALID_EDGE);
    }

    pdc_dg_edge_id_t cur_edge           = dg->edge_count;
    dg->edges[cur_edge]                 = (pdc_dg_edge_t *)PDC_calloc(1, sizeof(pdc_dg_edge_t));
    dg->edges[cur_edge]->data           = data;
    dg->edges[cur_edge]->edge_id        = cur_edge;
    dg->edges[cur_edge]->from_vertex_id = from_vertex_id;
    dg->edges[cur_edge]->to_vertex_id   = to_vertex_id;

    dg->edge_count++;

    FUNC_LEAVE(cur_edge);
}

bool
PDCdg_has_vertex(pdc_dg_t *dg, pdc_dg_vertex_id_t vertex_id)
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCdg_has_vertex was called\n");

    if (dg == NULL) {
        LOG_WARNING("pdc_dg_has_vertex called with NULL dg\n");
        FUNC_LEAVE(false);
    }

    for (int i = 0; i < dg->vertex_count; i++) {
        if (dg->vertices[i]->vertex_id == vertex_id)
            FUNC_LEAVE(true);
    }

    FUNC_LEAVE(false);
}

pdc_dg_vertex_id_t
PDCdg_has_vertex_data(pdc_dg_t *dg, bool (*is_data)(void *data, void *input), void *input)
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCdg_has_vertex_data was called\n");

    if (dg == NULL) {
        LOG_WARNING("pdc_dg_has_vertex called with NULL dg\n");
        FUNC_LEAVE(PDC_DG_INVALID_VERTEX);
    }

    for (int i = 0; i < dg->vertex_count; i++) {
        if (is_data(dg->vertices[i]->data, input))
            FUNC_LEAVE(dg->vertices[i]->vertex_id);
    }

    FUNC_LEAVE(PDC_DG_INVALID_VERTEX);
}

pdc_dg_edge_id_t
PDCdg_has_edge_data(pdc_dg_t *dg, bool (*is_data)(void *data, void *input), void *input)
{
    FUNC_ENTER(NULL);

    if (dg == NULL) {
        LOG_WARNING("PDCdg_has_edge_data called with NULL dg\n");
        FUNC_LEAVE(PDC_DG_INVALID_EDGE);
    }

    for (int i = 0; i < dg->edge_count; i++) {
        if (is_data(dg->edges[i]->data, input))
            FUNC_LEAVE(dg->edges[i]->edge_id);
    }

    FUNC_LEAVE(PDC_DG_INVALID_EDGE);
}

bool
PDCdg_shortest_path(pdc_dg_t *dg, pdc_dg_vertex_id_t from_vertex_id, pdc_dg_vertex_id_t to_vertex_id,
                    pdc_dg_edge_t **edges_out, uint32_t *num_edges)
{
    FUNC_ENTER(NULL);

    bool ret_value = false;
    *edges_out     = NULL;
    *num_edges     = 0;

    if (dg == NULL)
        PGOTO_ERROR(false, "dg was NULL");
    if (!PDCdg_has_vertex(dg, from_vertex_id))
        PGOTO_ERROR(false, "from_vertex_id: %d not found", from_vertex_id);
    if (!PDCdg_has_vertex(dg, to_vertex_id))
        PGOTO_ERROR(false, "to_vertex_id: %d not found", to_vertex_id);

    uint32_t vertex_count = dg->vertex_count;

    bool               *visited = (bool *)PDC_calloc(vertex_count, sizeof(bool));
    pdc_dg_vertex_id_t *prev    = (pdc_dg_vertex_id_t *)PDC_malloc(vertex_count * sizeof(pdc_dg_vertex_id_t));
    pdc_dg_vertex_id_t *path    = (pdc_dg_vertex_id_t *)PDC_malloc(vertex_count * sizeof(pdc_dg_vertex_id_t));
    pdc_dg_vertex_id_t *queue   = (pdc_dg_vertex_id_t *)PDC_malloc(vertex_count * sizeof(pdc_dg_vertex_id_t));

    for (uint32_t i = 0; i < vertex_count; i++)
        prev[i] = PDC_DG_INVALID_VERTEX;

    uint32_t front = 0, rear = 0;
    visited[from_vertex_id] = true;
    queue[rear++]           = from_vertex_id;

    bool found = false;
    while (front < rear) {
        pdc_dg_vertex_id_t current = queue[front++];

        if (current == to_vertex_id) {
            found = true;
            break;
        }

        for (uint32_t i = 0; i < dg->edge_count; i++) {
            if (dg->edges[i]->from_vertex_id == current) {
                pdc_dg_vertex_id_t neighbor = dg->edges[i]->to_vertex_id;
                if (!visited[neighbor]) {
                    visited[neighbor] = true;
                    prev[neighbor]    = current;
                    queue[rear++]     = neighbor;
                }
            }
        }
    }

    if (!found) {
        LOG_WARNING("No path found from vertex %d to %d\n", from_vertex_id, to_vertex_id);
        goto done;
    }

    // Reconstruct path
    uint32_t path_len = 0;
    for (pdc_dg_vertex_id_t at = to_vertex_id; at != PDC_DG_INVALID_VERTEX; at = prev[at]) {
        path[path_len++] = at;
    }

    // Allocate space for the edges (path_len - 1)
    *edges_out = (pdc_dg_edge_t *)PDC_malloc((path_len - 1) * sizeof(pdc_dg_edge_t));
    *num_edges = path_len - 1;

    for (uint32_t i = path_len - 1; i > 0; i--) {
        pdc_dg_vertex_id_t from = path[i];
        pdc_dg_vertex_id_t to   = path[i - 1];

        for (uint32_t j = 0; j < dg->edge_count; j++) {
            if (dg->edges[j]->from_vertex_id == from && dg->edges[j]->to_vertex_id == to) {
                memcpy(&(*edges_out)[path_len - 1 - i], dg->edges[j], sizeof(pdc_dg_edge_t));
                break;
            }
        }
    }

    ret_value = true;

done:
    if (visited)
        visited = PDC_free(visited);
    if (prev)
        prev = PDC_free(prev);
    if (queue)
        queue = PDC_free(queue);
    if (path)
        path = PDC_free(path);

    FUNC_LEAVE(ret_value);
}
