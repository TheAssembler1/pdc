#include <limits.h>
#include <ctype.h>
#include <inttypes.h>
#include "pdc_hash.h"
#include "pdc_timing.h"

/* Hash function for a pointer to an integer */

unsigned int
int_hash(void *vlocation)
{
    FUNC_ENTER(NULL);

    int *location;

    location = (int *)vlocation;

    FUNC_LEAVE((unsigned int)*location);
}

/* Hash function for a pointer to a uint64_t integer */

unsigned int
ui64_hash(void *vlocation)
{
    FUNC_ENTER(NULL);

    uint64_t *location;

    location = (uint64_t *)vlocation;

    FUNC_LEAVE((unsigned int)*location);
}

/* Hash function for a generic pointer */

unsigned int
pointer_hash(void *location)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE((unsigned int)(unsigned long)location);
}

/* String hash function */

unsigned int
string_hash(void *string)
{
    FUNC_ENTER(NULL);

    /* This is the djb2 string hash function */

    unsigned int   result = 5381;
    unsigned char *p;

    p = (unsigned char *)string;

    while (*p != '\0') {
        result = (result << 5) + result + *p;
        ++p;
    }

    FUNC_LEAVE(result);
}

/* The same function, with a tolower on every character so that
 * case is ignored.  This code is duplicated for performance. */

unsigned int
string_nocase_hash(void *string)
{
    FUNC_ENTER(NULL);

    unsigned int   result = 5381;
    unsigned char *p;

    p = (unsigned char *)string;

    while (*p != '\0') {
        result = (result << 5) + result + (unsigned int)tolower(*p);
        ++p;
    }

    FUNC_LEAVE(result);
}