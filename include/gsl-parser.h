#pragma once

#include <stdlib.h>

struct kndGslSpec;

typedef int (*kndParserCallBack)(void *self, const char *rec, size_t *total);

struct kndGslSpec
{
    const char *name;
    size_t name_size;

    char *buffer;
    size_t *buffer_size;

    kndParserCallBack cb;
    void *cb_arg;

    struct kndGslSpec *specs;
    size_t specs_num;
};

struct kndGslParser
{
    const struct kndGslSpec *root_spec;

    int (*parse)(struct kndGslParser *self, struct kndGslSpec *root_spec, const char *buffer, size_t *buffer_size);
};

int kndGslParser_new(struct kndGslParser **parser, struct kndGslSpec *root_spec);
int kndGslParser_delete(struct kndGslParser *self);

