#include "bulki_endianness_util.h"
#include "pdc_timing.h"

int
BULKI_is_system_le()
{
    FUNC_ENTER(NULL);

    uint16_t test = 0x1;
    uint8_t *byte = (uint8_t *)&test;

    FUNC_LEAVE(byte[0] == 0x1);
}

int
BULKI_is_system_be()
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(!BULKI_is_system_le());
}

uint8_t
BULKI_to_little_endian_u8(uint8_t value)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(value); // 8-bit values don't need conversion
}

uint16_t
BULKI_to_little_endian_u16(uint16_t value)
{
    FUNC_ENTER(NULL);

    if (BULKI_is_system_le()) {
        FUNC_LEAVE(value);
    }
    else {
        FUNC_LEAVE(((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8));
    }
}

uint32_t
BULKI_to_little_endian_u32(uint32_t value)
{
    FUNC_ENTER(NULL);

    if (BULKI_is_system_le()) {
        FUNC_LEAVE(value);
    }
    else {
        FUNC_LEAVE(((value & 0xFF000000) >> 24) | ((value & 0x00FF0000) >> 8) | ((value & 0x0000FF00) << 8) |
                   ((value & 0x000000FF) << 24));
    }
}

uint64_t
BULKI_to_little_endian_u64(uint64_t value)
{
    FUNC_ENTER(NULL);

    if (BULKI_is_system_le()) {
        FUNC_LEAVE(value);
    }
    else {
        FUNC_LEAVE(((value & 0xFF00000000000000ULL) >> 56) | ((value & 0x00FF000000000000ULL) >> 40) |
                   ((value & 0x0000FF0000000000ULL) >> 24) | ((value & 0x000000FF00000000ULL) >> 8) |
                   ((value & 0x00000000FF000000ULL) << 8) | ((value & 0x0000000000FF0000ULL) << 24) |
                   ((value & 0x000000000000FF00ULL) << 40) | ((value & 0x00000000000000FFULL) << 56));
    }
}

int8_t
BULKI_to_little_endian_i8(int8_t value)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE(value); // 8-bit values don't need conversion
}

int16_t
BULKI_to_little_endian_i16(int16_t value)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE((int16_t)BULKI_to_little_endian_u16((uint16_t)value));
}

int32_t
BULKI_to_little_endian_i32(int32_t value)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE((int32_t)BULKI_to_little_endian_u32((uint32_t)value));
}

int64_t
BULKI_to_little_endian_i64(int64_t value)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE((int64_t)BULKI_to_little_endian_u64((uint64_t)value));
}

float
BULKI_to_little_endian_f32(float value)
{
    FUNC_ENTER(NULL);

    union {
        float    f;
        uint32_t i;
    } u;
    u.f = value;
    u.i = BULKI_to_little_endian_u32(u.i);

    FUNC_LEAVE(u.f);
}

double
BULKI_to_little_endian_f64(double value)
{
    FUNC_ENTER(NULL);

    union {
        double   d;
        uint64_t i;
    } u;
    u.d = value;
    u.i = BULKI_to_little_endian_u64(u.i);

    FUNC_LEAVE(u.d);
}

int
BULKI_to_little_endian_int(int value)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE((int)BULKI_to_little_endian_u32((uint32_t)value));
}

long
BULKI_to_little_endian_long(long value)
{
    FUNC_ENTER(NULL);

    if (sizeof(long) == 4) {
        FUNC_LEAVE((long)BULKI_to_little_endian_u32((uint32_t)value));
    }
    else if (sizeof(long) == 8) {
        FUNC_LEAVE((long)BULKI_to_little_endian_u64((uint64_t)value));
    }
    else {
        // Handle unexpected long size
        FUNC_LEAVE(value);
    }
}

short
BULKI_to_little_endian_short(short value)
{
    FUNC_ENTER(NULL);
    FUNC_LEAVE((short)BULKI_to_little_endian_u16((uint16_t)value));
}

char
BULKI_to_little_endian_char(char value)
{
    FUNC_LEAVE(value); // char is typically 8-bit, so no conversion needed
}