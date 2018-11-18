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
#define DEBUG_PARSER_LEVEL_TMP 0

#define uninitialized_var(x) x = x

static bool
gsl_check_floating_boundary(char repeatee, size_t count,
                            char end_marker,
                            const char *rec,
                            size_t *total_size)
{
    assert(*rec == repeatee && count != 0);

    const char *c = rec;
    do {
        c++;
    } while (*c == repeatee);

    *total_size = c - rec;
    return count == (size_t)(c - rec) && *c == end_marker;
}

gsl_err_t
gsl_run_set_size_t(void *obj,
                   const char *val, size_t val_size)
{
    size_t *self = (size_t *)obj;
    char *num_end;
    unsigned long long num;

    assert(val && val_size != 0);

    if (!isdigit(val[0])) {
        if (DEBUG_PARSER_LEVEL_1)
            gsl_log("-- num size_t doesn't start from a digit: \"%.*s\"",
                    (int)val_size, val);
        return make_gsl_err(gsl_FORMAT);
    }

    errno = 0;
    num = strtoull(val, &num_end, GSL_NUM_ENCODE_BASE);  // FIXME(k15tfu): Null-terminated string is expected
    if (errno == ERANGE && num == ULLONG_MAX) {
        if (DEBUG_PARSER_LEVEL_1)
            gsl_log("-- num limit reached: %.*s max: %llu",
                    (int)val_size, val, ULLONG_MAX);
        return make_gsl_err(gsl_LIMIT);
    }
    else if (errno != 0 && num == 0) {
        if (DEBUG_PARSER_LEVEL_1)
            gsl_log("-- cannot convert \"%.*s\" to num: %d",
                    (int)val_size, val, errno);
        return make_gsl_err(gsl_FORMAT);
    }

    if (val + val_size != num_end) {
        if (DEBUG_PARSER_LEVEL_1)
            gsl_log("-- not all characters in \"%.*s\" were parsed: \"%.*s\"",
                    (int)val_size, val, (int)(num_end - val), val);
        return make_gsl_err(gsl_FORMAT);
    }

    if (ULLONG_MAX > SIZE_MAX && num > SIZE_MAX) {
        if (DEBUG_PARSER_LEVEL_1)
            gsl_log("-- num size_t limit reached: %llu max: %llu",
                    num, (unsigned long long)SIZE_MAX);
        return make_gsl_err(gsl_LIMIT);
    }

    *self = (size_t)num;
    if (DEBUG_PARSER_LEVEL_3)
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

    if (DEBUG_PARSER_LEVEL_4)
        gsl_log(".. parse num size_t: \"%.*s\"", 16, rec);

    err = gsl_parse_task(rec, total_size, specs, sizeof specs / sizeof specs[0]);
    if (err.code) return err;

    return make_gsl_err(gsl_OK);
}

