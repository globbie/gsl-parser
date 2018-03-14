#undef NDEBUG  // For developing

#include "gsl-parser.h"
#include "gsl-parser/config.h"
#include "gsl-parser/gsl_log.h"

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
#define DEBUG_PARSER_LEVEL_2 0
#define DEBUG_PARSER_LEVEL_3 0
#define DEBUG_PARSER_LEVEL_4 0
#define DEBUG_PARSER_LEVEL_TMP 1

static gsl_err_t gsl_parse_list(const char *rec,
                                size_t *total_size,
                                struct gslTaskSpec *specs,
                                size_t num_specs);

static gsl_err_t
check_name_limits(const char *b, const char *e, size_t *buf_size)
{
    *buf_size = e - b;
    if (!*buf_size) {
        gsl_log("-- empty name?");
        return make_gsl_err(gsl_FORMAT);
    }
    if (*buf_size > GSL_NAME_SIZE) {
        gsl_log("-- field tag too large: %zu", buf_size);
        return make_gsl_err(gsl_LIMIT);
    }
    return make_gsl_err(gsl_OK);
}

static gsl_err_t
gsl_parse_matching_braces(const char *c,
                          bool in_change,
                          size_t *chunk_size)
{
    const char *b = c;
    size_t brace_count = 1;

    const char open_brace = !in_change ? '{' : '(';
    const char close_brace = !in_change ? '}' : ')';

    for (; *c; c++) {
        if (*c == open_brace)
            brace_count++;
        else if (*c == close_brace) {
            brace_count--;
            if (!brace_count) {
                *chunk_size = c - b;
                return make_gsl_err(gsl_OK);
            }
        }
    }

    gsl_log("-- no matching closing brace '%c' found: \"%.*s\"",
            (!in_change ? '}' : ')'), 16, b);
    return make_gsl_err(gsl_FORMAT);
}

static gsl_err_t
gsl_run_set_size_t(void *obj,
                   const char *val, size_t val_size)
{
    size_t *self = (size_t *)obj;
    char *num_end;
    unsigned long long num;

    assert(val && val_size != 0);

    if (!isdigit(val[0])) {
        gsl_log("-- num size_t doesn't start from a digit: \"%.*s\"",
                (int)val_size, val);
        return make_gsl_err(gsl_FORMAT);
    }

    errno = 0;
    num = strtoull(val, &num_end, GSL_NUM_ENCODE_BASE);  // FIXME(ki.stfu): Null-terminated string is expected
    if (errno == ERANGE && num == ULLONG_MAX) {
        gsl_log("-- num limit reached: %.*s max: %llu",
                (int)val_size, val, ULLONG_MAX);
        return make_gsl_err(gsl_LIMIT);
    }
    else if (errno != 0 && num == 0) {
        gsl_log("-- cannot convert \"%.*s\" to num: %d",
                (int)val_size, val, errno);
        return make_gsl_err(gsl_FORMAT);
    }

    if (val + val_size != num_end) {
        gsl_log("-- not all characters in \"%.*s\" were parsed: \"%.*s\"",
                (int)val_size, val, (int)(num_end - val), val);
        return make_gsl_err(gsl_FORMAT);
    }

    if (ULLONG_MAX > SIZE_MAX && num > SIZE_MAX) {
        gsl_log("-- num size_t limit reached: %llu max: %llu",
                num, (unsigned long long)SIZE_MAX);
        return make_gsl_err(gsl_LIMIT);
    }

    *self = (size_t)num;
    if (DEBUG_PARSER_LEVEL_2)
        gsl_log("++ got num size_t: %zu",
                *self);

    return make_gsl_err(gsl_OK);
}

gsl_err_t
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
    gsl_err_t err;

    if (DEBUG_PARSER_LEVEL_2)
        gsl_log(".. parse num size_t: \"%.*s\"", 16, rec);

    err = gsl_parse_task(rec, total_size, specs, sizeof(specs) / sizeof(struct gslTaskSpec));
    if (err.code) return err;

    return make_gsl_err(gsl_OK);
}

