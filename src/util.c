#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
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
#include <json.h>

#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <curl/curl.h>

#include "util.h"


/*
 * cstr
 */
size_t
cstr_copy(char dest[], const char src[])
{
	assert(VAL_IS_NULL_OR(dest, src) == 0);

	const size_t slen = strlen(src);
	memcpy(dest, src, slen);
	dest[slen] = '\0';
	return slen;
}


size_t
cstr_copy_n(char dest[], size_t size, const char src[])
{
	assert(dest != NULL);
	if (size == 0)
		return 0;

	if ((size == 1) || (src == NULL)) {
		dest[0] = '\0';
		return 0;
	}

	size_t slen = strlen(src);
	if (slen >= size)
		slen = size - 1;

	memcpy(dest, src, slen);
	dest[slen] = '\0';
	return slen;
}


size_t
cstr_copy_n2(char dest[], size_t size, const char src[], size_t len)
{
	assert(dest != NULL);
	if (size <= len)
		len = size - 1;

	memcpy(dest, src, len);
	dest[len] = '\0';
	return len;
}


int
cstr_cmp_n2(const char a[], size_t a_len, const char b[], size_t b_len)
{
	if ((a == NULL) && (b == NULL))
		return 1;

	if (a_len != b_len)
		return 0;

	if (strncmp(a, b, b_len) != 0)
		return 0;

	return 1;
}


int
cstr_casecmp(const char a[], const char b[])
{
	if ((a == NULL) && (b == NULL))
		return 1;

	if (strcasecmp(a, b) != 0)
		return 0;

	return 1;
}


int
cstr_casecmp_n(const char a[], const char b[], size_t b_len)
{
	if ((a == NULL) && (b == NULL))
		return 1;

	const size_t a_len = strlen(a);
	if (a_len != b_len)
		return 0;

	if (strncasecmp(a, b, b_len) != 0)
		return 0;

	return 1;
}


int
cstr_casecmp_n2(const char a[], size_t a_len, const char b[], size_t b_len)
{
	if ((a == NULL) && (b == NULL))
		return 1;

	if (a_len != b_len)
		return 0;

	if (strncasecmp(a, b, b_len) != 0)
		return 0;

	return 1;
}


char *
cstr_trim_l(char dest[])
{
	assert(dest != NULL);

	char *ret = dest;
	while (*ret != '\0') {
		if (isspace(*ret) == 0)
			break;

		ret++;
	}

	return ret;
}


char *
cstr_escape(const char escape[], const char c, const char src[])
{
	if (CSTR_IS_EMPTY_OR(escape, src))
		return NULL;

	Str str;
	const size_t src_len = strlen(src);
	if (str_init_alloc(&str, src_len) < 0)
		return NULL;

	for (size_t i = 0; i < src_len; i++) {
		const char tmp = src[i];
		const char *eptr = escape;
		for (; *eptr != '\0'; eptr++) {
			if (tmp != *eptr)
				continue;

			if (str_append_c(&str, c) == NULL)
				goto err0;
		}

		if (str_append_c(&str, tmp) == NULL)
			goto err0;
	}

	return str.cstr;

err0:
	str_deinit(&str);
	return NULL;
}


char *
cstr_to_lower_n(char dest[], size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (dest[i] == '\0')
			break;

		dest[i] = (char)tolower(dest[i]);
	}

	return dest;
}


int
cstr_to_int64(const char cstr[], int64_t *ret)
{
	errno = 0;
	const long long _ret = strtoll(cstr, NULL, 10);
	if (errno == 0)
		*ret = (int64_t)_ret;

	return -errno;
}


int
cstr_to_int64_n(const char cstr[], size_t len, int64_t *ret)
{
	char buffer[INT64_DIGITS_LEN];
	cstr_copy_n2(buffer, LEN(buffer), cstr, len);

	errno = 0;
	const long long _ret = strtoll(buffer, NULL, 10);
	if (errno == 0)
		*ret = (int64_t)_ret;

	return -errno;
}


