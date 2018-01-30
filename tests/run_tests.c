#include <gsl-parser.h>

#include <check.h>

#include <assert.h>
#include <string.h>

// --------------------------------------------------------------------------------
// User -- testable object
struct User {
    char name[GSL_SHORT_NAME_SIZE]; size_t name_size;
    char sid[6]; size_t sid_size;
    char email[GSL_SHORT_NAME_SIZE]; size_t email_size; enum { EMAIL_NONE, EMAIL_HOME, EMAIL_WORK } email_type;
};

// --------------------------------------------------------------------------------
// TaskSpecs -- for passing inner specs into |parse_user|
struct TaskSpecs { struct gslTaskSpec *specs; size_t num_specs; };

// --------------------------------------------------------------------------------
// Common routines
static void test_case_fixture_setup(void) {
    extern struct User user;
    user.name_size = 0;
    user.sid_size = 0;
    user.email_size = 0; user.email_type = EMAIL_NONE;
}

static gsl_err_t parse_user(void *obj, const char *rec, size_t *total_size) {
    struct TaskSpecs *args = (struct TaskSpecs *)obj;
    ck_assert(args);
    ck_assert(rec); ck_assert(total_size);
    return gsl_parse_task(rec, total_size, args->specs, args->num_specs);
}

static gsl_err_t run_set_name(void *obj, const char *name, size_t name_size) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(name); ck_assert_uint_ne(name_size, 0);
    if (name_size > sizeof self->name)
        return make_gsl_err(gsl_LIMIT);
    if (self->name_size)
        return make_gsl_err(gsl_EXISTS);  // error: already specified, return gsl_EXISTS to match .buf case
    memcpy(self->name, name, name_size);
    self->name_size = name_size;
    return make_gsl_err(gsl_OK);
}

static gsl_err_t run_set_default_sid(void *obj, const char *val, size_t val_size) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(!val); ck_assert_uint_eq(val_size, 0);
    return make_gsl_err(gsl_FORMAT);  // error: sid is required, return gsl_FORMAT to match .buf & .run cases
}

static gsl_err_t parse_sid(void *obj, const char *rec, size_t *total_size) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(rec); ck_assert(total_size);

    struct gslTaskSpec specs[] = {
        {
          .is_implied = true,
          .buf = self->sid,
          .buf_size = &self->sid_size,
          .max_buf_size = sizeof self->sid
        },
        {
          .is_default = true,
          .run = run_set_default_sid,
          .obj = self
        }
    };

    return gsl_parse_task(rec, total_size, specs, sizeof specs / sizeof specs[0]);
}

static gsl_err_t run_set_sid(void *obj, const char *sid, size_t sid_size) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(sid);
    if (sid_size == 0)
        return make_gsl_err(gsl_FORMAT);  // error: sid is required, return gsl_FORMAT to match .buf & .parse cases
    if (sid_size > sizeof self->sid)
        return make_gsl_err(gsl_LIMIT);
    memcpy(self->sid, sid, sid_size);
    self->sid_size = sid_size;
    return make_gsl_err(gsl_OK);
}

static gsl_err_t run_set_default_email(void *obj, const char *val, size_t val_size) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(!val); ck_assert_uint_eq(val_size, 0);

    self->email_type = EMAIL_NONE;
    self->email_size = 0;
    return make_gsl_err(gsl_OK);  // ok: no email by default
}

static gsl_err_t parse_email_record(void *obj,
                                    const char *name, size_t name_size,
                                    const char *rec, size_t *total_size) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(name); ck_assert_uint_ne(name_size, 0);
    ck_assert(rec); ck_assert(total_size);

    struct gslTaskSpec specs[] = {
        {
          .is_implied = true,
          .buf = self->email,
          .buf_size = &self->email_size,
          .max_buf_size = sizeof self->email
        },
        {
          .is_default = true,
          .run = run_set_default_email,  // We are okay even if email is empty
          .obj = self
        }
    };
    gsl_err_t err;

    if (self->email_type != EMAIL_NONE)
        return make_gsl_err(gsl_FAIL);  // error: only 1 email address can be specified

    if (name_size == strlen("home") && !memcmp(name, "home", name_size))
        self->email_type = EMAIL_HOME;
    else if (name_size == strlen("work") && !memcmp(name, "work", name_size))
        self->email_type = EMAIL_WORK;
    else
        return make_gsl_err(gsl_FAIL);  // error: unknown type

    err = gsl_parse_task(rec, total_size, specs, sizeof specs  / sizeof specs[0]);
    if (err.code) {
        self->email_type = EMAIL_NONE;
        return err;
    }

    return make_gsl_err(gsl_OK);
}

