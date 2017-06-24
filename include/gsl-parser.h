#pragma once

#include <stdlib.h>

typedef int (*kndParserCallBack)(void *self, const char *rec, size_t *total);

struct kndGslSpec
{
    const char *name;
    size_t name_size;

    char *buffer;
    size_t *buffer_size;

    kndParserCallBack cb;
    void *cb_arg;
};

struct kndGslParser
{
    const struct kndGslSpec *specs;
    size_t specs_num;

    int (*parse)(struct kndGslParser *self, const char *buffer, size_t buffer_size);
};


int kndGslParser_new(struct kndGslParser **parser, struct kndGslSpec *specs, size_t specs_num);
int kndGslParser_delete(struct kndGslParser *self);




//int kndGslParser_init(struct kndGslParser *self);
//int kndGslParser_destroy(struct kndGslParser *self);

int kndGslParser_get_trailer(const char  *rec, size_t rec_size,
                             char  *name, size_t *name_size,
                             size_t *num_items,
                             char   *dir_rec, size_t *dir_rec_size);


int kndGslParser_read_UTF8_char(const char *rec, size_t rec_size,
                                size_t *val, size_t *len);

int kndGslParser_parse_matching_braces(const char *rec, size_t *chunk_size);

int kndGslParser_remove_nonprintables(char *data);

int kndGslParser_parse_num(const char *val, long *result);

int kndGslParser_read_name(char *output, size_t *output_size,
                           const char *rec, size_t rec_size);

int kndGslParser_parse_IPV4(char *ip, unsigned long *ip_val);

int kndGslParser_parse(const char *rec, size_t *total_size,
                       struct kndGslSpec *specs, size_t num_specs);

int kndGslParser_get_schema_name(const char *rec, char *buf, size_t *buf_size,
                                 size_t *total_size);