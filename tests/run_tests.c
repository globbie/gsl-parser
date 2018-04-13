#include <gsl-parser.h>

#include <check.h>

#include <assert.h>
#include <string.h>

#define USER_NAME_SIZE 64
#define USER_GROUPS_MAX_SIZE 4  // Note: Don't modify! Some tests rely this is equal to 4.   // TODO(ki.stfu): really?

// --------------------------------------------------------------------------------
// User -- testable object
struct User {
    char name[USER_NAME_SIZE]; size_t name_size;
    char sid[6]; size_t sid_size;
    char email[USER_NAME_SIZE]; size_t email_size;
    char mobile[USER_NAME_SIZE]; size_t mobile_size;
    struct Group { char gid[USER_NAME_SIZE]; size_t gid_size; } groups[USER_GROUPS_MAX_SIZE]; size_t num_groups;
    struct Language { char lang[USER_NAME_SIZE]; size_t lang_size; } languages[1]; size_t num_languages;  // Pseudo array
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
    user.email_size = 0;
    user.mobile_size = 0;
    for (size_t i = 0; i < user.num_groups; i++)
        user.groups[i].gid_size = 0;
    user.num_groups = 0;
    for (size_t i = 0; i < user.num_languages; i++)
        user.languages[i].lang_size = 0;
    user.num_languages = 0;
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
    if (!name_size)
        return make_gsl_err_external(gsl_FORMAT);  // error: name is required, return gsl_FORMAT to match .buf & .parse cases
    if (name_size > sizeof self->name)
        return make_gsl_err_external(gsl_LIMIT);  // error: too long, return gsl_LIMIT to match .buf case
    if (self->name_size)
        return make_gsl_err_external(gsl_EXISTS);  // error: already specified, return gsl_EXISTS to match .buf case
    memcpy(self->name, name, name_size);
    self->name_size = name_size;
    return make_gsl_err(gsl_OK);
}

static gsl_err_t run_set_default_sid(void *obj, const char *val, size_t val_size) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(!val); ck_assert_uint_eq(val_size, 0);
    return make_gsl_err_external(gsl_FORMAT);  // error: sid is required, return gsl_FORMAT to match .buf & .run cases
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

    if (self->sid_size)
        return make_gsl_err_external(gsl_EXISTS);  // error: already specified, return gsl_EXISTS to match .buf & .run cases

    return gsl_parse_task(rec, total_size, specs, sizeof specs / sizeof specs[0]);
}

static gsl_err_t run_set_sid(void *obj, const char *sid, size_t sid_size) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(sid);
    if (!sid_size)
        return make_gsl_err_external(gsl_FORMAT);  // error: sid is required, return gsl_FORMAT to match .buf & .parse cases
    if (sid_size > sizeof self->sid)
        return make_gsl_err_external(gsl_LIMIT);
    if (self->sid_size)
        return make_gsl_err_external(gsl_EXISTS);  // error: already specified, return gsl_EXISTS to match .buf & .parse cases
    memcpy(self->sid, sid, sid_size);
    self->sid_size = sid_size;
    return make_gsl_err(gsl_OK);
}

static gsl_err_t run_set_default_email_or_mobile(void *obj, const char *val, size_t val_size) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(!val); ck_assert_uint_eq(val_size, 0);
    return make_gsl_err_external(gsl_FORMAT);  // error: email/mobile is required
}

static gsl_err_t parse_contacts(void *obj,
                                const char *name, size_t name_size,
                                const char *rec, size_t *total_size) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(name); ck_assert_uint_ne(name_size, 0);
    ck_assert(rec); ck_assert(total_size);

    if (name_size == strlen("email") && !memcmp(name, "email", name_size)) {
        if (self->email_size)
            return make_gsl_err_external(gsl_EXISTS);  // error: already specified

        struct gslTaskSpec specs[] = {
            {
              .is_implied = true,
              .buf = self->email,
              .buf_size = &self->email_size,
              .max_buf_size = sizeof self->email
            },
            {
              .is_default = true,
              .run = run_set_default_email_or_mobile,
              .obj = self
            }
        };
        return gsl_parse_task(rec, total_size, specs, sizeof specs / sizeof specs[0]);
    }
    else if (name_size == strlen("mobile") && !memcmp(name, "mobile", name_size)) {
        if (self->mobile_size)
            return make_gsl_err_external(gsl_EXISTS);  // error: already specified

        struct gslTaskSpec specs[] = {
            {
              .is_implied = true,
              .buf = self->mobile,
              .buf_size = &self->mobile_size,
              .max_buf_size = sizeof self->mobile
            },
            {
              .is_default = true,
              .run = run_set_default_email_or_mobile,
              .obj = self
            }
        };
        return gsl_parse_task(rec, total_size, specs, sizeof specs / sizeof specs[0]);
    }

    return make_gsl_err_external(gsl_NO_MATCH);  // error: unknown contact type
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

static gsl_err_t alloc_group_atomic(void *obj, const char *gid, size_t gid_size,
                                    size_t count, void **item) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(gid); ck_assert_uint_ne(gid_size, 0);
    ck_assert_uint_eq(count, self->num_groups); ck_assert(item);

    if (self->num_groups == sizeof self->groups / sizeof self->groups[0])
        return make_gsl_err(gsl_LIMIT);  // TODO(ki.stfu): ?? use gsl_NOMEM

    if (gid_size > sizeof self->groups[0].gid)
        return make_gsl_err(gsl_LIMIT);

    struct Group *group = &self->groups[self->num_groups++];
    memcpy(group->gid, gid, gid_size);
    group->gid_size = gid_size;
    *item = group;
    return make_gsl_err(gsl_OK);
}

static gsl_err_t alloc_group_non_atomic(void *obj, const char *gid, size_t gid_size,
                                        size_t count, void **item) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(!gid); ck_assert_uint_eq(gid_size, 0);
    ck_assert_uint_eq(count, self->num_groups); ck_assert(item);

    if (self->num_groups == sizeof self->groups / sizeof self->groups[0])
        return make_gsl_err(gsl_LIMIT);  // TODO(ki.stfu): ?? use gsl_NOMEM

    struct Group *group = &self->groups[self->num_groups++];
    *item = group;
    return make_gsl_err(gsl_OK);
}

static gsl_err_t append_group(void *accu, void *item) {
    return make_gsl_err(gsl_OK);
}

static gsl_err_t run_set_default_group_non_atomic(void *obj, const char *val, size_t val_size) {
    struct Group *self = (struct Group *)obj;
    ck_assert(self);
    ck_assert(!val); ck_assert_uint_eq(val_size, 0);
    return make_gsl_err(gsl_FORMAT);  // error: gid is required
}

