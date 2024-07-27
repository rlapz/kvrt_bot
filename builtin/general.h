#ifndef __BUILTIN_GENERAL_H__
#define __BUILTIN_GENERAL_H__


#include <json.h>

#include "../module.h"
#include "../tg.h"
#include "../util.h"


int  general_message(Module *m, const TgMessage *message, const char cmd[], unsigned len);
void general_dump(Module *m, const TgMessage *message, json_object *json_obj);
void general_dump_admin(Module *m, const TgMessage *message);
void general_cmd_set(Module *m, const TgMessage *message, const BotCmdArg args[], unsigned args_len);

void general_test(Module *m, const TgMessage *message, const BotCmdArg args[], unsigned args_len);
void general_inval(Module *m, const TgMessage *message);


#endif
