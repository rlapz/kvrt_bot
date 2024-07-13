#ifndef __MODULE_H__
#define __MODULE_H__


#include <json.h>

#include "tg.h"
#include "tg_api.h"
#include "db.h"
#include "util.h"


typedef struct module {
	TgApi *api;
	Db    *db;
	Str    str;
} Module;

int  module_init(Module *m, TgApi *api, Db *db);
void module_deinit(Module *m);

void module_handle_text(Module *m, const TgMessage *msg, json_object *json_obj);
void module_handle_command(Module *m, const char cmd[], const TgMessage *msg, json_object *json_obj,
			   const char args[]);
void module_handle_media(Module *m, const TgMessage *msg, json_object *json_obj);


#endif