static gsl_err_t parse_email(void *obj, const char *rec, size_t *total_size) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(rec); ck_assert(total_size);

    struct gslTaskSpec specs[] = {
        {
          .is_validator = true,
          .validate = parse_email_record,
          .obj = self
        },
        {
          .is_default = true,
          .run = run_set_default_email,  // We are okay even if email is empty
          .obj = self
        }
    };

    return gsl_parse_task(rec, total_size, specs, sizeof specs / sizeof specs[0]);
}

static gsl_err_t run_set_anonymous_user(void *obj, const char *val, size_t val_size) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(!val); ck_assert_uint_eq(val_size, 0);

    const char *none = "(none)";
    size_t none_size = strlen("(none)");

    assert(none_size <= sizeof self->name);
    memcpy(self->name, none, none_size);
    self->name_size = none_size;
    return make_gsl_err(gsl_OK);
}

#define RESET_IS_COMPLETED(specs, num_specs)   \
    do {                                       \
        for (size_t i = 0; i < num_specs; ++i) \
          specs[i].is_completed = false;       \
    } while (0)

#define RESET_IS_COMPLETED_gslTaskSpec(specs) \
    RESET_IS_COMPLETED(specs, sizeof specs / sizeof specs[0])

#define RESET_IS_COMPLETED_TaskSpecs(task_specs) \
    RESET_IS_COMPLETED((task_specs)->specs, (task_specs)->num_specs)