static gsl_err_t parse_group_non_atomic(void *obj, const char *rec, size_t *total_size) {
    struct Group *self = (struct Group *)obj;
    ck_assert(self);
    ck_assert(rec); ck_assert(total_size);

    struct gslTaskSpec specs[] = {
        {
          .is_implied = true,
          .buf = self->gid,
          .buf_size = &self->gid_size,
          .max_buf_size = sizeof self->gid
        },
        {
          .is_default = true,
          .run = run_set_default_group_non_atomic,
          .obj = self
        }
    };

    return gsl_parse_task(rec, total_size, specs, sizeof specs / sizeof specs[0]);
}

static gsl_err_t alloc_language(void *obj, const char *lang, size_t lang_size,
                                size_t count, void **item) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(lang); ck_assert_uint_ne(lang_size, 0);
    ck_assert_uint_eq(count, self->num_languages); ck_assert(item);

    if (self->num_languages == sizeof self->languages / sizeof self->languages[0])
        return make_gsl_err(gsl_LIMIT);  // TODO(ki.stfu): ?? use gsl_NOMEM

    if (lang_size > sizeof self->languages[0].lang)
        return make_gsl_err(gsl_LIMIT);

    struct Language *language = &self->languages[self->num_languages++];
    memcpy(language->lang, lang, lang_size);
    language->lang_size = lang_size;
    *item = language;
    return make_gsl_err(gsl_OK);
}

static gsl_err_t append_language(void *accu, void *item) {
    return make_gsl_err(gsl_OK);
}

static gsl_err_t parse_skills(void *obj,
                              const char *name, size_t name_size,
                              const char *rec, size_t *total_size) {
    struct User *self = (struct User *)obj;
    ck_assert(self);
    ck_assert(name); ck_assert_uint_ne(name_size, 0);
    ck_assert(rec); ck_assert(total_size);

    if (name_size == strlen("languages") && !memcmp(name, "languages", name_size)) {
        if (self->num_languages)
            return make_gsl_err_external(gsl_EXISTS);  // error: already specified

        struct gslTaskSpec language_spec = {
            .is_list_item = true,
            .alloc = alloc_language,
            .append = append_language,
            .accu = obj
        };
        return gsl_parse_array(&language_spec, rec, total_size);
    }

    return make_gsl_err_external(gsl_NO_MATCH);  // error: unknown skill type
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
    return (struct gslTaskSpec){ .type = !(flags & SPEC_CHANGE) ? GSL_GET_STATE : GSL_SET_STATE,
                                 .name = "user", .name_size = strlen("user"),
                                 .parse = parse_user, .obj = args };
}

static struct gslTaskSpec gen_name_spec(struct User *self, int flags) {
    assert((flags & (SPEC_BUF | SPEC_RUN | SPEC_NAME | SPEC_SELECTOR | SPEC_CHANGE)) == flags &&
           "Valid flags: [SPEC_BUF | SPEC_RUN] [SPEC_NAME] [SPEC_SELECTOR] [SPEC_CHANGE]");
    if (flags & SPEC_RUN)
        return (struct gslTaskSpec){ .is_implied = true,
                                     .type = !(flags & SPEC_CHANGE) ? GSL_GET_STATE : GSL_SET_STATE,
                                     .name = (flags & SPEC_NAME) ? "name" : NULL, .name_size = (flags & SPEC_NAME) ? strlen("name") : 0,
                                     .is_selector = (flags & SPEC_SELECTOR),
                                     .run = run_set_name, .obj = self };
    return (struct gslTaskSpec){ .is_implied = true,
                                 .type = !(flags & SPEC_CHANGE) ? GSL_GET_STATE : GSL_SET_STATE,
                                 .name = (flags & SPEC_NAME) ? "name" : NULL, .name_size = (flags & SPEC_NAME) ? strlen("name") : 0,
                                 .is_selector = (flags & SPEC_SELECTOR),
                                 .buf = self->name, .buf_size = &self->name_size, .max_buf_size = sizeof self->name };
}

static struct gslTaskSpec gen_sid_spec(struct User *self, int flags) {
    assert((flags & (SPEC_BUF | SPEC_PARSE | SPEC_RUN | SPEC_SELECTOR | SPEC_CHANGE)) == flags &&
           "Valid flags: [SPEC_BUF | SPEC_PARSE | SPEC_RUN] [SPEC_SELECTOR] [SPEC_CHANGE]");
    if (flags & SPEC_PARSE)
        return (struct gslTaskSpec){ .type = !(flags & SPEC_CHANGE) ? GSL_GET_STATE : GSL_SET_STATE,
                                     .name = "sid", .name_size = strlen("sid"),
                                     .is_selector = (flags & SPEC_SELECTOR),
                                     .parse = parse_sid, .obj = self };
    if (flags & SPEC_RUN)
        return (struct gslTaskSpec){ .type = !(flags & SPEC_CHANGE) ? GSL_GET_STATE : GSL_SET_STATE,
                                     .name = "sid", .name_size = strlen("sid"),
                                     .is_selector = (flags & SPEC_SELECTOR),
                                     .run = run_set_sid, .obj = self };
    return (struct gslTaskSpec){ .type = !(flags & SPEC_CHANGE) ? GSL_GET_STATE : GSL_SET_STATE,
                                 .name = "sid", .name_size = strlen("sid"),
                                 .is_selector = (flags & SPEC_SELECTOR),
                                 .buf = self->sid, .buf_size = &self->sid_size, .max_buf_size = sizeof self->sid };
}

static struct gslTaskSpec gen_contacts_spec(struct User *self, int flags) {
    assert((flags & (SPEC_SELECTOR | SPEC_CHANGE)) == flags &&
           "Valid flags: [SPEC_SELECTOR] [SPEC_CHANGE]");
    return (struct gslTaskSpec){ .type = !(flags & SPEC_CHANGE) ? GSL_GET_STATE : GSL_SET_STATE,
                                 .is_validator = true,
                                 .is_selector = (flags & SPEC_SELECTOR),
                                 .validate = parse_contacts, .obj = self };
}

static struct gslTaskSpec gen_default_spec(struct User *self) {
    return (struct gslTaskSpec){ .is_default = true, .run = run_set_anonymous_user, .obj = self };
}

static struct gslTaskSpec gen_groups_item_spec(struct User *self, int flags) {
    assert((flags & SPEC_PARSE) == flags && "Valid flags: [SPEC_PARSE]");
    if (flags & SPEC_PARSE)
        return (struct gslTaskSpec){ .is_list_item = true,
                                     .alloc = alloc_group_non_atomic, .append = append_group, .accu = self,
                                     .parse = parse_group_non_atomic };
    return (struct gslTaskSpec){ .is_list_item = true,
                                 .alloc = alloc_group_atomic, .append = append_group, .accu = self };
}