static int
gsl_spec_is_correct(struct gslTaskSpec *spec)
{
    if (DEBUG_PARSER_LEVEL_4)
        gsl_log(".. check spec: \"%.*s\"..", spec->name_size, spec->name);

    // Check the fields are not mutually exclusive (by groups):

    assert(spec->type == GSL_GET_STATE || spec->type == GSL_GET_ARRAY_STATE ||
           spec->type == GSL_SET_STATE || spec->type == GSL_SET_ARRAY_STATE);

    assert((spec->name != NULL) == (spec->name_size != 0));

    assert(!spec->is_completed);

    if (spec->is_default)
        assert(!spec->is_selector && !spec->is_implied && !spec->is_list_item);
    if (spec->is_selector)
        assert(!spec->is_default && !spec->is_list_item);
    if (spec->is_implied)
        assert(!spec->is_default && !spec->is_list_item);
    if (spec->is_list_item)
        assert(!spec->is_default && !spec->is_selector && !spec->is_implied);

    assert((spec->buf != NULL) == (spec->buf_size != NULL));
    assert((spec->buf != NULL) == (spec->max_buf_size != 0));
    if (spec->buf)
        assert(*spec->buf_size == 0);

    // ?? assert(spec->obj == NULL);

    if (spec->buf)
        assert(spec->run == NULL && spec->parse == NULL && spec->validate == NULL);
    if (spec->parse)
        assert(spec->buf == NULL && spec->run == NULL && spec->validate == NULL);
    if (spec->validate)
        assert(spec->buf == NULL && spec->run == NULL && spec->parse == NULL);
    if (spec->run)
        assert(spec->buf == NULL && spec->parse == NULL && spec->validate == NULL);

    // Check that they are not mutually exclusive (in general):

    if (spec->type == GSL_SET_STATE) {
        assert(!spec->is_default && (!spec->is_implied || spec->name != NULL) && !spec->is_list_item);
        // ?? assert(spec->name != NULL);
    } else if (spec->type == GSL_GET_ARRAY_STATE || spec->type == GSL_SET_ARRAY_STATE) {
        assert(!spec->is_default && !spec->is_implied && !spec->is_list_item);
        assert(spec->name != NULL || spec->validate != NULL);  // FIXME(k15tfu)
        assert(spec->obj != NULL);
        assert(spec->parse != NULL || spec->validate != NULL);
    }

    if (spec->name) {
        // |spec->type| can be set
        assert(!spec->is_default && !spec->is_list_item);
        assert(spec->validate == NULL);
    }

    if (spec->is_default) {
        assert(spec->type == 0);  // type is useless for default_spec
        assert(spec->name == NULL);
        assert(spec->obj != NULL);
        assert(spec->run != NULL);
    }

    if (spec->is_implied) {
        assert(spec->type == 0 || spec->name != NULL);
        // |spec->name| can be set
        assert(spec->obj != NULL || spec->buf != NULL);
        assert(spec->run != NULL || spec->buf != NULL);
    }

    if (spec->is_list_item) {
        assert(spec->type == 0);  // type is useless for list items
        assert(spec->name == NULL);
        assert(spec->obj != NULL);
        assert(spec->buf == NULL);
        assert(spec->validate == NULL);
        assert(spec->run != NULL || spec->parse != NULL);
    }

    assert(spec->obj != NULL || spec->buf != NULL);

    if (spec->buf) {
        // |spec->type| can be set (depends on |spec->name|)
        assert(!spec->is_default && !spec->is_list_item);
        assert(spec->name != NULL || spec->is_implied);
        assert(spec->obj == NULL);
    }

    if (spec->run) {
        // |spec->type| can be set (depends on |spec->name|)
        assert(spec->name != NULL || spec->is_default || spec->is_implied || spec->is_list_item);
        assert(spec->obj != NULL);
    }

    if (spec->parse) {
        // |spec->type| can be set
        assert(!spec->is_default && !spec->is_implied);
        assert(spec->name != NULL || spec->is_list_item);
        assert(spec->obj != NULL);
    }

    if (spec->validate) {
        // |spec->type| can be set
        assert(spec->name == NULL);
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
    assert(spec->name != NULL || spec->is_default || spec->is_implied || spec->is_list_item || spec->validate != NULL);
    assert(spec->buf != NULL || spec->run != NULL || spec->parse != NULL || spec->validate != NULL);

    return 1;
}

static gsl_err_t
gsl_spec_buf_copy(struct gslTaskSpec *spec,
                  const char *val, size_t val_size)
{
    if (DEBUG_PARSER_LEVEL_4)
        gsl_log(".. writing val \"%.*s\" [%zu] to buf [max size: %zu]..",
                val_size, val, val_size, spec->max_buf_size);

    if (!val_size) {
        if (DEBUG_PARSER_LEVEL_1)
            gsl_log("-- empty value :(");
        return make_gsl_err(gsl_FORMAT);
    }
    if (val_size > spec->max_buf_size) {
        if (DEBUG_PARSER_LEVEL_1)
            gsl_log("-- %.*s: buf limit reached: %zu max: %zu",
                    spec->name_size, spec->name, val_size, spec->max_buf_size);
        return make_gsl_err(gsl_LIMIT);
    }

    if (*spec->buf_size) {
        if (DEBUG_PARSER_LEVEL_1)
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

        if (spec->validate) {
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
                spec_type == GSL_GET_STATE ? "GET" : spec_type == GSL_GET_ARRAY_STATE ?
                    "GET_ARRAY" : spec_type == GSL_SET_STATE ? "SET" : "SET_ARRAY",
                validator_spec);

    if (validator_spec) {
        *out_spec = validator_spec;
        return make_gsl_err(gsl_OK);
    }

    return make_gsl_err(gsl_NO_MATCH);
}

static gsl_err_t
gsl_check_matching_closing_brace(const char *c, gsl_task_spec_type in_field_type)
{
    switch (*c) {
    case '}':
        if (in_field_type == GSL_GET_STATE || in_field_type == GSL_SET_STATE)
            return make_gsl_err(gsl_OK);
        break;
    case ']':
        if (in_field_type == GSL_GET_ARRAY_STATE || in_field_type == GSL_SET_ARRAY_STATE)
            return make_gsl_err(gsl_OK);
        break;
    default:
        assert(0 && "no closing brace found");
    case '\0': // TODO(k15tfu): remove this case and return gsl_FORMAT at the end of gsl_parse_task()
        // Avoid an assert() for \0 symbol.
        break;
    }

    if (DEBUG_PARSER_LEVEL_1)
        gsl_log("-- no matching closing brace '%c' found: \"%.*s\"",
                (in_field_type == GSL_GET_STATE || in_field_type == GSL_SET_STATE ? '}' : ']'), 16, c);
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

    if (DEBUG_PARSER_LEVEL_3)
        gsl_log("++ got implied val: \"%.*s\" [%zu]",
                val_size, val, val_size);

    for (size_t i = 0; i < num_specs; i++) {
        spec = &specs[i];

        if (spec->is_implied) {
            assert(implied_spec == NULL && "implied_spec was already specified");
            implied_spec = spec;
        }
    }

    if (!implied_spec) {
        if (DEBUG_PARSER_LEVEL_1)
            gsl_log("-- no implied spec found to handle the \"%.*s\" val",
                    val_size, val);

        return make_gsl_err(gsl_NO_MATCH);
    }

    if (DEBUG_PARSER_LEVEL_3)
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
        if (DEBUG_PARSER_LEVEL_1)
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
        if (DEBUG_PARSER_LEVEL_1)
            gsl_log("-- empty field tag?");
        return make_gsl_err(gsl_FORMAT);
    }

    if (DEBUG_PARSER_LEVEL_3)
        gsl_log("++ BASIC LOOP got tag after brace: \"%.*s\" [%zu]",
                name_size, name, name_size);

    err = gsl_find_spec(name, name_size, type, specs, num_specs, out_spec);
    if (err.code) {
        if (DEBUG_PARSER_LEVEL_1)
            gsl_log("-- no spec found to handle the \"%.*s\" tag: %d",
                    name_size, name, err);
        return err;
    }

    if (DEBUG_PARSER_LEVEL_3)
        gsl_log("++ got SPEC: \"%.*s\" (default: %d) (is validator: %d)",
                (*out_spec)->name_size, (*out_spec)->name, (*out_spec)->is_default, (*out_spec)->validate != NULL);

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
            if (DEBUG_PARSER_LEVEL_2)
                gsl_log("-- ERR: %d validation spec for \"%.*s\" failed :(",
                        err.code, name_size, name);
            return err;
        }

        spec->is_completed = true;
        return make_gsl_err(gsl_OK);
    }

    if (spec->parse) {
        if (DEBUG_PARSER_LEVEL_4)
            gsl_log("\n    >>> further parsing required in \"%.*s\" FROM: \"%.*s\" FUNC: %p",
                    spec->name_size, spec->name, 16, rec, spec->parse);

        err = spec->parse(spec->obj, rec, total_size);
        if (err.code) {
            if (DEBUG_PARSER_LEVEL_2)
                gsl_log("-- ERR: %d parsing of spec \"%.*s\" failed :(",
                        err.code, spec->name_size, spec->name);
            return err;
        }

        spec->is_completed = true;
        return make_gsl_err(gsl_OK);
    }

    if (DEBUG_PARSER_LEVEL_4)
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

    if (DEBUG_PARSER_LEVEL_3)
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
        if (DEBUG_PARSER_LEVEL_1)
            gsl_log("-- \"%.*s\" func run failed: %d :(",
                    spec->name_size, spec->name, err.code);
        return err;
    }

    spec->is_completed = true;
    return make_gsl_err(gsl_OK);
}