static int
gsl_spec_is_correct(struct gslTaskSpec *spec)
{
    if (DEBUG_PARSER_LEVEL_2)
        gsl_log(".. check spec: \"%.*s\"..", spec->name_size, spec->name);

    // Check the fields are not mutually exclusive (by groups):

    assert(spec->type == GSL_GET_STATE || spec->type == GSL_CHANGE_STATE);

    assert((spec->name != NULL) == (spec->name_size != 0));

    assert(!spec->is_completed);

    if (spec->is_default)
        assert(!spec->is_selector && !spec->is_implied && !spec->is_validator && !spec->is_list && !spec->is_atomic);
    if (spec->is_selector)
        assert(!spec->is_default && !spec->is_validator && !spec->is_list && !spec->is_atomic);
    if (spec->is_implied)
        assert(!spec->is_default && !spec->is_validator && !spec->is_list && !spec->is_atomic);
    if (spec->is_validator)
        assert(!spec->is_default && !spec->is_selector && !spec->is_implied && !spec->is_list && !spec->is_atomic);
    // FIXME(ki.stfu): assert(!spec->is_list);  // TODO(ki.stfu): ?? Remove this field
    //assert(!spec->is_atomic);  // TODO(ki.stfu): ?? Remove this field

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

static gsl_err_t
gsl_spec_buf_copy(struct gslTaskSpec *spec,
                  const char *val, size_t val_size)
{
    if (DEBUG_PARSER_LEVEL_2)
        gsl_log(".. writing val \"%.*s\" [%zu] to buf [max size: %zu]..",
                val_size, val, val_size, spec->max_buf_size);

    if (!val_size) {
        gsl_log("-- empty value :(");
        return make_gsl_err(gsl_FORMAT);
    }
    if (val_size > spec->max_buf_size) {
        gsl_log("-- %.*s: buf limit reached: %zu max: %zu",
                spec->name_size, spec->name, val_size, spec->max_buf_size);
        return make_gsl_err(gsl_LIMIT);
    }

    if (*spec->buf_size) {
        gsl_log("-- %.*s: buf already contains \"%.*s\"",
                spec->name_size, spec->name, spec->buf_size, spec->buf);
        return make_gsl_err(gsl_EXISTS);
    }

    memcpy(spec->buf, val, val_size);
    *spec->buf_size = val_size;

    return make_gsl_err(gsl_OK);
}

static gsl_err_t
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
            return make_gsl_err(gsl_OK);
        }
    }

    if (DEBUG_PARSER_LEVEL_2)
        gsl_log("-- no named spec found for \"%.*s\" of type %s  validator: %p",
                name_size, name,
                spec_type == GSL_GET_STATE ? "GET" : "CHANGE",
                validator_spec);

    if (validator_spec) {
        *out_spec = validator_spec;
        return make_gsl_err(gsl_OK);
    }

    return make_gsl_err(gsl_NO_MATCH);
}

static gsl_err_t
gsl_check_matching_closing_brace(const char *c, bool in_change)
{
    switch (*c) {
    case '}':
        if (!in_change) return make_gsl_err(gsl_OK);
        break;
    case ')':
        if (in_change) return make_gsl_err(gsl_OK);
        break;
    default:
        assert("no closing brace found");
    }

    gsl_log("-- no matching closing brace found: \"%.*s\"",
            16, c);
    return make_gsl_err(gsl_FORMAT);
}

static gsl_err_t
gsl_check_implied_field(const char *val, size_t val_size,
                        struct gslTaskSpec *specs, size_t num_specs)
{
    struct gslTaskSpec *spec;
    struct gslTaskSpec *implied_spec = NULL;
    gsl_err_t err;

    assert(val_size && "implied val is empty");

    if (DEBUG_PARSER_LEVEL_2)
        gsl_log("++ got implied val: \"%.*s\" [%zu]",
                val_size, val, val_size);
    if (val_size > GSL_NAME_SIZE) return make_gsl_err(gsl_LIMIT);

    for (size_t i = 0; i < num_specs; i++) {
        spec = &specs[i];

        if (spec->is_implied) {
            assert(implied_spec == NULL && "implied_spec was already specified");
            implied_spec = spec;
        }
    }

