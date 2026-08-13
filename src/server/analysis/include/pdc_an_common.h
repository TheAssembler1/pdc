#ifndef PDC_AN_COMMON_H
#define PDC_AN_COMMON_H

#include "pdc_public.h"
#include "pdc_dg.h"
#include "pdc_vector.h"
#include "pdc_an_user.h"

/* Global registry of builtin (and dlopen-resolved external) analysis
 * functions, keyed by name + device. Mirrors pdc_tf_builtin_funcs_vector_g. */
extern PDC_VECTOR *pdc_an_builtin_funcs_vector_g;

/**
 * @brief Parses a JSON file describing a region-analysis graph and builds a
 * bipartite state/function directed graph from it.
 *
 * Expected JSON shape:
 * {
 *   "name": "...",
 *   "lib_path": "..."  (optional, required if any transformation is "external")
 *   "states": [
 *     { "name": "...", "persistence": "transient"|"persistent" }
 *   ],
 *   "transformations": [
 *     { "name": "...", "device": "CPU"|"GPU", "location": "builtin"|"external",
 *       "inputs": [ "state name", ... ], "outputs": [ "state name", ... ],
 *       "params": "..." (optional) }
 *   ]
 * }
 *
 * Every name in a transformation's "inputs"/"outputs" must already appear in
 * "states" (states are declared as vertices before transformations are
 * parsed). Each state may be produced by at most one transformation.
 * External functions are loaded via dlopen/dlsym using the top-level
 * "lib_path", exactly like the PDC TF JSON format.
 *
 * @param filepath Path to the JSON file.
 * @return Pointer to the newly created pdc_dg_t, or NULL on failure.
 */
pdc_dg_t *PDCan_dg_json_create_common(char *filepath);

/**
 * @brief Creates the (initially empty) global builtin analysis function
 * registry. No builtin region-analysis functions are compiled in yet;
 * this just gives external ("location": "external") functions a registry
 * to land in, and gives future builtins somewhere to register.
 */
perr_t PDCan_init_builtin_funcs(void);

/**
 * @brief Registers a single function pointer in the global builtin
 * analysis function registry, keyed by name + device.
 */
perr_t PDCan_add_builtin_func(char *func_name, a_func_t a_func, pdc_tf_dev_t dev);

/**
 * @brief Resolves a named builtin analysis function and binds it to a
 * pdc_an_func_t's `a_func` field.
 */
perr_t PDCan_link_builtin_func(char *func_name, pdc_tf_dev_t dev, pdc_an_func_t *f);

/**
 * @brief Looks up a node (state or transformation) in the graph by name.
 * @return Pointer to the node, or NULL if not found.
 */
pdc_an_node_t *PDCan_dg_get_node(pdc_dg_t *dg, const char *name);

/**
 * @brief Looks up a state node by name.
 * @return Pointer to the state, or NULL if not found or not a state vertex.
 */
pdc_an_state_t *PDCan_dg_get_state(pdc_dg_t *dg, const char *name);

#endif /* PDC_AN_COMMON_H */
