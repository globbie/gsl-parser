#pragma once

#include "gsl-parser/gsl_err.h"

#include <stdbool.h>
#include <stddef.h>

// TODO(ki.stfu): Delete this
#define GSL_NAME_SIZE 512
#define GSL_SHORT_NAME_SIZE 64

typedef enum { GSL_GET_STATE, GSL_CHANGE_STATE } gsl_task_spec_type;

struct gslTaskSpec {
    gsl_task_spec_type type;

    const char *name;
    size_t name_size;

    struct gslTaskSpec *specs;
    size_t num_specs;

    bool is_completed;
    bool is_default;
    bool is_selector;
    bool is_implied;
    bool is_validator;
    bool is_terminal;
    bool is_list;
    bool is_atomic;

    char *buf;
    size_t *buf_size;
    size_t max_buf_size;

    void *obj;
    void *accu;

    gsl_err_t (*parse)(void *obj, const char *rec, size_t *total_size);
    gsl_err_t (*validate)(void *obj, const char *name, size_t name_size,
                          const char *rec, size_t *total_size);
    gsl_err_t (*run)(void *obj, const char *val, size_t val_size);
    gsl_err_t (*append)(void *accu, void *item);
    gsl_err_t (*alloc)(void *accu, const char *name, size_t name_size, size_t count,
                       void **item);
};
