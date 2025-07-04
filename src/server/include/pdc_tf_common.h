#ifndef PDC_TF_COMMON_H
#define PDC_TF_COMMON_H

#include "pdc.h"

perr_t PDCtf_exec_graph(pdcid_t dg, pdcid_t current_state_id, pdcid_t desired_state_id);

#endif // PDC_TF_COMMON_H