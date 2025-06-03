#include "dart_math.h"
#include "pdc_timing.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

double
log_with_base(double base, double x)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(log(x) / log(base));
}

uint32_t
uint32_pow(uint32_t base, uint32_t pow)
{
    FUNC_ENTER(NULL);

    uint32_t p   = 0;
    uint32_t rst = 1;
    for (p = 0; p < pow; p++) {
        rst = base * rst;
    }

    FUNC_LEAVE(rst);
}

uint64_t
uint64_pow(uint64_t base, uint64_t pow)
{
    FUNC_ENTER(NULL);

    uint64_t p   = 0;
    uint64_t rst = 1;
    for (p = 0; p < pow; p++) {
        rst = base * rst;
    }

    FUNC_LEAVE(rst);
}

int
int_abs(int a)
{
    FUNC_ENTER(NULL);

    if (a < 0) {
        FUNC_LEAVE(0 - a);
    }

    FUNC_LEAVE(a);
}