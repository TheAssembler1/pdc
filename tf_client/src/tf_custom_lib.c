#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdc_tf_user.h"

bool
tf_client_write(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
                pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    printf("tf_client_write called\n");
    return true;
}

bool
tf_client_read(pdc_tf_internal_param internal_param, char *params_str, void **region_data,
               pdc_tf_region_t input_region, pdc_tf_region_t *output_region)
{
    printf("tf_client_read called\n");
    return true;
}