int
cstr_to_uint64(const char cstr[], uint64_t *ret)
{
	errno = 0;
	const unsigned long long _ret = strtoull(cstr, NULL, 10);
	if (errno == 0)
		*ret = (uint64_t)_ret;

	return -errno;
}


int
cstr_to_uint64_n(const char cstr[], size_t len, uint64_t *ret)
{
	char buffer[UINT64_DIGITS_LEN];
	cstr_copy_n2(buffer, LEN(buffer), cstr, len);

	errno = 0;
	const unsigned long long _ret = strtoull(buffer, NULL, 10);
	if (errno == 0)
		*ret = (uint64_t)_ret;

	return -errno;
}


int
cstr_is_empty_and_n(size_t count, ...)
{
	va_list va;
	va_start(va, count);

	int ret = 0;
	for (size_t i = 0; i < count; i++) {
		void *const curr = va_arg(va, void*);
		if ((curr != NULL) || (((char *)curr)[0] != '\0'))
			goto out0;
	}

	ret = 1;

out0:
	va_end(va);
	return ret;
}


int
cstr_is_empty_or_n(size_t count, ...)
{
	va_list va;
	va_start(va, count);

	int ret = 1;
	for (size_t i = 0; i < count; i++) {
		void *const curr = va_arg(va, void*);
		if ((curr == NULL) || (((char *)curr)[0] == '\0'))
			goto out0;
	}

	ret = 0;

out0:
	va_end(va);
	return ret;
}


char *
cstr_concat_n(size_t count, ...)
{
	va_list va;
	size_t len = 0;

	va_start(va, count);
	for (size_t i = 0; i < count; i++) {
		void *const curr = va_arg(va, void *);
		if (curr == NULL)
			continue;

		len += strlen((const char *)curr);
	}
	va_end(va);

	char *const ret = malloc(len + 1);
	if (ret == NULL)
		return NULL;

	va_start(va, count);
	size_t offt = 0;
	for (size_t i = 0; i < count; i++) {
		void *const curr = va_arg(va, void *);
		if (curr == NULL)
			continue;

		offt += cstr_copy(ret + offt, (const char *)curr);
	}
	va_end(va);

	return ret;
}


int
cstr_to_bool(const char cstr[])
{
	if (cstr_casecmp(cstr, "true"))
		return 1;
	if (cstr_casecmp(cstr, "false"))
		return 0;

	return -1;
}


/*
 * SpaceTokenizer
 */
const char *
space_tokenizer_next(SpaceTokenizer *s, const char raw[])
{
	const char *const value = cstr_trim_l((char *)raw);
	if (*value == '\0')
		return NULL;

	unsigned len = 0;
	while ((value[len] != '\0') && (isspace(value[len]) == 0))
		len++;

	s->len = len;
	s->value = value;
	return cstr_trim_l((char *)&value[len]);
}


/*
 * DList
 */
static void
_dlist_init_node(DList *d, DListNode *node)
{
	node->next = NULL;
	node->prev = NULL;
	d->first = node;
	d->last = node;
}


void
dlist_init(DList *d)
{
	d->first = NULL;
	d->last = NULL;
	d->len = 0;
}


void
dlist_append(DList *d, DListNode *node)
{
	if (d->last != NULL) {
		node->prev = d->last;
		node->next = NULL;
		d->last->next = node;
		d->last = node;
	} else {
		_dlist_init_node(d, node);
	}

	d->len++;
}


void
dlist_prepend(DList *d, DListNode *node)
{
	if (d->first != NULL) {
		node->next = d->first;
		node->prev = NULL;
		d->first->prev = node;
		d->first = node;
	} else {
		_dlist_init_node(d, node);
	}

	d->len++;
}


void
dlist_remove(DList *d, DListNode *node)
{
	assert(d->len > 0);
	if (node->next != NULL)
		node->next->prev = node->prev;
	else
		d->last = node->prev;

	if (node->prev != NULL)
		node->prev->next = node->next;
	else
		d->first = node->next;

	d->len--;
}


