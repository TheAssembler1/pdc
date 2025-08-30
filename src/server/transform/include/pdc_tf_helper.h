#ifndef PDC_TF_HELPER_H
#define PDC_TF_HELPER_H

#include <assert.h>

#include "pdc_tf_common.h"
#include "pdc_client_server_common.h"

/**
 * @brief Set state parameters.
 *
 * This macro wraps the call to PDCtf_set_state_param to set the value
 * of a named state parameter.
 *
 * @param name          [in]  The name of the state parameter to set.
 * @param params        [in]  Pointer to the data to set for the parameter.
 * @param params_size   [in]  Size in bytes of the data pointed to by params.
 */
#define SET_STATE_PARAMS(name, params, params_size)                                                          \
    do {                                                                                                     \
        assert(PDCtf_set_state_param(internal_param.dg, name, internal_param.flat_conceptual_offset, params, \
                                     params_size) == SUCCEED);                                               \
    } while (0)

/**
 * @brief Get state parameters.
 *
 * @param name          The name of the state parameter to retrieve.
 * @param params        [in,out] Pointer to a buffer pointer that will be set by the function.
 * @param params_size   [in,out] Pointer to a size_t that will be set to the size of the returned buffer.
 */
#define GET_STATE_PARAMS(name, params, params_size)                                                          \
    do {                                                                                                     \
        assert(PDCtf_get_state_param(internal_param.dg, name, internal_param.flat_conceptual_offset, params, \
                                     params_size) == SUCCEED);                                               \
    } while (0)

/**
 * @brief Set func parameters.
 *
 * This macro wraps the call to PDCtf_set_func_param to set the value
 * of a named func parameter.
 *
 * @param name          [in]  The name of the func parameter to set.
 * @param params        [in]  Pointer to the data to set for the parameter.
 * @param params_size   [in]  Size in bytes of the data pointed to by params.
 */
#define SET_FUNC_PARAMS(name, params, params_size)                                                           \
    do {                                                                                                     \
        assert(PDCtf_set_func_param(internal_param.dg, name, internal_param.flat_conceptual_offset, params,  \
                                    params_size) == SUCCEED);                                                \
    } while (0)

/**
 * @brief Get func parameters.
 *
 * @param name          The name of the func parameter to retrieve.
 * @param params        [in,out] Pointer to a buffer pointer that will be set by the function.
 * @param params_size   [in,out] Pointer to a size_t that will be set to the size of the returned buffer.
 */
#define GET_FUNC_PARAMS(name, params, params_size)                                                           \
    do {                                                                                                     \
        assert(PDCtf_get_func_param(internal_param.dg, name, internal_param.flat_conceptual_offset, params,  \
                                    params_size) == SUCCEED);                                                \
    } while (0)

#endif