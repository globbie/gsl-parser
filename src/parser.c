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

static gsl_err_t
gsl_parse_matching_braces(const char *c,
                          bool in_change,
                          bool in_array,
                          size_t *chunk_size)
{
    const char *b = c;
    size_t brace_count = 1;

    const char open_brace = in_change ? '(' : !in_array ? '{' : '[';
    const char close_brace = in_change ? ')' : !in_array ? '}' : ']';

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
            close_brace, 16, b);
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
        assert(!spec->is_selector && !spec->is_implied && !spec->is_validator && !spec->is_list && !spec->is_list_item && !spec->is_atomic);
    if (spec->is_selector)
        assert(!spec->is_default && !spec->is_list_item && !spec->is_atomic);
    if (spec->is_implied)
        assert(!spec->is_default && !spec->is_validator && !spec->is_list && !spec->is_list_item && !spec->is_atomic);
    if (spec->is_validator)
        assert(!spec->is_default && !spec->is_implied && !spec->is_list_item && !spec->is_atomic);
    if (spec->is_list)
        assert(!spec->is_default && !spec->is_implied && !spec->is_list_item && !spec->is_atomic);
    if (spec->is_list_item)
        assert(!spec->is_default && !spec->is_selector && !spec->is_implied && !spec->is_validator && !spec->is_list && !spec->is_atomic);
    assert(!spec->is_atomic);  // TODO(ki.stfu): ?? Remove this field

    assert((spec->buf != NULL) == (spec->buf_size != NULL));
    assert((spec->buf != NULL) == (spec->max_buf_size != 0));
    if (spec->buf)
        assert(*spec->buf_size == 0);

    // TODO(ki.stfu): ?? assert(spec->accu == NULL);  // TODO(ki.stfu): ?? remove this field
    if (spec->accu)
        assert(spec->obj == NULL);
    if (spec->obj)
        assert(spec->accu == NULL);

    if (spec->buf)
        assert(spec->parse == NULL && spec->validate == NULL && spec->run == NULL);
    if (spec->parse)
        assert(spec->buf == NULL && spec->validate == NULL && spec->run == NULL);
    if (spec->validate)
        assert(spec->buf == NULL && spec->parse == NULL && spec->run == NULL);
    if (spec->run)
        assert(spec->buf == NULL && spec->parse == NULL && spec->validate == NULL);

    assert((spec->alloc != NULL) == (spec->append != NULL));

    // Check that they are not mutually exclusive (in general):

    if (spec->type) {
        assert(!spec->is_default && (!spec->is_implied || spec->name != NULL) && !spec->is_list && !spec->is_list_item);
        // ?? assert(spec->name != NULL);
    }

    if (spec->name) {
        // |spec->type| can be set
        assert(!spec->is_default && !spec->is_validator && !spec->is_list_item);
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

    if (spec->is_validator) {
        // |spec->type| can be set
        assert(spec->name == NULL);
        assert(spec->obj != NULL);
        assert(spec->validate != NULL);
    }

    if (spec->is_list) {
        assert(spec->type == 0);  // type is useless for lists
        assert(spec->name != NULL || spec->is_validator);
        assert(spec->obj != NULL);
        assert(spec->parse != NULL || spec->validate != NULL);
    }

    if (spec->is_list_item) {
        assert(spec->type == 0);  // type is useless for list items
        assert(spec->name == NULL);
        assert(spec->accu != NULL);
        assert(spec->buf == NULL && spec->validate == NULL && spec->run == NULL);  // but |spec->parse| can be set
        assert(spec->alloc != NULL);
    }

        //assert(spec->obj != NULL || spec->buf != NULL); // ??

    if (spec->buf) {
        // |spec->type| can be set (depends on |spec->name|)
        assert(!spec->is_default && !spec->is_validator && !spec->is_list && !spec->is_list_item);
        assert(spec->name != NULL || spec->is_implied);
        assert(spec->obj == NULL && spec->accu == NULL);
    }

    if (spec->parse) {
        // |spec->type| can be set
        assert(!spec->is_default && !spec->is_implied && !spec->is_validator);
        assert(spec->name != NULL || spec->is_list_item);
        assert(spec->obj != NULL || spec->is_list_item);
    }

    // if (spec->validate)  -- already handled in spec->is_validator
    assert((spec->validate != NULL) == spec->is_validator);

    if (spec->run) {
        // |spec->type| can be set (depends on |spec->name|)
        assert(!spec->is_validator && !spec->is_list && !spec->is_list_item);
        assert(spec->name != NULL || spec->is_default || spec->is_implied);
        assert(spec->obj != NULL);
    }

    // if (spec->alloc)  -- already handled in spec->is_list_item
    assert((spec->alloc != NULL) == spec->is_list_item);

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
    assert(spec->name != NULL || spec->is_default || spec->is_implied || spec->is_validator || spec->is_list_item);
    assert(spec->buf != NULL || spec->parse != NULL || spec->validate != NULL || spec->run != NULL || spec->alloc != NULL);

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
              bool spec_type_list,
              struct gslTaskSpec *specs,
              size_t num_specs,
              struct gslTaskSpec **out_spec)
{
    struct gslTaskSpec *spec;
    struct gslTaskSpec *validator_spec = NULL;

