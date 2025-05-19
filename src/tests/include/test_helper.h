#ifndef TEST_HELPER_H
#define TEST_HELPER_H

#include "pdc.h"

#define TSUCCEED 0
#define TFAIL    1

/*
 * TGOTO_DONE macro. The argument is the return value which is
 * assigned to the `ret_value' variable.  Control branches to
 * the `done' label.
 */
#define TGOTO_DONE(ret_val)                                                                                  \
    do {                                                                                                     \
        ret_value = ret_val;                                                                                 \
        goto done;                                                                                           \
    } while (0)

#define TGOTO_DONE_VOID                                                                                      \
    do {                                                                                                     \
        goto done;                                                                                           \
    } while (0)

/*
 * TGOTO_ERROR macro. The arguments are the return value and an
 * error string.  The return value is assigned to a variable `ret_value' and
 * control branches to the `done' label.
 */
#define TGOTO_ERROR(ret_val, ...)                                                                            \
    do {                                                                                                     \
        LOG_ERROR(__VA_ARGS__);                                                                              \
        LOG_JUST_PRINT("\n");                                                                                \
        TGOTO_DONE(ret_val);                                                                                 \
    } while (0)

#define TGOTO_ERROR_VOID(...)                                                                                \
    do {                                                                                                     \
        LOG_ERROR(__VA_ARGS__);                                                                              \
        LOG_JUST_PRINT("\n");                                                                                \
        TGOTO_DONE_VOID;                                                                                     \
    } while (0)

#define TASSERT(status, success_message, fail_message)                                                       \
    do {                                                                                                     \
        if (!(status)) {                                                                                     \
            TGOTO_ERROR(TFAIL, fail_message);                                                                \
        }                                                                                                    \
        else {                                                                                               \
            LOG_INFO("%s\n", success_message);                                                               \
        }                                                                                                    \
    } while (0)

#endif