static struct gslTaskSpec gen_groups_spec(struct gslTaskSpec* item_spec, int flags) {
    assert((flags & SPEC_SELECTOR) == flags && "Valid flags: [SPEC_SELECTOR]");
    return (struct gslTaskSpec){ .type = GSL_SET_ARRAY_STATE,
                                 .name = "groups", .name_size = strlen("groups"),
                                 .is_selector = (flags & SPEC_SELECTOR),
                                 .parse = gsl_parse_array, .obj = item_spec };
}

static struct gslTaskSpec gen_skills_spec(struct User *self, int flags) {
    assert((flags & SPEC_SELECTOR) == flags && "Valid flags: [SPEC_SELECTOR]");
    return (struct gslTaskSpec){ .type = GSL_SET_ARRAY_STATE,
                                 .is_validator = true,
                                 .is_selector = (flags & SPEC_SELECTOR),
                                 .validate = parse_skills, .obj = self };
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
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.name_size, 0); ck_assert_uint_eq(user.sid_size, 0);
END_TEST

START_TEST(parse_task_empty_with_spaces)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "     ", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.name_size, 0); ck_assert_uint_eq(user.sid_size, 0);
END_TEST

START_TEST(parse_task_empty_with_closing_brace)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = " }     ", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

START_TEST(parse_task_with_spaces)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {name John Smith}}   ", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "   {user {name John Smith}}   ", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
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
}

START_TEST(parse_implied_field)
    // Case #1: .buf
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_BUF), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    check_parse_implied_field(specs, sizeof specs / sizeof specs[0], &parse_user_args);
  }

    // Case #2: .run
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_RUN), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    check_parse_implied_field(specs, sizeof specs / sizeof specs[0], &parse_user_args);
  }
END_TEST

START_TEST(parse_implied_field_with_name)
    // Case #1: .buf & .name
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_BUF | SPEC_NAME), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    check_parse_implied_field(specs, sizeof specs / sizeof specs[0], &parse_user_args);  // TODO(ki.stfu): ?? do check_parse_implied_field_with_name with only 2 cases, and remove the code below

    rc = gsl_parse_task(rec = "{user {name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0;  // reset
  }

    // Case #2: .run & .name
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

static void
check_parse_implied_field_with_spaces(int name_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, name_flags), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user   John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user   John Smith   }", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user    John Smith   {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0;  // reset
}

START_TEST(parse_implied_field_with_spaces)
    // Case #1: .buf
    check_parse_implied_field_with_spaces(SPEC_BUF);

    // Case #2: .run
    check_parse_implied_field_with_spaces(SPEC_RUN);
END_TEST

START_TEST(parse_implied_field_with_dashes)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user -John Smith-}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "-John Smith-");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user - John Smith -}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "- John Smith -");
END_TEST

static void
check_parse_implied_field_max_size(int name_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, name_flags), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

  {
    const char input[] = { '{', 'u', 's', 'e', 'r', ' ', [6 ... USER_NAME_SIZE + 5] = 'a', '}', '\0' };
    rc = gsl_parse_task(rec = input, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, strchr(input, 'a'), USER_NAME_SIZE);
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);
  }

  {
    const char input[] = { '{', 'u', 's', 'e', 'r', ' ', [6 ... USER_NAME_SIZE + 5] = 'a', '{', 's', 'i', 'd', ' ', '1', '2', '3', '4', '5', '6', '}', '}', '\0' };
    rc = gsl_parse_task(rec = input, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, strchr(input, 'a'), USER_NAME_SIZE);
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);
  }

  {
    const char input[] = { '{', 'u', 's', 'e', 'r', ' ', [6 ... USER_NAME_SIZE + 5] = 'a', ' ', '{', 's', 'i', 'd', ' ', '1', '2', '3', '4', '5', '6', '}', '}', '\0' };
    rc = gsl_parse_task(rec = input, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, strchr(input, 'a'), USER_NAME_SIZE);
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0;  // reset
  }
}

START_TEST(parse_implied_field_max_size)
    // Case #1: .buf
    check_parse_implied_field_max_size(SPEC_BUF);

    // Case #2: .run
    check_parse_implied_field_max_size(SPEC_RUN);
END_TEST

START_TEST(parse_implied_field_max_size_plus_one)
    const char input1[] = { '{', 'u', 's', 'e', 'r', ' ', [6 ... USER_NAME_SIZE + 6] = 'a', '}', '\0' };
    const char input2[] = { '{', 'u', 's', 'e', 'r', ' ', [6 ... USER_NAME_SIZE + 6] = 'a', '{', 's', 'i', 'd', ' ', '1', '2', '3', '4', '5', '6', '}', '}', '\0' };
    const char input3[] = { '{', 'u', 's', 'e', 'r', ' ', [6 ... USER_NAME_SIZE + 6] = 'a', ' ', '{', 's', 'i', 'd', ' ', '1', '2', '3', '4', '5', '6', '}', '}', '\0' };

    // Case #1: .buf
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_BUF));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = input1, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);

    rc = gsl_parse_task(rec = input2, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);

    rc = gsl_parse_task(rec = input3, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);
  }

    // Case #2: .run
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_RUN));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = input1, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_LIMIT);

    rc = gsl_parse_task(rec = input2, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_LIMIT);

    rc = gsl_parse_task(rec = input3, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_LIMIT);
  }
END_TEST

START_TEST(parse_implied_field_unknown)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, 0), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user John Smith{sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user John Smith {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

START_TEST(parse_implied_field_duplicate)
    // Case #1: .buf & .name
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_BUF | SPEC_NAME));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith{name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_EXISTS);
    user.name_size = 0; RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user John Smith {name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_EXISTS);
    user.name_size = 0; RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);
  }

    // Case #2: .run & .name
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_RUN | SPEC_NAME));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith{name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_EXISTS);
    user.name_size = 0; RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user John Smith {name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_EXISTS);
  }
END_TEST

static void
check_parse_implied_field_not_first(int name_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, name_flags), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456}John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
    user.sid_size = 0; RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {sid 123456} John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
    user.sid_size = 0;  // reset
}

START_TEST(parse_implied_field_not_first)
    // Case #1: .buf
    check_parse_implied_field_not_first(SPEC_BUF);

    // Case #2: .run
    check_parse_implied_field_not_first(SPEC_RUN);
END_TEST

START_TEST(parse_implied_field_with_braces)
    // Case #1: .buf
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_BUF));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user Jo{hn Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
    user.name_size = 0;  // reset
  }

    // Case #2: .run
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_RUN));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user Jo{hn Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
  }
END_TEST