    for (size_t i = 0; i < num_specs; i++) {
        spec = &specs[i];

        if (spec->type != spec_type) continue;
        if (spec->is_list != spec_type_list) continue;

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
gsl_check_matching_closing_brace(const char *c, bool in_change, bool in_array)
{
    assert(!in_change || !in_array);
    switch (*c) {
    case '}':
        if (!in_change && !in_array) return make_gsl_err(gsl_OK);
        break;
    case ')':
        if (in_change) return make_gsl_err(gsl_OK);
        break;
    case ']':
        if (in_array) return make_gsl_err(gsl_OK);
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
                    bool spec_type_list,
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

    err = gsl_find_spec(name, name_size, type, spec_type_list, specs, num_specs, out_spec);
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

    struct gslTaskSpec *spec;

    bool in_implied_field = false;
    bool in_field = false;
    bool in_change = false;
    bool in_array = false;
    bool in_tag = false;
    bool in_terminal = false;

    size_t chunk_size;
    gsl_err_t err;

    c = rec;

    if (DEBUG_PARSER_LEVEL_2)
        gsl_log("\n\n*** start basic PARSING: \"%.*s\" num specs: %zu [%p]",
                16, rec, num_specs, specs);

    // Check gslTaskSpec is properly filled
    for (size_t i = 0; i < num_specs; i++)
        assert(gsl_spec_is_correct(&specs[i]));

    while (*c) {
        switch (*c) {
        case '\n':
        case '\r':
        case '\t':
        case ' ':
            if (!in_field) {
                // Example: rec = "     {name...
                //                 ^^^^^  -- ignore spaces
                //      or: rec = "j smith {name...
                //                  ^  -- keep spaces in implied field
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

            err = gsl_check_field_tag(b, e - b, !in_change ? GSL_GET_STATE : GSL_CHANGE_STATE, in_array, specs, num_specs, &spec);
            if (err.code) return err;

            if (in_array != spec->is_list) return make_gsl_err(gsl_NO_MATCH);

            err = gsl_parse_field_value(b, e - b, spec, c, &chunk_size, &in_terminal);
            if (err.code) return err;

            if (in_terminal) {
                // Parse an atomic value.  Remember that we are in in_tag state.
                // Example: rec = "{name John Smith}"
                //                      ^  -- wait a terminal value
                // in_field == true
                // in_change == true | false
                // in_array == false
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

            err = gsl_check_matching_closing_brace(c + chunk_size, in_change, in_array);
            if (err.code) return err;

            in_field = false;
            in_change = false;
            in_array = false;
            // in_tag == false
            // in_terminal == false
            c += chunk_size;
            break;
        case '{':
        case '(':
        case '[':
            // Starting brace '{' or '(' or '['
            if (!in_field) {
                // Example: rec = "...{name John Smith}"
                //                    ^  -- parse a new field
                //      or: rec = "...[groups jsmith...]"
                //                    ^  -- same way
                if (in_implied_field) {
                    // Example: rec = "jsmith {name John Smith}"
                    //                 ^^^^^^  -- but first let's handle an implied field which is pointed by |b| & |e|
                    //      or: rec = "jsmith [groups jsmith...]"
                    //                 ^^^^^^  -- same way
                    err = gsl_check_implied_field(b, e - b, specs, num_specs);
                    if (err.code) return err;

                    in_implied_field = false;
                }

                in_field = true;
                in_change = (*c == '(');
                in_array = (*c == '[');
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
                gsl_log("-- terminal val for ATOMIC SPEC \"%.*s\" has an opening brace '%c': %.*s",
                        spec->name_size, spec->name, (in_change ? '(' : !in_array ? '{' : '['), c - b + 16, b);
                return make_gsl_err(gsl_FORMAT);
            }

            // Parse a tag after an inner field brace '{' or '('.  Means in_tag can be set to true.
            // Example: rec = "{name{...
            //                      ^  -- handle a tag
            //      or: rec = "{name[...
            //                      ^  -- same way
            //      or: rec = "[groups{...
            //                        ^  -- same way

            err = gsl_check_field_tag(b, e - b, !in_change ? GSL_GET_STATE : GSL_CHANGE_STATE, in_array, specs, num_specs, &spec);
            if (err.code) return err;

            if (in_array != spec->is_list) return make_gsl_err(gsl_NO_MATCH);

            err = gsl_parse_field_value(b, e - b, spec, c, &chunk_size, &in_terminal);
            if (err.code) return err;

            if (in_terminal) {
                // Example: rec = "{name{John Smith}}"
                //                      ^  -- terminal value cannot start with an opening brace
                // Example: rec = "{name[John Smith]}"
                //                      ^  -- same way
                gsl_log("-- terminal val for ATOMIC SPEC \"%.*s\" starts with an opening brace '%c': %.*s",
                        spec->name_size, spec->name, (in_change ? '(' : !in_array ? '{' : '['), c - b + 16, b);
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

            err = gsl_check_matching_closing_brace(c + chunk_size, in_change, in_array);
            if (err.code) return err;

            in_field = false;
            in_change = false;
            in_array = false;
            // in_tag == false
            // in_terminal == false
            c += chunk_size;
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

                err = gsl_check_default(rec, specs, num_specs);
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

            err = gsl_check_matching_closing_brace(c, in_change, in_array);
            if (err.code) return err;

            if (in_terminal) {
                // Example: rec = "{name John Smith}"
                //                       ^^^^^^^^^^  -- handle a terminal value
                err = gsl_check_field_terminal_value(b, e - b, spec);
                if (err.code) return err;

                in_field = false;
                in_change = false;
                // in_array == false
                in_tag = false;
                in_terminal = false;
                break;
            }

            // Parse a tag after a closing brace '}' / ')' in a field.  Means in_tag can be set to true.
            // Example: rec = "{name}"
            //                      ^  -- handle a tag

            err = gsl_check_field_tag(b, e - b, !in_change ? GSL_GET_STATE : GSL_CHANGE_STATE, in_array, specs, num_specs, &spec);
            if (err.code) return err;

            if (in_array != spec->is_list) return make_gsl_err(gsl_NO_MATCH);

            err = gsl_parse_field_value(b, e - b, spec, c, &chunk_size, &in_terminal);  // TODO(ki.stfu): allow in_terminal parsing
            if (err.code) return err;

            if (in_terminal) {
                // Example: rec = "{name}"
                //                      ^  -- terminal value is empty
                err = gsl_check_field_terminal_value(c, 0, spec);
                if (err.code) return err;

                // in_field == true
                // in_change == true | false
                // in_array == false
                // in_tag == false
                in_terminal = false;
            }

            in_field = false;
            in_change = false;
            // in_array == false
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

            err = gsl_check_matching_closing_brace(c, in_change, in_array);
            if (err.code) return err;

            // terminal value is not used with lists
            assert(!in_terminal);

            // Parse a tag after a closing brace ']' in a field.  Means in_tag can be set to true.
            // Example: rec = "[groups]"
            //                        ^  -- handle a tag

            err = gsl_check_field_tag(b, e - b, !in_change ? GSL_GET_STATE : GSL_CHANGE_STATE, in_array, specs, num_specs, &spec);
            if (err.code) return err;

            if (in_array != spec->is_list) return make_gsl_err(gsl_NO_MATCH);

            err = gsl_parse_field_value(b, e - b, spec, c, &chunk_size, &in_terminal);  // TODO(ki.stfu): allow in_terminal parsing
            if (err.code) return err;

            // terminal value is not used with lists
            assert(!in_terminal);

            in_field = false;
            // in_change == false
            in_array = false;
            // in_tag == false
            // in_terminal == false
            break;
        case '-':
            if (in_field && !in_tag && b == c) {
                // Example: rec = "...{-name John Smith}}"
                //                     ^  -- ignore the commented out field
                err = gsl_parse_matching_braces(c, in_change, in_array, &chunk_size);
                if (err.code) return err;

                in_field = false;
                in_change = false;
                in_array = false;
                // in_tag == false
                // in_terminal == false
                c += chunk_size;
                break;
            }

            // FALLTHROUGH
        default:
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

    if (DEBUG_PARSER_LEVEL_1)
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

    assert(spec->type == GSL_GET_STATE);
    assert(spec->name == NULL);
    assert(spec->name == NULL && gsl_spec_is_correct(spec));

    const char *b, *c, *e;
    void *item;
    size_t item_count = 0;

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
        case '(':
        case '}':
        case ')':
        case '[':
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

            if (DEBUG_PARSER_LEVEL_2)
                gsl_log("  == got new item: \"%.*s\"",
                        (int)(e - b), b);

            err = spec->alloc(spec->accu, b, e - b, item_count, &item);
            if (err.code) return err;

            in_item = false;
            item_count++;
            b = c + 1;
            e = b;
            break;
        case '{':
            if (is_atomic)
                return make_gsl_err(gsl_FORMAT);

            // Parse a non-atomic element after an element brace '{'.  Means in_item can be set to true.
            // Example: rec = "{user...
            //                 ^  -- allocate an element

            err = spec->alloc(spec->accu, NULL, 0, item_count, &item);
            if (err.code) {
                gsl_log("-- item alloc failed: %d :(", err.code);
                return err;
            }

            err = spec->parse(item, c + 1, &chunk_size);
            if (err.code) {
                gsl_log("-- ERR: %d parsing of spec \"%.*s\" failed :(",
                        err.code, spec->name_size, spec->name);
                return err;
            }

            err = spec->append(spec->accu, item);
            if (err.code) return err;

            // Example: rec = "{user Sam}]
            //                 ^^^^^^^^^^  -- handled by an inner call to .parse()
            //                           ^  -- c + chunk_size

            // in_item == false
            item_count++;
            c += chunk_size + 1;
            b = c;
            e = b;
            break;
        case ']':
            if (in_item) {
                // Example: rec = "... jsmith]
                //                     ^^^^^^  -- handle the last item

                assert(is_atomic);  // |is_item| is used only with atomic elements

                if (DEBUG_PARSER_LEVEL_2)
                    gsl_log("  == got new item: \"%.*s\"",
                            (int)(e - b), b);

                err = spec->alloc(spec->accu, b, e - b, item_count, &item);
                if (err.code) return err;

                in_item = false;
                item_count++;
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

    return make_gsl_err(gsl_FORMAT);
}
