#ifndef __MODULE_H__
#define __MODULE_H__


#include <stdint.h>
#include <json.h>

#include <update.h>
#include <tg_api.h>
#include <util.h>


int module_builtin_exec(Update *update, const TgMessage *msg, const BotCmd *cmd, json_object *json);
int module_extern_exec(Update *update, const TgMessage *msg, const BotCmd *cmd, json_object *json);


#endif
