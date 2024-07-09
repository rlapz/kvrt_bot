#ifndef __MODULE_H__
#define __MODULE_H__


#include <json.h>

#include "tg.h"
#include "tg_api.h"
#include "util.h"


typedef struct module {
	TgApi *api;
	Str    str;
} Module;

int  module_init(Module *m, TgApi *api);
void module_deinit(Module *m);
void module_builtin_handle_text(Module *m, const TgMessage *msg, const char text[]);
void module_builtin_handle_command(Module *m, json_object *json_obj, const char cmd[],
				   const TgMessage *msg, const char *args);


#endif
