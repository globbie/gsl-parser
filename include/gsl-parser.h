#pragma once

#include "gsl-parser/config.h"
#include "gsl-parser/gsl_task_spec.h"

#include <stddef.h>

extern int gsl_parse_size_t(void *obj, const char *rec, size_t *total_size);

extern int gsl_parse_task(const char *rec, size_t *total_size,
                          struct gslTaskSpec *specs, size_t num_specs);
