#ifndef __UTIL_H__
#define __UTIL_H__


#include <fcntl.h>
#include <json.h>
#include <stdarg.h>
#include <stddef.h>
#include <threads.h>

#include "config.h"
#include "picohttpparser.h"


#define ARGS_COUNT(...)                 (sizeof((const void *[]){__VA_ARGS__}) / sizeof(const void *))
#define FIELD_PARENT_PTR(T, FIELD, PTR) ((T *)(((char *)(PTR)) - offsetof(T, FIELD)))
#define MIN(a, b)                       (((a) < (b)) ? (a) : (b))
#define LEN(X)                          ((sizeof(X) / sizeof(*X)))

#define INT64_DIGITS_LEN  (24)
#define UINT64_DIGITS_LEN (24)


/*
 * cstr
 */
size_t cstr_copy(char dest[], const char src[]);
size_t cstr_copy_n(char dest[], size_t size, const char src[]);
size_t cstr_copy_n2(char dest[], size_t size, const char src[], size_t len);

size_t cstr_dup_n(char *dest[], const char src[], size_t len);

/* ret: ~0: equals */
int cstr_cmp_n(const char a[], const char b[], size_t b_len);
int cstr_cmp_n2(const char a[], size_t a_len, const char b[], size_t b_len);
int cstr_casecmp(const char a[], const char b[]);
int cstr_casecmp_n(const char a[], const char b[], size_t b_len);
int cstr_casecmp_n2(const char a[], size_t a_len, const char b[], size_t b_len);

char *cstr_trim_l(char dest[]);
char *cstr_trim_r(char dest[]);
char *cstr_escape(const char escape[], char c, const char src[]);

char *cstr_to_lower_n(char dest[], size_t len);

int cstr_to_int64(const char cstr[], int64_t *ret);
int cstr_to_int64_n(const char cstr[], size_t len, int64_t *ret);
int cstr_to_uint64_n(const char cstr[], size_t len, uint64_t *ret);

static inline int
cstr_is_empty(const char cstr[])
{
	return ((cstr == NULL) || (cstr[0] == '\0'));
}

int cstr_is_empty_and_n(size_t count, ...);
int cstr_is_empty_or_n(size_t count, ...);

#define CSTR_IS_EMPTY(...)    cstr_is_empty_and_n(ARGS_COUNT(__VA_ARGS__), __VA_ARGS__)
#define CSTR_IS_EMPTY_OR(...) cstr_is_empty_or_n(ARGS_COUNT(__VA_ARGS__), __VA_ARGS__)

static inline const char *
cstr_empty_if_null(const char cstr[])
{
	return (cstr == NULL) ? "" : cstr;
}

char *cstr_concat_n(size_t count, ...);

#define CSTR_CONCAT(...) cstr_concat_n(ARGS_COUNT(__VA_ARGS__), __VA_ARGS__)


/*
 * ArrayPtr
 */
typedef struct array_ptr {
	size_t   len;
	void   **items;
} ArrayPtr;

void array_ptr_init(ArrayPtr *a);
void array_ptr_deinit(ArrayPtr *a);
int  array_ptr_append(ArrayPtr *a, void *item);


/*
 * SpaceTokenizer
 */
typedef struct space_tokenizer {
	const char *value;
	unsigned    len;
} SpaceTokenizer;

const char *space_tokenizer_next(SpaceTokenizer *s, const char raw[]);


/*
 * DList
 */
typedef struct dlist_node {
	struct dlist_node *next;
	struct dlist_node *prev;
} DListNode;

typedef struct dlist {
	DListNode *first;
	DListNode *last;
	size_t     len;
} DList;

void       dlist_init(DList *d);
void       dlist_append(DList *d, DListNode *node);
void       dlist_prepend(DList *d, DListNode *node);
void       dlist_remove(DList *d, DListNode *node);
DListNode *dlist_pop(DList *d);


/*
 * Str
 */
typedef struct str {
	int     is_alloc;
	size_t  len;
	size_t  size;
	char   *cstr;
} Str;

int   str_init(Str *s, char buffer[], size_t size);
int   str_init_alloc(Str *s, size_t size);
void  str_deinit(Str *s);
char *str_append_n(Str *s, const char cstr[], size_t len);
char *str_append_c(Str *s, char c);
char *str_set_n(Str *s, const char cstr[], size_t len);
char *str_set_fmt(Str *s, const char fmt[], ...);
char *str_append_fmt(Str *s, const char fmt[], ...);
char *str_pop(Str *s, size_t count);
int   str_reset(Str *s, size_t offt);
char *str_dup(Str *s);


/*
 * CstrMap
 */
typedef struct cstr_map_item {
	const char *key;
	void       *value;
} CstrMapItem;

typedef struct cstr_map {
	size_t       size;
	CstrMapItem *items;
} CstrMap;

int   cstr_map_init(CstrMap *c, size_t size);
void  cstr_map_deinit(CstrMap *c);
int   cstr_map_set(CstrMap *c, const char key[], void *value);
void *cstr_map_get(CstrMap *c, const char key[]);


/*
 * int
 */
static inline int
int64_is_bool(int64_t val)
{
	return ((val == 0) || (val == 1));
}


/*
 * misc
 */
int val_is_null_and_n(size_t count, ...);
int val_is_null_or_n(size_t count, ...);

#define VAL_IS_NULL(...)    val_is_null_and_n(ARGS_COUNT(__VA_ARGS__), __VA_ARGS__)
#define VAL_IS_NULL_OR(...) val_is_null_or_n(ARGS_COUNT(__VA_ARGS__), __VA_ARGS__)

int  args_parse(char *args[], unsigned size, unsigned *out_len, const char src[]);
void args_free(char *args[], unsigned len);


/*
 * Chld
 */
int  chld_init(const char path[], const char log_file[]);
void chld_deinit(void);
int  chld_add_env(const char key_value[]);
int  chld_add_env_kv(const char key[], const char val[]);
int  chld_spawn(const char file[], char *const argv[]);
void chld_reap(void);
void chld_wait_all(void);


/*
 * Http
 */
#define HTTP_REQUEST_HEADER_LEN (16)

typedef struct http_request {
	const char        *method;
	size_t             method_len;
	const char        *path;
	size_t             path_len;
	int                min_ver;
	struct phr_header  hdrs[HTTP_REQUEST_HEADER_LEN];
	size_t             hdr_len;
} HttpRequest;

char *http_url_escape(const char src[]);
void  http_url_escape_free(char url[]);
char *http_send_get(const char url[], const char content_type[]);


/*
 * Dump
 */
void dump_json_str(const char ctx[], const char json[]);
void dump_json_obj(const char ctx[], json_object *json);


/*
 * Log
 */
int  log_init(size_t buffer_size);
void log_deinit(void);
void log_err(int errnum, const char fmt[], ...);
void log_debug(const char fmt[], ...);
void log_info(const char fmt[], ...);


#endif
