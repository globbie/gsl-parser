#pragma once

#include "gsl-parser/gsl_err.h"
#include "gsl-parser/gsl_task_spec.h"

#include <stddef.h>

extern gsl_err_t gsl_parse_size_t(void *obj, const char *rec, size_t *total_size);

extern gsl_err_t gsl_parse_task(const char *rec, size_t *total_size,
                                struct gslTaskSpec *specs, size_t num_specs);
