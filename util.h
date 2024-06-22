#ifndef __UTIL_H__
#define __UTIL_H__

#include <stddef.h>


#define LEN(X)                          ((sizeof(X) / sizeof(*X)))
#define FIELD_PARENT_PTR(T, FIELD, PTR) ((T *)(((char *)(PTR)) - offsetof(T, FIELD)))


/*
 * cstr
 */
void cstr_copy(char dest[], const char src[]);
void cstr_copy_n(char dest[], size_t size, const char src[]);
void cstr_copy_n2(char dest[], size_t size, const char src[], size_t len);


/*
 * Str
 */
typedef struct {
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
char *str_dup(Str *s);
int   str_reset(Str *s, size_t offt);


/*
 * Buffer
 */
typedef struct {
	char   *mem;
	size_t  size;
	size_t  max_size;
} Buffer;

int  buffer_init(Buffer *b, size_t init_size, size_t max_size);
void buffer_deinit(Buffer *b);
int  buffer_resize(Buffer *b, size_t len);


/*
 * SList
 */
typedef struct slist_node {
	struct slist_node *prev;
} SListNode;

typedef struct slist {
	SListNode *last;
} SList;

void       slist_init(SList *s);
void       slist_push(SList *s, SListNode *new_node);
SListNode *slist_pop(SList *s);


/*
 * CstrMap
 */
typedef struct {
	const char *key;
	void       *val;
} CstrMapItem;

typedef struct {
	int          is_alloc;
	size_t       mask;
	size_t       size;
	CstrMapItem *items;
} CstrMap;

int   cstrmap_init(CstrMap *c, CstrMapItem items[], size_t size);
int   cstrmap_init_alloc(CstrMap *c, size_t size);
void  cstrmap_deinit(CstrMap *c);
int   cstrmap_set(CstrMap *c, const char key[], void *val);
void *cstrmap_get(CstrMap *c, const char key[]);
void *cstrmap_del(CstrMap *c, const char key[]);


/*
 * Log
 */
int  log_init(void);
void log_deinit(void);
void log_err(int errnum, const char fmt[], ...);
void log_debug(const char fmt[], ...);
void log_info(const char fmt[], ...);


#endif