#define DEFINE_TaskSpecs(name, ...)                 \
    struct gslTaskSpec __specs##__LINE__[] = { __VA_ARGS__ }; \
    struct TaskSpecs name = { __specs##__LINE__, sizeof __specs##__LINE__ / sizeof __specs##__LINE__[0] }

#define ASSERT_STR_EQ(act, act_size, exp, exp_size...)  \
    do {                                                \
        const char *__act = (act);                      \
        size_t __act_size = (act_size);                 \
        const char *__exp = (exp);                      \
        size_t __exp_size = (exp_size-0) == 0 ? strlen(__exp) : (exp_size-0);                    \
        ck_assert_msg(__act_size == __exp_size && 0 == strncmp(__act, __exp, __act_size),        \
            "Assertion '%s' failed: %s == \"%.*s\" [len: %zu] but expected \"%.*s\" [len: %zu]", \
            #act" == "#exp, #act, __act_size, __act, __act_size, __exp_size, __exp, __exp_size); \
    } while (0)

// --------------------------------------------------------------------------------
// Generators for specs

enum { SPEC_BUF = 0x0, SPEC_PARSE = 0x1, SPEC_RUN = 0x2, SPEC_NAME = 0x4, SPEC_SELECTOR = 0x8, SPEC_CHANGE = 0x10 };

static struct gslTaskSpec gen_user_spec(struct TaskSpecs *args, int flags) {
    assert((flags & (SPEC_CHANGE)) == flags && "Valid flags: [SPEC_CHANGE]");
    return (struct gslTaskSpec){ .type = !(flags & SPEC_CHANGE) ? GSL_GET_STATE : GSL_CHANGE_STATE,
                                 .name = "user", .name_size = strlen("user"),
                                 .parse = parse_user, .obj = args };
}

static struct gslTaskSpec gen_name_spec(struct User *self, int flags) {
    assert((flags & (SPEC_BUF | SPEC_RUN | SPEC_NAME | SPEC_SELECTOR | SPEC_CHANGE)) == flags &&
           "Valid flags: [SPEC_BUF | SPEC_RUN] [SPEC_NAME] [SPEC_SELECTOR] [SPEC_CHANGE]");
    if (flags & SPEC_RUN)
        return (struct gslTaskSpec){ .is_implied = true,
                                     .type = !(flags & SPEC_CHANGE) ? GSL_GET_STATE : GSL_CHANGE_STATE,
                                     .name = (flags & SPEC_NAME) ? "name" : NULL, .name_size = (flags & SPEC_NAME) ? strlen("name") : 0,
                                     .is_selector = (flags & SPEC_SELECTOR),
                                     .run = run_set_name, .obj = self };
    return (struct gslTaskSpec){ .is_implied = true,
                                 .type = !(flags & SPEC_CHANGE) ? GSL_GET_STATE : GSL_CHANGE_STATE,
                                 .name = (flags & SPEC_NAME) ? "name" : NULL, .name_size = (flags & SPEC_NAME) ? strlen("name") : 0,
                                 .is_selector = (flags & SPEC_SELECTOR),
                                 .buf = self->name, .buf_size = &self->name_size, .max_buf_size = sizeof self->name };
}

static struct gslTaskSpec gen_sid_spec(struct User *self, int flags) {
    assert((flags & (SPEC_BUF | SPEC_PARSE | SPEC_RUN | SPEC_SELECTOR | SPEC_CHANGE)) == flags &&
           "Valid flags: [SPEC_BUF | SPEC_PARSE | SPEC_RUN] [SPEC_SELECTOR] [SPEC_CHANGE]");
    if (flags & SPEC_PARSE)
        return (struct gslTaskSpec){ .type = !(flags & SPEC_CHANGE) ? GSL_GET_STATE : GSL_CHANGE_STATE,
                                     .name = "sid", .name_size = strlen("sid"),
                                     .is_selector = (flags & SPEC_SELECTOR),
                                     .parse = parse_sid, .obj = self };
    if (flags & SPEC_RUN)
        return (struct gslTaskSpec){ .type = !(flags & SPEC_CHANGE) ? GSL_GET_STATE : GSL_CHANGE_STATE,
                                     .name = "sid", .name_size = strlen("sid"),
                                     .is_selector = (flags & SPEC_SELECTOR),
                                     .run = run_set_sid, .obj = self };
    return (struct gslTaskSpec){ .type = !(flags & SPEC_CHANGE) ? GSL_GET_STATE : GSL_CHANGE_STATE,
                                 .name = "sid", .name_size = strlen("sid"),
                                 .is_selector = (flags & SPEC_SELECTOR),
                                 .buf = self->sid, .buf_size = &self->sid_size, .max_buf_size = sizeof self->sid };
}

static struct gslTaskSpec gen_email_spec(struct User *self, int flags) {
    assert((flags & SPEC_SELECTOR) == flags && "Valid flags: [SPEC_SELECTOR]");
    return (struct gslTaskSpec){ .name = "email", .name_size = strlen("email"),
                                 .is_selector = (flags & SPEC_SELECTOR),
                                 .parse = parse_email, .obj = self };
}

static struct gslTaskSpec gen_default_spec(struct User *self) {
    return (struct gslTaskSpec){ .is_default = true, .run = run_set_anonymous_user, .obj = self };
}

// --------------------------------------------------------------------------------
// Common variables
gsl_err_t rc;
const char *rec;
size_t total_size;
struct User user;

// --------------------------------------------------------------------------------
// GSL_GET_STATE cases

START_TEST(parse_task_empty)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);  // TODO(ki.stfu): Call the default handler
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.name_size, 0); ck_assert_uint_eq(user.sid_size, 0);
END_TEST

START_TEST(parse_task_empty_with_spaces)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "     ", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);  // TODO(ki.stfu): Call the default handler
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.name_size, 0); ck_assert_uint_eq(user.sid_size, 0);
END_TEST

START_TEST(parse_task_empty_with_closing_brace)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = " }     ", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);  // TODO(ki.stfu): Call the default handler
    ck_assert_uint_eq(total_size, strchr(rec, '}') - rec);  // shared brace
END_TEST

