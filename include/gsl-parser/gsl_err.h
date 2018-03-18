#pragma once

#include <assert.h>

/* error codes */
typedef enum { gsl_OK, gsl_FAIL, gsl_LIMIT, gsl_NO_MATCH, gsl_FORMAT,
               gsl_EXISTS, gsl_EXTERNAL = 0x7f000000 }
  gsl_err_codes_t;

typedef struct {
    int code;
} gsl_err_t;

static inline gsl_err_t make_gsl_err(gsl_err_codes_t code) { return (gsl_err_t){ .code = code }; }

static inline gsl_err_t make_gsl_err_external(int ext_code) {
    return (gsl_err_t){ .code = gsl_EXTERNAL | ext_code };
}

static inline int is_gsl_err_external(gsl_err_t err) { return err.code & gsl_EXTERNAL; }

static inline int gsl_err_external_to_ext_code(gsl_err_t err) { assert(is_gsl_err_external(err)); return err.code & ~gsl_EXTERNAL; }

