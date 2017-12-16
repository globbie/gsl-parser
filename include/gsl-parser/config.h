#pragma once

/* return error codes */
typedef enum { gsl_OK, gsl_FAIL, gsl_LIMIT, gsl_NO_MATCH, gsl_FORMAT,
               gsl_EXISTS }
  gsl_parser_err_codes;

#define GSL_MAX_DEBUG_CONTEXT_SIZE 100  // TODO(ki.stfu): delete this

#define GSL_NUM_ENCODE_BASE 10

#define GSL_MAX_ARGS 16

