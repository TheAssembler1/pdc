#include "pdc_dg.h"
#include "pdc_logger.h"
#include "pdc_timing.h"
#include "pdc_malloc.h"

static perr_t
resize_dg(pdc_dg_t *dg, uint32_t new_vertex_count, uint32_t new_edge_count)
{
    FUNC_ENTER(NULL);

    LOG_DEBUG("resize_dg was called\n");

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
PDCdg_create(void *data, bool (*vertices_are_equal)(void *v1_data, void *v2_data),
             void (*dg_data_free)(void *data), void (*edge_data_free)(void *data),
             void (*vertex_data_free)(void *data))
{
    FUNC_ENTER(NULL);

    LOG_DEBUG("PDCdg_create was called\n");

    pdc_dg_t *ret_value = NULL;

    if (vertices_are_equal == NULL)
        PGOTO_ERROR(NULL, "vertices_are_equal function is required");

    ret_value = (pdc_dg_t *)PDC_calloc(1, sizeof(pdc_dg_t));

    ret_value->vertex_capacity = PDC_DG_INIT_VERTEX_CAPACITY;
    ret_value->edge_capacity   = PDC_DG_INIT_EDGE_CAPACITY;
    ret_value->vertices =
        (pdc_dg_vertex_t **)PDC_calloc(ret_value->vertex_capacity, sizeof(pdc_dg_vertex_t *));
    ret_value->edges = (pdc_dg_edge_t **)PDC_calloc(ret_value->edge_capacity, sizeof(pdc_dg_edge_t *));

    ret_value->vertex_count = 0;
    ret_value->edge_count   = 0;
    ret_value->data         = data;

    ret_value->dg_data_free       = dg_data_free;
    ret_value->edge_data_free     = edge_data_free;
    ret_value->vertex_data_free   = vertex_data_free;
    ret_value->vertices_are_equal = vertices_are_equal;

done:
    FUNC_LEAVE(ret_value);
}

void
PDCdg_destroy(pdc_dg_t *dg)
{
    FUNC_ENTER(NULL);

    LOG_DEBUG("PDCdg_destroy was called\n");

    if (dg == NULL) {
        LOG_ERROR("dg was NULL\n");
        FUNC_LEAVE_VOID();
    }

    LOG_DEBUG("Destroying graph with %d vertices, %d edges\n", dg->vertex_count, dg->edge_count);

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
    if (dg->dg_data_free && dg->data)
        dg->dg_data_free(dg->data);

    dg = (pdc_dg_t *)PDC_free(dg);

    FUNC_LEAVE_VOID();
}

pdc_dg_vertex_id_t
PDCdg_add_vertex(pdc_dg_t *dg, void *data)
{
    FUNC_ENTER(NULL);

    LOG_DEBUG("PDCdg_add_vertex was called\n");

    pdc_dg_vertex_id_t ret_value;

    if (dg == NULL)
        PGOTO_ERROR(PDC_DG_INVALID_VERTEX, "dg was NULL");

    if (resize_dg(dg, dg->vertex_count + 1, dg->edge_count) != SUCCEED) {
        LOG_ERROR("Failed to resize dg\n");
        FUNC_LEAVE(PDC_DG_INVALID_VERTEX);
    }

    ret_value                          = dg->vertex_count;
    dg->vertices[ret_value]            = (pdc_dg_vertex_t *)PDC_calloc(1, sizeof(pdc_dg_vertex_t));
    dg->vertices[ret_value]->data      = data;
    dg->vertices[ret_value]->vertex_id = ret_value;

    dg->vertex_count++;

done:
    FUNC_LEAVE(ret_value);
}

pdc_dg_edge_id_t
PDCdg_add_edge(pdc_dg_t *dg, void *v1_data, void *v2_data, void *edge_data)
{
    FUNC_ENTER(NULL);

    LOG_DEBUG("PDCdg_add_edge was called\n");

    pdc_dg_edge_id_t ret_value;

    if (dg == NULL)
        PGOTO_ERROR(PDC_DG_INVALID_EDGE, "dg was NULL");
    if (v1_data == NULL)
        PGOTO_ERROR(PDC_DG_INVALID_EDGE, "v1_data was NULL");
    if (v2_data == NULL)
        PGOTO_ERROR(PDC_DG_INVALID_EDGE, "v2_data was NULL");
    if (dg->vertices_are_equal(v1_data, v2_data))
        PGOTO_ERROR(PDC_DG_INVALID_EDGE, "Vertices of edge were not unique\n");
    if (resize_dg(dg, dg->vertex_count, dg->edge_count + 1) != SUCCEED)
        PGOTO_ERROR(PDC_DG_INVALID_EDGE, "Failed to resize dg\n");

    ret_value                     = dg->edge_count;
    dg->edges[ret_value]          = (pdc_dg_edge_t *)PDC_calloc(1, sizeof(pdc_dg_edge_t));
    dg->edges[ret_value]->data    = edge_data;
    dg->edges[ret_value]->edge_id = ret_value;

    pdc_dg_vertex_id_t v1_id, v2_id = PDC_DG_INVALID_VERTEX;

    if ((v1_id = PDCdg_vertex_exists(dg, v1_data)) == PDC_DG_INVALID_VERTEX)
        v1_id = PDCdg_add_vertex(dg, v1_data);
    if ((v2_id = PDCdg_vertex_exists(dg, v2_data)) == PDC_DG_INVALID_VERTEX)
        v2_id = PDCdg_add_vertex(dg, v2_data);

    dg->edges[ret_value]->v1_id = v1_id;
    dg->edges[ret_value]->v2_id = v2_id;

    dg->edge_count++;

done:
    FUNC_LEAVE(ret_value);
}

pdc_dg_vertex_id_t
PDCdg_vertex_exists(pdc_dg_t *dg, void *vertex_data)
{
    FUNC_ENTER(NULL);

    LOG_DEBUG("PDCdg_vertex_exists was called\n");

    if (dg == NULL) {
        LOG_WARNING("pdc_dg_has_vertex called with NULL dg\n");
        FUNC_LEAVE(PDC_DG_INVALID_VERTEX);
    }

    for (int i = 0; i < dg->vertex_count; i++) {
        if (dg->vertices_are_equal(dg->vertices[i]->data, vertex_data))
            FUNC_LEAVE(dg->vertices[i]->vertex_id);
    }

    FUNC_LEAVE(PDC_DG_INVALID_VERTEX);
}

bool
PDCdg_shortest_path(pdc_dg_t *dg, void *v1_data, void *v2_data, pdc_dg_edge_t **edges_out,
                    uint32_t *num_edges)
{
    FUNC_ENTER(NULL);

    LOG_DEBUG("PDCdg_shortest_path was called\n");

    bool                ret_value = false;
    bool               *visited   = NULL;
    pdc_dg_vertex_id_t *prev      = NULL;
    pdc_dg_vertex_id_t *queue     = NULL;
    pdc_dg_vertex_id_t *path      = NULL;
    *edges_out                    = NULL;
    *num_edges                    = 0;

    if (dg == NULL)
        PGOTO_ERROR(false, "dg was NULL");

    // Look up vertex IDs
    pdc_dg_vertex_id_t from_vertex_id = PDCdg_vertex_exists(dg, v1_data);
    pdc_dg_vertex_id_t to_vertex_id   = PDCdg_vertex_exists(dg, v2_data);

    // Validate vertices
    if (from_vertex_id >= dg->vertex_count)
        PGOTO_ERROR(false, "Source vertex ID %u out of range (vertex_count=%u)", from_vertex_id,
                    dg->vertex_count);
    if (to_vertex_id >= dg->vertex_count)
        PGOTO_ERROR(false, "Destination vertex ID %u out of range (vertex_count=%u)", to_vertex_id,
                    dg->vertex_count);
    if (from_vertex_id == PDC_DG_INVALID_VERTEX)
        PGOTO_ERROR(false, "Source vertex not found");
    if (to_vertex_id == PDC_DG_INVALID_VERTEX)
        PGOTO_ERROR(false, "Destination vertex not found");

    uint32_t vertex_count = dg->vertex_count;

    // Allocate BFS data structures
    visited = (bool *)PDC_calloc(vertex_count, sizeof(bool));
    prev    = (pdc_dg_vertex_id_t *)PDC_malloc(vertex_count * sizeof(pdc_dg_vertex_id_t));
    queue   = (pdc_dg_vertex_id_t *)PDC_malloc(vertex_count * sizeof(pdc_dg_vertex_id_t));

    if (!visited || !prev || !queue)
        PGOTO_ERROR(false, "Failed to allocate BFS structures");

    for (uint32_t i = 0; i < vertex_count; i++)
        prev[i] = PDC_DG_INVALID_VERTEX;

    // Breadth-first search
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
            if (dg->edges[i]->v1_id == current) {
                pdc_dg_vertex_id_t neighbor = dg->edges[i]->v2_id;
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

    // Reconstruct the path in reverse
    uint32_t path_len = 0;
    for (pdc_dg_vertex_id_t at = to_vertex_id; at != PDC_DG_INVALID_VERTEX; at = prev[at])
        path_len++;

    if (path_len < 2)
        PGOTO_ERROR(false, "Path is too short to contain any edges");

    // Recover path vertices
    path = (pdc_dg_vertex_id_t *)PDC_malloc(path_len * sizeof(pdc_dg_vertex_id_t));
    if (!path)
        PGOTO_ERROR(false, "Failed to allocate path array");

    pdc_dg_vertex_id_t at = to_vertex_id;
    for (int i = path_len - 1; i >= 0; i--) {
        path[i] = at;
        at      = prev[at];
    }

    // Allocate and populate edge array
    *edges_out = (pdc_dg_edge_t *)PDC_malloc((path_len - 1) * sizeof(pdc_dg_edge_t));
    if (!*edges_out)
        PGOTO_ERROR(false, "Failed to allocate edges_out");

    *num_edges = path_len - 1;

    for (uint32_t i = 0; i < *num_edges; i++) {
        pdc_dg_vertex_id_t from       = path[i];
        pdc_dg_vertex_id_t to         = path[i + 1];
        bool               edge_found = false;

        for (uint32_t j = 0; j < dg->edge_count; j++) {
            if (dg->edges[j]->v1_id == from && dg->edges[j]->v2_id == to) {
                memcpy(&(*edges_out)[i], dg->edges[j], sizeof(pdc_dg_edge_t));
                edge_found = true;
                break;
            }
        }

        if (!edge_found)
            PGOTO_ERROR(false, "Missing edge between path vertices");
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

/**
 * Retrieve data associated with vertex
 *
 * \param dg          Pointer to the graph.
 * \param vertex_id   Vertex id with data.
 *
 * \return Data of the vertex if it exists, or NULL.
 */
void *
PDCdg_get_vertex_data(pdc_dg_t *dg, pdc_dg_vertex_id_t vertex_id)
{
    FUNC_ENTER(NULL);

    void *ret_value = NULL;

    if (dg == NULL)
        PGOTO_ERROR(NULL, "dg was NULL");
    if (dg->vertices == NULL)
        PGOTO_ERROR(NULL, "dg->vertices was NULL");

    ret_value = dg->vertices[vertex_id]->data;

done:
    FUNC_LEAVE(ret_value);
}

/**
 * Retrieve data associated with edge
 *
 * \param dg          Pointer to the graph.
 * \param edge_id      Edge id with data.
 *
 * \return Data of the edge if it exists, or NULL.
 */
void *
PDCdg_get_edge_data(pdc_dg_t *dg, pdc_dg_edge_id_t edge_id)
{
    FUNC_ENTER(NULL);

    void *ret_value = NULL;

    if (dg == NULL)
        PGOTO_ERROR(NULL, "dg was NULL");
    if (dg->edges == NULL)
        PGOTO_ERROR(NULL, "dg->edges was NULL");

    ret_value = dg->edges[edge_id]->data;

done:
    FUNC_LEAVE(dg->edges[edge_id]);
}