    if (!implied_spec) {
        gsl_log("-- no implied spec found to handle the \"%.*s\" val",
                val_size, val);

        return make_gsl_err(gsl_NO_MATCH);
    }

    if (DEBUG_PARSER_LEVEL_2)
        gsl_log("++ got implied spec: \"%.*s\" buf: %p run: %p!",
                implied_spec->name_size, implied_spec->name, implied_spec->buf, implied_spec->run);

    if (implied_spec->buf) {
        err = gsl_spec_buf_copy(implied_spec, val, val_size);
        if (err.code) return err;

        implied_spec->is_completed = true;
        return make_gsl_err(gsl_OK);
    }

    err = implied_spec->run(implied_spec->obj, val, val_size);
    if (err.code) {
        gsl_log("-- implied func for \"%.*s\" failed: %d :(",
                val_size, val, err);
        return err;
    }

    implied_spec->is_completed = true;
    return make_gsl_err(gsl_OK);
}

static gsl_err_t
gsl_check_field_tag(const char *name,
                    size_t name_size,
                    gsl_task_spec_type type,
                    struct gslTaskSpec *specs,
                    size_t num_specs,
                    struct gslTaskSpec **out_spec)
{
    gsl_err_t err;

    if (!name_size) {
        gsl_log("-- empty field tag?");
        return make_gsl_err(gsl_FORMAT);
    }
    if (name_size > GSL_NAME_SIZE) {
        gsl_log("-- field tag too large: %zu bytes: \"%.*s\"",
                name_size, name_size, name);
        return make_gsl_err(gsl_LIMIT);
    }

    if (DEBUG_PARSER_LEVEL_2)
        gsl_log("++ BASIC LOOP got tag after brace: \"%.*s\" [%zu]",
                name_size, name, name_size);

    err = gsl_find_spec(name, name_size, type, specs, num_specs, out_spec);
    if (err.code) {
        gsl_log("-- no spec found to handle the \"%.*s\" tag: %d",
                name_size, name, err);
        return err;
    }

    if (DEBUG_PARSER_LEVEL_2)
        gsl_log("++ got SPEC: \"%.*s\" (default: %d) (is validator: %d)",
                (*out_spec)->name_size, (*out_spec)->name, (*out_spec)->is_default, (*out_spec)->is_validator);

    return make_gsl_err(gsl_OK);
}

static gsl_err_t
gsl_parse_field_value(const char *name,
                      size_t name_size,
                      struct gslTaskSpec *spec,
                      const char *rec,
                      size_t *total_size,
                      bool *in_terminal)
{
    gsl_err_t err;

    assert(name_size && "name is empty");
    assert(!*in_terminal && "gsl_parse_field_value is called for terminal value");

    if (spec->validate) {
        err = spec->validate(spec->obj, name, name_size, rec, total_size);
        if (err.code) {
            gsl_log("-- ERR: %d validation spec for \"%.*s\" failed :(",
                    err.code, name_size, name);
            return err;
        }

        spec->is_completed = true;
        return make_gsl_err(gsl_OK);
    }

    if (spec->parse) {
        if (DEBUG_PARSER_LEVEL_2)
            gsl_log("\n    >>> further parsing required in \"%.*s\" FROM: \"%.*s\" FUNC: %p",
                    spec->name_size, spec->name, 16, rec, spec->parse);

        err = spec->parse(spec->obj, rec, total_size);
        if (err.code) {
            gsl_log("-- ERR: %d parsing of spec \"%.*s\" failed :(",
                    err.code, spec->name_size, spec->name);
            return err;
        }

        spec->is_completed = true;
        return make_gsl_err(gsl_OK);
    }

    if (DEBUG_PARSER_LEVEL_2)
        gsl_log("== ATOMIC SPEC found: %.*s! no further parsing is required.",
                spec->name_size, spec->name);

    *in_terminal = true;
    // *total_size = 0;  // This actully isn't used.
    return make_gsl_err(gsl_OK);
}

static gsl_err_t
gsl_check_field_terminal_value(const char *val, size_t val_size,
                               struct gslTaskSpec *spec)
{
    gsl_err_t err;

