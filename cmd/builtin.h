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


/* see: builtin.c */
extern const CmdBuiltin cmd_builtins[];


#endif
