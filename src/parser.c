#include <stdbool.h>
#include <string.h>

#include <gsl-parser.h>

static int
parse(struct kndGslParser *self, const char *buffer, size_t buffer_size)
{
    const char *curr;
    const char *begin;
    const char *end;


    bool in_field = false;
    bool in_terminal = false;

    curr = buffer;
    begin = buffer;
    end = buffer;

    while (*curr) { // todo: buffer could be not null-terminated string
        switch (*curr) {
        case '\n':
        case '\r':
        case '\t':
        case ' ':
            if (!in_field) break;
            if (in_terminal) break;

            for (size_t i = 0; i < self->specs_num; ++i) {
                const struct kndGslSpec *spec = &self->specs[i];

                if (strncmp(begin, spec->name, spec->name_size))
            }

            break;
        case '{':
            if (!in_field) {
                in_field = true;
                begin = curr + 1;
                end = begin;
            }
            break;
        case '}':
            break;
        default:
            end = curr + 1;
            break;
        }
    }

    return -1;
}

int
kndGslParser_new(struct kndGslParser **self, struct kndGslSpec *specs, size_t specs_num)
{
    struct kndGslParser *parser = calloc(1, sizeof(*parser));
    if (!parser) return -1;

    parser->specs = specs;
    parser->specs_num = specs_num;

    parser->parse = parse;

    *self = parser;
    return 0;
}

int
kndGslParser_delete(struct kndGslParser *self)
{
    free(self);
    return 0;
}