    if (val_size > GSL_NAME_SIZE) {
        gsl_log("-- value too large: %zu bytes: \"%.*s\"",
                val_size, val_size, val);
        return make_gsl_err(gsl_LIMIT);
    }

    if (DEBUG_PARSER_LEVEL_2)
        gsl_log("++ got terminal val: \"%.*s\" [%zu]",
                val_size, val, val_size);

    assert(spec->parse == NULL && spec->validate == NULL && "spec for terminal val has .parse or .validate");

    if (spec->buf) {
        err = gsl_spec_buf_copy(spec, val, val_size);
        if (err.code) return err;

        spec->is_completed = true;
        return make_gsl_err(gsl_OK);
    }

    err = spec->run(spec->obj, val, val_size);
    if (err.code) {
        gsl_log("-- \"%.*s\" func run failed: %d :(",
                spec->name_size, spec->name, err.code);
        return err;
    }

    spec->is_completed = true;
    return make_gsl_err(gsl_OK);
}

static gsl_err_t
gsl_check_default(const char *rec,
                  gsl_task_spec_type type,
                  struct gslTaskSpec *specs,
                  size_t num_specs) {
    struct gslTaskSpec *spec;
    struct gslTaskSpec *default_spec = NULL;
    gsl_err_t err;

    for (size_t i = 0; i < num_specs; ++i) {
        spec = &specs[i];

        if (spec->is_default && spec->type == type) {
            assert(default_spec == NULL && "default_spec was already specified");
            default_spec = spec;
            continue;
        }

        if (!spec->is_selector && spec->is_completed)
            return make_gsl_err(gsl_OK);
    }

    if (!default_spec) {
        gsl_log("-- no default spec found to handle an empty field (ignoring selectors): %.*s",
                16, rec);
        return make_gsl_err(gsl_NO_MATCH);
    }

    err = default_spec->run(default_spec->obj, NULL, 0);
    if (err.code) {
        gsl_log("-- default func run failed: %d :(",
                err.code);
        return err;
    }

    return make_gsl_err(gsl_OK);
}