START_TEST(parse_tag_empty)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{ John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{{name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
END_TEST

START_TEST(parse_tag)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user{name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
END_TEST

START_TEST(parse_tag_with_dashes)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME), gen_default_spec(&user));
    struct gslTaskSpec specs[] = {
        {  // Make an alias for user spec
            .name = "u-s-e-r-", .name_size = strlen("u-s-e-r-"),
            .parse = parse_user, .obj = &parse_user_args
        },
    };

    rc = gsl_parse_task(rec = "{u-s-e-r- John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
END_TEST

START_TEST(parse_tag_unknown)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{use John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{usero John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{use{name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{usero{name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{use}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{usero}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

START_TEST(parse_tag_with_leading_spaces)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{ user John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
END_TEST

START_TEST(parse_value)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user{name John Smith}{sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {name John Smith}{sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {name John Smith} {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
END_TEST

START_TEST(parse_value_with_spaces)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user   {name John Smith} {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user   {name John Smith}   {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user   {name John Smith}   {sid 123456}   }", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
END_TEST

START_TEST(parse_value_unordered)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME), gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456} {name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
END_TEST

START_TEST(parse_value_unmatched_type)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user (sid 123456)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user [sid 123456]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

static void
check_parse_value_unmatched_braces(int sid_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, sid_flags));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
    user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);  // FIXME(ki.stfu): don't reset specs[0].is_completed

    rc = gsl_parse_task(rec = "{user {sid 123456]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
    user.sid_size = 0;  // reset
}

START_TEST(parse_value_unmatched_braces)
    // Case #1: .buf  (terminal)
    check_parse_value_unmatched_braces(SPEC_BUF);

    // Case #2: .run  (terminal)
    check_parse_value_unmatched_braces(SPEC_RUN);

    // Case #3: .parse
    check_parse_value_unmatched_braces(SPEC_PARSE);

    // Case #4: change cases  // TODO(ki.stfu): move to GSL_SET_STATE cases
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user (sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user (sid 123456]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
  }
END_TEST

START_TEST(parse_value_absent_braces)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FAIL);  // TODO(ki.stfu): ?? use gsl_INCOMPLETE
END_TEST

START_TEST(parse_value_named_empty)
    // Case #1: .buf  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_BUF));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid{}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
  }

    // Case #2: .run  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_RUN));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid{}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);
  }

    // Case #3: .parse
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_PARSE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid{}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);
  }
END_TEST

START_TEST(parse_value_named)
    // Case #1: .buf  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_BUF));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0;  // reset
  }

    // Case #2: .run  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_RUN));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0;  // reset
  }

    // Case #3: .parse
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_PARSE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
  }
END_TEST

START_TEST(parse_value_named_with_spaces)
    // Case #1: .buf  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_BUF));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456   }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {sid   123456   }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0;  // reset
  }

    // Case #2: .run  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_RUN));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456   }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {sid   123456   }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0;  // reset
  }

    // Case #3: .parse
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_PARSE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456   }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {sid   123456   }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
  }
END_TEST

START_TEST(parse_value_named_with_dashes)
    // Case #1: .buf  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_BUF));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid -2345-}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "-2345-");
    user.sid_size = 0;  // reset
  }

    // Case #2: .run  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_RUN));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid -2345-}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "-2345-");
    user.sid_size = 0;  // reset
  }

    // Case #3: .parse
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_PARSE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid -2345-}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "-2345-");
  }
END_TEST

START_TEST(parse_value_named_max_size)
    const char input[] = { '{', 'u', 's', 'e', 'r', ' ', '{', 's', 'i', 'd', ' ', [11 ... sizeof user.sid + 10] = '1', '}', '}', '\0' };

    // Case #1: .buf  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_BUF));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = input, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, strchr(input, '1'), sizeof user.sid);
    user.sid_size = 0;  // reset
  }

    // Case #2: .run  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_RUN));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = input, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, strchr(input, '1'), sizeof user.sid);
    user.sid_size = 0;  // reset
  }

    // Case #3: .parse
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_PARSE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = input, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, strchr(input, '1'), sizeof user.sid);
  }
END_TEST

START_TEST(parse_value_named_max_size_plus_one)
    const char input[] = { '{', 'u', 's', 'e', 'r', ' ', '{', 's', 'i', 'd', ' ', [11 ... sizeof user.sid + 11] = '1', '}', '}', '\0' };

    // Case #1: .buf  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_BUF));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = input, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);
  }

    // Case #2: .run  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_RUN));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = input, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_LIMIT);
  }

    // Case #3: .parse
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_PARSE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = input, &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_LIMIT);
  }
END_TEST

START_TEST(parse_value_named_unknown)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, 0), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {si 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user {sido 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

START_TEST(parse_value_named_duplicate)
    // Case #1: .buf  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_BUF));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456} {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_EXISTS);
    user.sid_size = 0;  // reset
  }

    // Case #2: .run  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_RUN));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456} {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_EXISTS);
    user.sid_size = 0;  // reset
  }

    // Case #3: .parse
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_PARSE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456} {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_EXISTS);
  }
END_TEST

