#ifndef __MODULE_H__
#define __MODULE_H__


#include <stdint.h>
#include <json.h>

#include "tg.h"
#include "tg_api.h"
#include "db.h"
#include "util.h"


typedef struct module {
	int64_t     owner_id;
	TgApi      *api;
	Db         *db;
	Str         str;
	const char *cmd_path;
} Module;

int  module_init(Module *m, int64_t owner_id, TgApi *api, Db *db, const char cmd_path[]);
void module_deinit(Module *m);

void module_handle_text(Module *m, const TgMessage *msg, json_object *json_obj);
void module_handle_command(Module *m, const TgMessage *msg, json_object *json_obj);
void module_handle_inline_query(Module *m, const TgInlineQuery *query, json_object *json_obj);
void module_handle_callback_query(Module *m, const TgCallbackQuery *query, json_object *json_obj);


#endif