static void
check_parse_implied_field(struct gslTaskSpec *specs,
                          size_t num_specs,
                          struct TaskSpecs *parse_user_args) {
    rc = gsl_parse_task(rec = "{user John Smith}", &total_size, specs, num_specs);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED(specs, num_specs); RESET_IS_COMPLETED_TaskSpecs(parse_user_args);

    rc = gsl_parse_task(rec = "{user John Smith{sid 123456}}", &total_size, specs, num_specs);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED(specs, num_specs); RESET_IS_COMPLETED_TaskSpecs(parse_user_args);

    rc = gsl_parse_task(rec = "{user John Smith {sid 123456}}", &total_size, specs, num_specs);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED(specs, num_specs); RESET_IS_COMPLETED_TaskSpecs(parse_user_args);

    rc = gsl_parse_task(rec = "{user   John Smith   {sid 123456}}", &total_size, specs, num_specs);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED(specs, num_specs); RESET_IS_COMPLETED_TaskSpecs(parse_user_args);

    rc = gsl_parse_task(rec = "{user {sid 123456}John Smith}", &total_size, specs, num_specs);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED(specs, num_specs); RESET_IS_COMPLETED_TaskSpecs(parse_user_args);

    rc = gsl_parse_task(rec = "{user {sid 123456} John Smith}", &total_size, specs, num_specs);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED(specs, num_specs); RESET_IS_COMPLETED_TaskSpecs(parse_user_args);

    rc = gsl_parse_task(rec = "{user {sid 123456}   John Smith   }", &total_size, specs, num_specs);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED(specs, num_specs); RESET_IS_COMPLETED_TaskSpecs(parse_user_args);
}

START_TEST(parse_implied_field)
    // Check implied field with .buf
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_BUF), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    check_parse_implied_field(specs, sizeof specs / sizeof specs[0], &parse_user_args);
  }

    // Check implied field with .run
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_RUN), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    check_parse_implied_field(specs, sizeof specs / sizeof specs[0], &parse_user_args);
  }
END_TEST

START_TEST(parse_implied_field_with_name)
    // Check implied field with .buf & .name
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_BUF | SPEC_NAME), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    check_parse_implied_field(specs, sizeof specs / sizeof specs[0], &parse_user_args);

    rc = gsl_parse_task(rec = "{user {name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
  }
    user.name_size = 0;  // reset

    // Check implied field with .run & .name
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_RUN | SPEC_NAME), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    check_parse_implied_field(specs, sizeof specs / sizeof specs[0], &parse_user_args);

    rc = gsl_parse_task(rec = "{user {name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
  }
END_TEST

START_TEST(parse_implied_field_unknown)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user John Smith{sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user John Smith {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

static void
check_parse_implied_field_max_size(int name_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, name_flags), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

  {
    const char buf[] = { '{', 'u', 's', 'e', 'r', ' ', [6 ... GSL_SHORT_NAME_SIZE + 5] = 'a', '}', '\0' };
    rc = gsl_parse_task(rec = buf, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, strchr(buf, 'a'), GSL_SHORT_NAME_SIZE);
  }
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

  {
    const char buf[] = { '{', 'u', 's', 'e', 'r', ' ', [6 ... GSL_SHORT_NAME_SIZE + 5] = 'a', ' ', '{', 's', 'i', 'd', ' ', '1', '2', '3', '4', '5', '6', '}', '}', '\0' };
    rc = gsl_parse_task(rec = buf, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, strchr(buf, 'a'), GSL_SHORT_NAME_SIZE);
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
  }
    user.name_size = 0; user.sid_size = 0;
}

START_TEST(parse_implied_field_max_size)
    check_parse_implied_field_max_size(SPEC_BUF);

    check_parse_implied_field_max_size(SPEC_RUN);
END_TEST

static void
check_parse_implied_field_max_size_plus_one(int name_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, name_flags));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

  {
    const char buf[] = { '{', 'u', 's', 'e', 'r', ' ', [6 ... GSL_SHORT_NAME_SIZE + 6] = 'a', '}', '\0' };
    rc = gsl_parse_task(rec = buf, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);
  }

  {
    const char buf[] = { '{', 'u', 's', 'e', 'r', ' ', [6 ... GSL_SHORT_NAME_SIZE + 6] = 'a', ' ', '{', 's', 'i', 'd', ' ', '1', '2', '3', '4', '5', '6', '}', '}', '\0' };
    rc = gsl_parse_task(rec = buf, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);
  }
}

START_TEST(parse_implied_field_max_size_plus_one)
    check_parse_implied_field_max_size_plus_one(SPEC_BUF);

    check_parse_implied_field_max_size_plus_one(SPEC_RUN);
END_TEST

static void
check_parse_implied_field_size_NAME_SIZE_plus_one(int name_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, name_flags));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

  {
    const char buf[] = { '{', 'u', 's', 'e', 'r', ' ', [6 ... GSL_NAME_SIZE + 6] = 'a', '}', '\0' };
    rc = gsl_parse_task(rec = buf, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);
  }

  {
    const char buf[] = { '{', 'u', 's', 'e', 'r', ' ', [6 ... GSL_NAME_SIZE + 6] = 'a', ' ', '{', 's', 'i', 'd', ' ', '1', '2', '3', '4', '5', '6', '}', '}', '\0' };
    rc = gsl_parse_task(rec = buf, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);
  }
}

