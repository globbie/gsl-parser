#include <gsl-parser.h>

int main(int argc __attribute__((unused)), const char **argv __attribute__((unused)))
{
    char buffer[] = "{request {frontend ipc://var/lib/knowdy/coll_font}"
        "{backend ipc://var/lib/knowdy/coll_back}}";

    struct kndGslParser *parser = NULL;
    struct kndGslSpec *request;

    int return_val = EXIT_FAILURE;
    int error_code;

    { // request spec init
        error_code = kndGslSpec_new(&request, NULL);
        if (error_code != 0) goto exit;

        request->name = "request";
        request->name_size = sizeof("request");
    }

    { // frontend and backend specs init
        struct kndGslSpec *frontend;
        struct kndGslSpec *backend;

        error_code = kndGslSpec_new(&frontend, request);
        if (error_code != 0) goto exit;

        frontend->name = "frontend";
        frontend->name_size = sizeof("frontend");

        error_code = kndGslSpec_new(&backend, request);
        if (error_code != 0) goto exit;

        backend->name = "backend";
        backend->name_size = sizeof("backend");
    }

    error_code = kndGslParser_new(&parser, request);
    if (error_code != 0) goto exit;

    size_t buffer_size = sizeof(buffer);
    error_code = parser->parse(parser, request, buffer, &buffer_size);
    if (error_code != 0) goto exit;

    return_val = EXIT_SUCCESS;

exit:
    if (request) kndGslSpec_delete(request);
    if (parser) kndGslParser_delete(parser);
    return return_val;
}
