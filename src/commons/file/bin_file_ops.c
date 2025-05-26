#include "bin_file_ops.h"
#include "pdc_timing.h"
#include "pdc_malloc.h"

// type 1 int, 2 double, 3 string, 4 uint64, 5 size_t

void
bin_append_int(int data, FILE *stream)
{
    FUNC_ENTER(NULL);

    int    type   = 1;
    size_t length = 1;
    fwrite(&type, sizeof(int), 1, stream);
    fwrite(&length, sizeof(size_t), 1, stream);
    fwrite(&data, sizeof(int), length, stream);

    FUNC_LEAVE_VOID();
}

void
bin_append_double(double data, FILE *stream)
{
    FUNC_ENTER(NULL);

    int    type   = 2;
    size_t length = 1;
    fwrite(&type, sizeof(int), 1, stream);
    fwrite(&length, sizeof(size_t), 1, stream);
    fwrite(&data, sizeof(double), length, stream);

    FUNC_LEAVE_VOID();
}

void
bin_append_string(char *data, FILE *stream)
{
    FUNC_ENTER(NULL);

    size_t length = strlen(data);
    bin_append_string_with_len(data, length, stream);

    FUNC_LEAVE_VOID();
}

void
bin_append_string_with_len(char *data, size_t len, FILE *stream)
{
    FUNC_ENTER(NULL);

    int type = 3;
    fwrite(&type, sizeof(int), 1, stream);
    fwrite(&len, sizeof(size_t), 1, stream);
    fwrite(data, sizeof(char), len, stream);

    FUNC_LEAVE_VOID();
}

void
bin_append_uint64(uint64_t data, FILE *stream)
{
    FUNC_ENTER(NULL);

    int    type   = 4;
    size_t length = 1;
    fwrite(&type, sizeof(int), 1, stream);
    fwrite(&length, sizeof(size_t), 1, stream);
    fwrite(&data, sizeof(uint64_t), length, stream);

    FUNC_LEAVE_VOID();
}

void
bin_append_size_t(size_t data, FILE *stream)
{
    FUNC_ENTER(NULL);

    int    type   = 5;
    size_t length = 1;
    fwrite(&type, sizeof(int), 1, stream);
    fwrite(&length, sizeof(size_t), 1, stream);
    fwrite(&data, sizeof(size_t), length, stream);

    FUNC_LEAVE_VOID();
}

void
bin_read_general(int *t, size_t *len, void **data, FILE *stream)
{
    FUNC_ENTER(NULL);

    int    type   = -1;
    size_t length = 0;
    fread(&type, sizeof(int), 1, stream);
    fread(&length, sizeof(size_t), 1, stream);
    void *_data;
    if (type == 1) {
        _data = (int *)PDC_calloc(length, sizeof(int));
        fread(_data, sizeof(int), length, stream);
    }
    else if (type == 2) {
        _data = (double *)PDC_calloc(length, sizeof(double));
        fread(_data, sizeof(double), length, stream);
    }
    else if (type == 3) {
        _data = (char *)PDC_calloc(length + 1, sizeof(char));
        fread(_data, sizeof(char), length, stream);
    }
    else if (type == 4) {
        _data = (uint64_t *)PDC_calloc(length, sizeof(uint64_t));
        fread(_data, sizeof(uint64_t), length, stream);
    }
    else if (type == 5) {
        _data = (size_t *)PDC_calloc(length, sizeof(size_t));
        fread(_data, sizeof(size_t), length, stream);
    }
    data[0] = (void *)_data;
    *t      = type;
    *len    = length;

    FUNC_LEAVE_VOID();
}

