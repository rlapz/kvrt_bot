#ifndef __UTIL_H__
#define __UTIL_H__


#include <stddef.h>
#include <threads.h>

#include "config.h"


#define LEN(X)                          ((sizeof(X) / sizeof(*X)))
#define FIELD_PARENT_PTR(T, FIELD, PTR) ((T *)(((char *)(PTR)) - offsetof(T, FIELD)))


/*
 * cstr
 */
void  cstr_copy(char dest[], const char src[]);
void  cstr_copy_n(char dest[], size_t size, const char src[]);
void  cstr_copy_n2(char dest[], size_t size, const char src[], size_t len);

/* ret: ~0: equals */
int   cstr_cmp_n(const char a[], const char b[], size_t b_len);
int   cstr_casecmp_n(const char a[], const char b[], size_t b_len);

char *cstr_trim_l(char dest[]);
char *cstr_trim_r(char dest[]);


/*
 * BotCmd
 */
#define BOT_CMD_ARGS_SIZE CFG_UTIL_BOT_CMD_ARGS_SIZE

typedef struct bot_cmd_arg {
	const char *name;
	unsigned    len;
} BotCmdArg;

unsigned bot_cmd_args_parse(BotCmdArg a[], unsigned size, const char src[]);


typedef struct bot_cmd {
	const char *name;
	unsigned    name_len;
	unsigned    args_len;
	BotCmdArg   args[BOT_CMD_ARGS_SIZE];
} BotCmd;

int bot_cmd_parse(BotCmd *b, char prefix, const char src[]);


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
char *str_dup(Str *s);


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
 * Chld
 */
#define CHLD_ITEM_SIZE CFG_UTIL_CHLD_ITEMS_SIZE

typedef struct chld {
	unsigned count;
	unsigned slots[CHLD_ITEM_SIZE];
	unsigned entries[CHLD_ITEM_SIZE];
	pid_t    pids[CHLD_ITEM_SIZE];
	mtx_t    mutex;
} Chld;

int  chld_init(Chld *c);
void chld_deinit(Chld *c);
int  chld_spawn(Chld *c, const char path[], char *const argv[]);
void chld_reap(Chld *c);


/*
 * Log
 */
int  log_init(size_t buffer_size);
void log_deinit(void);
void log_err(int errnum, const char fmt[], ...);
void log_debug(const char fmt[], ...);
void log_info(const char fmt[], ...);


#endif
