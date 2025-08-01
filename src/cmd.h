#ifndef __CMD_H__
#define __CMD_H__


#include <json.h>

#include "common.h"
#include "model.h"
#include "tg.h"
#include "util.h"


typedef struct cmd_param {
	int64_t          id_bot;
	int64_t          id_owner;
	int64_t          id_user;
	int64_t          id_chat;
	int64_t          id_message;
	const char      *id_callback;	/* NULL: not a callback */
	const char      *bot_username;
	const TgMessage *msg;
	json_object     *json;

	/* filled by cmd_exec() */
	int         has_username;
	char        name[MODEL_CMD_NAME_SIZE];
	const char *args;
} CmdParam;

int  cmd_init(void);
void cmd_exec(CmdParam *cmd, const char req[]);
int  cmd_get_list(ModelCmd list[], int len, MessageListPagination *pag, int flags, int is_private);


typedef struct cmd_builtin {
	int         flags;		/* model.h: MODEL_CMD_FLAG_* */
	const char *name;
	const char *description;
	void       (*callback_fn)(const CmdParam *);
} CmdBuiltin;


/*
 * Handlers
 */
void cmd_admin_reload(const CmdParam *cmd);
void cmd_admin_cmd_message(const CmdParam *cmd);
void cmd_admin_settings(const CmdParam *cmd);

void cmd_general_start(const CmdParam *cmd);
void cmd_general_help(const CmdParam *cmd);
void cmd_general_dump(const CmdParam *cmd);
void cmd_general_dump_admin(const CmdParam *cmd);
void cmd_general_schedule_message(const CmdParam *cmd);
void cmd_general_deleter(const CmdParam *cmd);

void cmd_extra_anime_sched(const CmdParam *cmd);


#ifdef DEBUG
void cmd_test_echo(const CmdParam *cmd);
void cmd_test_sched(const CmdParam *cmd);
void cmd_test_nsfw(const CmdParam *cmd);
void cmd_test_admin(const CmdParam *cmd);
void cmd_test_list(const CmdParam *cmd);
void cmd_test_photo(const CmdParam *cmd);
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
	.flags = MODEL_CMD_FLAG_DISALLOW_PRIVATE_CHAT,				\
},										\
{										\
	.name = "/sched",							\
	.description = "Schedule message",					\
	.callback_fn = cmd_general_schedule_message,				\
},										\
{										\
	.name = "/deleter",							\
	.description = "Message deleter",					\
	.callback_fn = cmd_general_deleter,					\
	.flags = MODEL_CMD_FLAG_CALLBACK | MODEL_CMD_FLAG_HIDDEN,		\
}


#define CMD_BUILTIN_LIST_ADMIN 							\
{										\
	.name = "/admin_reload",						\
	.description = "Reload admin list",					\
	.callback_fn = cmd_admin_reload,					\
	.flags = MODEL_CMD_FLAG_ADMIN | MODEL_CMD_FLAG_DISALLOW_PRIVATE_CHAT,	\
},										\
{										\
	.name = "/msg_set",							\
	.description = "Set/unset CMD Message",					\
	.callback_fn = cmd_admin_cmd_message,					\
	.flags = MODEL_CMD_FLAG_ADMIN | MODEL_CMD_FLAG_DISALLOW_PRIVATE_CHAT,	\
},										\
{										\
	.name = "/settings",							\
	.description = "Set bot configurations",				\
	.callback_fn = cmd_admin_settings,					\
	.flags = MODEL_CMD_FLAG_ADMIN,						\
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
},										\
{										\
	.name = "/test_sched",							\
	.description = "test sched",						\
	.callback_fn = cmd_test_sched,						\
},										\
{										\
	.name = "/test_nsfw",							\
	.description = "test nsfw",						\
	.callback_fn = cmd_test_nsfw,						\
	.flags = MODEL_CMD_FLAG_NSFW | MODEL_CMD_FLAG_EXTRA,			\
},										\
{										\
	.name = "/test_admin",							\
	.description = "test admin",						\
	.callback_fn = cmd_test_admin,						\
	.flags = MODEL_CMD_FLAG_ADMIN,						\
},										\
{										\
	.name = "/test_list",							\
	.description = "test message list",					\
	.callback_fn = cmd_test_list,						\
	.flags = MODEL_CMD_FLAG_CALLBACK,					\
},										\
{										\
	.name = "/test_photo",							\
	.description = "test send photo",					\
	.callback_fn = cmd_test_photo,						\
}
#else
#define CMD_BUILTIN_LIST_TEST { 0 }
#endif


#endif
