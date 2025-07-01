#ifndef PDC_DG_H
#define PDC_DG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define PDC_DG_INIT_EDGE_CAPACITY   4
#define PDC_DG_INIT_VERTEX_CAPACITY 4
#define PDC_DG_INVALID_EDGE         -1
#define PDC_DG_INVALID_VERTEX       -1

/**
 * Directed graph vertex identifier type.
 */
typedef int pdc_dg_vertex_id_t;
/**
 * Directed graph edge identifier type.
 */
typedef int pdc_dg_edge_id_t;

/**
 * Structure representing a vertex in the directed graph.
 *
 * \param vertex_id [IN]     Unique identifier for the vertex
 * \param data      [IN]     Pointer to user data associated with the vertex
 */
typedef struct pdc_dg_vertex_t {
    pdc_dg_vertex_id_t vertex_id;
    void              *data;
} pdc_dg_vertex_t;

/**
 * Structure representing an edge in the directed graph.
 *
 * \param edge_id        [IN]   Unique identifier for the edge
 * \param from_vertex_id [IN]   Vertex ID where the edge starts
 * \param to_vertex_id   [IN]   Vertex ID where the edge ends
 * \param data           [IN]   Pointer to user data associated with the edge
 */
typedef struct pdc_dg_edge_t {
    pdc_dg_edge_id_t   edge_id;
    pdc_dg_vertex_id_t from_vertex_id;
    pdc_dg_vertex_id_t to_vertex_id;
    void              *data;
} pdc_dg_edge_t;

/**
 * Structure representing the directed graph.
 *
 * \param edges           [IN] Array of pointers to edges in the graph
 * \param vertices        [IN] Array of pointers to vertices in the graph
 * \param edge_count      [IN] Number of edges currently in the graph
 * \param vertex_count    [IN] Number of vertices currently in the graph
 * \param vertex_capacity [IN] Maximum vertices allocated before resizing
 * \param edge_capacity   [IN] Maximum edges allocated before resizing
 * \param data            [IN] Pointer to user data associated with the graph
 * \param edge_free       [IN] Function to free edge
 * \param vertex_free     [IN] Function to free vertex
 */
typedef struct pdc_dg_t {
    // Array of edges and vertices
    pdc_dg_edge_t   **edges;
    pdc_dg_vertex_t **vertices;

    // current number of edges and vertices
    uint32_t edge_count;
    uint32_t vertex_count;

    // max number of vertices before realloc needed
    uint32_t vertex_capacity;
    uint32_t edge_capacity;

    void *data;
    void (*edge_free)(void *data);
    void (*vertex_free)(void *data);
} pdc_dg_t;

/**
 * Create a new directed graph.
 *
 * \param data        [IN] User data pointer to associate with the graph
 * \param edge_free   [IN] Function to free edge data
 * \param vertex_Free [IN] Function to free vertex data
 *
 * \return Pointer to the newly created directed graph, or NULL on failure
 */
pdc_dg_t *PDCdg_create(void *data, void (*edge_free)(void *data), void (*vertex_free)(void *data));
/**
 * Destroy a directed graph, freeing all associated memory.
 *
 * \param dg [IN] Pointer to the directed graph to destroy
 */
void PDCdg_destroy(pdc_dg_t *dg);

/**
 * Add a vertex to the directed graph.
 *
 * \param dg   [IN]  Pointer to the directed graph
 * \param data [IN]  User data pointer to associate with the vertex
 *
 * \return Vertex ID of the newly added vertex, or PDC_DG_INVALID_VERTEX on failure
 */
pdc_dg_vertex_id_t PDCdg_add_vertex(pdc_dg_t *dg, void *data);

/**
 * Add an edge to the directed graph.
 *
 * \param dg             [IN] Pointer to the directed graph
 * \param from_vertex_id [IN] ID of the source vertex
 * \param to_vertex_id   [IN] ID of the destination vertex
 * \param data           [IN] User data pointer to associate with the edge
 *
 * \return Edge ID of the newly added edge, or PDC_DG_INVALID_EDGE on failure
 */
pdc_dg_edge_id_t PDCdg_add_edge(pdc_dg_t *dg, pdc_dg_vertex_id_t from_vertex_id,
                                pdc_dg_vertex_id_t to_vertex_id, void *data);

/**
 * Check if an edge with the given ID exists in the graph.
 *
 * \param dg      [IN] Pointer to the directed graph
 * \param edge_id [IN] Edge ID to check
 *
 * \return true if edge exists, false otherwise
 */
bool PDCdg_has_edge(pdc_dg_t *dg, pdc_dg_edge_id_t edge_id);

/**
 * Check if a vertex with the given ID exists in the graph.
 *
 * \param dg        [IN] Pointer to the directed graph
 * \param vertex_id [IN] Vertex ID to check
 *
 * \return true if vertex exists, false otherwise
 */
bool PDCdg_has_vertex(pdc_dg_t *dg, pdc_dg_vertex_id_t vertex_id);

/**
 * Check if the data associated with the given vertex exists in graph
 *
 * \param dg        [IN] Pointer to the directed graph
 * \param is_data   [IN] Predicate function to evaluate the vertex data
 * \param input    [IN] Custom user data passed to predicate
 *
 * \return vetex_id if the vertex exists and the predicate returns true, PDC_DG_INVALID_VERTEX otherwise
 */
pdc_dg_vertex_id_t PDCdg_has_vertex_data(pdc_dg_t *dg, bool (*is_data)(void *data, void *input), void *input);

/**
 * Check if the data associated with the given edge exists in graph
 *
 * \param dg       [IN] Pointer to the directed graph
 * \param is_data  [IN] Predicate function to evaluate the edge data
 * \param input    [IN] Custom user data passed to predicate
 *
 * \return edge_id if the edge exists and the predicate returns true, PDC_DG_INVALID_EDGE otherwise
 */
pdc_dg_edge_id_t PDCdg_has_edge_data(pdc_dg_t *dg, bool (*is_data)(void *data, void *input), void *input);

#endif /* PDC_DG_H */
