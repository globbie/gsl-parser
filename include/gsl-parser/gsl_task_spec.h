#pragma once

#include "gsl-parser/gsl_err.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum { GSL_GET_STATE, GSL_GET_ARRAY_STATE, GSL_SET_STATE, GSL_SET_ARRAY_STATE } gsl_task_spec_type;

struct gslTaskSpec {
    gsl_task_spec_type type;

    const char *name;
    size_t name_size;

    bool is_completed;
    bool is_default;
    bool is_selector;
    bool is_implied;
    bool is_validator;
    bool is_list_item;

    char *buf;
    size_t *buf_size;
    size_t max_buf_size;

    void *obj;
    void *accu;  // TODO(ki.stfu): ?? remove and use obj instead

    gsl_err_t (*parse)(void *obj, const char *rec, size_t *total_size);
    gsl_err_t (*validate)(void *obj, const char *name, size_t name_size,
                          const char *rec, size_t *total_size);
    gsl_err_t (*run)(void *obj, const char *val, size_t val_size);

    gsl_err_t (*alloc)(void *accu, const char *name, size_t name_size, size_t count,
                       void **item);
    gsl_err_t (*append)(void *accu, void *item);
};
