#ifndef PDC_AN_BUILTIN_COMMON_H
#define PDC_AN_BUILTIN_COMMON_H

#include "pdc_an_user.h"

/**
 * @brief Elementwise vector magnitude: sqrt(sum of squares) across
 * num_inputs same-shaped input buffers (e.g. vx, vy, vz components of a
 * vector field), producing one double-precision output buffer of the same
 * shape. Every input must share the same ndim/size/pdc_var_type, and must
 * be PDC_FLOAT or PDC_DOUBLE.
 */
bool pdc_an_builtin_vector_magnitude(pdc_tf_internal_param *internal_param, char *params_str,
                                     void **input_bufs, pdc_tf_region_t *input_regions, int num_inputs,
                                     void **output_bufs, pdc_tf_region_t *output_regions, int num_outputs);

#endif /* PDC_AN_BUILTIN_COMMON_H */
