#undef NDEBUG  // For developing

#include "gsl-parser.h"
#include "gsl-parser/glb_p_log.h"

#include <assert.h>
#include <ctype.h>
#include <stdint.h> // for SIZE_MAX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* numeric conversion by strtol */
#include <errno.h>
#include <limits.h>

#define DEBUG_PARSER_LEVEL_1 0
#define DEBUG_PARSER_LEVEL_2 1
#define DEBUG_PARSER_LEVEL_3 0
#define DEBUG_PARSER_LEVEL_4 0
#define DEBUG_PARSER_LEVEL_TMP 1

static int gsl_parse_list(const char *rec,
                          size_t *total_size,
                          struct gslTaskSpec *specs,
                          size_t num_specs);

static int
check_name_limits(const char *b, const char *e, size_t *buf_size)
{
    *buf_size = e - b;
    if (!*buf_size) {
        glb_p_log("-- empty name?");
        return gsl_LIMIT;
    }
    if (*buf_size > GSL_NAME_SIZE) {
        glb_p_log("-- field tag too large: %zu", buf_size);
        return gsl_LIMIT;
    }
    return gsl_OK;
}

int gsl_parse_matching_braces(const char *rec,
                              size_t brace_count,
                              size_t *chunk_size)
{
    const char *b;
    const char *c;

    c = rec;
    b = c;

    while (*c) {
        switch (*c) {
        case '{':
            brace_count++;
            break;
        case '}':
            if (!brace_count)
                return gsl_FAIL;
            brace_count--;
            if (!brace_count) {
                *chunk_size = c - b;
                return gsl_OK;
            }
            break;
        default:
            break;
        }
        c++;
    }

    return gsl_FAIL;
}


static int
gsl_run_set_size_t(void *obj,
                   struct gslTaskArg *args, size_t num_args)
{
    size_t *self = (size_t *)obj;
    struct gslTaskArg *arg;
    char *num_end;
    unsigned long long num;

    assert(args && num_args == 1);
    arg = &args[0];

    assert(arg->name_size == strlen("_impl") && !memcmp(arg->name, "_impl", arg->name_size));
    assert(arg->val && arg->val_size != 0);

    if (!isdigit(arg->val[0])) {
        glb_p_log("-- num size_t doesn't start from a digit: \"%.*s\"",
                arg->val_size, arg->val);
        return gsl_FORMAT;
    }

    errno = 0;
    num = strtoull(arg->val, &num_end, GSL_NUM_ENCODE_BASE);  // FIXME(ki.stfu): Null-terminated string is expected
    if (errno == ERANGE && num == ULLONG_MAX) {
        glb_p_log("-- num limit reached: %.*s max: %llu",
                arg->val_size, arg->val, ULLONG_MAX);
        return gsl_LIMIT;
    }
    else if (errno != 0 && num == 0) {
        glb_p_log("-- cannot convert \"%.*s\" to num: %d",
                arg->val_size, arg->val, errno);
        return gsl_FORMAT;
    }

    if (arg->val + arg->val_size != num_end) {
        glb_p_log("-- not all characters in \"%.*s\" were parsed: \"%.*s\"",
                arg->val_size, arg->val, num_end - arg->val, arg->val);
        return gsl_FORMAT;
    }

    if (ULLONG_MAX > SIZE_MAX && num > SIZE_MAX) {
        glb_p_log("-- num size_t limit reached: %llu max: %llu",
                num, (unsigned long long)SIZE_MAX);
        return gsl_LIMIT;
    }

    *self = (size_t)num;
    if (DEBUG_PARSER_LEVEL_2)
        glb_p_log("++ got num size_t: %zu",
                *self);

    return gsl_OK;
}

int
gsl_parse_size_t(void *obj,
                 const char *rec,
                 size_t *total_size)
{
    struct gslTaskSpec specs[] = {
        { .is_implied = true,
          .run = gsl_run_set_size_t,
          .obj = obj
        }
    };
    int err;

    if (DEBUG_PARSER_LEVEL_2)
        glb_p_log(".. parse num size_t: \"%.*s\"", 16, rec);

    err = gsl_parse_task(rec, total_size, specs, sizeof(specs) / sizeof(struct gslTaskSpec));
    if (err) return err;

    return gsl_OK;
}

