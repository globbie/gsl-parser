#pragma once

/* return error codes */
typedef enum { gsl_OK, gsl_FAIL, gsl_LIMIT, gsl_NO_MATCH, gsl_FORMAT,
               gsl_EXISTS }
  gsl_parser_err_codes;

#define GSL_NUM_ENCODE_BASE 10