START_TEST(parse_value_named_with_braces)
    // Case #1: .buf  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_BUF));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid{123456}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid {123456}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid 123{456}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
  }

    // Case #2: .run  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_RUN));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid{123456}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid {123456}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {sid 123{456}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
  }

    // Case #3: .parse
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_PARSE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid{123456}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user {sid {123456}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user {sid 123{456}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
  }
END_TEST

START_TEST(parse_value_validate_empty)
    DEFINE_TaskSpecs(parse_user_args, gen_contacts_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {email }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {email{}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {email}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {mobile }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {mobile{}}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user {mobile}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);
END_TEST

START_TEST(parse_value_validate)
    DEFINE_TaskSpecs(parse_user_args, gen_contacts_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {email john@iserver.com}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
    user.email_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {mobile +1 724-227-0844}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.mobile, user.mobile_size, "+1 724-227-0844");
    user.mobile_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {email john@iserver.com} {mobile +1 724-227-0844}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
    ASSERT_STR_EQ(user.mobile, user.mobile_size, "+1 724-227-0844");
END_TEST

START_TEST(parse_value_validate_unknown)
    DEFINE_TaskSpecs(parse_user_args, gen_contacts_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {emai john@iserver.com}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user {emailo john@iserver.com}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user {mobil +1 724-227-0844}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user {mobileo +1 724-227-0844}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_NO_MATCH);
END_TEST

START_TEST(parse_value_validate_duplicate)
    DEFINE_TaskSpecs(parse_user_args, gen_contacts_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {email john@iserver.com} {email j.smith@gogel.com}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_EXISTS);
    user.email_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {mobile +1 724-227-0844} {mobile +1 410-848-4981}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_EXISTS);
END_TEST

START_TEST(parse_value_validate_unordered)
    DEFINE_TaskSpecs(parse_user_args, gen_contacts_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {email john@iserver.com} {mobile +1 724-227-0844}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
    ASSERT_STR_EQ(user.mobile, user.mobile_size, "+1 724-227-0844");
    user.email_size = 0; user.mobile_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {mobile +1 724-227-0844} {email john@iserver.com}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
    ASSERT_STR_EQ(user.mobile, user.mobile_size, "+1 724-227-0844");
END_TEST

START_TEST(parse_value_default_no_spec)
    DEFINE_TaskSpecs(parse_user_args);
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
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
check_parse_value_default_when_value_named(int sid_flags) {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, sid_flags), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0;  // reset
}

START_TEST(parse_value_default)
    // Case #1: no fields
  {
    DEFINE_TaskSpecs(parse_user_args, gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
    user.name_size = 0;  // reset
  }

    // Case #2: implied field
    check_parse_value_default_when_implied_field(SPEC_BUF);
    check_parse_value_default_when_implied_field(SPEC_RUN);

    // Case #3: value named
    check_parse_value_default_when_value_named(SPEC_BUF);
    check_parse_value_default_when_value_named(SPEC_RUN);
    check_parse_value_default_when_value_named(SPEC_PARSE);

    // Case #4: value validate
  {
    DEFINE_TaskSpecs(parse_user_args, gen_contacts_spec(&user, 0), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {email john@iserver.com}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
    user.email_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {mobile +1 724-227-0844}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.mobile, user.mobile_size, "+1 724-227-0844");
  }
END_TEST

START_TEST(parse_value_default_with_selectors)
    // Case #1: implied field with .is_selector
    // Case #2: implied field with .is_selector, and value named
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_SELECTOR), gen_sid_spec(&user, 0), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user John Smith {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0;  // reset
  }

    // Case #3: implied field with .is_selector, and value named with .is_selector
    // Case #4: implied field with .is_selector, and value named with .is_selector, and value validate
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_SELECTOR), gen_sid_spec(&user, SPEC_SELECTOR), gen_contacts_spec(&user, 0), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith {sid 123456}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user John Smith {sid 123456} {email john@iserver.com}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
    user.name_size = 0; user.sid_size = 0; user.email_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user John Smith {sid 123456} {mobile +1 724-227-0844}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    ASSERT_STR_EQ(user.mobile, user.mobile_size, "+1 724-227-0844");
    user.name_size = 0; user.sid_size = 0; user.mobile_size = 0;  // reset
  }

    // Case #5: implied field with .is_selector, and value named with .is_selector, and value validate with .is_selector
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_SELECTOR), gen_sid_spec(&user, SPEC_SELECTOR), gen_contacts_spec(&user, SPEC_SELECTOR), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user John Smith {sid 123456} {email john@iserver.com} {mobile +1 724-227-0844}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");  // TODO(ki.stfu): ?? allow .is_selector with .validate
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
    ASSERT_STR_EQ(user.mobile, user.mobile_size, "+1 724-227-0844");
  }
END_TEST

START_TEST(parse_comment_empty)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user{-}{name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {-}{name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {-} {name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {name John Smith}{-}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {name John Smith} {-}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
END_TEST

START_TEST(parse_comment)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {-name John Doe}{name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {-name John Doe} {name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {name John Smith}{-name John Doe}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {name John Smith} {-name John Doe}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
END_TEST

START_TEST(parse_comment_with_spaces)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user   {-  name John Doe  }{name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user   {-  name John Doe  }   {name John Smith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {name John Smith}   {-  name John Doe  }}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user {name John Smith}   {-  name John Doe  }   }", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
END_TEST

START_TEST(parse_comment_unmatched_braces)
    DEFINE_TaskSpecs(parse_user_args, gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {-name John Smith)]}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
END_TEST

// --------------------------------------------------------------------------------
// GSL_SET_STATE cases

static void
check_parse_change_implied_field(struct gslTaskSpec *specs,
                                 size_t num_specs,
                                 struct TaskSpecs *parse_user_args) {
    rc = gsl_parse_task(rec = "(user John Smith)", &total_size, specs, num_specs);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED(specs, num_specs); RESET_IS_COMPLETED_TaskSpecs(parse_user_args);

    rc = gsl_parse_task(rec = "(user John Smith(sid 123456))", &total_size, specs, num_specs);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED(specs, num_specs); RESET_IS_COMPLETED_TaskSpecs(parse_user_args);

    rc = gsl_parse_task(rec = "(user John Smith (sid 123456))", &total_size, specs, num_specs);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED(specs, num_specs); RESET_IS_COMPLETED_TaskSpecs(parse_user_args);
}

START_TEST(parse_change_implied_field)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0/*SPEC_CHANGE not allowed*/), gen_sid_spec(&user, SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    check_parse_change_implied_field(specs, sizeof specs / sizeof specs[0], &parse_user_args);
END_TEST

START_TEST(parse_change_implied_field_with_name)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME | SPEC_CHANGE), gen_sid_spec(&user, SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    check_parse_change_implied_field(specs, sizeof specs / sizeof specs[0], &parse_user_args);

    rc = gsl_parse_task(rec = "(user (name John Smith))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);
END_TEST

START_TEST(parse_change_implied_field_with_braces)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0/*SPEC_CHANGE not allowed*/));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user Jo(hn Smith))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

START_TEST(parse_change_tag_empty)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME | SPEC_CHANGE), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "( John Smith)", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "((name John Smith))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "()", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
END_TEST

START_TEST(parse_change_tag)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME | SPEC_CHANGE), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user John Smith)", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user(name John Smith))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user)", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
END_TEST

START_TEST(parse_change_value)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME | SPEC_CHANGE), gen_sid_spec(&user, SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user(name John Smith)(sid 123456))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user (name John Smith)(sid 123456))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user (name John Smith) (sid 123456))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
END_TEST

START_TEST(parse_change_value_unmatched_type)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user {sid 123456})", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "(user [sid 123456])", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

START_TEST(parse_change_value_unmatched_braces)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user (sid 123456})", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "(user (sid 123456])", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
END_TEST

START_TEST(parse_change_value_named_empty)
    // Case #1: .buf  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_BUF | SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user (sid ))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "(user (sid()))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "(user (sid))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
  }

    // Case #2: .run  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_RUN | SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user (sid ))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);

    rc = gsl_parse_task(rec = "(user (sid()))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "(user (sid))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);
  }

    // Case #3: .parse
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_PARSE | SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user (sid ))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);

    rc = gsl_parse_task(rec = "(user (sid()))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "(user (sid))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);
  }