static int
gsl_spec_is_correct(struct gslTaskSpec *spec)
{
    if (DEBUG_PARSER_LEVEL_2)
        glb_p_log(".. check spec: \"%.*s\"..", spec->name_size, spec->name);

    // Check the fields are not mutually exclusive (by groups):

    assert(spec->type == GSL_GET_STATE || spec->type == GSL_CHANGE_STATE);

    assert((spec->name != NULL) == (spec->name_size != 0));

    assert(spec->specs == NULL && spec->num_specs == 0);  // TODO(ki.stfu): Remove these fields

    assert(!spec->is_completed);

    if (spec->is_default)
        assert(!spec->is_selector && !spec->is_implied && !spec->is_validator && !spec->is_list && !spec->is_atomic);
    if (spec->is_selector)
        assert(!spec->is_default && !spec->is_validator && !spec->is_list && !spec->is_atomic);
    if (spec->is_implied)
        assert(!spec->is_default && !spec->is_validator && !spec->is_list && !spec->is_atomic);
    if (spec->is_validator)
        assert(!spec->is_default && !spec->is_selector && !spec->is_implied && !spec->is_list && !spec->is_atomic);
    assert(!spec->is_terminal);  // TODO(ki.stfu): ?? Remove this field
    // FIXME(ki.stfu): assert(!spec->is_list);  // TODO(ki.stfu): ?? Remove this field
    assert(!spec->is_atomic);  // TODO(ki.stfu): ?? Remove this field

    assert((spec->buf != NULL) == (spec->buf_size != NULL));
    assert((spec->buf != NULL) == (spec->max_buf_size != 0));
    if (spec->buf)
        assert(*spec->buf_size == 0);

    // TODO(ki.stfu): ?? assert(spec->accu == NULL);  // TODO(ki.stfu): ?? remove this field

    if (spec->buf)
        assert(spec->parse == NULL && spec->validate == NULL && spec->run == NULL);
    if (spec->parse)
        assert(spec->buf == NULL && spec->validate == NULL && spec->run == NULL);
    if (spec->validate)
        assert(spec->buf == NULL && spec->parse == NULL && spec->run == NULL);
    if (spec->run)
        assert(spec->buf == NULL && spec->parse == NULL && spec->validate == NULL);
    // TODO(ki.stfu): ?? assert(spec->append == NULL);  // TODO(ki.stfu): ?? remove this field
    // TODO(ki.stfu): ?? assert(spec->alloc == NULL);  // TODO(ki.stfu): ?? remove this field

    // Check that they are not mutually exclusive (in general):

    if (spec->type) {
        assert(!spec->is_default && (!spec->is_implied || spec->name));
        assert(spec->name || spec->is_validator);
    }

    if (spec->name)
        assert(!spec->is_default && !spec->is_validator);

    if (spec->is_default) {
        assert(spec->type == 0);  // type is useless for default_spec
        assert(spec->name == NULL);
        assert(spec->obj != NULL);
        assert(spec->run != NULL);
    }

    if (spec->is_implied) {
        // |spec->type| can be set (depends on |spec->name|)
        // |spec->name| can be NULL
        if (spec->buf)
            assert(spec->obj == NULL);
        if (spec->run)
            assert(spec->obj != NULL);
    }

    assert(spec->is_validator == (spec->validate != NULL));
    if (spec->is_validator) {
        // |spec->type| can be set
        assert(spec->name == NULL);
        assert(spec->obj != NULL);
        assert(spec->validate != NULL);
    }

    if (spec->is_list) {
        // TODO(ki.stfu): ?? assert(spec->type == 0);
        assert(spec->accu != NULL);
        assert(spec->alloc != NULL);
        assert(spec->append != NULL);
        assert(spec->parse != NULL);
        assert(spec->obj == NULL);
    }

    if (spec->buf) {
        // |spec->type| can be set (depends on |spec->name|)
        assert(spec->is_implied || spec->name != NULL);
        assert(spec->obj == NULL);
    }

    if (spec->parse) {
        // |spec->type| can be set
        assert(spec->name != NULL);
        if (!spec->is_list)
            assert(spec->obj != NULL);
    }

    // if (spec->validate)  -- already handled in spec->is_validator

    if (spec->run) {
        // |spec->type| can be set (depends on |spec->name|)
        assert(spec->is_default || spec->is_implied || spec->name != NULL);
        assert(spec->obj != NULL);
    }

    // Test plans:
    //   buf:
    //     gsl_check_implied_field:
    //       } - OK
    //       { - OK
    //       ( - NOT TESTED!
    //       ) - NOT TESTED!
    //     gsl_check_field_terminal_value:
    //       } - OK
    //   parse:
    //     gsl_parse_field_value:
    //       <space> - OK
    //       { - OK
    //       } - OK
    //       ( - NOT TESTED!
    //       ) - NOT TESTED!
    //   validate:
    //     gsl_parse_field_value:
    //       <space> - OK
    //       { - OK
    //       } - OK
    //       ( - NOT TESTED!
    //       ) - NOT TESTED!
    //   run:
    //     gsl_check_implied_field:
    //       } - OK
    //       { - OK
    //       ( - NOT TESTED!
    //       ) - NOT TESTED!
    //     gsl_check_field_terminal_value:
    //       } - OK
    //     gsl_check_default:
    //       } - NOT TESTED!
    //       ) - NOT TESTED!
    assert(spec->buf != NULL || spec->parse != NULL || spec->validate != NULL || spec->run != NULL);

    return 1;
}

