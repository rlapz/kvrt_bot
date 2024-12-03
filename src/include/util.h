#ifndef __UTIL_H__
#define __UTIL_H__


#include <fcntl.h>
#include <stddef.h>
#include <threads.h>

#include <config.h>


#define LEN(X)                          ((sizeof(X) / sizeof(*X)))
#define FIELD_PARENT_PTR(T, FIELD, PTR) ((T *)(((char *)(PTR)) - offsetof(T, FIELD)))
#define STR_HELPER(X)                   #X
#define STR(X)                          STR_HELPER(X)


/*
 * cstr
 */
size_t cstr_copy(char dest[], const char src[]);
size_t cstr_copy_n(char dest[], size_t size, const char src[]);
size_t cstr_copy_n2(char dest[], size_t size, const char src[], size_t len);

/* ret: ~0: equals */
int cstr_cmp_n(const char a[], const char b[], size_t b_len);
int cstr_casecmp_n(const char a[], const char b[], size_t b_len);

char *cstr_trim_l(char dest[]);
char *cstr_trim_r(char dest[]);
char *cstr_escape(char dest[], const char esc[], int c, const char src[]);

long long          cstr_to_llong_n(const char cstr[], size_t len);
unsigned long long cstr_to_ullong_n(const char cstr[], size_t len);


/*
 * SpaceTokenizer
 */
typedef struct space_tokenizer {
	const char *value;
	unsigned    len;
} SpaceTokenizer;

const char *space_tokenizer_next(SpaceTokenizer *s, const char raw[]);


/*
 * BotCmd
 */
#define BOT_CMD_ARGS_SIZE CFG_UTIL_BOT_CMD_ARGS_SIZE

typedef struct bot_cmd_arg {
	const char *value;
	unsigned    len;
} BotCmdArg;

typedef struct bot_cmd {
	const char *name;
	unsigned    name_len;
	unsigned    args_len;
	BotCmdArg   args[BOT_CMD_ARGS_SIZE];
} BotCmd;

int bot_cmd_parse(BotCmd *b, char prefix, const char src[]);
int bot_cmd_compare(const BotCmd *b, const char cmd[]);


/*
 * CallbackQuery
 */
#define CALLBACK_QUERY_ARGS_SIZE CFG_UTIL_CALLBACK_QUERY_ITEMS_SIZE

typedef struct callback_query_arg {
	const char *value;
	unsigned    len;
} CallbackQueryArg;

typedef struct callback_query {
	const char       *context;
	unsigned          context_len;
	unsigned          args_len;
	CallbackQueryArg  args[CALLBACK_QUERY_ARGS_SIZE];
} CallbackQuery;

int      callback_query_parse(CallbackQuery *c, const char src[]);
int      callback_query_compare(const CallbackQuery *c, const char callback[]);
unsigned callback_query_merge(const CallbackQuery *c, char buffer[], unsigned size);


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
 * Mp: Memory pool
 */
typedef struct mp_node MpNode;

typedef struct mp {
	size_t  nmemb;
	MpNode *head;
} Mp;

int   mp_init(Mp *m, size_t nmemb, size_t size);
void  mp_deinit(Mp *m);
void *mp_alloc(Mp *m);
void  mp_reserve(Mp *m, void *mem);


/*
 * Chld
 */
#define CHLD_ITEMS_SIZE CFG_UTIL_CHLD_ITEMS_SIZE

typedef struct chld {
	const char *path;
	char       *const *envp;
	Str         str;
	unsigned    count;
	unsigned    slots[CHLD_ITEMS_SIZE];
	unsigned    entries[CHLD_ITEMS_SIZE];
	pid_t       pids[CHLD_ITEMS_SIZE];
	mtx_t       mutex;
} Chld;

int  chld_init(Chld *c, const char path[], char *const envp[]);
void chld_deinit(Chld *c);
int  chld_spawn(Chld *c, const char file[], char *const argv[]);
void chld_reap(Chld *c);
void chld_wait_all(Chld *c);


/*
 * Log
 */
int  log_init(size_t buffer_size);
void log_deinit(void);
void log_err(int errnum, const char fmt[], ...);
void log_debug(const char fmt[], ...);
void log_info(const char fmt[], ...);


#endif
