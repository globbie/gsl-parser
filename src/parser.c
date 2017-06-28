#include <stdbool.h>
#include <string.h>

#include <gsl-parser.h>
#include <stdio.h>

#if 1
#define DBG_LOG(...) do { printf(__VA_ARGS__); } while (false)
#else
#define DBG_LOG(...) do { } while (false)
#endif

enum parser_state
{
    ps_none,
    ps_block_started,
    ps_field_started,
    ps_field_ended
};

static int
spec_find(struct kndGslSpec *specs, size_t specs_num,
          const char *name, size_t name_size,
          struct kndGslSpec **result)
{
    return -1;
}

static int
parse(struct kndGslParser *self, const char *buffer, size_t *buffer_size)
{
    const char *curr = buffer;
    const char *field_begin = NULL;
    size_t field_size = 0;
    const char *field_end = NULL;
    size_t tail_size = *buffer_size;

    int error_code;

    enum parser_state state = ps_none;

    while (curr < buffer + *buffer_size) {

        switch (*curr) {
        case '{':
            DBG_LOG("OPEN BRACKET\n");

            if (state == ps_none) {
                state = ps_block_started;
            }

            if (state == ps_field_ended) {
                DBG_LOG("\tentering sub block\n");
                // recursive parse
                size_t chunk_size = tail_size;
                error_code = parse(self, curr, &chunk_size);
                curr += chunk_size;
                break;
            }

            break;
        case '}':
            DBG_LOG("CLOSE BRACKET\n");
            if (state == ps_field_ended) {
                state = ps_none;
            }
            break;
        case '\n':
        case '\r':
        case '\t':
        case ' ':

            if (state == ps_block_started) {
                break;
            }

            if (state == ps_field_started) {
                DBG_LOG("== field ended '%.*s'\n", (int) field_size, field_begin);
                state = ps_field_ended;
                break;
            }

            if (state == ps_field_ended) {
                break;
            }

            break;
        default:

            if (state == ps_block_started) {
                DBG_LOG("== field started\n");

                state = ps_field_started;
                field_begin = curr;
                field_size = 1;

                break;
            }

            if (state == ps_field_started) {
                ++field_size;
            }

            if (state == ps_field_ended) {

            }


            break;
        }

        ++curr;
        --tail_size;
    }

    return 0;
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