static int
gsl_spec_buf_copy(struct gslTaskSpec *spec,
                  const char *val,
                  size_t val_size)
{
    if (DEBUG_PARSER_LEVEL_2)
        glb_p_log(".. writing val \"%.*s\" [%zu] to buf [max size: %zu]..",
                val_size, val, val_size, spec->max_buf_size);

    assert(val_size && "val is empty");

    if (val_size > spec->max_buf_size) {
        glb_p_log("-- %.*s: buf limit reached: %zu max: %zu",
                spec->name_size, spec->name, val_size, spec->max_buf_size);
        return gsl_LIMIT;
    }

    if (*spec->buf_size) {
        glb_p_log("-- %.*s: buf already contains \"%.*s\"",
                spec->name_size, spec->name, spec->buf_size, spec->buf);
        return gsl_EXISTS;
    }

    memcpy(spec->buf, val, val_size);
    *spec->buf_size = val_size;

    return gsl_OK;
}

static int
gsl_find_spec(const char *name,
              size_t name_size,
              gsl_task_spec_type spec_type,
              struct gslTaskSpec *specs,
              size_t num_specs,
              struct gslTaskSpec **out_spec)
{
    struct gslTaskSpec *spec;
    struct gslTaskSpec *validator_spec = NULL;

    for (size_t i = 0; i < num_specs; i++) {
        spec = &specs[i];

        if (spec->type != spec_type) continue;

        if (spec->is_validator) {
            assert(validator_spec == NULL && "validator_spec was already specified");
            validator_spec = spec;
            continue;
        }

        if (spec->name_size == name_size && !memcmp(spec->name, name, spec->name_size)) {
            *out_spec = spec;
            return gsl_OK;
        }
    }

    if (DEBUG_PARSER_LEVEL_2)
        glb_p_log("-- no named spec found for \"%.*s\" of type %s  validator: %p",
                name_size, name,
                spec_type == GSL_GET_STATE ? "GET" : "CHANGE",
                validator_spec);

    if (validator_spec) {
        *out_spec = validator_spec;
        return gsl_OK;
    }

    return gsl_NO_MATCH;
}

static int
gsl_args_push_back(const char *name,
                   size_t name_size,
                   const char *val,
                   size_t val_size,
                   struct gslTaskArg *args,
                   size_t *num_args)
{
    struct gslTaskArg *arg;

    // TODO(ki.stfu): ?? do not use gslTaskArg(s)
    *num_args = 0;  // clear args

    if (DEBUG_PARSER_LEVEL_2)
        glb_p_log(".. adding (\"%.*s\", \"%.*s\") to args [size: %zu]..",
                name_size, name, val_size, val, *num_args);

    if (*num_args == GSL_MAX_ARGS) {
        glb_p_log("-- no slot for \"%.*s\" arg [num_args: %zu] :(",
                name_size, name, *num_args);
        return gsl_LIMIT;
    }
    assert(name_size <= GSL_NAME_SIZE && "arg name is longer than GSL_NAME_SIZE");
    assert(val_size <= GSL_NAME_SIZE && "arg val is longer than GSL_NAME_SIZE");

    arg = &args[*num_args];
    memcpy(arg->name, name, name_size);
    arg->name[name_size] = '\0';
    arg->name_size = name_size;

    memcpy(arg->val, val, val_size);
    arg->val[val_size] = '\0';
    arg->val_size = val_size;

    (*num_args)++;

    return gsl_OK;
}

