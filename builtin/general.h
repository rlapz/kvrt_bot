#ifndef __BUILTIN_GENERAL_H__
#define __BUILTIN_GENERAL_H__


#include <json.h>

#include "../tg.h"
#include "../module.h"


void general_start(Module *m, const TgMessage *message);
void general_help(Module *m, const TgMessage *message);
void general_settings(Module *m, const TgMessage *message);
void general_dump(Module *m, const TgMessage *message, json_object *json_obj);
void general_inval(Module *m, const TgMessage *message);


#endif
