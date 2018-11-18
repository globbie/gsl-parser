#pragma once

#include "gsl-parser/gsl_err.h"
#include "gsl-parser/gsl_task_spec.h"

#include <stddef.h>

// obj of type size_t*
extern gsl_err_t gsl_run_set_size_t(void *obj, const char *val, size_t val_size);
extern gsl_err_t gsl_parse_size_t(void *obj, const char *rec, size_t *total_size);

// obj of type gslTaskSpec*
extern gsl_err_t gsl_parse_array(void *obj, const char *rec, size_t *total_size);

// obj of type gslTaskSpec*
extern gsl_err_t gsl_parse_cdata(void *obj, const char *rec, size_t *total_size);

extern gsl_err_t gsl_parse_task(const char *rec, size_t *total_size,
                                struct gslTaskSpec *specs, size_t num_specs);

