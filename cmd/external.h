#ifndef __EXTERNAL_H__
#define __EXTERNAL_H__


#include "../update.h"


int  cmd_external_exec(Update *u, const TgMessage *msg, const BotCmd *cmd, json_object *json);
void cmd_external_list(Update *u);


#endif
