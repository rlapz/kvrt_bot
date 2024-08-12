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
#include <spawn.h>

#include <sys/wait.h>
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

	if ((size == 1) || (src == NULL)) {
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
	if (a_len != b_len)
		return 0;

	if (strncmp(a, b, b_len) != 0)
		return 0;

	/* equals */
	return 1;
}


int
cstr_casecmp_n(const char a[], const char b[], size_t b_len)
{
	const size_t a_len = strlen(a);
	if (a_len != b_len)
		return 0;

	if (strncasecmp(a, b, b_len) != 0)
		return 0;

	/* equals */
	return 1;
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
 * cmd
 */
unsigned
bot_cmd_args_parse(BotCmdArg a[], unsigned size, const char src[])
{
	if (src == NULL)
		return 0;

	unsigned args_len = 0;
	while ((*src != '\0') && (args_len < size)) {
		const char *const p = strchr(src, ' ');
		if (p == NULL) {
			a[args_len++] = (BotCmdArg){ .name = src, .len = (unsigned)strlen(src) };
			break;
		}

		if (isblank(*src)) {
			src = p + 1;
			continue;
		}

		a[args_len++] = (BotCmdArg){ .name = src, .len = (unsigned)(p - src) };
		src = p + 1;
	}

	return args_len;
}


int
bot_cmd_parse(BotCmd *b, char prefix, const char src[])
{
	const char *name = src;
	unsigned name_len = 0;
	while ((*name != '\0') && isblank(*name))
		name++;

	if (*name != prefix)
		return -1;

	const char *name_end = strchr(name, ' ');
	const char *const username = strchr(name, '@');
	if (name_end == NULL) {
		/* ignore @username */
		if (username != NULL)
			name_len = (unsigned)(username - name);
		else
			name_len = (unsigned)strlen(name);
	} else {
		/* ignore @username */
		if ((username != NULL) && (username < name_end))
			name_len = (unsigned)(username - name);
		else
			name_len = (unsigned)(name_end - name);
	}

	if (name_len == 1)
		return -1;

	b->name = name;
	b->name_len = name_len;
	b->args_len = bot_cmd_args_parse(b->args, BOT_CMD_ARGS_SIZE, name_end);
	return 0;
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
		return -1;

	const size_t _rsize = (slen - remn_size) + size + 1;
	char *const _new_cstr = realloc(s->cstr, _rsize);
	if (_new_cstr == NULL)
		return -1;

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

	if (_str_resize(s, len) < 0) {
		errno = ENOMEM;
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

	if (_str_resize(s, len) < 0) {
		errno = ENOMEM;
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

	if (_str_resize(s, cstr_len) < 0) {
		errno = ENOMEM;
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

	if (_str_resize(s, cstr_len) < 0) {
		errno = ENOMEM;
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


int
str_reset(Str *s, size_t offt)
{
	if (offt >= s->len)
		return -EINVAL;

	s->cstr[offt] = '\0';
	s->len = offt;
	return 0;
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
void
buffer_init(Buffer *b, size_t max_size)
{
	b->mem = NULL;
	b->size = 0;
	b->max_size = max_size;
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
 * Chld
 */
int
chld_init(Chld *c)
{
	if (mtx_init(&c->mutex, mtx_plain) != 0)
		return -1;

	unsigned i = CHLD_ITEM_SIZE;
	while (i--) {
		c->slots[i] = i;
		c->items[i].pid = -1;
	}

	c->count = 0;
	return 0;
}


void
chld_deinit(Chld *c)
{
	for (unsigned i = 0; i < CHLD_ITEM_SIZE; i++) {
		const pid_t pid = c->items[i].pid;
		if (pid < 0)
			continue;

		log_info("chld: chld_deinit: waiting child process: %d...", pid);
		waitpid(pid, NULL, 0);
	}

	mtx_destroy(&c->mutex);
}


int
chld_spawn(Chld *c, const char path[], char *const argv[])
{
	int ret = -1;
	mtx_lock(&c->mutex);
	const unsigned count = c->count;
	if (count == CHLD_ITEM_SIZE)
		goto out0;

	const unsigned slot = c->slots[count];
	ChldItem *const chld = &c->items[slot];
	if (posix_spawn(&chld->pid, path, NULL, NULL, argv, NULL) != 0)
		goto out0;

	chld->slot = slot;
	c->count = count + 1;
	ret = 0;
	log_debug("chld: chld_spawn: spawn: (%u:%u): %d", c->count, slot, chld->pid);

out0:
	mtx_unlock(&c->mutex);
	return ret;
}


/* TODO: optimize */
void
chld_reap(Chld *c)
{
	mtx_lock(&c->mutex);

	for (unsigned i = 0; i < CHLD_ITEM_SIZE; i++) {
		ChldItem *const chld = &c->items[i];
		if (chld->pid < 0)
			continue;

		const pid_t ret = waitpid(chld->pid, NULL, WNOHANG);
		if ((ret < 0) || (ret == chld->pid)) {
			if (c->count == 0)
				break;

			c->count--;
			c->slots[c->count] = chld->slot;

			log_debug("chld: chld_reap: reap: (%u:%u): %d", c->count, chld->slot, chld->pid);
			chld->pid = -1;
		}
	}

	mtx_unlock(&c->mutex);
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