END_TEST

START_TEST(parse_change_value_named)
    // Case #1: .buf  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_BUF | SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user (sid 123456))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0;  // reset
  }

    // Case #2: .run  (terminal)
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_RUN | SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user (sid 123456))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0;  // reset
  }

    // Case #3: .parse
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_PARSE | SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user (sid 123456))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
  }
END_TEST

START_TEST(parse_change_value_named_with_braces)
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user (sid(123456)))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "(user (sid (123456)))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "(user (sid 123(456)))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
END_TEST

START_TEST(parse_change_value_validate_empty)
    DEFINE_TaskSpecs(parse_user_args, gen_contacts_spec(&user, SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user (email ))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);

    rc = gsl_parse_task(rec = "(user (email()))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "(user (email))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);

    rc = gsl_parse_task(rec = "(user (mobile ))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);

    rc = gsl_parse_task(rec = "(user (mobile()))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "(user (mobile))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_FORMAT);
END_TEST

START_TEST(parse_change_value_validate)
    DEFINE_TaskSpecs(parse_user_args, gen_contacts_spec(&user, SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user (email john@iserver.com))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
    user.email_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user (mobile +1 724-227-0844))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.mobile, user.mobile_size, "+1 724-227-0844");
    user.mobile_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user (email john@iserver.com) (mobile +1 724-227-0844))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
    ASSERT_STR_EQ(user.mobile, user.mobile_size, "+1 724-227-0844");
END_TEST

START_TEST(parse_change_value_default_no_spec)
    DEFINE_TaskSpecs(parse_user_args);
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user)", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

START_TEST(parse_change_value_default)
    // Case #1: no fields
  {
    DEFINE_TaskSpecs(parse_user_args, gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user)", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
    user.name_size = 0;  // reset
  }

    // Case #2: implied field
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, 0), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user John Smith)", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0;  // reset
  }

    // Case #3: value named
  {
    DEFINE_TaskSpecs(parse_user_args, gen_sid_spec(&user, SPEC_CHANGE), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user (sid 123456))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.sid_size = 0;  // reset
  }

    // Case #4: value validate
  {
    DEFINE_TaskSpecs(parse_user_args, gen_contacts_spec(&user, SPEC_CHANGE), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user (email john@iserver.com))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
    user.email_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user (mobile +1 724-227-0844))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.mobile, user.mobile_size, "+1 724-227-0844");
  }
END_TEST

START_TEST(parse_change_value_default_with_selectors)
    // Case #1: implied field with .is_selector
    // Case #2: implied field with .is_selector, and value named
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_SELECTOR), gen_sid_spec(&user, SPEC_CHANGE), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user John Smith)", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user John Smith (sid 123456))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0;  // reset
  }

    // Case #3: implied field with .is_selector, and value named with .is_selector
    // Case #4: implied field with .is_selector, and value named with .is_selector, and value validate
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_SELECTOR), gen_sid_spec(&user, SPEC_SELECTOR | SPEC_CHANGE), gen_contacts_spec(&user, SPEC_CHANGE), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user John Smith (sid 123456))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    user.name_size = 0; user.sid_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user John Smith (sid 123456) (email john@iserver.com))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
    user.name_size = 0; user.sid_size = 0; user.email_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user John Smith (sid 123456) (mobile +1 724-227-0844))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    ASSERT_STR_EQ(user.mobile, user.mobile_size, "+1 724-227-0844");
    user.name_size = 0; user.sid_size = 0; user.mobile_size = 0;  // reset
  }

    // Case #5: implied field with .is_selector, and value named with .is_selector, and value validate with .is_selector
  {
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_SELECTOR), gen_sid_spec(&user, SPEC_SELECTOR | SPEC_CHANGE), gen_contacts_spec(&user, SPEC_SELECTOR | SPEC_CHANGE), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user John Smith (sid 123456) (email john@iserver.com) (mobile +1 724-227-0844))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");  // TODO(ki.stfu): ?? allow .is_selector with .validate
    ASSERT_STR_EQ(user.sid, user.sid_size, "123456");
    ASSERT_STR_EQ(user.email, user.email_size, "john@iserver.com");
    ASSERT_STR_EQ(user.mobile, user.mobile_size, "+1 724-227-0844");
  }
END_TEST

START_TEST(parse_change_comment_empty)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME | SPEC_CHANGE));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user(-)(name John Smith))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user (-)(name John Smith))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user (-) (name John Smith))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user (name John Smith)(-))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user (name John Smith) (-))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
END_TEST

START_TEST(parse_change_comment)
    DEFINE_TaskSpecs(parse_user_args, gen_name_spec(&user, SPEC_NAME | SPEC_CHANGE), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user (-name John Doe)(name John Smith))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user (-name John Doe) (name John Smith))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user (name John Smith)(-name John Doe))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
    user.name_size = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "(user (name John Smith) (-name John Doe))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "John Smith");
END_TEST

START_TEST(parse_change_comment_unmatched_braces)
    DEFINE_TaskSpecs(parse_user_args, gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, SPEC_CHANGE) };

    rc = gsl_parse_task(rec = "(user (-name John Smith}]))", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
END_TEST

// --------------------------------------------------------------------------------
// GSL_SET_ARRAY_STATE cases

START_TEST(parse_array_tag_empty)
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, 0);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [ ]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user [{}]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user []}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
END_TEST

START_TEST(parse_array_tag)
  {
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, 0);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [groups jsmith]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
    user.groups[0].gid_size = 0; user.num_groups = 0;  // reset
  }

  {
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, SPEC_PARSE);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [groups{jsmith}]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
    user.groups[0].gid_size = 0; user.num_groups = 0;  // reset
  }

  {
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, 0);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [groups]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 0);
  }
END_TEST

START_TEST(parse_array_value_empty)
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, 0);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user[groups]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 0);
    RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 0);
END_TEST

START_TEST(parse_array_value_empty_with_spaces)
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, 0);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [groups   ]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 0);
    RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups   ]   }", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 0);
END_TEST

START_TEST(parse_array_value_selector)
    // Case #1: atomic & non-atomic values
  {
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, 0);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, SPEC_SELECTOR), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [groups jsmith]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
    user.name_size = 0; user.groups[0].gid_size = 0; user.num_groups = 0;  // reset
  }

    // Case #2: value validate
  {
    DEFINE_TaskSpecs(parse_user_args, gen_skills_spec(&user, SPEC_SELECTOR), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [languages english]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
    ck_assert_uint_eq(user.num_languages, 1); ASSERT_STR_EQ(user.languages[0].lang, user.languages[0].lang_size, "english");
  }
END_TEST

START_TEST(parse_array_value_unmatched_type)
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, 0);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user {groups}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user (groups)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_NO_MATCH);
END_TEST