static int
gsl_check_matching_closing_brace(const char *c, bool in_change)
{
    switch (*(c - 1)) {
    case '}':
        if (!in_change) return gsl_OK;
        break;
    case ')':
        if (in_change) return gsl_OK;
        break;
    default:
        assert("no closing brace found");
    }

    glb_p_log("-- no matching brace found: \"%.*s\"",
              16, c);
    return gsl_FORMAT;
}

static int
gsl_check_implied_field(const char *val,
                        size_t val_size,
                        struct gslTaskSpec *specs,
                        size_t num_specs,
                        struct gslTaskArg *args,
                        size_t *num_args)
{
    struct gslTaskSpec *spec;
    struct gslTaskSpec *implied_spec = NULL;
    const char *impl_arg_name = "_impl";
    size_t impl_arg_name_size = strlen("_impl");
    int err;

    assert(impl_arg_name_size <= GSL_NAME_SIZE && "\"_impl\" is longer than GSL_NAME_SIZE");
    assert(val_size && "implied val is empty");

    if (DEBUG_PARSER_LEVEL_2)
        glb_p_log("++ got implied val: \"%.*s\" [%zu]",
                val_size, val, val_size);
    if (val_size > GSL_NAME_SIZE) return gsl_LIMIT;

    for (size_t i = 0; i < num_specs; i++) {
        spec = &specs[i];

        if (spec->is_implied) {
            assert(implied_spec == NULL && "implied_spec was already specified");
            implied_spec = spec;
        }
    }

    if (!implied_spec) {
        glb_p_log("-- no implied spec found to handle the \"%.*s\" val",
                val_size, val);

        return gsl_NO_MATCH;
    }

    if (DEBUG_PARSER_LEVEL_2)
        glb_p_log("++ got implied spec: \"%.*s\" buf: %p run: %p!",
                implied_spec->name_size, implied_spec->name, implied_spec->buf, implied_spec->run);

    if (implied_spec->buf) {
        err = gsl_spec_buf_copy(implied_spec, val, val_size);
        if (err) return err;

        implied_spec->is_completed = true;
        return gsl_OK;
    }

    err = gsl_args_push_back(impl_arg_name, impl_arg_name_size, val, val_size, args, num_args);
    if (err) return err;

    err = implied_spec->run(implied_spec->obj, args, *num_args);
    if (err) {
        glb_p_log("-- implied func for \"%.*s\" failed: %d :(",
                val_size, val, err);
        return err;
    }

    implied_spec->is_completed = true;
    return gsl_OK;
}

static int
gsl_check_field_tag(const char *name,
                    size_t name_size,
                    gsl_task_spec_type type,
                    struct gslTaskSpec *specs,
                    size_t num_specs,
                    struct gslTaskSpec **out_spec)
{
    int err;

    if (!name_size) {
        glb_p_log("-- empty field tag?");
        return gsl_FORMAT;
    }
    if (name_size > GSL_NAME_SIZE) {
        glb_p_log("-- field tag too large: %zu bytes: \"%.*s\"",
                name_size, name_size, name);
        return gsl_LIMIT;
    }

    if (DEBUG_PARSER_LEVEL_2)
        glb_p_log("++ BASIC LOOP got tag after brace: \"%.*s\" [%zu]",
                name_size, name, name_size);

    err = gsl_find_spec(name, name_size, type, specs, num_specs, out_spec);
    if (err) {
        glb_p_log("-- no spec found to handle the \"%.*s\" tag: %d",
                name_size, name, err);
        return err;
    }

    if (DEBUG_PARSER_LEVEL_2)
        glb_p_log("++ got SPEC: \"%.*s\" (default: %d) (is validator: %d)",
                (*out_spec)->name_size, (*out_spec)->name, (*out_spec)->is_default, (*out_spec)->is_validator);

    return gsl_OK;
}