DListNode *
dlist_pop(DList *d)
{
	DListNode *const node = d->last;
	if (node == NULL)
		return NULL;

	dlist_remove(d, node);
	return node;
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

	if (s->is_alloc == 0) {
		errno = ENOMEM;
		return -1;
	}

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
	if (size > 0)
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
	void *cstr = NULL;
	if (size > 0) {
		cstr = malloc(size);
		if (cstr == NULL)
			return -ENOMEM;
	}

	str_init(s, cstr, size);
	s->is_alloc = 1;
	return 0;
}


void
str_deinit(Str *s)
{
	if (s->is_alloc)
		free(s->cstr);

	s->cstr = NULL;
}


char *
str_set(Str *s, const char cstr[])
{
	const size_t len = strlen(cstr);
	if (len == 0) {
		s->len = 0;
		s->cstr[0] = '\0';
		return s->cstr;
	}

	if (_str_resize(s, len) < 0)
		return NULL;

	memcpy(s->cstr, cstr, len + 1);
	s->len = len;
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
str_append(Str *s, const char cstr[])
{
	const size_t cstr_len = strlen(cstr);
	if (cstr_len == 0)
		return s->cstr;

	if (_str_resize(s, cstr_len) < 0)
		return NULL;

	const size_t len = s->len;
	memcpy(s->cstr + len, cstr, cstr_len + 1);
	s->len = len + cstr_len;
	return s->cstr;
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
str_append_c(Str *s, char c)
{
	if (_str_resize(s, 1) < 0)
		return NULL;

	size_t slen = s->len;
	s->cstr[slen] = c;

	slen++;
	s->len = slen;
	s->cstr[slen] = '\0';
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
		if (s->cstr != NULL)
			s->cstr[0] = '\0';

		s->len = 0;
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
str_pop(Str *s, size_t count)
{
	if ((s->len == 0) || (s->len < count))
		return NULL;

	s->len -= count;
	s->cstr[s->len] = '\0';
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
 * CstrMap
 *
 * 64bit FNV-1a case-insensitive hash function & hash map
 * ref: https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
 */
static CstrMapItem *
_cstrmap_map_exec(CstrMap *c, const char key[])
{
	uint64_t hash = 0xcbf29ce484222325; /* FNV-1 offset */
	for (const char *p = key; *p != '\0'; p++) {
		hash ^= (uint64_t)((unsigned char)tolower(*p));
		hash *= 0x00000100000001b3; /* FNV-1 prime */
	}

	const size_t size = c->size;
	size_t index = (size_t)(hash % size);
	CstrMapItem *item = &c->items[index];
	for (size_t i = 0; (i < size) && (item->key != NULL); i++) {
		if (strcasecmp(item->key, key) == 0)
			return item;

		log_debug("cstrmap: _cstrmap_map: linear probing: [%s:%s]:[%zu:%zu]", key, item->key, i, index);
		index = (index + 1) % size;
		item = &c->items[index];
	}

	/* slot full, no free key */
	if (item->key != NULL)
		return NULL;

	return item;
}


int
cstr_map_init(CstrMap *c, size_t size)
{
	assert(size != 0);

	void *const items = calloc(size, sizeof(CstrMapItem));
	if (items == NULL)
		return -1;

	c->size = size;
	c->items = items;
	return 0;
}


void
cstr_map_deinit(CstrMap *c)
{
	free(c->items);
}


int
cstr_map_set(CstrMap *c, const char key[], void *value)
{
	CstrMapItem *const item = _cstrmap_map_exec(c, key);
	if (item == NULL)
		return -1;

	item->key = key;
	item->value = value;
	return 0;
}


void *
cstr_map_get(CstrMap *c, const char key[])
{
	CstrMapItem *const item = _cstrmap_map_exec(c, key);
	if ((item == NULL) || (item->key == NULL))
		return NULL;

	return item->value;
}


/*
 * Chld
 */
typedef struct chld {
	const char *path;
	const char *log_file;
	unsigned    count;
	unsigned    slots[CFG_CHLD_ITEMS_SIZE];
	unsigned    entries[CFG_CHLD_ITEMS_SIZE];
	pid_t       pids[CFG_CHLD_ITEMS_SIZE];
	unsigned    envp_len;
	char       *envp[CFG_CHLD_ENVP_SIZE + 1]; /* +1 NULL */
	mtx_t       mutex;
} Chld;

static Chld *_chld_instance = NULL;


int
chld_init(const char path[], const char log_file[])
{
	assert(CFG_CHLD_ENVP_SIZE > 0);
	assert(CSTR_IS_EMPTY_OR(path, log_file) == 0);

	Chld *const c = malloc(sizeof(Chld));
	if (c == NULL)
		return -1;

	memset(c, 0, sizeof(Chld));
	if (mtx_init(&c->mutex, mtx_plain) != 0) {
		free(c);
		return -1;
	}

	unsigned i = CFG_CHLD_ITEMS_SIZE;
	while (i--)
		c->slots[i] = i;

	c->path = path;
	c->log_file = log_file;
	_chld_instance = c;
	return 0;
}


void
chld_deinit(void)
{
	assert(_chld_instance != NULL);
	for (unsigned i = 0; i < _chld_instance->envp_len; i++)
		free(_chld_instance->envp[i]);

	mtx_destroy(&_chld_instance->mutex);

	free(_chld_instance);
	_chld_instance = NULL;
}


int
chld_add_env(const char key_value[])
{
	if (cstr_is_empty(key_value)) {
		errno = EINVAL;
		return -1;
	}

	Chld *const c = _chld_instance;
	if (c->envp_len == CFG_CHLD_ENVP_SIZE) {
		errno = ENOMEM;
		return -1;
	}

	char *const new_kv = strdup(key_value);
	if (new_kv == NULL)
		return -1;

	log_debug("chld: chld_add_env: \"%s\"", key_value);
	c->envp[c->envp_len++] = new_kv;
	return 0;
}


int
chld_add_env_kv(const char key[], const char val[])
{
	Chld *const c = _chld_instance;
	assert(c != NULL);

	if (CSTR_IS_EMPTY_OR(key, val)) {
		errno = EINVAL;
		return -1;
	}

	if (c->envp_len == CFG_CHLD_ENVP_SIZE) {
		errno = ENOMEM;
		return -1;
	}

	char *const kv = CSTR_CONCAT(key, "=", val);
	if (kv == NULL)
		return -1;

	log_debug("chld: chld_add_env_kv: \"%s=%s\"", key, val);
	c->envp[c->envp_len++] = kv;
	return 0;
}


int
chld_spawn(const char file[], char *const argv[])
{
	int ret = -1;
	Chld *const c = _chld_instance;
	assert(c != NULL);

	mtx_lock(&c->mutex);

	const unsigned count = c->count;
	if (count == CFG_CHLD_ITEMS_SIZE) {
		log_err(0, "chld_spawn: slot full");
		goto out0;
	}

	posix_spawn_file_actions_t act;
	if (posix_spawn_file_actions_init(&act) != 0) {
		log_err(errno, "chld_spawn: posix_spawn_file_actions_init: \"%s\"", file);
		goto out0;
	}

	const int flags = O_WRONLY | O_CREAT | O_APPEND;
	if (posix_spawn_file_actions_addopen(&act, STDOUT_FILENO, c->log_file, flags, 0644) != 0) {
		log_err(errno, "chld_spawn: posix_spawn_file_actions_addopen: STDOUT: \"%s\"", file);
		goto out1;
	}

	if (posix_spawn_file_actions_adddup2(&act, STDOUT_FILENO, STDERR_FILENO) != 0) {
		log_err(errno, "chld_spawn: posix_spawn_file_actions_adddup2: \"%s\"", file);
		goto out1;
	}

	if (posix_spawn_file_actions_addchdir_np(&act, c->path) != 0) {
		log_err(errno, "chld_spawn: posix_spawn_file_actions_addchdir_np: \"%s\"", file);
		goto out1;
	}

	const unsigned slot = c->slots[count];
	if (posix_spawn(&c->pids[slot], file, &act, NULL, argv, c->envp) != 0) {
		log_err(errno, "chld_spawn: posix_spawn: \"%s\"", file);
		goto out1;
	}

	log_debug("chld_spawn: spawn: \"%s\": %d: [%u:%u]", file, c->pids[slot], slot, c->count);

	c->entries[count] = slot;
	c->count = count + 1;
	ret = 0;

out1:
	posix_spawn_file_actions_destroy(&act);
out0:
	mtx_unlock(&c->mutex);
	return ret;
}


void
chld_reap(void)
{
	Chld *const c = _chld_instance;
	assert(c != NULL);

	mtx_lock(&c->mutex);

	unsigned rcount = 0;
	unsigned *const entries = c->entries;
	unsigned count = c->count;
	for (unsigned i = 0; i < count; i++) {
		const unsigned slot = entries[i];
		if (waitpid(c->pids[slot], NULL, WNOHANG) == 0)
			continue;

		count--;
		entries[i] = entries[count];
		c->slots[count] = slot;
		rcount++;
		i--;

		log_debug("chld_reap: reap: %d: [%u:%u]", c->pids[slot], slot, count);
	}

	assert(rcount <= c->count);
	if (rcount > 0)
		c->count -= rcount;

	mtx_unlock(&c->mutex);
}


void
chld_wait_all(void)
{
	Chld *const c = _chld_instance;
	assert(c != NULL);

	mtx_lock(&c->mutex);

	const unsigned count = c->count;
	for (unsigned i = 0; i < count; i++) {
		const unsigned slot = c->entries[i];
		const pid_t pid = c->pids[slot];

		log_info("chld: chld_wait: waiting child process: (%u:%u): %d...", i, slot, pid);

		waitpid(pid, NULL, 0);
	}

	mtx_unlock(&c->mutex);
}


/*
 * misc
 */
int
val_is_null_and_n(size_t count, ...)
{
	va_list va;
	va_start(va, count);

	int ret = 0;
	for (size_t i = 0; i < count; i++) {
		void *const curr = va_arg(va, void *);
		if (curr != NULL)
			goto out0;
	}

	ret = 1;

out0:
	va_end(va);
	return ret;
}


int
val_is_null_or_n(size_t count, ...)
{
	va_list va;
	va_start(va, count);

	int ret = 1;
	for (size_t i = 0; i < count; i++) {
		void *const curr = va_arg(va, void *);
		if (curr == NULL)
			goto out0;
	}

	ret = 0;

out0:
	va_end(va);
	return ret;
}


int
val_cmp_n2(const void *a, size_t a_len, const void *b, size_t b_len)
{
	if ((a == NULL) && (b == NULL))
		return 1;

	if (a_len != b_len)
		return 0;

	if (memcmp(a, b, b_len) != 0)
		return 0;

	return 1;
}


enum {
	_ARGS_PARSE_STATE_NORMAL,
	_ARGS_PARSE_STATE_SINGLE_QUOTE,
	_ARGS_PARSE_STATE_DOUBLE_QUOTE,
	_ARGS_PARSE_STATE_ESCAPE,
};

int
args_parse(char *args[], unsigned size, unsigned *out_len, const char src[])
{
	int ret = -1;
	if (VAL_IS_NULL_OR(args, out_len) || (size == 0) || cstr_is_empty(src))
		return ret;

	const size_t src_len = strlen(src);
	char *const tmp = malloc(src_len + 1);
	if (tmp == NULL)
		return ret;

	unsigned argc = 0;
	unsigned pos = 0;
	int state = _ARGS_PARSE_STATE_NORMAL;
	int is_started = 0;
	for (size_t i = 0; (i < src_len) && (argc < size); i++) {
		char chr = src[i];
		if (chr == '\0')
			break;

		switch (state) {
		case _ARGS_PARSE_STATE_NORMAL:
			if (isspace(chr)) {
				if (is_started == 0)
					continue;

				tmp[pos] = '\0';
				char *const new_item = strdup(tmp);
				if (new_item == NULL)
					goto out0;

				args[argc++] = new_item;
				pos = 0;
				is_started = 0;
				continue;
			}

			switch (chr) {
			case '\'':
				i++;
				if (src[i] == '\'') {
					chr = '\0';
				} else {
					state = _ARGS_PARSE_STATE_SINGLE_QUOTE;
					chr = src[i];
				}
				break;
			case '\"':
				i++;
				if (src[i] == '\"') {
					chr = '\0';
				} else {
					state = _ARGS_PARSE_STATE_DOUBLE_QUOTE;
					chr = src[i];
				}
				break;
			case '\\':
				state = _ARGS_PARSE_STATE_ESCAPE;
				break;
			}

			is_started = 1;
			break;
		case _ARGS_PARSE_STATE_SINGLE_QUOTE:
			switch (chr) {
			case '\\':
				state = _ARGS_PARSE_STATE_ESCAPE;
				continue;
			case '\'':
				state = _ARGS_PARSE_STATE_NORMAL;
				continue;
			}
			break;
		case _ARGS_PARSE_STATE_DOUBLE_QUOTE:
			switch (chr) {
			case '\\':
				state = _ARGS_PARSE_STATE_ESCAPE;
				continue;
			case '\"':
				state = _ARGS_PARSE_STATE_NORMAL;
				continue;
			}
			break;
		case _ARGS_PARSE_STATE_ESCAPE:
			state = _ARGS_PARSE_STATE_NORMAL;
			is_started = 1;
			break;
		}

		tmp[pos++] = chr;
	}

	switch (state) {
	case _ARGS_PARSE_STATE_SINGLE_QUOTE:
	case _ARGS_PARSE_STATE_DOUBLE_QUOTE:
	case _ARGS_PARSE_STATE_ESCAPE:
		goto out0;
	}

	if (is_started || (pos > 0)) {
		tmp[pos] = '\0';
		char *const new_item = strdup(tmp);
		if (new_item == NULL)
			goto out0;

		args[argc++] = new_item;
	}

	*out_len = argc;
	ret = 0;

out0:
	if (ret < 0)
		args_free(args, argc);

	free(tmp);
	return ret;
}


void
args_free(char *args[], unsigned len)
{
	if (args == NULL)
		return;

	for (unsigned i = 0; i < len; i++)
		free(args[i]);
}


int
file_read_all(const char path[], char buffer[], size_t *len)
{
	int ret = -1;
	const int fd = open(path, O_RDONLY);
	if (fd < 0)
		return -errno;

	struct stat st;
	if (fstat(fd, &st) < 0) {
		ret = -errno;
		goto out0;
	}

	size_t total = 0;
	const size_t rsize = MIN((size_t)st.st_size, *len);
	while (total < rsize) {
		const ssize_t rd = read(fd, buffer + total, rsize - total);
		if (rd < 0) {
			ret = -errno;
			goto out0;
		}

		if (rd == 0)
			break;

		total += (size_t)rd;
	}

	*len = total;
	ret = 0;

out0:
	close(fd);
	return ret;
}


int
file_write_all(const char path[], const char buffer[], size_t *len)
{
	int ret = -1;
	const int fd = creat(path, 00644);
	if (fd < 0)
		return -errno;

	size_t total = 0;
	const size_t rsize = *len;
	while (total < rsize) {
		const ssize_t wr = write(fd, buffer + total, rsize - total);
		if (ret < 0) {
			ret = -errno;
			goto out0;
		}

		if (wr == 0)
			break;

		total += (size_t)wr;
	}

	*len = total;
	ret = 0;

out0:
	close(fd);
	return ret;
}


/*
 * Http
 */
char *
http_url_escape(const char src[])
{
	return curl_easy_escape(NULL, src, -1);
}


void
http_url_escape_free(char url[])
{
	curl_free(url);
}


static size_t
_http_writer(void *ctx, size_t size, size_t nmemb, void *udata)
{
	Str *const s = (Str *)udata;
	if (str_append_n(s, (const char *)ctx, nmemb * size) == NULL)
		return 0;

	return nmemb * size;
}


char *
http_send_get(const char url[], const char content_type[])
{
	Str str;
	char *ret = NULL;
	if (cstr_is_empty(url))
		return NULL;

	log_debug("http: http_send_get: url: %s", url);
	if (str_init_alloc(&str, 128) < 0)
		return NULL;

	CURL *const handle = curl_easy_init();
	if (handle == NULL)
		goto out0;

	if (curl_easy_setopt(handle, CURLOPT_URL, url) != CURLE_OK)
		goto out1;

	if (curl_easy_setopt(handle, CURLOPT_WRITEDATA, &str) != CURLE_OK)
		goto out1;

	if (curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, _http_writer) != CURLE_OK)
		goto out1;

	struct curl_slist *slist = NULL;
	if (cstr_is_empty(content_type) == 0) {
		char *const ctp = CSTR_CONCAT("Content-Type: ", content_type);
		if (ctp == NULL)
			goto out1;

		slist = curl_slist_append(NULL, ctp);
		free(ctp);

		if (slist == NULL)
			goto out1;

		if (curl_easy_setopt(handle, CURLOPT_HTTPHEADER, slist) != CURLE_OK)
			goto out2;
	}

	if (curl_easy_perform(handle) != CURLE_OK)
		goto out2;

	if (cstr_is_empty(content_type) == 0) {
		char *ct = NULL;
		if (curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &ct) != CURLE_OK)
			goto out2;

		if (ct == NULL)
			goto out2;

#ifdef DEBUG
		if (strcasecmp(ct, "application/json") == 0)
			dump_json_str("http: http_send_get", str.cstr);
#endif

		if (strcasecmp(ct, content_type) != 0)
			goto out2;
	}

	ret = str_dup(&str);

out2:
	curl_slist_free_all(slist);
out1:
	curl_easy_cleanup(handle);
out0:
	str_deinit(&str);
	return ret;
}


/*
 * Dump
 */
void
dump_json_str(const char ctx[], const char json[])
{
#ifdef DEBUG
	json_object *obj = json_tokener_parse(json);
	if (obj == NULL)
		return;

	log_debug("\n----[Dump JSON]---\n%s:\n%s\n------------------", ctx,
		  json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PRETTY));

	json_object_put(obj);
#else
	(void)ctx;
	(void)json;
#endif
}


void
dump_json_obj(const char ctx[], json_object *json)
{
#ifdef DEBUG
	if (json == NULL)
		return;

	log_debug("\n----[Dump JSON]---\n%s:\n%s\n------------------", ctx,
		  json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY));
#else
	(void)ctx;
	(void)json;
#endif
}


/*
 * Log
 */
static char   *_log_buffer = NULL;
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
	if (_log_buffer != NULL)
		return 0;

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

	if (_log_buffer == NULL)
		return;

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

	if (_log_buffer == NULL)
		return;

	mtx_lock(&_log_mutex); // lock

	va_start(va, fmt);
	ret = vsnprintf(_log_buffer, _log_buffer_size, fmt, va);
	va_end(va);

	if (ret <= 0)
		_log_buffer[0] = '\0';

	if ((size_t)ret >= _log_buffer_size)
		_log_buffer[_log_buffer_size - 1] = '\0';

	fprintf(stderr, "D: [%s]: %s\n", _log_datetime_now(datetm, sizeof(datetm)), _log_buffer);
	fflush(stderr);

	mtx_unlock(&_log_mutex); // unlock
#endif
}


void
log_info(const char fmt[], ...)
{
	int ret;
	va_list va;
	char datetm[32];

	if (_log_buffer == NULL)
		return;

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