size_t
miqs_skip_field(FILE *stream)
{
    FUNC_ENTER(NULL);

    size_t rst    = 0;
    int    type   = -1;
    size_t length = 0;
    fread(&type, sizeof(int), 1, stream);
    if (type == EOF) {
        FUNC_LEAVE(rst); // end of file, nothing to skip
    }
    rst += sizeof(int);
    fread(&length, sizeof(size_t), 1, stream);
    rst += sizeof(size_t);
    void *_data;
    if (type == 1) {
        _data = (int *)PDC_calloc(length, sizeof(int));
        fread(_data, sizeof(int), length, stream);
        rst += sizeof(int) * length;
    }
    else if (type == 2) {
        _data = (double *)PDC_calloc(length, sizeof(double));
        fread(_data, sizeof(double), length, stream);
        rst += sizeof(double) * length;
    }
    else if (type == 3) {
        _data = (char *)PDC_calloc(length + 1, sizeof(char));
        fread(_data, sizeof(char), length, stream);
        rst += sizeof(char) * length;
    }
    else if (type == 4) {
        _data = (uint64_t *)PDC_calloc(length, sizeof(uint64_t));
        fread(_data, sizeof(uint64_t), length, stream);
        rst += sizeof(uint64_t) * length;
    }
    else if (type == 5) {
        _data = (size_t *)PDC_calloc(length, sizeof(size_t));
        fread(_data, sizeof(size_t), length, stream);
        rst += sizeof(size_t) * length;
    }
    _data = (void *)PDC_free(_data);

    FUNC_LEAVE(rst);
}

void *
bin_read_index_numeric_value(int *is_float, FILE *file)
{
    FUNC_ENTER(NULL);

    int    type = 1;
    size_t len  = 1;
    void **data = (void **)PDC_calloc(1, sizeof(void *));
    bin_read_general(&type, &len, data, file);
    if (len == 1) {
        if (type == 1) {
            *is_float = 0;
        }
        else if (type == 2) {
            *is_float = 1;
        }
    }

    FUNC_LEAVE(*data);
}

int *
bin_read_int(FILE *file)
{
    FUNC_ENTER(NULL);

    int    type = 1;
    size_t len  = 1;
    void **data = (void **)PDC_calloc(1, sizeof(void *));
    bin_read_general(&type, &len, data, file);
    if (type == 1 && len == 1) {
        FUNC_LEAVE((int *)*data);
    }

    FUNC_LEAVE(NULL);
}

double *
bin_read_double(FILE *file)
{
    FUNC_ENTER(NULL);

    int    type = 2;
    size_t len  = 1;
    void **data = (void **)PDC_calloc(1, sizeof(void *));
    bin_read_general(&type, &len, data, file);
    if (type == 2 && len == 1) {
        FUNC_LEAVE((double *)*data);
    }

    FUNC_LEAVE(NULL);
}

char *
bin_read_string(FILE *file)
{
    FUNC_ENTER(NULL);

    int    type = 3;
    size_t len  = 1;
    void **data = (void **)PDC_calloc(1, sizeof(void *));
    bin_read_general(&type, &len, data, file);
    if (type == 3) {
        FUNC_LEAVE((char *)*data);
    }

    FUNC_LEAVE(NULL);
}

uint64_t *
bin_read_uint64(FILE *file)
{
    FUNC_ENTER(NULL);

    int    type = 4;
    size_t len  = 1;
    void **data = (void **)PDC_calloc(1, sizeof(void *));
    bin_read_general(&type, &len, data, file);
    if (type == 4 && len == 1) {
        FUNC_LEAVE((uint64_t *)*data);
    }

    FUNC_LEAVE(NULL);
}

size_t *
bin_read_size_t(FILE *file)
{
    FUNC_ENTER(NULL);

    int    type = 5;
    size_t len  = 1;
    void **data = (void **)PDC_calloc(1, sizeof(void *));
    bin_read_general(&type, &len, data, file);
    if (type == 5 && len == 1) {
        FUNC_LEAVE((size_t *)*data);
    }

    FUNC_LEAVE(NULL);
}

// type: 1, int, 2, float, 3. string, 4. uint64 5. size_t
void
bin_append_type(int type, FILE *stream)
{
    FUNC_ENTER(NULL);

    bin_append_int(type, stream);

    FUNC_LEAVE_VOID();
}