START_TEST(parse_implied_field_size_NAME_SIZE_plus_one)
    check_parse_implied_field_size_NAME_SIZE_plus_one(SPEC_BUF);

    check_parse_implied_field_size_NAME_SIZE_plus_one(SPEC_RUN);
END_TEST

static void
check_parse_implied_field_duplicate(int name_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, name_flags));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user John Smith{name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_EXISTS);
    user.name_size = 0; RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user John Smith {name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_EXISTS);
    user.name_size = 0; RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {name John Smith} {name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_EXISTS);
    user.name_size = 0;
}

START_TEST(parse_implied_field_duplicate)
    check_parse_implied_field_duplicate(SPEC_BUF | SPEC_NAME);

    check_parse_implied_field_duplicate(SPEC_RUN | SPEC_NAME);
END_TEST

START_TEST(parse_tag_empty)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user{}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user{{home john@imloud.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {{home john@imloud.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
END_TEST

START_TEST(parse_tag_empty_with_spaces)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user{   }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user{   123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user{   {home john@imloud.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {   }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {   123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {   {home john@imloud.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
END_TEST

START_TEST(parse_tag_unknown)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user{s 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user{si 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user{sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user{sido 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user {s 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user {si 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {sido 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user(sid 123456)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user (sid 123456)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

static void
check_parse_value_terminal_empty(int sid_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, sid_flags));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user{sid}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user{sid{}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user{sid   }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user{sid   {}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid{}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid   }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid   {}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
}

START_TEST(parse_value_terminal_empty)
    check_parse_value_terminal_empty(SPEC_BUF);

    check_parse_value_terminal_empty(SPEC_PARSE);

    check_parse_value_terminal_empty(SPEC_RUN);
END_TEST

static void
check_parse_value_terminal_max_size(int sid_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, sid_flags));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user{sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0;  // reset
}

START_TEST(parse_value_terminal_max_size)
    check_parse_value_terminal_max_size(SPEC_BUF);

    check_parse_value_terminal_max_size(SPEC_PARSE);

    check_parse_value_terminal_max_size(SPEC_RUN);
END_TEST

static void
check_parse_value_terminal_max_size_plus_one(int sid_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, sid_flags));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user{sid 1234567}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);

    rc = gsl_parse_task(rec = "{user {sid 1234567}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);
}

START_TEST(parse_value_terminal_max_size_plus_one)
    check_parse_value_terminal_max_size_plus_one(SPEC_BUF);

    check_parse_value_terminal_max_size_plus_one(SPEC_PARSE);

    check_parse_value_terminal_max_size_plus_one(SPEC_RUN);
END_TEST

static void
check_parse_value_terminal_NAME_SIZE_plus_one(int sid_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, sid_flags));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

  {
    const char buf[] = { '{', 'u', 's', 'e', 'r', '{', 's', 'i', 'd', ' ', [10 ... GSL_NAME_SIZE + 10] = '1', '}', '}', '\0' };
    rc = gsl_parse_task(rec = buf, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);
  }

  {
    const char buf[] = { '{', 'u', 's', 'e', 'r', ' ', '{', 's', 'i', 'd', ' ', [11 ... GSL_NAME_SIZE + 11] = '1', '}', '}', '\0' };
    rc = gsl_parse_task(rec = buf, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);
  }
}

START_TEST(parse_value_terminal_NAME_SIZE_plus_one)
    check_parse_value_terminal_NAME_SIZE_plus_one(SPEC_BUF);

    check_parse_value_terminal_NAME_SIZE_plus_one(SPEC_PARSE);

    check_parse_value_terminal_NAME_SIZE_plus_one(SPEC_RUN);
END_TEST

START_TEST(parse_value_terminal_with_braces)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user{sid{123456}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user{sid {123456}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user{sid 123{456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid{123456}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid {123456}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid 123{456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
END_TEST

START_TEST(parse_value_validate_empty)
    DEFINE_TaskSpecs(parse_user_args, gen_email_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user{email}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_NONE); ck_assert_uint_eq(user.email_size, 0);
    RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_NONE); ck_assert_uint_eq(user.email_size, 0);
    RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email{home}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_NONE); ck_assert_uint_eq(user.email_size, 0);
    RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email{home{}}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);  // error: empty tag, but .validate has been ?successfully? called

    rc = gsl_parse_task(rec = "{user {email{home   }}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_NONE); ck_assert_uint_eq(user.email_size, 0);
    RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email{home   {}}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);  // error: empty tag, but .validate has been ?successfully? called

    rc = gsl_parse_task(rec = "{user {email {home}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_NONE); ck_assert_uint_eq(user.email_size, 0);
    RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email {home{}}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);  // error: empty tag, but .validate has been ?successfully? called

    rc = gsl_parse_task(rec = "{user {email {home   }}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_NONE); ck_assert_uint_eq(user.email_size, 0);
    RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email {home   {}}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);  // error: empty tag, but .validate has been ?successfully? called

    rc = gsl_parse_task(rec = "{user {email{home}{work}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_NONE); ck_assert_uint_eq(user.email_size, 0);
    RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email{home} {work}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_NONE); ck_assert_uint_eq(user.email_size, 0);
    RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email {home}{work}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_NONE); ck_assert_uint_eq(user.email_size, 0);
    RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email {home} {work}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_NONE); ck_assert_uint_eq(user.email_size, 0);
    RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);
END_TEST

START_TEST(parse_value_validate_single)
    DEFINE_TaskSpecs(parse_user_args, gen_email_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {email{home john@iserver.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_HOME); ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
    user.email_type = EMAIL_NONE; user.email_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email{work j.smith@gogel.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_WORK); ASSERT_STR_EQ(user.email, user.email_size, "j.smith@gogel.com");
    user.email_type = EMAIL_NONE; user.email_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email {home john@iserver.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_HOME); ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
    user.email_type = EMAIL_NONE; user.email_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email {work j.smith@gogel.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_WORK); ASSERT_STR_EQ(user.email, user.email_size, "j.smith@gogel.com");
END_TEST

START_TEST(parse_value_validate_several)
    DEFINE_TaskSpecs(parse_user_args, gen_email_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {email{home john@iserver.com}{work j.smith@gogel.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FAIL);  // defined in parse_email_record()
    user.email_type = EMAIL_NONE; user.email_size = 0; RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email{home john@iserver.com} {work j.smith@gogel.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FAIL);  // defined in parse_email_record()
    user.email_type = EMAIL_NONE; user.email_size = 0; RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email {home john@iserver.com}{work j.smith@gogel.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FAIL);  // defined in parse_email_record()
    user.email_type = EMAIL_NONE; user.email_size = 0; RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email {home john@iserver.com} {work j.smith@gogel.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FAIL);  // defined in parse_email_record()
END_TEST

START_TEST(parse_value_validate_max_size)
    DEFINE_TaskSpecs(parse_user_args, gen_email_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

  {
    const char buf[] = { '{', 'u', 's', 'e', 'r', ' ', '{', 'e', 'm', 'a', 'i', 'l', '{', 'h', 'o', 'm', 'e', ' ', [18 ... GSL_SHORT_NAME_SIZE + 17] = 'b', '}', '}', '}', '\0' };
    rc = gsl_parse_task(rec = buf, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_HOME); ASSERT_STR_EQ(user.email, user.email_size, strchr(buf, 'b'), GSL_SHORT_NAME_SIZE);
  }
  user.email_type = EMAIL_NONE; user.email_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

  {
    const char buf[] = { '{', 'u', 's', 'e', 'r', ' ', '{', 'e', 'm', 'a', 'i', 'l', ' ', '{', 'w', 'o', 'r', 'k', ' ', [19 ... GSL_SHORT_NAME_SIZE + 18] = 'b', '}', '}', '}', '\0' };
    rc = gsl_parse_task(rec = buf, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_int_eq(user.email_type, EMAIL_WORK); ASSERT_STR_EQ(user.email, user.email_size, strchr(buf, 'b'), GSL_SHORT_NAME_SIZE);
  }
END_TEST

START_TEST(parse_value_validate_max_size_plus_one)
    DEFINE_TaskSpecs(parse_user_args, gen_email_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

  {
    const char buf[] = { '{', 'u', 's', 'e', 'r', ' ', '{', 'e', 'm', 'a', 'i', 'l', '{', 'h', 'o', 'm', 'e', ' ', [18 ... GSL_SHORT_NAME_SIZE + 18] = 'b', '}', '}', '}', '\0' };
    rc = gsl_parse_task(rec = buf, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);
  }

  {
    const char buf[] = { '{', 'u', 's', 'e', 'r', ' ', '{', 'e', 'm', 'a', 'i', 'l', ' ', '{', 'w', 'o', 'r', 'k', ' ', [19 ... GSL_SHORT_NAME_SIZE + 19] = 'b', '}', '}', '}', '\0' };
    rc = gsl_parse_task(rec = buf, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);
  }
END_TEST

START_TEST(parse_value_validate_NAME_SIZE_plus_one)
    DEFINE_TaskSpecs(parse_user_args, gen_email_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

  {
    const char buf[] = { '{', 'u', 's', 'e', 'r', ' ', '{', 'e', 'm', 'a', 'i', 'l', '{', 'h', 'o', 'm', 'e', ' ', [18 ... GSL_NAME_SIZE + 18] = 'b', '}', '}', '}', '\0' };
    rc = gsl_parse_task(rec = buf, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);
  }

  {
    const char buf[] = { '{', 'u', 's', 'e', 'r', ' ', '{', 'e', 'm', 'a', 'i', 'l', ' ', '{', 'w', 'o', 'r', 'k', ' ', [19 ... GSL_NAME_SIZE + 19] = 'b', '}', '}', '}', '\0' };
    rc = gsl_parse_task(rec = buf, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);
  }
END_TEST

static void
check_parse_value_default_when_implied_field(int name_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, name_flags), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0;  // reset
}

static void
check_parse_value_default_when_value_terminal(int sid_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, sid_flags), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0;  // reset
}

START_TEST(parse_value_default)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
  }

  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
    user.name_size = 0;  // reset
  }

    check_parse_value_default_when_implied_field(SPEC_BUF);

    check_parse_value_default_when_implied_field(SPEC_RUN);

    check_parse_value_default_when_value_terminal(SPEC_BUF);

    check_parse_value_default_when_value_terminal(SPEC_PARSE);

    check_parse_value_default_when_value_terminal(SPEC_RUN);

  {
    DEFINE_TaskSpecs(parse_user_args, gen_email_spec(&user, 0), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {email {home john@iserver.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "");
    ck_assert_int_eq(user.email_type, EMAIL_HOME); ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
  }
    user.email_type = EMAIL_NONE; user.email_size = 0;  // reset

  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0), gen_sid_spec(&user, 0), gen_email_spec(&user, 0), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
  }
END_TEST

START_TEST(parse_value_default_with_selectors)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_SELECTOR), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
  }
    user.name_size = 0;  // reset

  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_SELECTOR), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
  }
    user.name_size = 0;  // reset

  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_SELECTOR), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
  }
    user.name_size = 0; user.sid_size = 0;  // reset

  {
    DEFINE_TaskSpecs(parse_user_args, gen_email_spec(&user, SPEC_SELECTOR), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {email {home john@iserver.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
    ck_assert_int_eq(user.email_type, EMAIL_HOME); ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
  }
    user.name_size = 0; user.email_type = EMAIL_NONE; user.email_size = 0; // reset

  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0), gen_sid_spec(&user, SPEC_SELECTOR), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
  }
    user.name_size = 0; user.sid_size = 0;  // reset

  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_SELECTOR), gen_sid_spec(&user, SPEC_SELECTOR), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
  }
    user.name_size = 0; user.sid_size = 0;  // reset

  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0), gen_sid_spec(&user, SPEC_SELECTOR), gen_email_spec(&user, SPEC_SELECTOR), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith {sid 123456} {email {home john@iserver.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    ck_assert_int_eq(user.email_type, EMAIL_HOME); ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
  }
    user.name_size = 0; user.sid_size = 0; user.email_type = EMAIL_NONE; user.email_size = 0; // reset

  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_SELECTOR), gen_sid_spec(&user, SPEC_SELECTOR), gen_email_spec(&user, SPEC_SELECTOR), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith {sid 123456} {email {home john@iserver.com}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    ck_assert_int_eq(user.email_type, EMAIL_HOME); ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
  }
END_TEST

START_TEST(parse_value_unmatched_braces)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {sid 123456)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user (sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user (sid 123456)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

START_TEST(parse_comment_empty)
    DEFINE_TaskSpecs(parse_user_args);
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {-}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

START_TEST(parse_comment)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {-sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user {-sid 123456} {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
END_TEST

START_TEST(parse_comment_unmatched_braces)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {-sid 123456}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {-sid 123456)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

// --------------------------------------------------------------------------------
// GSL_CHANGE_STATE cases

START_TEST(parse_change_tag_unknown)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, 0), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(u{sid 123456})", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "(us{sid 123456})", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "(use{sid 123456})", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "(user{sid 123456})", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(usero{sid 123456})", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "(u {sid 123456})", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "(us {sid 123456})", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "(use {sid 123456})", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "(user {sid 123456})", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(usero {sid 123456})", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "(u)", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "(us)", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "(use)", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "(user)", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(usero)", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

// START_TEST(parse_change_implied_field) -- There is no difference with GSL_GET_STATE cases.

START_TEST(parse_change_implied_field_with_name)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME | SPEC_CHANGE), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    check_parse_implied_field(specs, sizeof specs / sizeof specs[0], &parse_user_args);

    rc = gsl_parse_task(rec = "{user (name John Smith)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);
END_TEST

START_TEST(parse_change_value_terminal)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user(sid 123456)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user (sid 123456)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);
END_TEST

START_TEST(parse_change_value_unmatched_braces)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user (sid 123456)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user (sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid 123456)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

// --------------------------------------------------------------------------------
// main

int main() {
    Suite* s = suite_create("suite");

    TCase* tc_get = tcase_create("get cases");
    tcase_add_checked_fixture(tc_get, test_case_fixture_setup, NULL);
    tcase_add_test(tc_get, parse_task_empty);
    tcase_add_test(tc_get, parse_task_empty_with_spaces);
    tcase_add_test(tc_get, parse_task_empty_with_closing_brace);
    tcase_add_test(tc_get, parse_implied_field);
    tcase_add_test(tc_get, parse_implied_field_with_name);
    tcase_add_test(tc_get, parse_implied_field_unknown);
    tcase_add_test(tc_get, parse_implied_field_max_size);
    tcase_add_test(tc_get, parse_implied_field_max_size_plus_one);
    tcase_add_test(tc_get, parse_implied_field_size_NAME_SIZE_plus_one);
    tcase_add_test(tc_get, parse_implied_field_duplicate);
    tcase_add_test(tc_get, parse_tag_empty);
    tcase_add_test(tc_get, parse_tag_empty_with_spaces);
    tcase_add_test(tc_get, parse_tag_unknown);
    tcase_add_test(tc_get, parse_value_terminal_empty);
    tcase_add_test(tc_get, parse_value_terminal_max_size);
    tcase_add_test(tc_get, parse_value_terminal_max_size_plus_one);
    tcase_add_test(tc_get, parse_value_terminal_NAME_SIZE_plus_one);
    tcase_add_test(tc_get, parse_value_terminal_with_braces);
    tcase_add_test(tc_get, parse_value_validate_empty);
    tcase_add_test(tc_get, parse_value_validate_single);
    tcase_add_test(tc_get, parse_value_validate_several);
    tcase_add_test(tc_get, parse_value_validate_max_size);
    tcase_add_test(tc_get, parse_value_validate_max_size_plus_one);
    tcase_add_test(tc_get, parse_value_validate_NAME_SIZE_plus_one);
    tcase_add_test(tc_get, parse_value_default);
    tcase_add_test(tc_get, parse_value_default_with_selectors);
    tcase_add_test(tc_get, parse_value_unmatched_braces);
    tcase_add_test(tc_get, parse_comment_empty);
    tcase_add_test(tc_get, parse_comment);
    tcase_add_test(tc_get, parse_comment_unmatched_braces);
    suite_add_tcase(s, tc_get);

    TCase* tc_change = tcase_create("change cases");
    tcase_add_checked_fixture(tc_change, test_case_fixture_setup, NULL);
    tcase_add_test(tc_change, parse_change_tag_unknown);
    // tcase_add_test(tc_change, parse_change_implied_field) -- There is no difference with GSL_GET_STATE cases.
    tcase_add_test(tc_change, parse_change_implied_field_with_name);
    tcase_add_test(tc_change, parse_change_value_terminal);
    tcase_add_test(tc_change, parse_change_value_unmatched_braces);
    suite_add_tcase(s, tc_change);

    SRunner* sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    srunner_free(sr);
    return 0;
}