static int
gsl_parse_field_value(const char *name,
                      size_t name_size,
                      struct gslTaskSpec *spec,
                      const char *rec,
                      size_t *total_size,
                      bool *in_terminal)
{
    int err;

    assert(name_size && "name is empty");
    assert(!*in_terminal && "gsl_parse_field_value is called for terminal value");

    if (spec->validate) {
        err = spec->validate(spec->obj, name, name_size, rec, total_size);
        if (err) {
            glb_p_log("-- ERR: %d validation spec for \"%.*s\" failed :(",
                    err, name_size, name);
            return err;
        }

        spec->is_completed = true;
        return gsl_OK;
    }

    if (spec->parse) {
        if (DEBUG_PARSER_LEVEL_2)
            glb_p_log("\n    >>> further parsing required in \"%.*s\" FROM: \"%.*s\" FUNC: %p",
                    spec->name_size, spec->name, 16, rec, spec->parse);

        err = spec->parse(spec->obj, rec, total_size);
        if (err) {
            glb_p_log("-- ERR: %d parsing of spec \"%.*s\" failed :(",
                    err, spec->name_size, spec->name);
            return err;
        }

        spec->is_completed = true;
        return gsl_OK;
    }

    if (DEBUG_PARSER_LEVEL_2)
        glb_p_log("== ATOMIC SPEC found: %.*s! no further parsing is required.",
                spec->name_size, spec->name);

    *in_terminal = true;
    // *total_size = 0;  // This actully isn't used.
    return gsl_OK;
}

static int
gsl_check_field_terminal_value(const char *val,
                               size_t val_size,
                               struct gslTaskSpec *spec,
                               struct gslTaskArg *args,
                               size_t *num_args)
{
    int err;

    if (!val_size) {
        glb_p_log("-- empty value :(");
        return gsl_FORMAT;
    }
    if (val_size > GSL_NAME_SIZE) {
        glb_p_log("-- value too large: %zu bytes: \"%.*s\"",
                val_size, val_size, val);
        return gsl_LIMIT;
    }

    if (DEBUG_PARSER_LEVEL_2)
        glb_p_log("++ got terminal val: \"%.*s\" [%zu]",
                val_size, val, val_size);

    assert(spec->parse == NULL && spec->validate == NULL && "spec for terminal val has .parse or .validate");

    if (spec->buf) {
        err = gsl_spec_buf_copy(spec, val, val_size);
        if (err) return err;

        spec->is_completed = true;
        return gsl_OK;
    }

    // FIXME(ki.stfu): ?? valid case
    // FIXME(ki.stfu): ?? push to args only if spec->run != NULL
    err = gsl_args_push_back(spec->name, spec->name_size, val, val_size, args, num_args);
    if (err) return err;

    err = spec->run(spec->obj, args, *num_args);
    if (err) {
        glb_p_log("-- \"%.*s\" func run failed: %d :(",
                spec->name_size, spec->name, err);
        return err;
    }

    spec->is_completed = true;
    return gsl_OK;
}

static int
gsl_check_default(const char *rec,
                  gsl_task_spec_type type,
                  struct gslTaskSpec *specs,
                  size_t num_specs) {
    struct gslTaskSpec *spec;
    struct gslTaskSpec *default_spec = NULL;
    int err;

    for (size_t i = 0; i < num_specs; ++i) {
        spec = &specs[i];

        if (spec->is_default && spec->type == type) {
            assert(default_spec == NULL && "default_spec was already specified");
            default_spec = spec;
            continue;
        }

        if (!spec->is_selector && spec->is_completed)
            return gsl_OK;
    }

    if (!default_spec) {
        glb_p_log("-- no default spec found to handle an empty field (ignoring selectors): %.*s",
                16, rec);
        return gsl_NO_MATCH;
    }

    err = default_spec->run(default_spec->obj, NULL, 0);
    if (err) {
        glb_p_log("-- default func run failed: %d :(",
                err);
        return err;
    }

    return gsl_OK;
}