static gsl_err_t
gsl_check_default(const char *rec,
                  struct gslTaskSpec *specs,
                  size_t num_specs) {
    struct gslTaskSpec *spec;
    struct gslTaskSpec *default_spec = NULL;
    gsl_err_t err;

    for (size_t i = 0; i < num_specs; ++i) {
        spec = &specs[i];

        if (spec->is_default) {
            assert(default_spec == NULL && "default_spec was already specified");
            default_spec = spec;
            continue;
        }

        if (!spec->is_selector && spec->is_completed)
            return make_gsl_err(gsl_OK);
    }

    if (!default_spec) {
        if (DEBUG_PARSER_LEVEL_1)
            gsl_log("-- no default spec found to handle an empty field (ignoring selectors): %.*s",
                    16, rec);
        return make_gsl_err(gsl_NO_MATCH);
    }

    err = default_spec->run(default_spec->obj, NULL, 0);
    if (err.code) {
        if (DEBUG_PARSER_LEVEL_1)
            gsl_log("-- default func run failed: %d :(",
                    err.code);
        return err;
    }

    return make_gsl_err(gsl_OK);
}

static gsl_err_t
gsl_parse_comment(gsl_task_spec_type in_field_type,
                  const char *rec,
                  size_t *total_size)
{
    const char *c;
    const char closing_brace = in_field_type == GSL_GET_STATE || in_field_type == GSL_SET_STATE ? '}' : ']';

    size_t dash_count = 0;
    for (c = rec; *c == '-'; c++)
        dash_count++;

    size_t chunk_size;
    gsl_err_t err;

    for (; *c; c++) {
        if (*c != '-') continue;

        err.code = !gsl_check_floating_boundary('-', dash_count, closing_brace, c, &chunk_size);
        if (err.code) {
            c += chunk_size - 1;
            continue;
        }

        // in_comment = false;
        c += chunk_size;

        *total_size = c - rec;
        return make_gsl_err(gsl_OK);
    }

    if (DEBUG_PARSER_LEVEL_1)
        gsl_log("-- no matching closing sequence -%zutimes %c found: \"%.*s\"",
                dash_count, (in_field_type == GSL_GET_STATE || in_field_type == GSL_SET_STATE ? '}' : ']'), 16, rec);
    return make_gsl_err(gsl_FORMAT);
}

