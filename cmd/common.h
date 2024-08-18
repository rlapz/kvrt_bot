#ifndef __COMMON_H__
#define __COMMON_H__


#include "../update.h"


int  common_privileges_check(Update *u, const TgMessage *msg);
int  common_cmd_message(Update *u, const TgMessage *msg, const BotCmd *cmd);
void common_cmd_invalid(Update *u, const TgMessage *msg);
void common_admin_reload(Update *u, const TgMessage *msg, int need_reply);


#endif