START_TEST(parse_array_value_unmatched_braces)
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, 0);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [groups}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user [groups jsmith}}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
    user.groups[0].gid_size = 0; user.num_groups = 0;  // reset

    rc = gsl_parse_task(rec = "{user [groups)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user [groups jsmith)}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);
END_TEST

START_TEST(parse_array_value_absent_braces)
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, 0);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [groups", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FAIL);  // TODO(ki.stfu): ?? use gsl_INCOMPLETE
    RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);  // TODO(ki.stfu): remove this

    rc = gsl_parse_task(rec = "{user [groups jsmith", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FAIL);  // TODO(ki.stfu): ?? use gsl_INCOMPLETE
END_TEST

START_TEST(parse_array_value_atomic)
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, 0);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [groups jsmith]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
    user.groups[0].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups jsmith audio]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 2); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith"); ASSERT_STR_EQ(user.groups[1].gid, user.groups[1].gid_size, "audio");
    user.groups[0].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups jsmith audio sudo]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 3); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith"); ASSERT_STR_EQ(user.groups[1].gid, user.groups[1].gid_size, "audio"); ASSERT_STR_EQ(user.groups[2].gid, user.groups[2].gid_size, "sudo");
END_TEST

START_TEST(parse_array_value_atomic_with_spaces)
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, 0);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [groups   jsmith]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
    user.groups[0].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups   jsmith   audio]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 2); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith"); ASSERT_STR_EQ(user.groups[1].gid, user.groups[1].gid_size, "audio");
    user.groups[0].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups   jsmith   audio   sudo]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 3); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith"); ASSERT_STR_EQ(user.groups[1].gid, user.groups[1].gid_size, "audio"); ASSERT_STR_EQ(user.groups[2].gid, user.groups[2].gid_size, "sudo");
    user.groups[0].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups   jsmith   audio   sudo   ]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 3); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith"); ASSERT_STR_EQ(user.groups[1].gid, user.groups[1].gid_size, "audio"); ASSERT_STR_EQ(user.groups[2].gid, user.groups[2].gid_size, "sudo");
END_TEST

START_TEST(parse_array_value_non_atomic)
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, SPEC_PARSE);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [groups{jsmith}]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
    user.groups[0].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups {jsmith}{audio}]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 2); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith"); ASSERT_STR_EQ(user.groups[1].gid, user.groups[1].gid_size, "audio");
    user.groups[0].gid_size = 0; user.groups[1].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups {jsmith} {audio}{sudo}]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 3); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith"); ASSERT_STR_EQ(user.groups[1].gid, user.groups[1].gid_size, "audio"); ASSERT_STR_EQ(user.groups[2].gid, user.groups[2].gid_size, "sudo");
    user.groups[0].gid_size = 0; user.groups[1].gid_size = 0; user.groups[2].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups {jsmith} {audio} {sudo}]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 3); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith"); ASSERT_STR_EQ(user.groups[1].gid, user.groups[1].gid_size, "audio"); ASSERT_STR_EQ(user.groups[2].gid, user.groups[2].gid_size, "sudo");
END_TEST

START_TEST(parse_array_value_non_atomic_with_spaces)
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, SPEC_PARSE);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [groups   {jsmith}]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
    user.groups[0].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups   {jsmith}   {audio}]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 2); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith"); ASSERT_STR_EQ(user.groups[1].gid, user.groups[1].gid_size, "audio");
    user.groups[0].gid_size = 0; user.groups[1].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups   {jsmith}   {audio}   {sudo}]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 3); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith"); ASSERT_STR_EQ(user.groups[1].gid, user.groups[1].gid_size, "audio"); ASSERT_STR_EQ(user.groups[2].gid, user.groups[2].gid_size, "sudo");
    user.groups[0].gid_size = 0; user.groups[1].gid_size = 0; user.groups[2].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups   {jsmith}   {audio}   {sudo}   ]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 3); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith"); ASSERT_STR_EQ(user.groups[1].gid, user.groups[1].gid_size, "audio"); ASSERT_STR_EQ(user.groups[2].gid, user.groups[2].gid_size, "sudo");
END_TEST

START_TEST(parse_array_value_validate_empty)
    DEFINE_TaskSpecs(parse_user_args, gen_skills_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [languages ]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_languages, 0);
    RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [languages{}]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_FORMAT);

    rc = gsl_parse_task(rec = "{user [languages]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_languages, 0);
END_TEST

START_TEST(parse_array_value_validate)
    DEFINE_TaskSpecs(parse_user_args, gen_skills_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [languages english]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_languages, 1); ASSERT_STR_EQ(user.languages[0].lang, user.languages[0].lang_size, "english");
END_TEST

START_TEST(parse_array_value_validate_unknown)
    DEFINE_TaskSpecs(parse_user_args, gen_skills_spec(&user, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [language english]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_NO_MATCH);

    rc = gsl_parse_task(rec = "{user [languageso english]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert(is_gsl_err_external(rc)); ck_assert_int_eq(gsl_err_external_to_ext_code(rc), gsl_NO_MATCH);
END_TEST

START_TEST(parse_array_comment_empty)
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, 0);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user[-][groups jsmith]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
    user.groups[0].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [-][groups jsmith]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
    user.groups[0].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [-] [groups jsmith]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
    user.groups[0].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups jsmith][-]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
    user.groups[0].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups jsmith] [-]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
END_TEST