gsl_err_t gsl_parse_task(const char *rec,
                         size_t *total_size,
                         struct gslTaskSpec *specs,
                         size_t num_specs)
{
    const char *b, *c, *e;

    struct gslTaskSpec *uninitialized_var(spec);

    bool in_implied_field = false;
    bool in_field = false;
    gsl_task_spec_type in_field_type = -1;
    bool in_tag = false;
    bool in_terminal = false;

    size_t chunk_size;
    gsl_err_t err;

    c = rec;
    b = NULL;
    e = NULL;

    if (DEBUG_PARSER_LEVEL_4)
        gsl_log("\n\n*** start basic PARSING: \"%.*s\" num specs: %zu [%p]",
                16, rec, num_specs, specs);

    // Check gslTaskSpec is properly filled
    for (size_t i = 0; i < num_specs; i++)
        assert(gsl_spec_is_correct(&specs[i]));

    while (*c) {
        switch (*c) {
        case '!':
            if (!in_field) {
                // Example: rec = "!! {name...
                //                 ^^  -- keep exclamation marks in implied field
                //      or: rec = "j smith! {name...
                //                        ^  -- same way
                goto default_case;
            }
            if (in_terminal) {
                // Example: rec = "{name John! Smith...
                //                           ^  -- keep exclamation marks in terminal value
                goto default_case;
            }

            // Exclamation mark before a tag means switching to GSL_SET_STATE / GSL_SET_ARRAY_STATE.

            if (b != c) {
                // Example: rec = "{n!ame John Smith...
                //                   ^  -- nothing interesting, go to the default case
                goto default_case;
            }

            // Example: rec = "{!name John Smith...
            //                  ^  -- switch to set state
            // in_field = true
            in_field_type = in_field_type == GSL_GET_STATE ? GSL_SET_STATE : GSL_SET_ARRAY_STATE;
            // in_tag == false
            // in_terminal == false
            b = c + 1;
            e = b;
            break;
        case '\n':
        case '\r':
        case '\t':
        case ' ':
            if (!in_field) {
                // Example: rec = "     {name...
                //                 ^^^^^  -- ignore spaces
                //      or: rec = "j smith  {name...
                //                  ^     ^^  -- keep spaces in implied field, and ignore them after it
                break;
            }
            if (in_terminal) {
                // Example: rec = "{name   John Smith...
                //                       ^^    ^  -- keep spaces in terminal value, and ignore them before/after it

                if (b == c) {
                    // Example: rec = "{name   John Smith...
                    //                       ^^  -- ignore spaces
                    b = c + 1;
                    e = b;
                }

                break;
            }

            if (DEBUG_PARSER_LEVEL_4)
                gsl_log("+ whitespace in basic PARSING!");

            // Parse a tag after a first space.  Means in_tag can be set to true.
            // Example: rec = "{name ...
            //                      ^  -- handle a tag

            err = gsl_check_field_tag(b, e - b, in_field_type, specs, num_specs, &spec);
            if (err.code) return *total_size = c - rec, err;

            err = gsl_parse_field_value(b, e - b, spec, c, &chunk_size, &in_terminal);
            if (err.code) return *total_size = c + chunk_size - rec, err;

            if (in_terminal) {
                // Parse an atomic value.  Remember that we are in in_tag state.
                // Example: rec = "{name John Smith}"
                //                      ^  -- wait a terminal value
                // in_field == true
                // in_field_type == GSL_GET_STATE | GSL_SET_STATE
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

            err = gsl_check_matching_closing_brace(c + chunk_size, in_field_type);
            if (err.code) return *total_size = c + chunk_size - rec, err;

            in_field = false;
            in_field_type = -1;
            // in_tag == false
            // in_terminal == false
            c += chunk_size;
            break;
        case '{':
        case '[':
            // Starting brace '{' or '['.
            if (!in_field) {
                // Example: rec = "...{name John Smith}"
                //                    ^  -- parse a new field
                //      or: rec = "...[!groups jsmith...]"
                //                    ^  -- same way
                if (in_implied_field) {
                    // Example: rec = "jsmith {name John Smith}"
                    //                 ^^^^^^  -- but first let's handle an implied field which is pointed by |b| & |e|
                    //      or: rec = "jsmith [!groups jsmith...]"
                    //                 ^^^^^^  -- same way
                    err = gsl_check_implied_field(b, e - b, specs, num_specs);
                    if (err.code) return *total_size = c - rec, err;

                    in_implied_field = false;
                }

                in_field = true;
                in_field_type = *c == '{' ? GSL_GET_STATE : GSL_GET_ARRAY_STATE;
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
                //      or: rec = "{name John [Smith]}"
                //                            ^  -- same way
                if (DEBUG_PARSER_LEVEL_1)
                    gsl_log("-- terminal val for ATOMIC SPEC \"%.*s\" has an opening brace '%c': %.*s",
                            spec->name_size, spec->name, *c, c - b + 16, b);
                *total_size = c - rec;
                return make_gsl_err(gsl_FORMAT);
            }

            // Parse a tag after an inner field brace '{' or '['.  Means in_tag can be set to true.
            // Example: rec = "{name{...
            //                      ^  -- handle a tag
            //      or: rec = "{name[...
            //                      ^  -- same way
            //      or: rec = "[groups{...
            //                        ^  -- same way

            err = gsl_check_field_tag(b, e - b, in_field_type, specs, num_specs, &spec);
            if (err.code) return *total_size = c - rec, err;

            err = gsl_parse_field_value(b, e - b, spec, c, &chunk_size, &in_terminal);
            if (err.code) return *total_size = c + chunk_size - rec, err;

            if (in_terminal) {
                // Example: rec = "{name{John Smith}}"
                //                      ^  -- terminal value cannot start with an opening brace
                // Example: rec = "{name[John Smith]}"
                //                      ^  -- same way
                if (DEBUG_PARSER_LEVEL_1)
                    gsl_log("-- terminal val for ATOMIC SPEC \"%.*s\" starts with an opening brace '%c': %.*s",
                            spec->name_size, spec->name, *c, c - b + 16, b);
                *total_size = c - rec;  // chunk_size should be 0
                return make_gsl_err(gsl_FORMAT);
            }
            // else {
            //   Example: rec = "{name{first John} {last Smith}}"
            //                        ^^^^^^^^^^^^^^^^^^^^^^^^^  -- handled by an inner call to .parse() or .validate()
            //                                                 ^  -- c + chunk_size
            //        or: rec = "{name[first_last John Smith]}"
            //                        ^^^^^^^^^^^^^^^^^^^^^^^  -- same way
            //                                               ^  -- c + chunk_size
            // }

            err = gsl_check_matching_closing_brace(c + chunk_size, in_field_type);
            if (err.code) return *total_size = c + chunk_size - rec, err;

            in_field = false;
            in_field_type = -1;
            // in_tag == false
            // in_terminal == false
            c += chunk_size;
            break;
        case '}':
            // Closing brace '}'.
            if (!in_field) {
                // Example: rec = "... }"
                //                     ^  -- end of parsing
                if (in_implied_field) {
                    // Example: rec = "... jsmith }"
                    //                     ^^^^^^  -- but first let's handle an implied field which is pointed by |b| & |e|
                    err = gsl_check_implied_field(b, e - b, specs, num_specs);
                    if (err.code) return *total_size = c - rec, err;

                    in_implied_field = false;
                }

                err = gsl_check_default(rec, specs, num_specs);
                if (err.code) return *total_size = c - rec, err;

                *total_size = c - rec;
                return make_gsl_err(gsl_OK);
            }

            assert(in_tag == in_terminal);

            // Closing brace in a field
            // Example: rec = "{name John Smith}"
            //                                 ^  -- after a terminal value
            //      or: rec = "{name}"
            //                      ^  -- after a tag (i.e. field with an empty value)

            err = gsl_check_matching_closing_brace(c, in_field_type);
            if (err.code) return *total_size = c - rec, err;

            if (in_terminal) {
                // Example: rec = "{name John Smith}"
                //                       ^^^^^^^^^^  -- handle a terminal value
                err = gsl_check_field_terminal_value(b, e - b, spec);
                if (err.code) return *total_size = c - rec, err;

                in_field = false;
                in_field_type = -1;
                in_tag = false;
                in_terminal = false;
                break;
            }

            // Parse a tag after a closing brace '}' in a field.  Means in_tag can be set to true.
            // Example: rec = "{name}"
            //                      ^  -- handle a tag

            err = gsl_check_field_tag(b, e - b, in_field_type, specs, num_specs, &spec);
            if (err.code) return *total_size = c - rec, err;

            err = gsl_parse_field_value(b, e - b, spec, c, &chunk_size, &in_terminal);  // TODO(k15tfu): allow in_terminal parsing
            if (err.code) return *total_size = c + chunk_size - rec, err;
            // ?? assert(chunk_size == 0);

            if (in_terminal) {
                // Example: rec = "{name}"
                //                      ^  -- terminal value is empty
                err = gsl_check_field_terminal_value(c, 0, spec);
                if (err.code) return *total_size = c - rec, err;  // chunk_size should be 0

                // in_field == true
                // in_field_type == GSL_GET_STATE | GSL_SET_STATE
                // in_tag == false
                in_terminal = false;
            }

            in_field = false;
            in_field_type = -1;
            // in_tag == false
            // in_terminal == false
            break;
        case ']':
            // Closing brace ']'
            if (!in_field) {
                // Example: rec = "... ]"
                //                     ^  -- end of parsing
                if (in_implied_field) {
                    // Example: rec = "... jsmith ]"
                    //                     ^^^^^^  -- implied field is not used in lists
                    *total_size = c - rec;
                    return make_gsl_err(gsl_FORMAT);
                }

                // Example: rec = "... {gid jsmith}]"
                //                                 ^  -- end of parsing
                *total_size = c - rec;
                return make_gsl_err(gsl_OK);
            }

            assert(in_tag == in_terminal);

            // Closing brace in a field
            // Example: rec = "[groups]
            //                        ^  -- after a tag (i.e. field with an empty value)

            err = gsl_check_matching_closing_brace(c, in_field_type);
            if (err.code) return *total_size = c - rec, err;

            // terminal value is not used with lists
            assert(!in_terminal);

            // Parse a tag after a closing brace ']' in a field.  Means in_tag can be set to true.
            // Example: rec = "[groups]"
            //                        ^  -- handle a tag

            err = gsl_check_field_tag(b, e - b, in_field_type, specs, num_specs, &spec);
            if (err.code) return *total_size = c - rec, err;

            err = gsl_parse_field_value(b, e - b, spec, c, &chunk_size, &in_terminal);  // TODO(k15tfu): allow in_terminal parsing
            if (err.code) return *total_size = c + chunk_size - rec, err;
            // ?? assert(chunk_size == 0);

            // terminal value is not used with lists
            assert(!in_terminal);

            in_field = false;
            in_field_type = -1;
            // in_tag == false
            // in_terminal == false
            break;
        case '-':
            if (in_field && !in_tag && b == c) {
                // Example: rec = "...{-name John Smith-}}"
                //                     ^  -- ignore the commented out field
                err = gsl_parse_comment(in_field_type, c, &chunk_size);
                if (err.code) return *total_size = c + chunk_size - rec, err;

                in_field = false;
                in_field_type = -1;
                // in_tag == false
                // in_terminal == false
                c += chunk_size;
                break;
            }

            // FALLTHROUGH
        default:
default_case:
            // Example: rec = " <ch> jsmith {name John Smith}}"
            //                  ^^^^ ^^^^^^  ^^^^ ^^^^ ^^^^^  -- handle all non-special characters

            if (!in_field && !in_implied_field) {
                // Example: rec = " <ch>  jsmith {name John Smith}}"
                //                  ^^^^  -- start recording an implied field
                b = c;
                in_implied_field = true;
            }

            // Example: rec = " <ch> jsmith {name John Smith}}"
            //                  ^^^^ ^^^^^^  ^^^^ ^^^^ ^^^^^  -- move the end pointer
            e = c + 1;
            break;
        }
        c++;
    }

    if (DEBUG_PARSER_LEVEL_4)
        gsl_log("\n\n--- end of basic PARSING: \"%s\" num specs: %zu [%p]",
                rec, num_specs, specs);

    *total_size = c - rec;
    return make_gsl_err(gsl_OK);
}

gsl_err_t
gsl_parse_array(void *obj,
                const char *rec,
                size_t *total_size)
{
    struct gslTaskSpec *spec = (struct gslTaskSpec *)obj;

    assert(spec->type == 0);  // type is useless for list items
    assert(spec->name == NULL);
    assert(spec->run != NULL || spec->parse != NULL);
    assert(gsl_spec_is_correct(spec));

    const char *b, *c, *e;

    const bool is_atomic = spec->parse == NULL;
    bool in_item = false;

    size_t chunk_size;
    gsl_err_t err;

    c = rec;
    b = rec;
    e = rec;

    while (*c) {
        switch (*c) {
        case '-':
        case '}':
        case '[':
            *total_size = c - rec;
            return make_gsl_err(gsl_FORMAT);
        case '\n':
        case '\r':
        case '\t':
        case ' ':
            if (!in_item) {
                // Example: rec = "     jsmith...
                //                 ^^^^^  -- ignore spaces beetween atomic elements
                //      or: rec = "     {user...
                //                 ^^^^^  -- ignore spaces between non-atomic elements
                break;
            }

            assert(is_atomic);  // |is_item| is used only with atomic elements

            if (DEBUG_PARSER_LEVEL_3)
                gsl_log("  == got new item: \"%.*s\"",
                        (int)(e - b), b);

            err = spec->run(spec->obj, b, e - b);
            if (err.code) return *total_size = c - rec, err;

            in_item = false;
            b = c + 1;
            e = b;
            break;
        case '{':
            if (is_atomic) {
                *total_size = c - rec;
                return make_gsl_err(gsl_FORMAT);
            }

            // Parse a non-atomic element after an element brace '{'.  Means in_item can be set to true.
            // Example: rec = "{user...
            //                 ^  -- allocate an element

            err = spec->parse(spec->obj, c + 1, &chunk_size);
            if (err.code) return *total_size = c + chunk_size - rec, err;

            // Example: rec = "{user Sam}]
            //                 ^^^^^^^^^^  -- handled by an inner call to .parse()
            //                           ^  -- c + chunk_size

            // in_item == false
            c += chunk_size + 1;
            b = c;
            e = b;
            break;
        case ']':
            if (in_item) {
                // Example: rec = "... jsmith]
                //                     ^^^^^^  -- handle the last item

                assert(is_atomic);  // |is_item| is used only with atomic elements

                if (DEBUG_PARSER_LEVEL_3)
                    gsl_log("  == got new item: \"%.*s\"",
                            (int)(e - b), b);

                err = spec->run(spec->obj, b, e - b);
                if (err.code) return *total_size = c - rec, err;

                in_item = false;
                // b = c + 1;
                // e = b;
            }

            *total_size = c - rec;
            return make_gsl_err(gsl_OK);
        default:
            e = c + 1;
            if (!in_item) {
                b = c;
                in_item = true;
            }
            break;
        }
        c++;
    }

    *total_size = c - rec;
    return make_gsl_err(gsl_FORMAT);
}

gsl_err_t
gsl_parse_cdata(void *obj,
                const char *rec,
                size_t *total_size)
{
    struct gslTaskSpec *spec = (struct gslTaskSpec *)obj;

    assert(spec->type == GSL_GET_STATE || spec->type == GSL_SET_STATE);
    assert(spec->buf != NULL || spec->run != NULL);
    assert(gsl_spec_is_correct(spec));

    bool in_cdata = false;
    size_t num_quotes = 0;

    size_t chunk_size;
    gsl_err_t err;

    const char *b, *c, *e;

    c = rec;
    while (*c && isspace(*c))
        c++;

    if (*c != '{' || *++c != '"') {
        *total_size = c - rec;
        return make_gsl_err(gsl_FORMAT);
    }

    for (; *c; c++) {
        switch (*c) {
        case '\n':
        case '\r':
        case '\t':
        case ' ':
            // Ignore all spaces
            break;
        case '"':
            if (!in_cdata) {
                num_quotes++;
                break;
            }

            err.code = !gsl_check_floating_boundary('"', num_quotes, '}', c, &chunk_size);
            if (err.code) {
                // We found something interesting at |c + chunk_size|.  Skip |chunk_size - 1| elements.
                c += chunk_size - 1;
                break;
            }

            err = gsl_check_field_terminal_value(b, e - b, spec);
            if (err.code) return *total_size = c - rec, err;

            // in_cdata = false;
            //printf("c is %s;;  c + chunk_size is %s\n", c, c + chunk_size);
            // TODO(k15tfu): Nice way to deal with it is to update .parse() implementations to
            //               handle spaces before the actual data, and exit exactly right after that.
            c += chunk_size + 1;  // Shamaning with spaces..

            // TODO(k15tfu): remove this
            while (*c && isspace(*c))
                c++;

            *total_size = c - rec;
            return make_gsl_err(gsl_OK);
        default:
            e = c + 1;
            if (!in_cdata) {
                b = c;
                in_cdata = true;
            }
            break;
        }
    }

    *total_size = c - rec;
    return make_gsl_err(gsl_FORMAT);
}