gsl_err_t gsl_parse_task(const char *rec,
                         size_t *total_size,
                         struct gslTaskSpec *specs,
                         size_t num_specs)
{
    const char *b, *c, *e;
    size_t name_size;

    struct gslTaskSpec *spec;

    bool in_implied_field = false;
    bool in_field = false;
    bool in_change = false;
    bool in_tag = false;
    bool in_terminal = false;

    size_t chunk_size;
    gsl_err_t err;

    c = rec;
    b = rec;
    e = rec;

    if (DEBUG_PARSER_LEVEL_2)
        gsl_log("\n\n*** start basic PARSING: \"%.*s\" num specs: %zu [%p]",
                16, rec, num_specs, specs);

    // Check gslTaskSpec is properly filled
    for (size_t i = 0; i < num_specs; i++)
        assert(gsl_spec_is_correct(&specs[i]));

    while (*c) {
        switch (*c) {
        case '-':
            // FIXME(ki.stfu): fix these conditions
            if (!in_field) {
                e = c + 1;
                break;
            }
            if (in_tag) {
                e = c + 1;
                break;
            }
            err = gsl_parse_matching_braces(c, in_change, &chunk_size);
            if (err.code) return err;
            c += chunk_size;
            in_field = false;
            b = c;
            e = b;
            break;
        case '\n':
        case '\r':
        case '\t':
        case ' ':
            if (!in_field) {
                // Example: rec = "     {name...
                //                 ^^^^^  -- ignore spaces
                break;
            }
            if (in_terminal) {
                // Example: rec = "{name John Smith...
                //                           ^  -- keep spaces in terminal value
                break;
            }

            if (DEBUG_PARSER_LEVEL_2)
                gsl_log("+ whitespace in basic PARSING!");

            // Parse a tag after a first space.  Means in_tag can be set to true.
            // Example: rec = "{name ...
            //                      ^  -- handle a tag

            err = gsl_check_field_tag(b, e - b, !in_change ? GSL_GET_STATE : GSL_CHANGE_STATE, specs, num_specs, &spec);
            if (err.code) return err;

            err = gsl_parse_field_value(b, e - b, spec, c, &chunk_size, &in_terminal);
            if (err.code) return err;

            if (in_terminal) {
                // Parse an atomic value.  Remember that we are in in_tag state.
                // Example: rec = "{name John Smith}"
                //                      ^  -- wait a terminal value
                // in_field == true
                // in_change == true | false
                in_tag = true;
                // in_terminal == true
                b = c + 1;
                e = b;
                break;
            }
            // else {
            //   Example: rec = "{name John Smith}"
            //                        ^^^^^^^^^^^  -- handled by an inner call to .parse() or .validate()
            //                                   ^  -- c + chunk_size
            //        or: rec = "{name {first John} {last Smith}}"
            //                        ^^^^^^^^^^^^^^^^^^^^^^^^^^  -- same way
            //                                                  ^  -- c + chunk_size
            // }

            err = gsl_check_matching_closing_brace(c + chunk_size, in_change);
            if (err.code) return err;

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
            // Starting brace '{' or '('
            if (!in_field) {
                // Example: rec = "...{name John Smith}"
                //                    ^  -- parse a new field
                if (in_implied_field) {
                    // Example: rec = "jsmith {name John Smith}"
                    //                 ^^^^^^  -- but first let's handle an implied field which is pointed by |b| & |e|
                    err = gsl_check_implied_field(b, e - b, specs, num_specs);
                    if (err.code) return err;

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
                // Example: rec = "{name John {Smith}}"
                //                            ^  -- terminal value cannot contain braces
                gsl_log("-- terminal val for ATOMIC SPEC \"%.*s\" has an opening brace '%c': %.*s",
                        spec->name_size, spec->name, (!in_change ? '{' : '('), c - b + 16, b);
                return make_gsl_err(gsl_FORMAT);
            }

            // Parse a tag after an inner field brace '{' or '('.  Means in_tag can be set to true.
            // Example: rec = "{name{...
            //                      ^  -- handle a tag

            err = gsl_check_field_tag(b, e - b, !in_change ? GSL_GET_STATE : GSL_CHANGE_STATE, specs, num_specs, &spec);
            if (err.code) return err;

            err = gsl_parse_field_value(b, e - b, spec, c, &chunk_size, &in_terminal);
            if (err.code) return err;

            if (in_terminal) {
                // Example: rec = "{name{John Smith}}"
                //                      ^  -- terminal value cannot start with an opening brace
                gsl_log("-- terminal val for ATOMIC SPEC \"%.*s\" starts with an opening brace '%c': %.*s",
                        spec->name_size, spec->name, (!in_change ? '{' : '('), c - b + 16, b);
                return make_gsl_err(gsl_FORMAT);
            }
            // else {
            //   Example: rec = "{name{first John} {last Smith}}"
            //                        ^^^^^^^^^^^^^^^^^^^^^^^^^  -- handled by an inner call to .parse() or .validate()
            //                                                 ^  -- c + chunk_size
            // }

            err = gsl_check_matching_closing_brace(c + chunk_size, in_change);
            if (err.code) return err;

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
            // Closing brace '}' or ')'
            if (!in_field) {
                // Example: rec = "... }"
                //                     ^  -- end of parsing
                if (in_implied_field) {
                    // Example: rec = "... jsmith }"
                    //                     ^^^^^^  -- but first let's handle an implied field which is pointed by |b| & |e|
                    err = gsl_check_implied_field(b, e - b, specs, num_specs);
                    if (err.code) return err;

                    in_implied_field = false;
                }

                err = gsl_check_default(rec, !in_change ? GSL_GET_STATE : GSL_CHANGE_STATE, specs, num_specs);
                if (err.code) return err;

                *total_size = c - rec;
                return make_gsl_err(gsl_OK);
            }

            assert(in_tag == in_terminal);

            // Closing brace in a field
            // Example: rec = "{name John Smith}"
            //                                 ^  -- after a terminal value
            //      or: rec = "{name}"
            //                      ^  -- after a tag (i.e. field with an empty value)

            err = gsl_check_matching_closing_brace(c, in_change);
            if (err.code) return err;

            if (in_terminal) {
                // Example: rec = "{name John Smith}"
                //                       ^^^^^^^^^^  -- handle a terminal value
                err = gsl_check_field_terminal_value(b, e - b, spec);
                if (err.code) return err;

                in_field = false;
                in_change = false;
                in_tag = false;
                in_terminal = false;
                b = c + 1;
                e = b;
                break;
            }

            // Parse a tag after a closing brace '}' / ')' in a field.  Means in_tag can be set to false.
            // Example: rec = "{name}"
            //                      ^  -- handle a tag

            err = gsl_check_field_tag(b, e - b, !in_change ? GSL_GET_STATE : GSL_CHANGE_STATE, specs, num_specs, &spec);
            if (err.code) return err;

            err = gsl_parse_field_value(b, e - b, spec, c, &chunk_size, &in_terminal);  // TODO(ki.stfu): allow in_terminal parsing
            if (err.code) return err;

            if (in_terminal) {
                // Example: rec = "{name}"
                //                      ^  -- terminal value is empty
                err = gsl_check_field_terminal_value(c, 0, spec);
                if (err.code) return err;

                // in_field == true
                // in_change == true | false
                // in_tag == false
                in_terminal = false;
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
                        err = gsl_check_implied_field(b, name_size, specs, num_specs);
                        if (err.code) return err;
                    }
                    in_implied_field = false;
                }
            }
            else {
                err = check_name_limits(b, e, &name_size);
                if (err.code) return err;

                if (DEBUG_PARSER_LEVEL_2)
                    gsl_log("++ BASIC LOOP got tag before square bracket: \"%.*s\" [%zu]",
                            name_size, b, name_size);

                err = gsl_find_spec(b, name_size, GSL_GET_STATE, specs, num_specs, &spec);
                if (err.code) {
                    gsl_log("-- no spec found to handle the \"%.*s\" tag: %d",
                            name_size, b, err.code);
                    return err;
                }

                if (spec->validate) {
                    err = spec->validate(spec->obj,
                                         (const char*)spec->buf, *spec->buf_size,
                                         c, &chunk_size);
                    if (err.code) {
                        gsl_log("-- ERR: %d validation of spec \"%s\" failed :(",
                                err.code, spec->name);
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
            if (err.code) {
                gsl_log("-- basic LOOP failed to parse the list area \"%.*s\" :(", 32, c);
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
        gsl_log("\n\n--- end of basic PARSING: \"%s\" num specs: %zu [%p]",
                rec, num_specs, specs);

    *total_size = c - rec;
    return make_gsl_err(gsl_OK);
}

static gsl_err_t
gsl_parse_list(const char *rec,
               size_t *total_size,
               struct gslTaskSpec *specs,
               size_t num_specs)
{
    const char *b, *c, *e;
    size_t name_size;

    void *accu = NULL;
    void *item = NULL;

    struct gslTaskSpec *spec = NULL;
    gsl_err_t (*append_item)(void *accu, void *item) = NULL;
    gsl_err_t (*alloc_item)(void *accu, const char *name, size_t name_size, size_t count, void **item) = NULL;

    bool in_list = false;
    bool got_tag = false;
    bool in_item = false;
    size_t chunk_size = 0;
    size_t item_count = 0;
    gsl_err_t err;

    c = rec;
    b = rec;
    e = rec;

    if (DEBUG_PARSER_LEVEL_2)
        gsl_log(".. start list parsing: \"%.*s\" num specs: %lu [%p]",
                16, rec, (unsigned long)num_specs, specs);

    while (*c) {
        switch (*c) {
        case '\n':
        case '\r':
        case '\t':
        case ' ':
            if (!in_list) break;

            /*if (!in_item) {
		break;
		}*/

            if (got_tag) {
                if (spec->is_atomic) {
                    /* get atomic item */
                    err = check_name_limits(b, e, &name_size);
                    if (err.code) return err;

                    if (DEBUG_PARSER_LEVEL_2)
                        gsl_log("  == got new atomic item: \"%.*s\"",
                                name_size, b);
                    err = alloc_item(accu, b, name_size, item_count, &item);
                    if (err.code) return err;

                    item_count++;
                    b = c + 1;
                    e = b;
                    break;
                }
                /* get list item's name */
                err = check_name_limits(b, e, &name_size);
                if (err.code) return err;
                if (DEBUG_PARSER_LEVEL_2)
                    gsl_log("  == list got new item: \"%.*s\"",
                            name_size, b);
                err = alloc_item(accu, b, name_size, item_count, &item);
                if (err.code) {
                    gsl_log("-- item alloc failed: %d :(", err.code);
                    return err;
                }
                item_count++;

                /* parse item */
                err = spec->parse(item, c, &chunk_size);
                if (err.code) {
                    gsl_log("-- list item parsing failed :(");
                    return err;
                }
                c += chunk_size;

                err = append_item(accu, item);
                if (err.code) return err;

                in_item = false;
                b = c + 1;
                e = b;
                break;
            }

	    

            err = check_name_limits(b, e, &name_size);
            if (err.code) return err;

            if (DEBUG_PARSER_LEVEL_2)
                gsl_log("++ list got tag: \"%.*s\" [%lu]",
                        name_size, b, (unsigned long)name_size);

            err = gsl_find_spec(b, name_size, GSL_GET_STATE, specs, num_specs, &spec);
            if (err.code) {
                gsl_log("-- no spec found to handle the \"%.*s\" list tag :(",
                        name_size, b);
                return err;
            }

            if (DEBUG_PARSER_LEVEL_2)
                gsl_log("++ got list SPEC: \"%s\"",
                        spec->name);

            if (spec->is_atomic) {
                if (!spec->accu) return make_gsl_err(gsl_FAIL);
                if (!spec->alloc) return make_gsl_err(gsl_FAIL);
                got_tag = true;
                accu = spec->accu;
                alloc_item = spec->alloc;
                b = c + 1;
                e = b;
                break;
            }

            if (!spec->append) return make_gsl_err(gsl_FAIL);
            if (!spec->accu) return make_gsl_err(gsl_FAIL);
            if (!spec->alloc) return make_gsl_err(gsl_FAIL);
            if (!spec->parse) return make_gsl_err(gsl_FAIL);

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
                if (err.code) return err;

                if (DEBUG_PARSER_LEVEL_2)
                    gsl_log("++ list got tag: \"%.*s\" [%lu]",
                            name_size, b, (unsigned long)name_size);

                err = gsl_find_spec(b, name_size, GSL_GET_STATE, specs, num_specs, &spec);
                if (err.code) {
                    gsl_log("-- no spec found to handle the \"%.*s\" list tag :(",
                            name_size, b);
                    return err;
                }

                if (DEBUG_PARSER_LEVEL_2)
                    gsl_log("++ got list SPEC: \"%s\"", spec->name);

                if (!spec->append) return make_gsl_err(gsl_FAIL);
                if (!spec->accu) return make_gsl_err(gsl_FAIL);
                if (!spec->alloc) return make_gsl_err(gsl_FAIL);
                if (!spec->parse) return make_gsl_err(gsl_FAIL);

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
            return make_gsl_err(gsl_FAIL);
        case '[':
            if (in_list) return make_gsl_err(gsl_FAIL);
            in_list = true;
            b = c + 1;
            e = b;
            break;
        case ']':
            if (!in_list) return make_gsl_err(gsl_FAIL);

            /* list requires a tag and some items */
            if (!got_tag) return make_gsl_err(gsl_FAIL);

            if (got_tag) {
                if (spec->is_atomic) {
                    /* get atomic item */
                    err = check_name_limits(b, e, &name_size);
                    if (err.code) return err;

                    if (DEBUG_PARSER_LEVEL_2)
                        gsl_log("  == got new atomic item: \"%.*s\"",
                                name_size, b);
                    err = alloc_item(accu, b, name_size, item_count, &item);
                    if (err.code) return err;

                    item_count++;
                    b = c + 1;
                    e = b;
                }
            }
	    
            *total_size = c - rec;
            return make_gsl_err(gsl_OK);
        default:
            e = c + 1;

            break;
        }
        c++;
    }

    return make_gsl_err(gsl_FAIL);
}
