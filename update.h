#ifndef __UPDATE_H__
#define __UPDATE_H__


#include <json.h>
#include <stdint.h>

#include "db.h"
#include "util.h"
#include "tg.h"
#include "tg_api.h"


typedef struct update {
	int64_t     bot_id;
	int64_t     owner_id;
	const char *base_api;
	const char *cmd_path;
	Chld       *chld;
	Str         str;
	Db          db;
	TgApi       api;
} Update;

int  update_init(Update *u, int64_t bot_id, int64_t owner_id, const char base_api[],
		 const char db_path[], Chld *chld);
void update_deinit(Update *u);
void update_handle(Update *u, json_object *json);


#endif
