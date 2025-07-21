#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <uuid/uuid.h>
#include "dart_utils.h"
#include "pdc_timing.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void
random_seed(long int seed)
{
    FUNC_ENTER(NULL);

    if (seed > 0)
        srand(seed);
    else if (seed == 0)
        srand(time(NULL));

    FUNC_LEAVE_VOID();
}

double
random_range(double min, double max)
{
    FUNC_ENTER(NULL);

    // return numbers between a given range
    double range = (max - min);
    double div   = RAND_MAX / range;

    FUNC_LEAVE(min + (rand() / div));
}

double
random_uniform()
{
    FUNC_ENTER(NULL);
    // return numbers between 0 and 1
    FUNC_LEAVE((double)rand() / (RAND_MAX + 1.0));
}

double
random_normal(double mean, double dev)
{
    FUNC_ENTER(NULL);

    // returns numbers based on normal distribution given mean and std dev

    // normal distribution centered on 0 with std dev of 1
    // Box-Muller transform
    double randomNum_normal =
        sqrt(-2 * log((rand() + 1.0) / (RAND_MAX + 1.0))) * cos(2 * M_PI * (rand() + 1.0) / (RAND_MAX + 1.0));
    double y = mean + dev * randomNum_normal;

    FUNC_LEAVE(y);
}

double
random_exponential(double mean)
{
    FUNC_ENTER(NULL);

    // returns numbers based on exp distr given mean
    double x = 1.0 / mean; // inverse lambda

    double u; // this will be my uniform random variable
    double exp_value;

    // Pull a uniform random number (0 < z < 1)
    do {
        u = random_uniform();
    } while ((u == 0) || (u == 1));
    exp_value = -x * log(u);

    FUNC_LEAVE(exp_value);
}

int64_t
get_timestamp_ns()
{
    FUNC_ENTER(NULL);

    struct timespec current;
    int64_t         rst = -1;

    if (clock_gettime(CLOCK_REALTIME, &current) == -1) {
        FUNC_LEAVE(rst);
    }

    FUNC_LEAVE(current.tv_sec * (int64_t)1e9 + current.tv_nsec);
}

int64_t
get_timestamp_us()
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(get_timestamp_ns() / 1000);
}

int64_t
get_timestamp_ms()
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(get_timestamp_us() / 1000);
}

int64_t
get_timestamp_s()
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(get_timestamp_ms() / 1000);
}