int gsl_parse_task(const char *rec,
                   size_t *total_size,
                   struct gslTaskSpec *specs,
                   size_t num_specs)
{
    const char *b, *c, *e;
    size_t name_size;

    struct gslTaskSpec *spec;

    struct gslTaskArg args[GSL_MAX_ARGS];
    size_t num_args = 0;

    bool in_implied_field = false;
    bool in_field = false;
    bool in_change = false;
    bool in_tag = false;
    bool in_terminal = false;

    size_t chunk_size;
    int err;

    c = rec;
    b = rec;
    e = rec;

    if (DEBUG_PARSER_LEVEL_2)
        glb_p_log("\n\n*** start basic PARSING: \"%.*s\" num specs: %zu [%p]",
                16, rec, num_specs, specs);

    // Check gslTaskSpec is properly filled
    for (size_t i = 0; i < num_specs; i++)
        assert(gsl_spec_is_correct(&specs[i]));

    while (*c) {
        switch (*c) {
        case '-':
            if (!in_field) {
                e = c + 1;
                break;
            }
            if (in_tag) {
                e = c + 1;
                break;
            }
            /* comment out this region */
            err = gsl_parse_matching_braces(c, 1, &chunk_size);
            if (err) return err;
            c += chunk_size;
            in_field = false;
            b = c;
            e = b;
            break;
        case '\n':
        case '\r':
        case '\t':
        case ' ':
            if (!in_field)
                break;
            if (in_terminal)
                break;

            if (DEBUG_PARSER_LEVEL_2)
                glb_p_log("+ whitespace in basic PARSING!");

            // Parse a tag after a first space.  Means in_tag can be set to true.

            err = gsl_check_field_tag(b, e - b, !in_change ? GSL_GET_STATE : GSL_CHANGE_STATE, specs, num_specs, &spec);
            if (err) return err;

            err = gsl_parse_field_value(b, e - b, spec, c, &chunk_size, &in_terminal);
            if (err) return err;

            if (in_terminal) {
                // Parse an atomic value.  Remember that we are in in_tag state.
                // in_field == true
                // in_change == true | false
                in_tag = true;
                // in_terminal == true
                b = c + 1;
                e = b;
                break;
            }

            err = gsl_check_matching_closing_brace(c + chunk_size + 1, in_change);
            if (err) return err;

            in_field = false;
            in_change = false;
            // in_tag == false
            // in_terminal == false
            c += chunk_size;
            b = c;
            e = b;
            break;
        case '{':
        case '(':
            /* starting brace '{' or '(' */
            if (!in_field) {
                if (in_implied_field) {
                    err = gsl_check_implied_field(b, e - b, specs, num_specs, args, &num_args);
                    if (err) return err;

                    in_implied_field = false;
                }

                in_field = true;
                in_change = (*c == '(');
                // in_tag == false
                // in_terminal == false
                b = c + 1;
                e = b;
                break;
            }

            assert(in_tag == in_terminal);

            if (in_terminal) {  // or in_tag
                glb_p_log("-- terminal val for ATOMIC SPEC \"%.*s\" has an opening brace '%c': %.*s",
                          spec->name_size, spec->name, (!in_change ? '{' : '('), c - b + 16, b);
                return gsl_FORMAT;
            }

            // Parse a tag after an inner field brace '{' or '('.  Means in_tag can be set to true.

            err = gsl_check_field_tag(b, e - b, !in_change ? GSL_GET_STATE : GSL_CHANGE_STATE, specs, num_specs, &spec);
            if (err) return err;

            err = gsl_parse_field_value(b, e - b, spec, c, &chunk_size, &in_terminal);
            if (err) return err;

            if (in_terminal) {
                glb_p_log("-- terminal val for ATOMIC SPEC \"%.*s\" starts with an opening brace '%c': %.*s",
                          spec->name_size, spec->name, (!in_change ? '{' : '('), c - b + 16, b);
                return gsl_FORMAT;
            }

            err = gsl_check_matching_closing_brace(c + chunk_size + 1, in_change);
            if (err) return err;

            in_field = false;
            in_change = false;
            // in_tag == false
            // in_terminal == false
            c += chunk_size;
            b = c;
            e = b;
            break;
        case '}':
        case ')':
            /* empty field? */
            if (!in_field) {
                if (in_implied_field) {
                    err = gsl_check_implied_field(b, e - b, specs, num_specs, args, &num_args);
                    if (err) return err;

                    in_implied_field = false;
                }

                err = gsl_check_default(rec, !in_change ? GSL_GET_STATE : GSL_CHANGE_STATE, specs, num_specs);
                if (err) return err;

                *total_size = c - rec;
                return gsl_OK;
            }

            assert(in_tag == in_terminal);

            err = gsl_check_matching_closing_brace(c + 1, in_change);
            if (err) return err;

            if (in_terminal) {
                err = gsl_check_field_terminal_value(b, e - b, spec, args, &num_args);
                if (err) return err;

                in_field = false;
                in_change = false;
                in_tag = false;
                in_terminal = false;
                b = c + 1;
                e = b;
                break;
            }

            // Parse a tag after a closing brace '}' / ')' in an inner field.  Means in_tag can be set to true.

            err = gsl_check_field_tag(b, e - b, !in_change ? GSL_GET_STATE : GSL_CHANGE_STATE, specs, num_specs, &spec);
            if (err) return err;

            err = gsl_parse_field_value(b, e - b, spec, c, &chunk_size, &in_terminal);  // TODO(ki.stfu): allow in_terminal parsing
            if (err) return err;

            if (in_terminal) {
                glb_p_log("-- empty terminal val for ATOMIC SPEC \"%.*s\": %.*s",
                        spec->name_size, spec->name, c - b + 16, b);
                return gsl_FORMAT;
            }

            in_field = false;
            in_change = false;
            // in_tag == false
            // in_terminal == false
            break;
        case '[':
            /* starting brace */
            if (!in_field) {
                if (in_implied_field) {
                    name_size = e - b;
                    if (name_size) {
                        err = gsl_check_implied_field(b, name_size,
                                                      specs, num_specs,
                                                      args, &num_args);
                        if (err) return err;
                    }
                    in_implied_field = false;
                }
            }
            else {
                err = check_name_limits(b, e, &name_size);
                if (err) return err;

                if (DEBUG_PARSER_LEVEL_2)
                    glb_p_log("++ BASIC LOOP got tag before square bracket: \"%.*s\" [%zu]",
                            name_size, b, name_size);

                err = gsl_find_spec(b, name_size, GSL_GET_STATE, specs, num_specs, &spec);
                if (err) {
                    glb_p_log("-- no spec found to handle the \"%.*s\" tag: %d",
                            name_size, b, err);
                    return err;
                }

                if (spec->validate) {
                    err = spec->validate(spec->obj,
                                         (const char*)spec->buf, *spec->buf_size,
                                         c, &chunk_size);
                    if (err) {
                        glb_p_log("-- ERR: %d validation of spec \"%s\" failed :(",
                                err, spec->name);
                        return err;
                    }
                }
                c += chunk_size;
                spec->is_completed = true;
                in_field = false;
                b = c;
                e = b;
                break;
            }

            err = gsl_parse_list(c, &chunk_size, specs, num_specs);
            if (err) {
                glb_p_log("-- basic LOOP failed to parse the list area \"%.*s\" :(", 32, c);
                return err;
            }
            c += chunk_size;
            in_field = false;
            in_terminal = false;
            in_implied_field = false;
            b = c + 1;
            e = b;
            break;
        default:
            e = c + 1;
            if (!in_field) {
                if (!in_implied_field) {
                    b = c;
                    in_implied_field = true;
                }
            }
            break;
        }
        c++;
    }

    if (DEBUG_PARSER_LEVEL_1)
        glb_p_log("\n\n--- end of basic PARSING: \"%s\" num specs: %zu [%p]",
                rec, num_specs, specs);

    *total_size = c - rec;
    return gsl_OK;
}

