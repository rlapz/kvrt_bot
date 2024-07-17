#ifndef __UTIL_H__
#define __UTIL_H__


#include <stddef.h>


#define LEN(X)                          ((sizeof(X) / sizeof(*X)))
#define FIELD_PARENT_PTR(T, FIELD, PTR) ((T *)(((char *)(PTR)) - offsetof(T, FIELD)))


/*
 * cstr
 */
void  cstr_copy(char dest[], const char src[]);
void  cstr_copy_n(char dest[], size_t size, const char src[]);
void  cstr_copy_n2(char dest[], size_t size, const char src[], size_t len);
int   cstr_cmp_n(const char a[], const char b[], size_t b_len);
char *cstr_trim_l(char dest[]);
char *cstr_trim_r(char dest[]);


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
char *str_set_n(Str *s, const char cstr[], size_t len);
char *str_set_fmt(Str *s, const char fmt[], ...);
char *str_append_fmt(Str *s, const char fmt[], ...);
int   str_reset(Str *s, size_t offt);


/*
 * Buffer
 */
typedef struct buffer {
	char   *mem;
	size_t  size;
	size_t  max_size;
} Buffer;

void buffer_init(Buffer *b, size_t max_size);
void buffer_deinit(Buffer *b);
int  buffer_resize(Buffer *b, size_t len);


/*
 * Log
 */
int  log_init(size_t buffer_size);
void log_deinit(void);
void log_err(int errnum, const char fmt[], ...);
void log_debug(const char fmt[], ...);
void log_info(const char fmt[], ...);


#endif
