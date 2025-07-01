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
PDCdg_create(void *data, void (*edge_free)(void *data), void (*vertex_free)(void *data))
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCdg_create was called\n");

    if (edge_free == NULL) {
        LOG_ERROR("edge_free was NULL\n");
        return NULL;
    }
    if (vertex_free == NULL) {
        LOG_ERROR("vertex_free was NULL\n");
        return NULL;
    }

    pdc_dg_t *dg = (pdc_dg_t *)PDC_calloc(1, sizeof(pdc_dg_t));

    dg->vertex_capacity = PDC_DG_INIT_VERTEX_CAPACITY;
    dg->edge_capacity   = PDC_DG_INIT_EDGE_CAPACITY;
    dg->vertices        = (pdc_dg_vertex_t **)PDC_calloc(dg->vertex_capacity, sizeof(pdc_dg_vertex_t *));
    dg->edges           = (pdc_dg_edge_t **)PDC_calloc(dg->edge_capacity, sizeof(pdc_dg_edge_t *));

    dg->vertex_count = 0;
    dg->edge_count   = 0;
    dg->data         = data;

    dg->edge_free   = edge_free;
    dg->vertex_free = vertex_free;

    FUNC_LEAVE(dg);
}

void
PDCdg_destroy(pdc_dg_t *dg)
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCdg_destroy was called\n");
    LOG_INFO("Destroying graph with %d vertices, %d edges\n", dg->vertex_count, dg->edge_count);

    if (dg == NULL) {
        LOG_ERROR("pdc_dg_destroy called with NULL dg\n");
        FUNC_LEAVE_VOID();
    }

    if (dg->edges) {
        for (int i = 0; i < dg->edge_count; i++) {
            if (dg->edges[i] && dg->edges[i]->data)
                dg->edge_free(dg->edges[i]->data);
            if (dg->edges[i])
                dg->edges[i] = PDC_free(dg->edges[i]);
        }
        dg->edges = (pdc_dg_edge_t **)PDC_free(dg->edges);
    }
    if (dg->vertices) {
        for (int i = 0; i < dg->vertex_count; i++) {
            if (dg->vertices[i] && dg->vertices[i]->data)
                dg->vertex_free(dg->vertices[i]->data);
            if (dg->vertices[i])
                dg->vertices[i] = PDC_free(dg->vertices[i]);
        }
        dg->vertices = (pdc_dg_vertex_t **)PDC_free(dg->vertices);
    }

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

    LOG_INFO("PDCdg_add_edge was called\n");

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

bool
PDCdg_has_vertex_data(pdc_dg_t *dg, bool (*is_data)(void *data, void *input), void *input)
{
    FUNC_ENTER(NULL);

    LOG_INFO("PDCdg_add_edge was called\n");

    if (dg == NULL) {
        LOG_WARNING("pdc_dg_has_vertex called with NULL dg\n");
        FUNC_LEAVE(false);
    }

    for (int i = 0; i < dg->vertex_count; i++) {
        if (is_data(dg->vertices[i]->data, input))
            FUNC_LEAVE(true);
    }

    FUNC_LEAVE(false);
}

bool
PDCdg_has_edge_data(pdc_dg_t *dg, bool (*is_data)(void *data, void *input), void *input)
{
    FUNC_ENTER(NULL);

    if (dg == NULL) {
        LOG_WARNING("pdc_dg_has_edge called with NULL dg\n");
        FUNC_LEAVE(false);
    }

    for (int i = 0; i < dg->edge_count; i++) {
        if (is_data(dg->edges[i]->data, input))
            FUNC_LEAVE(true);
    }

    FUNC_LEAVE(false);
}