static int gsl_parse_list(const char *rec,
                          size_t *total_size,
                          struct gslTaskSpec *specs,
                          size_t num_specs)
{
    const char *b, *c, *e;
    size_t name_size;

    void *accu = NULL;
    void *item = NULL;

    struct gslTaskSpec *spec = NULL;
    int (*append_item)(void *accu, void *item) = NULL;
    int (*alloc_item)(void *accu, const char *name, size_t name_size, size_t count, void **item) = NULL;

    bool in_list = false;
    bool got_tag = false;
    bool in_item = false;
    size_t chunk_size = 0;
    size_t item_count = 0;
    int err;

    c = rec;
    b = rec;
    e = rec;

    if (DEBUG_PARSER_LEVEL_2)
        glb_p_log(".. start list parsing: \"%.*s\" num specs: %lu [%p]",
                16, rec, (unsigned long)num_specs, specs);

    while (*c) {
        switch (*c) {
        case '\n':
        case '\r':
        case '\t':
        case ' ':
            if (!in_list) break;
            if (!in_item) break;

            if (got_tag) {

                if (spec->is_atomic) {
                    /* get atomic item */
                    err = check_name_limits(b, e, &name_size);
                    if (err) return err;

                    if (DEBUG_PARSER_LEVEL_2)
                        glb_p_log("  == got new item: \"%.*s\"",
                                name_size, b);
                    err = alloc_item(accu, b, name_size, item_count, &item);
                    if (err) return err;

                    item_count++;
                    b = c + 1;
                    e = b;
                    break;
                }
                /* get list item's name */
                err = check_name_limits(b, e, &name_size);
                if (err) return err;
                if (DEBUG_PARSER_LEVEL_2)
                    glb_p_log("  == list got new item: \"%.*s\"",
                            name_size, b);
                err = alloc_item(accu, b, name_size, item_count, &item);
                if (err) {
                    glb_p_log("-- item alloc failed: %d :(", err);
                    return err;
                }
                item_count++;

                /* parse item */
                err = spec->parse(item, c, &chunk_size);
                if (err) {
                    glb_p_log("-- list item parsing failed :(");
                    return err;
                }
                c += chunk_size;

                err = append_item(accu, item);
                if (err) return err;

                in_item = false;
                b = c + 1;
                e = b;
                break;
            }

            err = check_name_limits(b, e, &name_size);
            if (err) return err;

            if (DEBUG_PARSER_LEVEL_2)
                glb_p_log("++ list got tag: \"%.*s\" [%lu]",
                        name_size, b, (unsigned long)name_size);

            err = gsl_find_spec(b, name_size, GSL_GET_STATE, specs, num_specs, &spec);
            if (err) {
                glb_p_log("-- no spec found to handle the \"%.*s\" list tag :(",
                        name_size, b);
                return err;
            }

            if (DEBUG_PARSER_LEVEL_2)
                glb_p_log("++ got list SPEC: \"%s\"",
                        spec->name);

            if (spec->is_atomic) {
                if (!spec->accu) return gsl_FAIL;
                if (!spec->alloc) return gsl_FAIL;
                got_tag = true;
                accu = spec->accu;
                alloc_item = spec->alloc;
                b = c + 1;
                e = b;
                break;
            }

            if (!spec->append) return gsl_FAIL;
            if (!spec->accu) return gsl_FAIL;
            if (!spec->alloc) return gsl_FAIL;
            if (!spec->parse) return gsl_FAIL;

            append_item = spec->append;
            accu = spec->accu;
            alloc_item = spec->alloc;

            got_tag = true;
            b = c + 1;
            e = b;
            break;
        case '{':
            if (!in_list) break;
            if (!got_tag) {
                err = check_name_limits(b, e, &name_size);
                if (err) return err;

                if (DEBUG_PARSER_LEVEL_2)
                    glb_p_log("++ list got tag: \"%.*s\" [%lu]",
                            name_size, b, (unsigned long)name_size);

                err = gsl_find_spec(b, name_size, GSL_GET_STATE, specs, num_specs, &spec);
                if (err) {
                    glb_p_log("-- no spec found to handle the \"%.*s\" list tag :(",
                            name_size, b);
                    return err;
                }

                if (DEBUG_PARSER_LEVEL_2)
                    glb_p_log("++ got list SPEC: \"%s\"", spec->name);

                if (!spec->append) return gsl_FAIL;
                if (!spec->accu) return gsl_FAIL;
                if (!spec->alloc) return gsl_FAIL;
                if (!spec->parse) return gsl_FAIL;

                append_item = spec->append;
                accu = spec->accu;
                alloc_item = spec->alloc;

                got_tag = true;
            }

            /* new list item */
            if (!in_item) {
                in_item = true;
                b = c + 1;
                e = b;
                break;
            }

            b = c + 1;
            e = b;
            break;
        case '}':
            return gsl_FAIL;
        case '[':
            if (in_list) return gsl_FAIL;
            in_list = true;
            b = c + 1;
            e = b;
            break;
        case ']':
            if (!in_list) return gsl_FAIL;

            /* list requires a tag and some items */
            if (!got_tag) return gsl_FAIL;

            *total_size = c - rec;
            return gsl_OK;
        default:
            e = c + 1;

            break;
        }
        c++;
    }

    return gsl_FAIL;
}
