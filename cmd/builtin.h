#ifndef __BUILTIN_H__
#define __BUILTIN_H__


#include "../update.h"


int cmd_builtin_exec(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json);


#endif
