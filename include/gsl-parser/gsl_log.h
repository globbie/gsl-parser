#pragma once

#include <stdarg.h>
#include <stdio.h>

static inline void
gsl_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    vprintf(fmt, args);
    printf("\n");
    
    va_end(args);
}

