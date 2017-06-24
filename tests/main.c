#include <gsl-parser.h>
#include <string.h>

static int
parse_service_addr(void *self, const char *rec, size_t *total)
{
    char frontend[100];
    size_t frontend_size = sizeof(frontend);

    char backend[100];
    size_t backend_size = sizeof(backend);

    struct kndGslSpec specs[] = {
            {
                    .name = "frontend",
                    .name_size = strlen("frontend"),
                    .buffer = frontend,
                    .buffer_size = &frontend_size
            },
            {
                    .name = "backend",
                    .name_size = strlen("backend"),
                    .buffer = backend,
                    .buffer_size = &backend_size
            }
    };

    int error_code;

    error_code = -1;

    if (error_code != 0) return error_code;

    return 0;
}

int main(int argc, const char **argv)
{
    struct kndGslParser *parser;
    struct kndGslSpec specs[] = {

            { .name = "request",
              .name_size = strlen("request"),
              .cb = parse_service_addr,
              .cb_arg = NULL
            }
    };
    char buffer[] = "{request {frontend ipc://var/lib/knowdy/coll_font}"
                    "{backend ipc://var/lib/knowdy/coll_back}}";

    int return_val = EXIT_FAILURE;
    int error_code;

    error_code = kndGslParser_new(&parser, specs, sizeof(specs) / sizeof(struct kndGslSpec));
    if (error_code != 0) goto exit;

    error_code = parser->parse(parser, buffer, sizeof(buffer));
    if (error_code != 0) goto exit;

    return_val = EXIT_SUCCESS;
exit:
    kndGslParser_delete(parser);
    return return_val;
}
