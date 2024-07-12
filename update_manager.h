#ifndef __UPDATE_MANAGER_H__
#define __UPDATE_MANAGER_H__


#include <json.h>

#include "db.h"
#include "module.h"
#include "tg_api.h"
#include "util.h"


typedef struct update_manager {
	TgApi   api;
	Module  module;
	Db     *db;
} UpdateManager;

int  update_manager_init(UpdateManager *u, const char base_api[], Db *db);
void update_manager_deinit(UpdateManager *u);
void update_manager_handle(UpdateManager *u, json_object *json_obj);


#endif
