#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdint.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "util.h"


/*
 * cstr
 */
void
cstr_copy(char dest[], const char src[])
{
	const size_t slen = strlen(src);
	memcpy(dest, src, slen);
	dest[slen] = '\0';
}


void
cstr_copy_n(char dest[], size_t size, const char src[])
{
	if (size == 0)
		return;

	if (size == 1) {
		dest[0] = '\0';
		return;
	}

	size_t slen = strlen(src);
	if (slen >= size)
		slen = size - 1;

	memcpy(dest, src, slen);
	dest[slen] = '\0';
}


void
cstr_copy_n2(char dest[], size_t size, const char src[], size_t len)
{
	if (size <= len)
		len = size - 1;

	memcpy(dest, src, len);
	dest[len] = '\0';
}


int
cstr_cmp_n(const char a[], const char b[], size_t b_len)
{
	const size_t a_len = strlen(a);
	if (a_len < b_len)
		return -1;

	if (a_len > b_len)
		return 1;

	return strncmp(a, b, b_len);
}


char *
cstr_trim_l(char dest[])
{
	char *ret = dest;
	while (*ret != '\0') {
		if (isblank(*ret) == 0)
			break;

		ret++;
	}

	return ret;
}


char *
cstr_trim_r(char dest[])
{
	char *ptr = dest;
	if (*ptr == '\0')
		return ptr;

	ptr += (strlen(ptr) - 1);
	while ((*ptr != '\0') && (ptr > dest)) {
		if (isblank(*ptr) == 0)
			break;

		*(ptr--) = '\0';
	}

	return dest;
}


/*
 * Str
 */
static int
_str_resize(Str *s, size_t slen)
{
	size_t remn_size = 0;
	const size_t size = s->size;
	if (size > slen)
		remn_size = size - s->len;

	if (slen < remn_size)
		return 0;

	if (s->is_alloc == 0)
		return -ENOMEM;

	const size_t _rsize = (slen - remn_size) + size + 1;
	char *const _new_cstr = realloc(s->cstr, _rsize);
	if (_new_cstr == NULL)
		return -ENOMEM;

	s->size = _rsize;
	s->cstr = _new_cstr;
	return 0;
}


int
str_init(Str *s, char buffer[], size_t size)
{
	if (size == 0)
		return -EINVAL;

	buffer[0] = '\0';
	s->is_alloc = 0;
	s->size = size;
	s->len = 0;
	s->cstr = buffer;
	return 0;
}


int
str_init_alloc(Str *s, size_t size)
{
	if (size == 0)
		size = 1;

	void *const cstr = malloc(size);
	if (cstr == NULL)
		return -ENOMEM;

	str_init(s, cstr, size);
	s->is_alloc = 1;
	return 0;
}


void
str_deinit(Str *s)
{
	if (s->is_alloc)
		free(s->cstr);
}


char *
str_append_n(Str *s, const char cstr[], size_t len)
{
	if (len == 0)
		return s->cstr;

	const int ret = _str_resize(s, len);
	if (ret < 0) {
		errno = -ret;
		return NULL;
	}

	size_t slen = s->len;
	memcpy(s->cstr + slen, cstr, len);

	slen += len;
	s->len = slen;
	s->cstr[slen] = '\0';
	return s->cstr;
}


char *
str_set_n(Str *s, const char cstr[], size_t len)
{
	if (len == 0) {
		s->len = 0;
		s->cstr[0] = '\0';
		return s->cstr;
	}

	const int ret = _str_resize(s, len);
	if (ret < 0) {
		errno = -ret;
		return NULL;
	}

	memcpy(s->cstr, cstr, len);
	s->len = len;
	s->cstr[len] = '\0';
	return s->cstr;
}


char *
str_set_fmt(Str *s, const char fmt[], ...)
{
	int ret;
	va_list va;


	/* determine required size */
	va_start(va, fmt);
	ret = vsnprintf(NULL, 0, fmt, va);
	va_end(va);

	if (ret < 0)
		return NULL;

	const size_t cstr_len = (size_t)ret;
	if (cstr_len == 0) {
		s->len = 0;
		s->cstr[0] = '\0';
		return s->cstr;
	}

	ret = _str_resize(s, cstr_len);
	if (ret < 0) {
		errno = -ret;
		return NULL;
	}

	va_start(va, fmt);
	ret = vsnprintf(s->cstr, cstr_len + 1, fmt, va);
	va_end(va);

	if (ret < 0)
		return NULL;

	s->len = (size_t)ret;
	s->cstr[ret] = '\0';
	return s->cstr;
}


