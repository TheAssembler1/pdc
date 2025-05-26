#include "timer_utils.h"
#include "pdc_timing.h"

#define MICROSECONDS_IN_SECONDS 1000000

/**
 * Function that returns the current timestamp in microseconds
 * since the Epoch (1970-01-01 00:00:00:000:000 UTC).
 */
int64_t
timer_us_timestamp()
{
    FUNC_ENTER(NULL);

    struct timeval tmp;
    gettimeofday(&(tmp), NULL);
    FUNC_LEAVE(((int64_t)tmp.tv_usec) + (int64_t)(tmp.tv_sec * MICROSECONDS_IN_SECONDS));
}

/**
 * Function that returns the current timestamp in miliseconds
 * since the Epoch (1970-01-01 00:00:00:000 UTC).
 */
int64_t
timer_ms_timestamp()
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(timer_us_timestamp() / 1000);
}

/**
 * Function that returns the current timestamp in seconds
 * since the Epoch (1970-01-01 00:00:00 UTC).
 */
int64_t
timer_s_timestamp()
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(timer_ms_timestamp() / 1000);
}

void
timer_start(stopwatch_t *t)
{
    FUNC_ENTER(NULL);

    t->start_mark = timer_us_timestamp();
    t->pause_mark = 0;
    t->running    = true;
    t->paused     = false;

    FUNC_LEAVE_VOID();
}

void
timer_pause(stopwatch_t *t)
{
    FUNC_ENTER(NULL);

    if (!(t->running) || (t->paused))
        FUNC_LEAVE_VOID();

    t->pause_mark = timer_us_timestamp() - (t->start_mark);
    t->running    = false;
    t->paused     = true;

    FUNC_LEAVE_VOID();
}

void
timer_unpause(stopwatch_t *t)
{
    FUNC_ENTER(NULL);

    if (t->running || !(t->paused))
        FUNC_LEAVE_VOID();

    t->start_mark = timer_us_timestamp() - (t->pause_mark);
    t->running    = true;
    t->paused     = false;

    FUNC_LEAVE_VOID();
}

double
timer_delta_us(stopwatch_t *t)
{
    FUNC_ENTER(NULL);

    if (t->running)
        FUNC_LEAVE(timer_us_timestamp() - (t->start_mark));

    if (t->paused)
        FUNC_LEAVE(t->pause_mark);

    // Will never actually get here
    FUNC_LEAVE((double)((t->pause_mark) - (t->start_mark)));
}

double
timer_delta_ms(stopwatch_t *t)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(timer_delta_us(t) / 1000.0);
}

double
timer_delta_s(stopwatch_t *t)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(timer_delta_ms(t) / 1000.0);
}

double
timer_delta_m(stopwatch_t *t)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(timer_delta_s(t) / 60.0);
}

double
timer_delta_h(stopwatch_t *t)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(timer_delta_m(t) / 60.0);
}
