#include <gsl-parser.h>
#include <string.h>

int main(int argc __attribute__((unused)), const char **argv __attribute__((unused)))
{
    struct kndGslParser *parser;

    struct kndGslSpec request_subspecs[] = {
        {
            .name = "frontend",
            .name_size = strlen("frontend"),
            .specs = NULL
        },

        {
            .name = "backend",
            .name_size = strlen("backend"),
            .specs = NULL
        }
    };

    struct kndGslSpec reuqest_spec = {
        .name = "request",
        .name_size = strlen("request"),
        .specs = request_subspecs,
        .specs_num = 2

    };

    struct kndGslSpec root_spec = {
        .name = "root",
        .name_size = strlen("root"),
        .specs = &reuqest_spec,
        .specs_num = 1
    };

    char buffer[] = "{request {frontend ipc://var/lib/knowdy/coll_font}"
                             "{backend ipc://var/lib/knowdy/coll_back}}";

    int return_val = EXIT_FAILURE;
    int error_code;

    error_code = kndGslParser_new(&parser, &root_spec);
    if (error_code != 0) goto exit;

    size_t buffer_size = sizeof(buffer);
    error_code = parser->parse(parser, &root_spec, buffer, &buffer_size);
    if (error_code != 0) goto exit;

    return_val = EXIT_SUCCESS;
exit:
    kndGslParser_delete(parser);
    return return_val;
}
