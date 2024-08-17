#ifndef __MODULE_H__
#define __MODULE_H__


#include <stdint.h>
#include <json.h>

#include "tg.h"
#include "tg_api.h"
#include "db.h"
#include "util.h"


typedef struct module {
	int64_t     bot_id;
	int64_t     owner_id;
	TgApi       api;
	Db          db;
	Str         str;
	Chld       *chld;
	const char *cmd_path;
} Module;

int  module_init(Module *m, Chld *chld, int64_t bot_id, int64_t owner_id, const char base_api[],
		 const char cmd_path[], const char db_path[]);
void module_deinit(Module *m);
void module_handle(Module *m, json_object *json_obj);


#endif
