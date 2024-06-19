#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <threads.h>
#include <unistd.h>

#include <sys/socket.h>

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
		return -errno;

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

	const int ret = str_init(s, cstr, size);
	if (ret < 0) {
		free(cstr);
		return ret;
	}

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

	if (_str_resize(s, len) < 0)
		return NULL;

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

	if (_str_resize(s, len) < 0)
		return NULL;

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

	if (_str_resize(s, cstr_len) < 0)
		return NULL;

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

	if (_str_resize(s, cstr_len) < 0)
		return NULL;

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


/*
 * Buffer
 */
int
buffer_init(Buffer *b, size_t init_size, size_t max_size)
{
	if (init_size > max_size) {
		errno = EINVAL;
		return -1;
	}

	char *const mem = malloc(init_size);
	if (mem == NULL)
		return -1;

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
	if (new_size >= b->max_size) {
		errno = ENOMEM;
		return -1;
	}

	char *const new_mem = realloc(b->mem, new_size);
	if (new_mem == NULL)
		return -1;
	
	b->mem = new_mem;
	b->size = new_size;
	return 1;
}


/*
 * Log
 */
static mtx_t _log_mutex;


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
log_init(void)
{
	if (mtx_init(&_log_mutex, mtx_plain) != 0)
		return -1;
	
	return 0;
}


void
log_deinit(void)
{
	mtx_destroy(&_log_mutex);
}


void
log_err(int errnum, const char fmt[], ...)
{
	int ret;
	va_list va;
	char datetm[32];
	char buffer[1024];


	va_start(va, fmt);
	ret = vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	if (ret <= 0)
		buffer[0] = '\0';

	if ((size_t)ret >= sizeof(buffer))
		buffer[sizeof(buffer) - 1] = '\0';

	mtx_lock(&_log_mutex); // lock

	const char *const dt_now = _log_datetime_now(datetm, sizeof(datetm));
	if (errnum != 0)
		fprintf(stderr, "[ERROR]: [%s]: %s: %s\n", dt_now, buffer, strerror(abs(errnum)));
	else
		fprintf(stderr, "[ERROR]: [%s]: %s\n", dt_now, buffer);

	mtx_unlock(&_log_mutex); // unlock
}


void
log_debug(const char fmt[], ...)
{
#ifdef DEBUG
	int ret;
	va_list va;
	char datetm[32];
	char buffer[1024];


	va_start(va, fmt);
	ret = vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	if (ret <= 0)
		buffer[0] = '\0';

	if ((size_t)ret >= sizeof(buffer))
		buffer[sizeof(buffer) - 1] = '\0';

	mtx_lock(&_log_mutex); // lock
	fprintf(stdout, "[DEBUG]: [%s]: %s\n", _log_datetime_now(datetm, sizeof(datetm)), buffer);
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
	char buffer[1024];


	va_start(va, fmt);
	ret = vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	if (ret <= 0)
		buffer[0] = '\0';

	if ((size_t)ret >= sizeof(buffer))
		buffer[sizeof(buffer) - 1] = '\0';

	mtx_lock(&_log_mutex); // lock
	fprintf(stdout, "[INFO]: [%s]: %s\n", _log_datetime_now(datetm, sizeof(datetm)), buffer);
	fflush(stdout);
	mtx_unlock(&_log_mutex); // unlock
}

