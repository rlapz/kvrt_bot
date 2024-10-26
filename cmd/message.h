#ifndef __MESSAGE_H__
#define __MESSAGE_H__


#include "../update.h"


int  cmd_message_send(Update *u, const TgMessage *msg, const BotCmd *cmd);
void cmd_message_list(Update *u);


#endif