char *
str_append_fmt(Str *s, const char fmt[], ...)
{
	int ret;
	va_list va;


	/* determine required size */
	va_start(va, fmt);
	ret = vsnprintf(NULL, 0, fmt, va);
	va_end(va);

	if (ret < 0)
		return NULL;

	const size_t cstr_len = (size_t)ret;
	if (cstr_len == 0)
		return s->cstr;

	ret = _str_resize(s, cstr_len);
	if (ret < 0) {
		errno = -ret;
		return NULL;
	}

	size_t len = s->len;
	va_start(va, fmt);
	ret = vsnprintf(s->cstr + len, cstr_len + 1, fmt, va);
	va_end(va);

	if (ret < 0)
		return NULL;

	len += (size_t)ret;
	s->len = len;
	s->cstr[len] = '\0';
	return s->cstr;
}


char *
str_dup(Str *s)
{
	const size_t len = s->len + 1; /* including '\0' */
	char *const ret = malloc(len);
	if (ret == NULL)
		return NULL;

	return (char *)memcpy(ret, s->cstr, len);
}


int
str_reset(Str *s, size_t offt)
{
	if (offt >= s->len)
		return -EINVAL;

	s->cstr[offt] = '\0';
	s->len = offt;
	return 0;
}


/*
 * Buffer
 */
int
buffer_init(Buffer *b, size_t init_size, size_t max_size)
{
	if (init_size > max_size)
		return -EINVAL;

	char *const mem = malloc(init_size);
	if (mem == NULL)
		return -ENOMEM;

	mem[0] = '\0';
	b->mem = mem;
	b->size = init_size;
	b->max_size = max_size;
	return 0;
}


void
buffer_deinit(Buffer *b)
{
	free(b->mem);
}


int
buffer_resize(Buffer *b, size_t len)
{
	if (len < b->size)
		return 0;

	const size_t new_size = b->size + len;
	if (new_size >= b->max_size)
		return -ENOMEM;

	char *const new_mem = realloc(b->mem, new_size);
	if (new_mem == NULL)
		return -ENOMEM;

	b->mem = new_mem;
	b->size = new_size;
	return 1;
}


/*
 * SList
 */
void
slist_init(SList *s)
{
	s->last = NULL;
}


void
slist_push(SList *s, SListNode *new_node)
{
	new_node->prev = s->last;
	s->last = new_node;
}


SListNode *
slist_pop(SList *s)
{
	SListNode *const ret = s->last;
	if (ret != NULL)
		s->last = ret->prev;

	return ret;
}



/*
 * CstrMap
 *
 * 32bit FNV-1a case-insensitive hash function & hash map
 * ref: https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
 */
static CstrMapItem *
_cstrmap_map(CstrMap *c, const char key[])
{
	uint32_t hash = 0x811c9dc5; /* FNV-1 offset */
	for (const char *p = key; *p != '\0'; p++) {
		hash ^= (uint32_t)((unsigned char)tolower(*p));
		hash *= 0x01000193; /* FNV-1 prime */
	}

	const size_t size = c->size;
	size_t index = (size_t)(hash & c->mask);
	CstrMapItem *item = &c->items[index];
	for (size_t i = 0; (i < size) && (item->key != NULL); i++) {
		if (strcasecmp(item->key, key) == 0) {
			/* found matched key */
			return item;
		}

#ifdef DEBUG
		printf("cstrmap: _cstrmap_map: linear probing: [%s:%s]:[%zu:%zu]\n", key, item->key, i, index);
#endif

		index = (index + 1) & c->mask;
		item = &c->items[index];
	}

	/* slot full, no free key */
	if (item->key != NULL)
		return NULL;

	return item;
}


int
cstrmap_init(CstrMap *c, CstrMapItem items[], size_t size)
{
	if ((size == 0) || (size & 1) != 0)
		return -EINVAL;

	c->is_alloc = 0;
	c->mask = size - 1;
	c->size = size;
	c->items = items;
	return 0;
}


int
cstrmap_init_alloc(CstrMap *c, size_t size)
{
	if (size == 0)
		size = 2;

	while ((size & 1) != 0)
		size++;

	void *const items = calloc(size, sizeof(CstrMapItem));
	if (items == NULL)
		return -ENOMEM;

	c->is_alloc = 1;
	c->mask = size - 1;
	c->size = size;
	c->items = items;
	return 0;
}


