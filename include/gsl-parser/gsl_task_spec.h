#pragma once

#include <stdbool.h>
#include <stddef.h>

// TODO(ki.stfuk: Delete this
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

    int (*parse)(void *obj, const char *rec, size_t *total_size);
    int (*validate)(void *obj, const char *name, size_t name_size,
                    const char *rec, size_t *total_size);
    int (*run)(void *obj, const char *val, size_t val_size);
    int (*append)(void *accu, void *item);
    int (*alloc)(void *accu, const char *name, size_t name_size, size_t count,
                 void **item);
};