START_TEST(parse_array_comment)
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, 0);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [-groups jdoe][groups jsmith]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
    user.groups[0].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [-groups jdoe] [groups jsmith]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
    user.groups[0].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups jsmith][-groups jdoe]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
    user.groups[0].gid_size = 0; user.num_groups = 0; RESET_IS_COMPLETED_gslTaskSpec(specs); RESET_IS_COMPLETED_TaskSpecs(&parse_user_args);

    rc = gsl_parse_task(rec = "{user [groups jsmith] [-groups jdoe]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ck_assert_uint_eq(user.num_groups, 1); ASSERT_STR_EQ(user.groups[0].gid, user.groups[0].gid_size, "jsmith");
END_TEST

START_TEST(parse_array_comment_unmatched_braces)
    struct gslTaskSpec groups_item_spec = gen_groups_item_spec(&user, 0);
    DEFINE_TaskSpecs(parse_user_args, gen_groups_spec(&groups_item_spec, 0), gen_default_spec(&user));
    struct gslTaskSpec specs[] = { gen_user_spec(&parse_user_args, 0) };

    rc = gsl_parse_task(rec = "{user [-groups jdoe})]}", &total_size, specs, sizeof specs / sizeof specs[0]);
    ck_assert_int_eq(rc.code, gsl_OK);
    ck_assert_uint_eq(total_size, strlen(rec));
    ASSERT_STR_EQ(user.name, user.name_size, "(none)");
END_TEST

// --------------------------------------------------------------------------------
// main

// TODO(ki.stfu): Rename SPEC_CHANGE to SPEC_SET_STATE
// TODO(ki.stfu): Rename parse_change_* to parse_set_*

int main() {
    Suite* s = suite_create("suite");

    TCase* tc_get = tcase_create("get cases");
    tcase_add_checked_fixture(tc_get, test_case_fixture_setup, NULL);
    tcase_add_test(tc_get, parse_task_empty);
    tcase_add_test(tc_get, parse_task_empty_with_spaces);
    tcase_add_test(tc_get, parse_task_empty_with_closing_brace);
    tcase_add_test(tc_get, parse_task_with_spaces);
    tcase_add_test(tc_get, parse_implied_field);
    tcase_add_test(tc_get, parse_implied_field_with_name);
    tcase_add_test(tc_get, parse_implied_field_with_spaces);
    tcase_add_test(tc_get, parse_implied_field_with_dashes);
    tcase_add_test(tc_get, parse_implied_field_max_size);
    tcase_add_test(tc_get, parse_implied_field_max_size_plus_one);
    tcase_add_test(tc_get, parse_implied_field_unknown);
    tcase_add_test(tc_get, parse_implied_field_duplicate);
    tcase_add_test(tc_get, parse_implied_field_not_first);
    tcase_add_test(tc_get, parse_implied_field_with_braces);
    tcase_add_test(tc_get, parse_tag_empty);
    tcase_add_test(tc_get, parse_tag);
    tcase_add_test(tc_get, parse_tag_with_dashes);
    tcase_add_test(tc_get, parse_tag_unknown);
    tcase_add_test(tc_get, parse_tag_with_leading_spaces);
    tcase_add_test(tc_get, parse_value);
    tcase_add_test(tc_get, parse_value_with_spaces);
    tcase_add_test(tc_get, parse_value_unordered);
    tcase_add_test(tc_get, parse_value_unmatched_type);
    tcase_add_test(tc_get, parse_value_unmatched_braces);
    tcase_add_test(tc_get, parse_value_absent_braces);
    tcase_add_test(tc_get, parse_value_named_empty);
    tcase_add_test(tc_get, parse_value_named);
    tcase_add_test(tc_get, parse_value_named_with_spaces);
    tcase_add_test(tc_get, parse_value_named_with_dashes);
    tcase_add_test(tc_get, parse_value_named_max_size);
    tcase_add_test(tc_get, parse_value_named_max_size_plus_one);
    tcase_add_test(tc_get, parse_value_named_unknown);
    tcase_add_test(tc_get, parse_value_named_duplicate);
    tcase_add_test(tc_get, parse_value_named_with_braces);
    tcase_add_test(tc_get, parse_value_validate_empty);
    tcase_add_test(tc_get, parse_value_validate);
    tcase_add_test(tc_get, parse_value_validate_unknown);
    tcase_add_test(tc_get, parse_value_validate_duplicate);
    tcase_add_test(tc_get, parse_value_validate_unordered);
    tcase_add_test(tc_get, parse_value_default_no_spec);
    tcase_add_test(tc_get, parse_value_default);
    tcase_add_test(tc_get, parse_value_default_with_selectors);
    tcase_add_test(tc_get, parse_comment_empty);
    tcase_add_test(tc_get, parse_comment);
    tcase_add_test(tc_get, parse_comment_with_spaces);
    tcase_add_test(tc_get, parse_comment_unmatched_braces);
    suite_add_tcase(s, tc_get);

    TCase* tc_change = tcase_create("change cases");
    tcase_add_checked_fixture(tc_change, test_case_fixture_setup, NULL);
    tcase_add_test(tc_change, parse_change_implied_field);
    tcase_add_test(tc_change, parse_change_implied_field_with_name);
    tcase_add_test(tc_change, parse_change_implied_field_with_braces);
    tcase_add_test(tc_change, parse_change_tag_empty);
    tcase_add_test(tc_change, parse_change_tag);
    tcase_add_test(tc_change, parse_change_value);
    tcase_add_test(tc_change, parse_change_value_unmatched_type);
    tcase_add_test(tc_change, parse_change_value_unmatched_braces);
    tcase_add_test(tc_change, parse_change_value_named_empty);
    tcase_add_test(tc_change, parse_change_value_named);
    tcase_add_test(tc_change, parse_change_value_named_with_braces);
    tcase_add_test(tc_change, parse_change_value_validate_empty);
    tcase_add_test(tc_change, parse_change_value_validate);
    tcase_add_test(tc_change, parse_change_value_default_no_spec);
    tcase_add_test(tc_change, parse_change_value_default);
    tcase_add_test(tc_change, parse_change_value_default_with_selectors);
    tcase_add_test(tc_change, parse_change_comment_empty);
    tcase_add_test(tc_change, parse_change_comment);
    tcase_add_test(tc_change, parse_change_comment_unmatched_braces);
    suite_add_tcase(s, tc_change);

    TCase* tc_array = tcase_create("array cases");
    tcase_add_checked_fixture(tc_array, test_case_fixture_setup, NULL);
    tcase_add_test(tc_array, parse_array_tag_empty);
    tcase_add_test(tc_array, parse_array_tag);
    tcase_add_test(tc_array, parse_array_value_empty);
    tcase_add_test(tc_array, parse_array_value_empty_with_spaces);
    tcase_add_test(tc_array, parse_array_value_selector);
    tcase_add_test(tc_array, parse_array_value_unmatched_type);
    tcase_add_test(tc_array, parse_array_value_unmatched_braces);
    tcase_add_test(tc_array, parse_array_value_absent_braces);
    tcase_add_test(tc_array, parse_array_value_atomic);
    tcase_add_test(tc_array, parse_array_value_atomic_with_spaces);
    tcase_add_test(tc_array, parse_array_value_non_atomic);
    tcase_add_test(tc_array, parse_array_value_non_atomic_with_spaces);
    tcase_add_test(tc_array, parse_array_value_validate_empty);
    tcase_add_test(tc_array, parse_array_value_validate);
    tcase_add_test(tc_array, parse_array_value_validate_unknown);
    tcase_add_test(tc_array, parse_array_comment_empty);
    tcase_add_test(tc_array, parse_array_comment);
    tcase_add_test(tc_array, parse_array_comment_unmatched_braces);
    suite_add_tcase(s, tc_array);

    SRunner* sr = srunner_create(s);
    //srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    srunner_free(sr);
    return 0;
}