void
cstrmap_deinit(CstrMap *c)
{
	if (c->is_alloc)
		free(c->items);
}


int
cstrmap_set(CstrMap *c, const char key[], void *val)
{
	CstrMapItem *const item = _cstrmap_map(c, key);
	if (item == NULL)
		return -1;

	item->key = key;
	item->val = val;
	return 0;
}


void *
cstrmap_get(CstrMap *c, const char key[])
{
	CstrMapItem *const item = _cstrmap_map(c, key);
	if ((item == NULL) || (item->key == NULL))
		return NULL;

	return item->val;
}


void *
cstrmap_del(CstrMap *c, const char key[])
{
	CstrMapItem *const item = _cstrmap_map(c, key);
	if ((item == NULL) || (item->key == NULL))
		return NULL;

	void *const val = item->val;
	item->key = NULL;
	item->val = NULL;
	return val;
}


/*
 * Log
 */
static char   *_log_buffer;
static size_t  _log_buffer_size;
static mtx_t   _log_mutex;


static const char *
_log_datetime_now(char dest[], size_t size)
{
	const time_t tm_r = time(NULL);
	struct tm *const tm = localtime(&tm_r);
	if (tm == NULL)
		goto out0;

	const char *const res = asctime(tm);
	if (res == NULL)
		goto out0;

	const size_t res_len = strlen(res);
	if ((res_len == 0) || (res_len >= size))
		goto out0;

	memcpy(dest, res, res_len - 1);
	dest[res_len - 1] = '\0';
	return dest;

out0:
	return "???";
}


int
log_init(size_t buffer_size)
{
	char *const buffer = malloc(buffer_size);
	if (buffer == NULL)
		return -1;

	if (mtx_init(&_log_mutex, mtx_plain) != 0) {
		free(buffer);
		return -1;
	}

	_log_buffer = buffer;
	_log_buffer_size = buffer_size;
	return 0;
}


void
log_deinit(void)
{
	mtx_destroy(&_log_mutex);
	free(_log_buffer);
	_log_buffer_size = 0;
}


void
log_err(int errnum, const char fmt[], ...)
{
	int ret;
	va_list va;
	char datetm[32];


	mtx_lock(&_log_mutex); // lock

	va_start(va, fmt);
	ret = vsnprintf(_log_buffer, _log_buffer_size, fmt, va);
	va_end(va);

	if (ret <= 0)
		_log_buffer[0] = '\0';

	if ((size_t)ret >= _log_buffer_size)
		_log_buffer[_log_buffer_size - 1] = '\0';

	const char *const dt_now = _log_datetime_now(datetm, sizeof(datetm));
	if (errnum != 0)
		fprintf(stderr, "E: [%s]: %s: %s\n", dt_now, _log_buffer, strerror(abs(errnum)));
	else
		fprintf(stderr, "E: [%s]: %s\n", dt_now, _log_buffer);

	mtx_unlock(&_log_mutex); // unlock
}


void
log_debug(__attribute_maybe_unused__ const char fmt[], ...)
{
#ifdef DEBUG
	int ret;
	va_list va;
	char datetm[32];


	mtx_lock(&_log_mutex); // lock

	va_start(va, fmt);
	ret = vsnprintf(_log_buffer, _log_buffer_size, fmt, va);
	va_end(va);

	if (ret <= 0)
		_log_buffer[0] = '\0';

	if ((size_t)ret >= _log_buffer_size)
		_log_buffer[_log_buffer_size - 1] = '\0';

	fprintf(stdout, "D: [%s]: %s\n", _log_datetime_now(datetm, sizeof(datetm)), _log_buffer);
	fflush(stdout);

	mtx_unlock(&_log_mutex); // unlock
#endif
}


void
log_info(const char fmt[], ...)
{
	int ret;
	va_list va;
	char datetm[32];


	mtx_lock(&_log_mutex); // lock

	va_start(va, fmt);
	ret = vsnprintf(_log_buffer, _log_buffer_size, fmt, va);
	va_end(va);

	if (ret <= 0)
		_log_buffer[0] = '\0';

	if ((size_t)ret >= _log_buffer_size)
		_log_buffer[_log_buffer_size - 1] = '\0';

	fprintf(stdout, "I: [%s]: %s\n", _log_datetime_now(datetm, sizeof(datetm)), _log_buffer);
	fflush(stdout);

	mtx_unlock(&_log_mutex); // unlock
}

