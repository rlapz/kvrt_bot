#ifndef __CMD_H__
#define __CMD_H__


#include <json.h>

#include "common.h"
#include "model.h"
#include "tg.h"
#include "util.h"


typedef struct cmd {
	int              is_callback;
	int64_t          id_bot;
	int64_t          id_owner;
	const char      *username;
	const TgMessage *msg;
	union {
		struct {
			BotCmd       bot_cmd;
			json_object *json;
		};
		struct {
			CallbackQuery          query;
			const TgCallbackQuery *callback;
		};
	};
} Cmd;

int  cmd_init(void);
void cmd_deinit(void);
void cmd_exec(const Cmd *c);


typedef struct cmd_builtin {
	int         flags;		/* model.h: MODEL_CMD_FLAG_* */
	const char *name;
	const char *description;
	void       (*callback_fn)(const Cmd *);
} CmdBuiltin;

int cmd_builtin_get_list(CmdBuiltin *list[], int chat_flags, unsigned *start_num,
			 MessageListPagination *pag);
int cmd_builtin_is_exists(const char name[]);


/*
 * Handlers
 */
void cmd_admin_reload(const Cmd *cmd);
void cmd_admin_cmd_message(const Cmd *cmd);
void cmd_admin_params(const Cmd *cmd);

void cmd_general_start(const Cmd *cmd);
void cmd_general_help(const Cmd *cmd);
void cmd_general_dump(const Cmd *cmd);
void cmd_general_dump_admin(const Cmd *cmd);

void cmd_extra_anime_sched(const Cmd *cmd);


#ifdef DEBUG
void cmd_test_echo(const Cmd *cmd);
void cmd_test_sched(const Cmd *cmd);
void cmd_test_nsfw(const Cmd *cmd);
void cmd_test_admin(const Cmd *cmd);
void cmd_test_list(const Cmd *cmd);
#endif


/*
 * CmdBuiltin
 */
#define CMD_BUILTIN_LIST_GENERAL 						\
{										\
	.name = "/start",							\
	.description = "Start command",						\
	.callback_fn = cmd_general_start,					\
},										\
{										\
	.name = "/help",							\
	.description = "Show help",						\
	.callback_fn = cmd_general_help,					\
	.flags = MODEL_CMD_FLAG_CALLBACK,					\
},										\
{										\
	.name = "/dump",							\
	.description = "Dump raw json",						\
	.callback_fn = cmd_general_dump,					\
},										\
{										\
	.name = "/admin_dump",							\
	.description = "Dump admin list in raw json",				\
	.callback_fn = cmd_general_dump_admin,					\
}


#define CMD_BUILTIN_LIST_ADMIN 							\
{										\
	.name = "/admin_reload",						\
	.description = "Reload admin list",					\
	.callback_fn = cmd_admin_reload,					\
	.flags = MODEL_CMD_FLAG_ADMIN,						\
},										\
{										\
	.name = "/msg_set",							\
	.description = "Set/unset CMD Message",					\
	.callback_fn = cmd_admin_cmd_message,					\
	.flags = MODEL_CMD_FLAG_ADMIN,						\
},										\
{										\
	.name = "/params",							\
	.description = "Set/unset bot parameters",				\
	.callback_fn = cmd_admin_params,					\
	.flags = MODEL_CMD_FLAG_ADMIN | MODEL_CMD_FLAG_CALLBACK,		\
}

#define CMD_BUILTIN_LIST_EXTRA							\
{										\
	.name = "/anime_sched",							\
	.description = "Get anime schedule list",				\
	.callback_fn = cmd_extra_anime_sched,					\
	.flags = MODEL_CMD_FLAG_EXTRA | MODEL_CMD_FLAG_CALLBACK,		\
}


#ifdef DEBUG
#define CMD_BUILTIN_LIST_TEST 							\
{										\
	.name = "/test_echo",							\
	.description = "just a test",						\
	.callback_fn = cmd_test_echo,						\
	.flags = MODEL_CMD_FLAG_TEST,						\
},										\
{										\
	.name = "/test_sched",							\
	.description = "test sched",						\
	.callback_fn = cmd_test_sched,						\
	.flags = MODEL_CMD_FLAG_TEST,						\
},										\
{										\
	.name = "/test_nsfw",							\
	.description = "test nsfw",						\
	.callback_fn = cmd_test_nsfw,						\
	.flags = MODEL_CMD_FLAG_TEST | MODEL_CMD_FLAG_NSFW | MODEL_CMD_FLAG_EXTRA,\
},										\
{										\
	.name = "/test_admin",							\
	.description = "test admin",						\
	.callback_fn = cmd_test_admin,						\
	.flags = MODEL_CMD_FLAG_TEST | MODEL_CMD_FLAG_ADMIN,			\
},										\
{										\
	.name = "/test_list",							\
	.description = "test message list",					\
	.callback_fn = cmd_test_list,						\
	.flags = MODEL_CMD_FLAG_TEST | MODEL_CMD_FLAG_CALLBACK,			\
}
#else
#define CMD_BUILTIN_LIST_TEST { 0 }
#endif


#endif
