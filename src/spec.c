#include "gsl-parser.h"

static int
child_add(struct kndGslSpec *self, struct kndGslSpec *child)
{
    struct kndGslSpec **specs = realloc(self->specs, (self->specs_num + 1) * sizeof(*specs));
    if (!specs) return -1;

    specs[self->specs_num] = child;
    self->specs = specs;
    ++self->specs_num;

    return 0;
}

int kndGslSpec_new(struct kndGslSpec **self, struct kndGslSpec *parent)
{
    struct kndGslSpec *spec = calloc(1, sizeof(*spec));
    if (!spec) return -1;

    if (parent) {
        int error_code = parent->child_add(parent, spec);
        if (error_code != 0) goto error;
    }

    spec->child_add = child_add;

    *self = spec;
    return 0;
error:
    kndGslSpec_delete(spec);
    return -1;
}

int kndGslSpec_delete(struct kndGslSpec *spec)
{
    for (size_t i = 0; i < spec->specs_num; ++i) {
        kndGslSpec_delete(spec->specs[i]);
    }

    free(spec);

    return 0;
}
