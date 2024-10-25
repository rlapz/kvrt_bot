#ifndef __BUILTIN_H__
#define __BUILTIN_H__


#include "../update.h"


typedef void (*CmdBuiltinFn)(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json);

typedef struct cmd_builtin {
	const char   *name;
	const char   *description;
	CmdBuiltinFn  func;
	int           admin_only;
} CmdBuiltin;


int builtin_cmd_message(Update *u, const TgMessage *msg, const BotCmd *cmd);


/* see: builtin.c */
/* TODO: using string hashmap */
extern const CmdBuiltin builtin_cmds[];


#endif
